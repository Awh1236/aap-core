#ifndef AAP_CORE_AAPXS_RUNTIME_H
#define AAP_CORE_AAPXS_RUNTIME_H

// AAPXS v2 runtime - strongly typed.

#include <assert.h>
#include <cstdint>
#include <vector>
#include <future>
#include "../../unstable/aapxs-vnext.h"
#include "../../android-audio-plugin.h"

namespace aap {
    template<typename T, typename R>
    struct WithPromise {
        T* context;
        std::promise<R>* promise;
    };

    class UridMapping {
        std::vector<uint8_t> urids{};
        std::vector<const char*> uris{};

    public:
        static const uint8_t UNMAPPED_URID = 0;

        UridMapping() {
            urids.resize(UINT8_MAX);
            uris.resize(UINT8_MAX);
        }

        uint8_t tryAdd(const char* uri) {
            assert(uris.size() < UINT8_MAX);
            auto existing = getUrid(uri);
            if (existing > 0)
                return existing;
            uris.emplace_back(uri);
            uint8_t urid = uris.size();
            urids.emplace_back(urid);
            return urid;
        }

        uint8_t getUrid(const char* uri) {
            // starts from 1, as 0 is "unmapped"
            for (size_t i = 1, n = uris.size(); i < n; i++)
                if (uri == uris[n])
                    return urids[i];
            for (size_t i = 1, n = uris.size(); i < n; i++)
                if (!strcmp(uri, uris[n]))
                    return urids[i];
            return UNMAPPED_URID;
        }

        const char* getUri(uint8_t urid) {
            // starts from 1, as 0 is "unmapped"
            for (size_t i = 1, n = urids.size(); i < n; i++)
                if (urid == urids[n])
                    return uris[i];
            return nullptr;
        }
    };

    template <typename T>
    class AAPXSUridMapping {
        UridMapping urid_mapping{};
        std::vector<T> items{};
        bool frozen{false};

    public:
        AAPXSUridMapping() {
            items.resize(UINT8_MAX);
        }
        virtual ~AAPXSUridMapping() {}

        UridMapping* getUridMapping() { return &urid_mapping; }

        inline void freezeFeatureSet() { frozen = true; }
        inline bool isFrozen() { return frozen; }

        void add(T feature, const char* uri) {
            uint8_t urid = urid_mapping.tryAdd(uri);
            items[urid] = feature;
        }
        T& getByUri(const char* uri) {
            uint8_t urid = urid_mapping.getUrid(uri);
            return items[urid];
        }
        T& getByUrid(uint8_t urid) {
            return items[urid];
        }

        using container=std::vector<T>;
        using iterator=typename container::iterator;
        using const_iterator=typename container::const_iterator;

        iterator begin() { return items.begin(); }
        iterator end() { return items.end(); }
        const_iterator begin() const { return items.begin(); }
        const_iterator end() const { return items.end(); }
    };

    class AAPXSInitiatorInstanceMap : public AAPXSUridMapping<AAPXSInitiatorInstance> {};
    class AAPXSRecipientInstanceMap : public AAPXSUridMapping<AAPXSRecipientInstance> {};

    class AAPXSDefinitionClientRegistry;
    class AAPXSDefinitionServiceRegistry;

    typedef uint32_t (*initiator_get_new_request_id_func) (AAPXSInitiatorInstance* instance);
    typedef uint32_t (*recipient_get_new_request_id_func) (AAPXSRecipientInstance* instance);
    typedef void (*aapxs_initiator_send_func) (AAPXSInitiatorInstance* instance, AAPXSRequestContext* context);
    typedef void (*aapxs_initiator_receive_func) (AAPXSInitiatorInstance* instance, AAPXSRequestContext* context);
    typedef void (*aapxs_recipient_send_func) (AAPXSRecipientInstance* instance, AAPXSRequestContext* context);
    typedef void (*aapxs_recipient_receive_func) (AAPXSRecipientInstance* instance, AAPXSRequestContext* context);

    class AAPXSDispatcher {
    protected:
        AAPXSInitiatorInstanceMap initiators{};
        AAPXSRecipientInstanceMap recipients{};

        inline void addInitiator(AAPXSInitiatorInstance initiator, const char* uri) { initiators.add(initiator, uri); }
        inline void addRecipient(AAPXSRecipientInstance recipient, const char* uri) { recipients.add(recipient, uri); }
    };

    class AAPXSClientDispatcher : public AAPXSDispatcher {
        AAPXSInitiatorInstance
        populateAAPXSInitiatorInstance(AAPXSSerializationContext *serialization,
                                       aapxs_initiator_send_func sendAAPXSRequest,
                                       aapxs_initiator_receive_func processIncomingAAPXSReply,
                                       initiator_get_new_request_id_func getNewRequestId);
        AAPXSRecipientInstance
        populateAAPXSRecipientInstance(AAPXSSerializationContext *serialization,
                                       aapxs_recipient_receive_func processIncomingAapxsRequest,
                                       aapxs_recipient_send_func sendAapxsReply,
                                       recipient_get_new_request_id_func recipientGetNewRequestId);

    public:
        AAPXSClientDispatcher(AAPXSDefinitionClientRegistry* registry);

        inline AAPXSInitiatorInstance& getPluginAAPXSByUri(const char* uri) { return initiators.getByUri(uri); }
        inline AAPXSInitiatorInstance& getPluginAAPXSByUrid(uint8_t urid) { return initiators.getByUrid(urid); };
        inline AAPXSRecipientInstance& getHostAAPXSByUri(const char* uri) { return recipients.getByUri(uri); }
        inline AAPXSRecipientInstance& getHostAAPXSByUrid(uint8_t urid) { return recipients.getByUrid(urid); };

        void
        setupInstances(AAPXSDefinitionClientRegistry *registry,
                       AAPXSSerializationContext *serialization,
                       aapxs_initiator_send_func sendAAPXSRequest,
                       aapxs_initiator_receive_func processIncomingAAPXSReply,
                       aapxs_recipient_receive_func processIncomingAAPXSRequestFunc,
                       aapxs_recipient_send_func sendAAPXSReplyFunc,
                       initiator_get_new_request_id_func initiatorGetNewRequestId,
                       recipient_get_new_request_id_func recipientGetNewRequestId);
    };

    class AAPXSServiceDispatcher : public AAPXSDispatcher {
        AAPXSInitiatorInstance populateAAPXSInitiatorInstance(
                AAPXSSerializationContext* serialization,
                aapxs_initiator_send_func sendHostAAPXSRequest,
                aapxs_initiator_receive_func processIncomingHostAAPXSReply,
                initiator_get_new_request_id_func getNewRequestId);
        AAPXSRecipientInstance populateAAPXSRecipientInstance(
                AAPXSSerializationContext* serialization,
                aap::aapxs_recipient_receive_func processIncomingAAPXSRequest,
                aap::aapxs_recipient_send_func sendAAPXSReply,
                recipient_get_new_request_id_func getNewRequestId);

    public:
        AAPXSServiceDispatcher(AAPXSDefinitionServiceRegistry* registry);

        AAPXSRecipientInstance& getPluginAAPXSByUri(const char* uri) { return recipients.getByUri(uri); }
        AAPXSRecipientInstance& getPluginAAPXSByUrid(uint8_t urid) { return recipients.getByUrid(urid); };
        AAPXSInitiatorInstance& getHostAAPXSByUri(const char* uri) { return initiators.getByUri(uri); }
        AAPXSInitiatorInstance& getHostAAPXSByUrid(uint8_t urid) { return initiators.getByUrid(urid); };

        void
        setupInstances(AAPXSDefinitionServiceRegistry *registry,
                       AAPXSSerializationContext *serialization,
                       aapxs_initiator_send_func sendAAPXSRequest,
                       aapxs_initiator_receive_func processIncomingAAPXSReply,
                       aapxs_recipient_receive_func processIncomingAapxsRequest,
                       aapxs_recipient_send_func sendAapxsReply,
                       initiator_get_new_request_id_func initiatorGetNewRequestId,
                       recipient_get_new_request_id_func recipientGetNewRequestId);
    };

    class AAPXSDefinitionClientRegistry : public AAPXSUridMapping<AAPXSDefinition*> {
    public:
        virtual void setupClientInstances(aap::AAPXSClientDispatcher *client, AAPXSSerializationContext* serialization) = 0;
    };

    class AAPXSDefinitionServiceRegistry : public AAPXSUridMapping<AAPXSDefinition*> {
    public:
        virtual void setupServiceInstances(aap::AAPXSServiceDispatcher *client, AAPXSSerializationContext* serialization) = 0;
    };
}

#endif //AAP_CORE_AAPXS_RUNTIME_H

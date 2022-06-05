#ifndef AAP_CORE_STATE_SERVICE_H
#define AAP_CORE_STATE_SERVICE_H

#include <cstdint>
#include <functional>
#include "aap/android-audio-plugin.h"
#include "aap/unstable/aapxs.h"
#include "aap/unstable/state.h"
#include "aap/unstable/logging.h"
#include "aap/core/host/audio-plugin-host.h"
#include "aap/core/host/extension-service.h"
#include "extension-service-impl.h"

namespace aap {

// implementation details
    const int32_t OPCODE_GET_STATE_SIZE = 0;
    const int32_t OPCODE_GET_STATE = 1;
    const int32_t OPCODE_SET_STATE = 2;

    const int32_t STATE_SHARED_MEMORY_SIZE = 0x100000; // 1M

    class StatePluginClientExtension : public PluginClientExtensionImplBase {
    public:
        class Instance {
            friend class StatePluginClientExtension;

            aap_state_extension_t proxy{};

            static int32_t internalGetStateSize(AndroidAudioPlugin *plugin);

            static void internalGetState(AndroidAudioPlugin *plugin, aap_state_t *state);

            static void internalSetState(AndroidAudioPlugin *plugin, aap_state_t *state);

            StatePluginClientExtension *owner;
            AAPXSClientInstance* aapxsInstance;

        public:
            Instance(StatePluginClientExtension *owner, AAPXSClientInstance *clientInstance);

            void clientInvokePluginExtension(int32_t opcode) {
                owner->clientInvokePluginExtension(aapxsInstance, opcode);
            }

            int32_t getStateSize() {
                clientInvokePluginExtension(OPCODE_GET_STATE_SIZE);
                return *((int32_t *) aapxsInstance->data);
            }

            void getState(aap_state_t* result) {
                clientInvokePluginExtension(OPCODE_GET_STATE);
                result->data_size = *((int32_t *) aapxsInstance->data);
                memcpy(result->data, (int32_t*) aapxsInstance->data + 1, result->data_size);
            }

            void setState(aap_state_t* source) {
                *((int32_t *) aapxsInstance->data) = source->data_size;
                memcpy((int32_t*) aapxsInstance->data + 1, source->data, source->data_size);
                clientInvokePluginExtension(OPCODE_SET_STATE);
            }

            void *asProxy() {
                proxy.get_state_size = internalGetStateSize;
                proxy.get_state = internalGetState;
                proxy.set_state = internalSetState;
                return &proxy;
            }
        };
    private:

// FIXME: This tells there is maximum # of instances - we need some better method to retain pointers
//  to each Instance that at least lives as long as AAPXSClientInstance lifetime.
//  (Should we add `addDisposableListener` at AAPXSClient to make it possible to free
//  this Instance at plugin instance disposal? Maybe when if 1024 for instances sounds insufficient...)
#define STATE_MAX_INSTANCE_COUNT 1024

        std::unique_ptr<Instance> instances[STATE_MAX_INSTANCE_COUNT]{};
        std::map<int32_t,int32_t> instance_map{}; // map from instanceId to the index of the Instance in `instances`.

    public:
        StatePluginClientExtension()
                : PluginClientExtensionImplBase() {
        }

        void *asProxy(AAPXSClientInstance *clientInstance) override {
            size_t last = 0;
            for (; last < STATE_MAX_INSTANCE_COUNT; last++) {
                if (instances[last] == nullptr)
                    break;
                if (instances[last]->aapxsInstance == clientInstance)
                    return instances[instance_map[last]]->asProxy();
            }
            instances[last] = std::make_unique<Instance>(this, clientInstance);
            instance_map[clientInstance->plugin_instance_id] = (int32_t) last;
            return instances[last]->asProxy();
        }
    };

    class StatePluginServiceExtension : public PluginServiceExtensionImplBase {

        template<typename T>
        void withStateExtension(aap::LocalPluginInstance *instance, T defaultValue,
                                 std::function<void(aap_state_extension_t *,
                                                    AndroidAudioPlugin *)> func);

    public:
        StatePluginServiceExtension()
                : PluginServiceExtensionImplBase(AAP_STATE_EXTENSION_URI) {
        }

        // invoked by AudioPluginService
        void onInvoked(void* contextInstance, AAPXSServiceInstance *extensionInstance,
                       int32_t opcode) override;
    };


    class StateExtensionFeature : public PluginExtensionFeatureImpl {
        std::unique_ptr<PluginClientExtensionImplBase> client;
        std::unique_ptr<PluginServiceExtensionImplBase> service;

    public:
        StateExtensionFeature()
                : PluginExtensionFeatureImpl(AAP_STATE_EXTENSION_URI, STATE_SHARED_MEMORY_SIZE),
                  client(std::make_unique<StatePluginClientExtension>()),
                  service(std::make_unique<StatePluginServiceExtension>()) {
        }

        PluginClientExtensionImplBase* getClient() { return client.get(); }
        PluginServiceExtensionImplBase* getService() { return service.get(); }
    };

} // namespace aap


#endif //AAP_CORE_STATE_SERVICE_H

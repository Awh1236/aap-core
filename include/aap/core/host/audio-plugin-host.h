#pragma once

#ifndef _ANDROID_AUDIO_PLUGIN_HOST_HPP_
#define _ANDROID_AUDIO_PLUGIN_HOST_HPP_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <dlfcn.h>
#include <time.h>
#include <unistd.h>
#include <memory>
#include <vector>
#include <map>
#include <string>
#include "aap/android-audio-plugin.h"
#include "aap/port-properties.h"
#include "aap/unstable/logging.h"
#include "aap/ext/parameters.h"
#include "aap/ext/presets.h"
#include "aap/ext/state.h"
#include "aap/ext/port-config.h"
#include "aap/ext/plugin-info.h"
#include "plugin-connections.h"
#include "../aapxs/extension-service.h"
#include "../aapxs/standard-extensions.h"

namespace aap {

//-------------------------------------------------------

class PluginInstance;
class LocalPluginInstance;
class RemotePluginInstance;

/* Common foundation for both Plugin service and Plugin client. */
class PluginHost
{
	std::unique_ptr<AAPXSRegistry> aapxs_registry;

protected:
	PluginListSnapshot* plugin_list{nullptr};
	std::vector<PluginInstance*> instances{};
	PluginInstance* instantiateLocalPlugin(const PluginInformation *pluginInfo, int sampleRate);

public:
	PluginHost(PluginListSnapshot* contextPluginList);

	virtual ~PluginHost() {}

	AAPXSFeature* getExtensionFeature(const char* uri);

	void destroyInstance(PluginInstance* instance);

	size_t getInstanceCount() { return instances.size(); }

    // Note that the argument is NOT instanceId
	PluginInstance* getInstanceByIndex(int32_t index);

    PluginInstance* getInstanceById(int32_t instanceId);
};


class PluginService : public PluginHost {
public:
	PluginService(PluginListSnapshot* contextPluginList)
			: PluginHost(contextPluginList)
	{
	}

	int createInstance(std::string identifier, int sampleRate);

	inline LocalPluginInstance* getLocalInstance(int32_t instanceId) {
		return (LocalPluginInstance*) getInstanceById(instanceId);
	}
};

class PluginClient : public PluginHost {
	PluginClientConnectionList* connections;

	template<typename T>
	struct Result {
		T value;
		std::string error;
	};

	Result<int32_t> instantiateRemotePlugin(const PluginInformation *pluginInfo, int sampleRate);

public:
	PluginClient(PluginClientConnectionList* pluginConnections, PluginListSnapshot* contextPluginList)
		: PluginHost(contextPluginList), connections(pluginConnections)
	{
	}

	inline PluginClientConnectionList* getConnections() { return connections; }

	// Synchronous version that does not expect service connection on the fly (fails immediately).
	// It is probably better suited for Kotlin client to avoid complicated JNI interop.
	Result<int32_t> createInstance(std::string identifier, int sampleRate, bool isRemoteExplicit);

	// Asynchronous version that allows service connection on the fly.
	[[deprecated("ensureServiceConnected for async connection establishment, and then createInstance instead of this function.")]]
	void createInstanceAsync(std::string identifier, int sampleRate, bool isRemoteExplicit, std::function<void(int32_t, std::string&)>& callback);
};

//-------------------------------------------------------

class PluginBuffer;
class PluginSharedMemoryStore;

class PluginInstance
{
	friend class PluginHost;
	friend class PluginClient;
	friend class PluginListSnapshot;
    friend class LocalPluginInstanceStandardExtensions;
	friend class RemotePluginInstanceStandardExtensions;

	int sample_rate{44100};

	AndroidAudioPluginFactory *plugin_factory;
	std::unique_ptr<PluginBuffer> plugin_buffer{nullptr};

protected:
	int instance_id{-1};
    PluginInstantiationState instantiation_state{PLUGIN_INSTANTIATION_STATE_INITIAL};
	bool are_ports_configured{false};
	AndroidAudioPlugin *plugin;
	PluginSharedMemoryStore *aapxs_shared_memory_store;
    const PluginInformation *pluginInfo;
    std::unique_ptr<std::vector<PortInformation>> configured_ports{nullptr};
	std::unique_ptr<std::vector<ParameterInformation>> cached_parameters{nullptr};

	PluginInstance(const PluginInformation* pluginInformation, AndroidAudioPluginFactory* loadedPluginFactory, int sampleRate);

	virtual AndroidAudioPluginHost* getHostFacadeForCompleteInstantiation() = 0;

public:

    virtual ~PluginInstance();

    virtual int32_t getInstanceId() = 0;

    // As numPorts is required, the client and the plugin need agreement on how many ports will be used first (not including AAPXS).
	int32_t allocateAudioPluginBuffer(size_t numPorts, size_t numFrames, size_t defaultControlBytesPerBlock);

	// It may or may not be shared memory buffer.
	AndroidAudioPluginBuffer* getAudioPluginBuffer();

	PluginBuffer* getPluginBuffer() { return plugin_buffer.get(); }

	const PluginInformation* getPluginInformation()
	{
		return pluginInfo;
	}

	void completeInstantiation();

	// common to both service and client.
	void startPortConfiguration() {
		configured_ports = std::make_unique<std::vector<PortInformation>>();

		/* FIXME: enable this once we fix configurePorts() for service.
        // Add mandatory system common ports
        PortInformation core_midi_in{-1, "System Common Host-To-Plugin", AAP_CONTENT_TYPE_MIDI2, AAP_PORT_DIRECTION_INPUT};
        PortInformation core_midi_out{-2, "System Common Plugin-To-Host", AAP_CONTENT_TYPE_MIDI2, AAP_PORT_DIRECTION_OUTPUT};
        PortInformation core_midi_rt{-3, "System Realtime (HtP)", AAP_CONTENT_TYPE_MIDI2, AAP_PORT_DIRECTION_INPUT};
        configured_ports->emplace_back(core_midi_in);
        configured_ports->emplace_back(core_midi_out);
        configured_ports->emplace_back(core_midi_rt);
        */
	}

	void setupPortConfigDefaults();

	// This means that there was no configured ports by extensions.
	void setupPortsViaMetadata() {
		if (are_ports_configured)
			return;
		are_ports_configured = true;

		for (int i = 0, n = pluginInfo->getNumDeclaredPorts(); i < n; i++)
			configured_ports->emplace_back(PortInformation{*pluginInfo->getDeclaredPort(i)});
	}

	void scanParametersAndBuildList();

    int32_t getNumParameters() {
		return cached_parameters ? cached_parameters->size() : pluginInfo->getNumDeclaredParameters();
    }

    const ParameterInformation* getParameter(int32_t index) {
		if (!cached_parameters)
			return pluginInfo->getDeclaredParameter(index);
		assert(cached_parameters->size() > index);
		return &(*cached_parameters)[index];
    }

	int32_t getNumPorts() {
		return configured_ports ? configured_ports->size() : pluginInfo->getNumDeclaredPorts();
	}

	const PortInformation* getPort(int32_t index) {
		if (!configured_ports)
			return pluginInfo->getDeclaredPort(index);
		assert(configured_ports->size() > index);
		return &(*configured_ports)[index];
	}

	void prepare(int maximumExpectedSamplesPerBlock, AndroidAudioPluginBuffer *preparedBuffer)
	{
		assert(instantiation_state == PLUGIN_INSTANTIATION_STATE_UNPREPARED || instantiation_state == PLUGIN_INSTANTIATION_STATE_INACTIVE);

		plugin->prepare(plugin, preparedBuffer);
		instantiation_state = PLUGIN_INSTANTIATION_STATE_INACTIVE;
	}

	void activate()
	{
		if (instantiation_state == PLUGIN_INSTANTIATION_STATE_ACTIVE)
			return;
		assert(instantiation_state == PLUGIN_INSTANTIATION_STATE_INACTIVE);
		
		plugin->activate(plugin);
		instantiation_state = PLUGIN_INSTANTIATION_STATE_ACTIVE;
	}
	
	void deactivate()
	{
		if (instantiation_state == PLUGIN_INSTANTIATION_STATE_INACTIVE)
			return;
		assert(instantiation_state == PLUGIN_INSTANTIATION_STATE_ACTIVE);
		
		plugin->deactivate(plugin);
		instantiation_state = PLUGIN_INSTANTIATION_STATE_INACTIVE;
	}
	
	void dispose();

	// FIXME: we could simply remove this buffer argument, as nowadays it is acquired from getAudioPluginBuffer()
	void process(AndroidAudioPluginBuffer *buffer, int32_t timeoutInNanoseconds)
	{
		plugin->process(plugin, buffer, timeoutInNanoseconds);
	}

	virtual StandardExtensions& getStandardExtensions() = 0;

	uint32_t getTailTimeInMilliseconds()
	{
		// TODO: FUTURE - most likely just a matter of plugin property
		return 0;
	}
};

class LocalPluginInstanceStandardExtensionsImpl : public LocalPluginInstanceStandardExtensions {
	LocalPluginInstance *owner;

public:
	LocalPluginInstanceStandardExtensionsImpl(LocalPluginInstance* owner)
			: owner(owner) {
	}
	AndroidAudioPlugin* getPlugin() override;
};

/**
 * A plugin instance that could use dlopen() and dlsym(). It can be either client side or host side.
 */
class LocalPluginInstance : public PluginInstance {
	PluginHost *service;
	AndroidAudioPluginHost plugin_host_facade{};
	AAPXSInstanceMap<AAPXSServiceInstance> aapxsServiceInstances;
	LocalPluginInstanceStandardExtensionsImpl standards;
	aap_host_plugin_info_extension_t host_plugin_info{};
	aap_host_parameters_extension_t host_parameters_extension{};

	static aap_plugin_info_t get_plugin_info(AndroidAudioPluginHost* host, const char* pluginId);
	static enum aap_parameters_mapping_policy get_mapping_policy(AndroidAudioPluginHost* host, const char* pluginId);
	static void on_parameters_changed(AndroidAudioPluginHost* host, AndroidAudioPlugin* plugin);

	inline static void* internalGetExtensionData(AndroidAudioPluginHost *host, const char* uri) {
		if (strcmp(uri, AAP_PLUGIN_INFO_EXTENSION_URI) == 0) {
			auto instance = (LocalPluginInstance*) host->context;
			instance->host_plugin_info.get = get_plugin_info;
			return &instance->host_plugin_info;
		}
		if (strcmp(uri, AAP_PARAMETERS_EXTENSION_URI) == 0) {
			auto instance = (LocalPluginInstance*) host->context;
			instance->host_parameters_extension.get_user_mapping_policy = get_mapping_policy;
			instance->host_parameters_extension.on_parameters_changed = on_parameters_changed;
			return &instance->host_parameters_extension;
		}
		return nullptr;
	}

protected:
	AndroidAudioPluginHost* getHostFacadeForCompleteInstantiation() override;

public:
	LocalPluginInstance(PluginHost *service, int32_t instanceId, const PluginInformation* pluginInformation, AndroidAudioPluginFactory* loadedPluginFactory, int sampleRate);

    int32_t getInstanceId() override { return instance_id; }

    void confirmPorts() {
		// FIXME: implementation is feature parity with client side so far, but it should be based on port config negotiation.
		auto ext = plugin->get_extension(plugin, AAP_PORT_CONFIG_EXTENSION_URI);
		if (ext != nullptr) {
			// configure ports using port-config extension.

			// FIXME: implement

		} else if (pluginInfo->getNumDeclaredPorts() == 0)
			setupPortConfigDefaults();
		else
			setupPortsViaMetadata();
    }

	inline AndroidAudioPlugin* getPlugin() { return plugin; }

	// It often moved between LocalPluginInstance and PluginInstance - currently client does not need it.
	inline PluginSharedMemoryStore* getAAPXSSharedMemoryStore() { return aapxs_shared_memory_store; }

	// unlike client host side, this function is invoked for each `addExtension()` Binder call,
	// which is way simpler.
	AAPXSServiceInstance* setupAAPXSInstance(AAPXSFeature *feature, int32_t dataSize = -1) {
		const char* uri = aapxsServiceInstances.addOrGetUri(feature->uri);
		assert (aapxsServiceInstances.get(uri) == nullptr);
		if (dataSize < 0)
			dataSize = feature->shared_memory_size;
		aapxsServiceInstances.add(uri, std::make_unique<AAPXSServiceInstance>(AAPXSServiceInstance{this, uri, getInstanceId(), nullptr, dataSize}));
		return aapxsServiceInstances.get(uri);
	}

	AAPXSServiceInstance* getInstanceFor(const char* uri) {
		auto ret = aapxsServiceInstances.get(uri);
		assert(ret);
		return ret;
	}

	StandardExtensions& getStandardExtensions() override { return standards; }

	// It is invoked by AudioPluginInterfaceImpl, and supposed to dispatch request to extension service
	void controlExtension(const std::string &uri, int32_t opcode)
	{
		auto aapxsInstance = getInstanceFor(uri.c_str());
		auto feature = service->getExtensionFeature(uri.c_str());
		feature->on_invoked(feature, getPlugin(), aapxsInstance, opcode);
	}
};

class RemotePluginInstanceStandardExtensionsImpl : public RemotePluginInstanceStandardExtensions {
	RemotePluginInstance *owner;

public:
	RemotePluginInstanceStandardExtensionsImpl(RemotePluginInstance* owner)
			: owner(owner) {
	}

	AndroidAudioPlugin* getPlugin() override;
	AAPXSClientInstanceManager* getAAPXSManager() override;
};

class RemotePluginInstance : public PluginInstance {
	PluginClient *client;
	AndroidAudioPluginHost plugin_host_facade{};
    RemotePluginInstanceStandardExtensionsImpl standards;
    std::unique_ptr<AAPXSClientInstanceManager> aapxs_manager;

    friend class RemoteAAPXSManager;
protected:
	AndroidAudioPluginHost* getHostFacadeForCompleteInstantiation() override;

public:
	// The `instantiate()` member of the plugin factory is supposed to invoke `setupAAPXSInstances()`.
	// (binder-client-as-plugin does so, and desktop implementation should do so too.)
    RemotePluginInstance(PluginClient *client, const PluginInformation* pluginInformation, AndroidAudioPluginFactory* loadedPluginFactory, int sampleRate);

    int32_t getInstanceId() override {
        // Make sure that we never try to retrieve it before being initialized at completeInstantiation() (at client)
        assert(instantiation_state != PLUGIN_INSTANTIATION_STATE_INITIAL);
        return instance_id;
    }

	void setInstanceId(int32_t instanceId) { instance_id = instanceId; }

    // It is performed after endCreate() and beginPrepare(), to configure ports using relevant AAP extensions.
    void configurePorts();

    /** it is an unwanted exposure, but we need this internal-only member as public. You are not supposed to use it. */
    std::function<void(const char*, int32_t, int32_t)> send_extension_message_impl;

	inline AndroidAudioPlugin* getPlugin() { return plugin; }

	inline PluginClient* getClient() { return client; }

    AAPXSClientInstanceManager* getAAPXSManager() { return aapxs_manager.get(); }

    void sendExtensionMessage(const char *uri, int32_t opcode);

	StandardExtensions& getStandardExtensions() override { return standards; }
};

class RemoteAAPXSManager : public AAPXSClientInstanceManager {
    RemotePluginInstance *owner;

protected:

    static void staticSendExtensionMessage(AAPXSClientInstance* clientInstance, int32_t opcode);

public:
    RemoteAAPXSManager(RemotePluginInstance* owner)
            : owner(owner) {
    }

    const PluginInformation* getPluginInformation() override { return owner->getPluginInformation(); }
    AndroidAudioPlugin* getPlugin() override { return owner->plugin; }
    AAPXSFeature* getExtensionFeature(const char* uri) override {
        return owner->getClient()->getExtensionFeature(uri);
    }
    AAPXSClientInstance* setupAAPXSInstance(AAPXSFeature *feature, int32_t dataSize) override;
};

} // namespace

#endif // _ANDROID_AUDIO_PLUGIN_HOST_HPP_



// As a principle, there must not be Android-specific references in this code.
// Anything Android specific must to into `android` directory.
#include <sys/stat.h>
#include <sys/mman.h>
#include "aap/core/host/audio-plugin-host.h"
#include "aap/unstable/logging.h"
#include "shared-memory-extension.h"
#include "audio-plugin-host-internals.h"
#include <vector>


namespace aap
{


//-----------------------------------

PluginInformation::PluginInformation(bool isOutProcess, const char* pluginPackageName,
									 const char* pluginLocalName, const char* displayName,
									 const char* manufacturerName, const char* versionString,
									 const char* pluginID, const char* sharedLibraryFilename,
									 const char* libraryEntrypoint, const char* metadataFullPath,
									 const char* primaryCategory)
			: is_out_process(isOutProcess),
			  plugin_package_name(pluginPackageName),
			  plugin_local_name(pluginLocalName),
			  display_name(displayName),
			  manufacturer_name(manufacturerName),
			  version(versionString),
			  shared_library_filename(sharedLibraryFilename),
			  library_entrypoint(libraryEntrypoint),
			  plugin_id(pluginID),
			  metadata_full_path(metadataFullPath),
			  primary_category(primaryCategory)
{
	struct tm epoch{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	last_info_updated_unixtime_milliseconds = (int64_t) (1000.0 * difftime(time(nullptr), mktime(&epoch)));

	char *cp;
	size_t len = (size_t) snprintf(nullptr, 0, "%s+%s+%s", display_name.c_str(), plugin_id.c_str(), version.c_str());
	cp = (char*) calloc(len, 1);
	snprintf(cp, len, "%s+%s+%s", display_name.c_str(), plugin_id.c_str(), version.c_str());
	identifier_string = cp;
	free(cp);
}


//-----------------------------------

std::vector<PluginInformation*> PluginHostPAL::getInstalledPlugins(bool returnCacheIfExists, std::vector<std::string>* searchPaths) {
	std::vector<std::string> aapPaths{};
	for (auto path : getPluginPaths())
		getAAPMetadataPaths(path, aapPaths);
	auto ret = getPluginsFromMetadataPaths(aapPaths);
	return ret;
}

//-----------------------------------

PluginBuffer::~PluginBuffer() {
	if (buffer) {
		for (size_t i = 0; i < buffer->num_buffers; i++)
			free(buffer->buffers[i]);
		free(buffer->buffers);
	}
}

bool PluginBuffer::allocateBuffer(size_t numPorts, size_t numFrames) {
	assert(!buffer); // already allocated

	buffer = std::make_unique<AndroidAudioPluginBuffer>();
	if (!buffer)
		return false;

	size_t memSize = numFrames * sizeof(float);
	buffer->num_buffers = numPorts;
	buffer->num_frames = numFrames;
	buffer->buffers = (void **) calloc(numPorts, sizeof(float *));
	if (!buffer->buffers)
		return false;

	for (size_t i = 0; i < numPorts; i++) {
		buffer->buffers[i] = calloc(1, memSize);
		if (!buffer->buffers[i])
			return false;
	}

	return true;
}

//-----------------------------------

int32_t PluginSharedMemoryBuffer::allocateClientBuffer(size_t numPorts, size_t numFrames) {
	memory_origin = PLUGIN_BUFFER_ORIGIN_LOCAL;

	size_t memSize = numFrames * sizeof(float);
	buffer->num_buffers = numPorts;
	buffer->num_frames = numFrames;
	buffer->buffers = (void **) calloc(numPorts, sizeof(float *));
	if (!buffer->buffers)
		return PluginMemoryAllocatorResult::PLUGIN_MEMORY_ALLOCATOR_FAILED_LOCAL_ALLOC;

	for (size_t i = 0; i < numPorts; i++) {
		int32_t fd = getPluginHostPAL()->createSharedMemory(memSize);
		if (!fd)
			return PluginMemoryAllocatorResult::PLUGIN_MEMORY_ALLOCATOR_FAILED_SHM_CREATE;
		shared_memory_fds->emplace_back(fd);
		buffer->buffers[i] = mmap(nullptr, memSize,
								  PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
		if (!buffer->buffers[i])
			return PluginMemoryAllocatorResult::PLUGIN_MEMORY_ALLOCATOR_FAILED_MMAP;
	}

	return PluginMemoryAllocatorResult::PLUGIN_MEMORY_ALLOCATOR_SUCCESS;
}

int32_t PluginSharedMemoryBuffer::allocateServiceBuffer(std::vector<int32_t>& clientFDs, size_t numFrames) {
	memory_origin = PLUGIN_BUFFER_ORIGIN_REMOTE;

	size_t numPorts = clientFDs.size();
	size_t memSize = numFrames * sizeof(float);
	buffer->num_buffers = numPorts;
	buffer->num_frames = numFrames;
	buffer->buffers = (void **) calloc(numPorts, sizeof(float *));
	if (!buffer->buffers)
		return PluginMemoryAllocatorResult::PLUGIN_MEMORY_ALLOCATOR_FAILED_LOCAL_ALLOC;

	for (size_t i = 0; i < numPorts; i++) {
		int32_t fd = clientFDs[i];
		if (!fd)
			return PluginMemoryAllocatorResult::PLUGIN_MEMORY_ALLOCATOR_FAILED_SHM_CREATE;
		shared_memory_fds->emplace_back(fd);
		buffer->buffers[i] = mmap(nullptr, memSize,
								  PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
		if (!buffer->buffers[i])
			return PluginMemoryAllocatorResult::PLUGIN_MEMORY_ALLOCATOR_FAILED_MMAP;
	}

	return PluginMemoryAllocatorResult::PLUGIN_MEMORY_ALLOCATOR_SUCCESS;
}

//-----------------------------------

void PluginClient::createInstanceAsync(std::string identifier, int sampleRate, bool isRemoteExplicit, std::function<void(int32_t, std::string)> userCallback)
{
	const PluginInformation *descriptor = plugin_list->getPluginInformation(identifier);
	assert (descriptor != nullptr);

	// For local plugins, they can be directly loaded using dlopen/dlsym.
	// For remote plugins, the connection has to be established through binder.
	auto internalCallback = [=](PluginInstance* instance, std::string error) {
		if (instance != nullptr) {
			instances.emplace_back(instance);
			userCallback(instances.size() - 1, "");
		}
		else
			userCallback(-1, error);
	};
	if (isRemoteExplicit || descriptor->isOutProcess())
		instantiateRemotePlugin(descriptor, sampleRate, internalCallback);
	else {
		try {
			auto instance = instantiateLocalPlugin(descriptor, sampleRate);
			internalCallback(instance, "");
		} catch(std::exception& ex) {
			internalCallback(nullptr, ex.what());
		}
	}
}

void PluginHost::destroyInstance(PluginInstance* instance)
{
	instances.erase(std::find(instances.begin(), instances.end(), instance));
	delete instance;
}

int32_t PluginService::createInstance(std::string identifier, int sampleRate)  {
	auto info = plugin_list->getPluginInformation(identifier);
	if (!info) {
		aap::a_log_f(AAP_LOG_LEVEL_ERROR, "libandroidaudioplugin", "Plugin information was not found for: %s ", identifier.c_str());
		return -1;
	}
	instances.emplace_back(instantiateLocalPlugin(info, sampleRate));
	return instances.size() - 1;
}

PluginInstance* PluginHost::instantiateLocalPlugin(const PluginInformation *descriptor, int sampleRate)
{
	dlerror(); // clean up any previous error state
	auto file = descriptor->getLocalPluginSharedLibrary();
	auto metadataFullPath = descriptor->getMetadataFullPath();
	if (!metadataFullPath.empty()) {
		size_t idx = metadataFullPath.find_last_of('/');
		if (idx > 0) {
			auto soFullPath = metadataFullPath.substr(0, idx + 1) + file;
			struct stat st;
			if (stat(soFullPath.c_str(), &st) == 0)
				file = soFullPath;
		}
	}
	auto entrypoint = descriptor->getLocalPluginLibraryEntryPoint();
	auto dl = dlopen(file.length() > 0 ? file.c_str() : "libandroidaudioplugin.so", RTLD_LAZY);
	if (dl == nullptr) {
		aap::a_log_f(AAP_LOG_LEVEL_ERROR, "AAP library %s could not be loaded.\n", file.c_str());
		return nullptr;
	}
	auto factoryGetter = (aap_factory_t) dlsym(dl, entrypoint.length() > 0 ? entrypoint.c_str() : "GetAndroidAudioPluginFactory");
	if (factoryGetter == nullptr) {
		aap::a_log_f(AAP_LOG_LEVEL_ERROR, "AAP factory %s was not found in %s.\n", entrypoint.c_str(), file.c_str());
		return nullptr;
	}
	auto pluginFactory = factoryGetter();
	if (pluginFactory == nullptr) {
		aap::a_log_f(AAP_LOG_LEVEL_ERROR, "AAP factory %s could not instantiate a plugin.\n", entrypoint.c_str());
		return nullptr;
	}
	return new PluginInstance(descriptor, pluginFactory, sampleRate);
}

void PluginClient::instantiateRemotePlugin(const PluginInformation *descriptor, int sampleRate, std::function<void(PluginInstance*, std::string)> callback)
{
	// We first ensure to bind the remote plugin service, and then create a plugin instance.
	//  Since binding the plugin service must be asynchronous while instancing does not have to be,
	//  we call ensureServiceConnected() first and pass the rest as its callback.
	auto internalCallback = [=](std::string error) {
		if (error.empty()) {
#if ANDROID
			auto pluginFactory = GetAndroidAudioPluginFactoryClientBridge();
#else
			auto pluginFactory = GetDesktopAudioPluginFactoryClientBridge();
#endif
			assert (pluginFactory != nullptr);
			auto instance = new PluginInstance(descriptor, pluginFactory, sampleRate);
			callback(instance, "");
		}
		else
			callback(nullptr, error);
	};
	auto service = connections->getServiceHandleForConnectedPlugin(descriptor->getPluginPackageName(), descriptor->getPluginLocalName());
	if (service != nullptr)
		internalCallback("");
	else
		getPluginHostPAL()->ensurePluginServiceConnected(connections, descriptor->getPluginPackageName(), internalCallback);
}

PluginExtension::PluginExtension(AndroidAudioPluginExtension src) {
	uri.reset(strdup(src.uri));
	auto dataMem = calloc(1, src.transmit_size);
	memcpy(dataMem, src.data, src.transmit_size);
	data.reset((uint8_t*) dataMem);
	dataSize = src.transmit_size;
}

AndroidAudioPluginExtension PluginExtension::asTransient() const {
	AndroidAudioPluginExtension ret;
	ret.uri = uri.get();
	ret.data = data.get();
	ret.transmit_size = dataSize;
	return ret;
}

//-----------------------------------

PluginInstance::PluginInstance(const PluginInformation* pluginInformation, AndroidAudioPluginFactory* loadedPluginFactory, int sampleRate)
		: sample_rate(sampleRate),
		pluginInfo(pluginInformation),
		plugin_factory(loadedPluginFactory),
		plugin(nullptr),
		instantiation_state(PLUGIN_INSTANTIATION_STATE_INITIAL) {
	assert(pluginInformation);
	assert(loadedPluginFactory);
}

PluginInstance::~PluginInstance() { dispose(); }

int32_t PluginInstance::allocateAudioPluginBuffer(size_t numPorts, size_t numFrames) {
	assert(!plugin_buffer);
	plugin_buffer = std::make_unique<PluginBuffer>();
	return plugin_buffer->allocateBuffer(numPorts, numFrames);
}

AndroidAudioPluginBuffer* PluginInstance::getAudioPluginBuffer(size_t numPorts, size_t numFrames) {
	if (!plugin_buffer)
		allocateAudioPluginBuffer(numPorts, numFrames);
	return plugin_buffer->getAudioPluginBuffer();
}

void PluginInstance::completeInstantiation()
{
	assert(instantiation_state == PLUGIN_INSTANTIATION_STATE_INITIAL);

	AndroidAudioPluginExtension extArr[extensions.size() + 1];
	for (size_t i = 0; i < extensions.size(); i++)
		extArr[i] = extensions[i]->asTransient();
	AndroidAudioPluginExtension* extPtrArr[extensions.size() + 1];
	for (size_t i = 0; i < extensions.size(); i++)
		extPtrArr[i] = &extArr[i];
	extPtrArr[extensions.size()] = nullptr;
	plugin = plugin_factory->instantiate(plugin_factory, pluginInfo->getPluginID().c_str(), sample_rate, extPtrArr);
	assert(plugin);

	instantiation_state = PLUGIN_INSTANTIATION_STATE_UNPREPARED;
}

PluginListSnapshot PluginListSnapshot::queryServices() {
    PluginListSnapshot ret{};
	for (auto p : getPluginHostPAL()->getInstalledPlugins())
		ret.plugins.emplace_back(p);
	return ret;
}

void* PluginClientConnectionList::getServiceHandleForConnectedPlugin(std::string packageName, std::string className)
{
	for (int i = 0; i < serviceConnections.size(); i++) {
		auto s = serviceConnections[i];
		if (s->getPackageName() == packageName && s->getClassName() == className)
			return serviceConnections[i]->getConnectionData();
	}
	return nullptr;
}

void* PluginClientConnectionList::getServiceHandleForConnectedPlugin(std::string pluginId)
{
	auto pl = getPluginHostPAL()->getInstalledPlugins();
	for (auto &plugin : pl)
		if (plugin->getPluginID() == pluginId)
			return getServiceHandleForConnectedPlugin(plugin->getPluginPackageName(),
                                                      plugin->getPluginLocalName());
	return nullptr;
}


} // namespace

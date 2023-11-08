
// As a principle, there must not be Android-specific references in this code.
// Anything Android specific must to into `android` directory.
#include <sys/stat.h>
#include <sys/mman.h>
#include <vector>
#include "aap/core/host/audio-plugin-host.h"
#include "aap/unstable/logging.h"
#include "aap/core/host/shared-memory-store.h"
#include "plugin-service-list.h"
#include "audio-plugin-host-internals.h"
#include "aap/core/host/plugin-client-system.h"
#include "aap/core/host/plugin-instance.h"
#include "aap/core/AAPXSMidi2ReceiverSession.h"


#define LOG_TAG "AAP_HOST"

namespace aap
{

std::unique_ptr<PluginServiceList> plugin_service_list{nullptr};

PluginServiceList* PluginServiceList::getInstance() {
	if (!plugin_service_list)
		plugin_service_list = std::make_unique<PluginServiceList>();
	return plugin_service_list.get();
}

//-----------------------------------

bool AbstractPluginBuffer::initialize(int32_t numPorts, int32_t numFrames) {
	assert(!buffers); // already allocated
	assert(!buffer_sizes); // already allocated

	num_ports = numPorts;
	num_frames = numFrames;
	buffers = (void **) calloc(numPorts, sizeof(float *)); // this part is always local
	if (!buffers)
		return false;
	buffer_sizes = (int32_t*) calloc(numPorts, sizeof(int32_t)); // this part is always local
	if (!buffer_sizes)
		return false;
	return true;
}

//-----------------------------------

int32_t ClientPluginSharedMemoryStore::allocateClientBuffer(size_t numPorts, size_t numFrames, aap::PluginInstance& instance, size_t defaultControllBytesPerBlock) {
	memory_origin = PLUGIN_BUFFER_ORIGIN_LOCAL;

	size_t commonMemSize = numFrames * sizeof(float);
	port_buffer = std::make_unique<SharedMemoryPluginBuffer>(&instance);
	assert(port_buffer);
	port_buffer->initialize(numPorts, numFrames);

	for (size_t i = 0; i < numPorts; i++) {
		auto port = instance.getPort(i);
		auto memSize = port->hasProperty(AAP_PORT_MINIMUM_SIZE) ? port->getPropertyAsInteger(AAP_PORT_MINIMUM_SIZE) :
				port->getContentType() == AAP_CONTENT_TYPE_AUDIO ? commonMemSize : defaultControllBytesPerBlock;
		int32_t fd = PluginClientSystem::getInstance()->createSharedMemory(memSize);
		if (!fd)
			return PluginMemoryAllocatorResult::PLUGIN_MEMORY_ALLOCATOR_FAILED_SHM_CREATE;
		port_buffer_fds->emplace_back(fd);
		auto mapped = mmap(nullptr, memSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
		if (!mapped)
			return PluginMemoryAllocatorResult::PLUGIN_MEMORY_ALLOCATOR_FAILED_MMAP;
		port_buffer->setBuffer(i, mapped);
	}

	return PluginMemoryAllocatorResult::PLUGIN_MEMORY_ALLOCATOR_SUCCESS;
}

int32_t ServicePluginSharedMemoryStore::allocateServiceBuffer(std::vector<int32_t>& clientFDs, size_t numFrames, aap::PluginInstance& instance, size_t defaultControllBytesPerBlock) {
	memory_origin = PLUGIN_BUFFER_ORIGIN_REMOTE;

	size_t numPorts = clientFDs.size();
	size_t commonMemSize = numFrames * sizeof(float);
	port_buffer = std::make_unique<SharedMemoryPluginBuffer>(&instance);
	assert(port_buffer);
	if (!port_buffer->initialize(numPorts, numFrames))
		return PluginMemoryAllocatorResult::PLUGIN_MEMORY_ALLOCATOR_FAILED_LOCAL_ALLOC;

	for (size_t i = 0; i < numPorts; i++) {
		auto port = instance.getPort(i);
		auto memSize = port->hasProperty(AAP_PORT_MINIMUM_SIZE) ? port->getPropertyAsInteger(AAP_PORT_MINIMUM_SIZE) :
					   port->getContentType() == AAP_CONTENT_TYPE_AUDIO ? commonMemSize : defaultControllBytesPerBlock;
		int32_t fd = clientFDs[i];
		if (!fd)
			return PluginMemoryAllocatorResult::PLUGIN_MEMORY_ALLOCATOR_FAILED_SHM_CREATE;
		port_buffer_fds->emplace_back(fd);
		auto mapped = mmap(nullptr, memSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
		if (!mapped)
			return PluginMemoryAllocatorResult::PLUGIN_MEMORY_ALLOCATOR_FAILED_MMAP;
        port_buffer->setBuffer(i, mapped);
	}

	return PluginMemoryAllocatorResult::PLUGIN_MEMORY_ALLOCATOR_SUCCESS;
}

} // namespace


#include <sys/mman.h>
#include <grpc++/grpc++.h>
#include "gen/AudioPluginService.grpc.pb.h"
#include "aap/android-audio-plugin-host.hpp"

namespace aap {

// This is a service class that is instantiated by a local AudioPluginService instance.
// It is instantiated for one plugin per client.
// One client can instantiate multiple plugins.
class AudioPluginInterfaceImpl : public org::androidaudioplugin::AudioPluginService::Service {
    std::unique_ptr<aap::PluginHost> host;
    std::unique_ptr<aap::PluginHostManager> manager;
    std::vector<AndroidAudioPluginBuffer> buffers{};

public:
    AudioPluginInterfaceImpl() {
        manager.reset(new aap::PluginHostManager());
        host.reset(new aap::PluginHost(manager.get()));
    }

	grpc::Status Create(::grpc::ServerContext* context, const ::org::androidaudioplugin::CreateRequest* request, ::org::androidaudioplugin::InstanceId* response) override {
        int32_t instanceId = host->createInstance(request->plugin_id().c_str(), request->sample_rate());
        if (instanceId < 0) {
            // FIXME: implement error return
            assert(false);
        }

        auto instance =host->getInstance(instanceId);
        response->set_instance_id(instanceId);
        auto shm = new SharedMemoryExtension();
        shm->getPortBufferFDs().resize(instance->getPluginInformation()->getNumPorts());
        AndroidAudioPluginExtension ext{SharedMemoryExtension::URI, 0, shm};
        instance->addExtension(ext);
        buffers.resize(instanceId + 1);
        auto & buffer = buffers[instanceId];
        buffer.buffers = nullptr;
        buffer.num_buffers = 0;
        buffer.num_frames = 0;

        return grpc::Status::OK;
	}

    grpc::Status Destroy(::grpc::ServerContext* context, const ::org::androidaudioplugin::InstanceId* request, ::org::androidaudioplugin::Unit* response) override {
        host->destroyInstance(host->getInstance(request->instance_id()));
        return grpc::Status::OK;
    }

	grpc::Status IsPluginAlive(::grpc::ServerContext* context, const ::org::androidaudioplugin::InstanceId* request, ::org::androidaudioplugin::AliveStatus* response) override {
        auto instance = host->getInstance(request->instance_id());
        response->set_is_alive(instance != nullptr); // this is the best thing that we can check, locally.
        return grpc::Status::OK;
    }

	grpc::Status GetStateSize(::grpc::ServerContext* context, const ::org::androidaudioplugin::InstanceId* request, ::org::androidaudioplugin::Size* response) override {
        auto instance = host->getInstance(request->instance_id());
        response->set_size(instance->getStateSize());
        return grpc::Status::OK;
    }

	grpc::Status GetState(::grpc::ServerContext* context, const ::org::androidaudioplugin::GetStateRequest* request, ::org::androidaudioplugin::Unit* response) override {
        int32_t instanceId = request->instance_id();
        assert(instanceId < host->getInstanceCount());
        auto instance = host->getInstance(instanceId);
        auto state = instance->getState();
        auto dst = mmap(nullptr, state.data_size, PROT_READ | PROT_WRITE, MAP_SHARED, dup(request->shared_memory_fd()), 0);
        memcpy(dst, state.raw_data, state.data_size);
        munmap(dst, state.data_size);

        return grpc::Status::OK;
    }

	grpc::Status SetState(::grpc::ServerContext* context, const ::org::androidaudioplugin::SetStateRequest* request, ::org::androidaudioplugin::Unit* response) override {
        int32_t instanceId = request->instance_id();
        int32_t size = request->size();
        assert(instanceId < host->getInstanceCount());
        auto instance = host->getInstance(instanceId);
        auto src = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, dup(request->shared_memory_fd()), 0);
        instance->setState(src, size);
        munmap(src, size);

        return grpc::Status::OK;
    }

    void freeBuffers(PluginInstance* instance, AndroidAudioPluginBuffer& buffer)
    {
        if (buffer.buffers)
            for (int i = 0; i < instance->getSharedMemoryExtension()->getPortBufferFDs().size(); i++)
                if (buffer.buffers[i])
                    munmap(buffer.buffers[i], buffer.num_buffers * sizeof(float));
    }

	grpc::Status Prepare(::grpc::ServerContext* context, const ::org::androidaudioplugin::PrepareRequest* request, ::org::androidaudioplugin::Unit* response) override {
        int32_t instanceId = request->instance_id();
        assert(instanceId < host->getInstanceCount());
        int32_t frameCount = request->frame_count();
        auto instance = host->getInstance(instanceId);

        auto& buffer = buffers[instanceId];
        int ret = resetBuffers(instance, buffer, frameCount);
        if (ret != 0) {
            // FIXME: implement error reporting
            assert(false);
            return grpc::Status(grpc::StatusCode::INTERNAL, "failed to reset buffers");
        }

        host->getInstance(instanceId)->prepare(request->frame_count(), &buffers[instanceId]);
        return grpc::Status::OK;
    }

    // It is mostly copy of Android implementation so far...
    int resetBuffers(PluginInstance* instance, AndroidAudioPluginBuffer& buffer, int frameCount)
    {
        int nPorts = instance->getPluginInformation()->getNumPorts();
        auto& FDs = instance->getSharedMemoryExtension()->getPortBufferFDs();
        if (FDs.size() != nPorts) {
            freeBuffers(instance, buffer);
            FDs.resize(nPorts, 0);
        }

        buffer.num_buffers = nPorts;
        buffer.num_frames = frameCount;
        int n = buffer.num_buffers;
        if (buffer.buffers == nullptr)
            buffer.buffers = (void **) calloc(sizeof(void *), n);
        for (int i = 0; i < n; i++) {
            if (buffer.buffers[i])
                munmap(buffer.buffers[i], buffer.num_frames * sizeof(float));
            buffer.buffers[i] = mmap(nullptr, buffer.num_frames * sizeof(float), PROT_READ | PROT_WRITE,
                                     MAP_SHARED, FDs[i], 0);
            if (buffer.buffers[i] == MAP_FAILED) {
                int err = errno; // FIXME : define error codes
                // FIXME: error reporting
                assert(false);
                return err;
            }
            assert(buffer.buffers[i] != nullptr);
        }
        return 0;
    }

	grpc::Status Activate(::grpc::ServerContext* context, const ::org::androidaudioplugin::InstanceId* request, ::org::androidaudioplugin::Unit* response) override {
        int32_t instanceId = request->instance_id();
        assert(instanceId < host->getInstanceCount());
        host->getInstance(instanceId)->activate();
        return grpc::Status::OK;
    }

	grpc::Status Deactivate(::grpc::ServerContext* context, const ::org::androidaudioplugin::InstanceId* request, ::org::androidaudioplugin::Unit* response) override {
        int32_t instanceId = request->instance_id();
        assert(instanceId < host->getInstanceCount());
        host->getInstance(instanceId)->deactivate();
        return grpc::Status::OK;
    }

    grpc::Status Process(::grpc::ServerContext* context, const ::org::androidaudioplugin::ProcessRequest* request, ::org::androidaudioplugin::Unit* response) override {
        int32_t instanceId = request->instance_id();
        assert(instanceId < host->getInstanceCount());
        host->getInstance(instanceId)->process(&buffers[instanceId], request->timeout_nanoseconds());
        return grpc::Status::OK;
    }
};

}


#ifndef AAP_CORE_ANDROIDAUDIODEVICEMANAGER_H
#define AAP_CORE_ANDROIDAUDIODEVICEMANAGER_H

#include "AudioDeviceManager.h"
#include <oboe/Oboe.h>

namespace aap {

    class OboeAudioDeviceIn;
    class OboeAudioDeviceOut;

    class OboeAudioDeviceManager : public AudioDeviceManager {
        uint32_t frames_per_callback;
        std::shared_ptr<OboeAudioDeviceIn> input{nullptr};
        std::shared_ptr<OboeAudioDeviceOut> output{nullptr};

    public:
        OboeAudioDeviceManager() {}
        AudioDeviceIn * openDefaultInput(uint32_t sampleRate, uint32_t framesPerCallback, int32_t numChannels) override;

        AudioDeviceOut * openDefaultOutput(uint32_t sampleRate, uint32_t framesPerCallback, int32_t numChannels) override;
    };
}


#endif //AAP_CORE_ANDROIDAUDIODEVICEMANAGER_H

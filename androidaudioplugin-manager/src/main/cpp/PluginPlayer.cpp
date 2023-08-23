#include "PluginPlayer.h"
#include <aap/core/host/plugin-instance.h>

aap::PluginPlayer::PluginPlayer(aap::PluginPlayerConfiguration &pluginPlayerConfiguration) :
                                configuration(pluginPlayerConfiguration),
                                graph(configuration.getFramesPerCallback(), configuration.getChannelCount()) {
}

void aap::PluginPlayer::setAudioSource(uint8_t *data, int32_t dataLength, const char *filename) {
    // TODO: implement uncompressing `data` into AAP audio data.
    auto aapAudioData = (AudioData*) data;
    int32_t numFrames = dataLength;
    int32_t numChannels = 2;

    graph.setAudioData(aapAudioData, numFrames, numChannels);
}

void aap::PluginPlayer::startProcessing() {
    graph.startProcessing();
}

void aap::PluginPlayer::pauseProcessing() {
    graph.pauseProcessing();
}

void aap::PluginPlayer::enableAudioRecorder() {
    graph.enableAudioRecorder();
}

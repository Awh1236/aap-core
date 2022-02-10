
#include <jni.h>
#include <android/binder_ibinder.h>
#include <android/binder_ibinder_jni.h>

#include <aap/audio-plugin-host.h>
#include <aap/audio-plugin-host-android.h>
#include <aap/android-application-context.h>
#include <aap/logging.h>
#include "AAPMidiProcessor_android.h"


// JNI entrypoints

// This returns std::string by value. Do not use for large chunk of strings.
const char* dupFromJava(JNIEnv *env, jstring s) {
    jboolean isCopy;
    if (!s)
        return "";
    const char *u8 = env->GetStringUTFChars(s, &isCopy);
    auto ret = strdup(u8);
    if (isCopy)
        env->ReleaseStringUTFChars(s, u8);
    return ret;
}

extern "C" {

aapmidideviceservice::AAPMidiProcessorAndroid *processor{nullptr};

aapmidideviceservice::AudioDriverType driver_type{
    aapmidideviceservice::AudioDriverType::AAP_MIDI_PROCESSOR_AUDIO_DRIVER_TYPE_OBOE};

void startNewDeviceInstance() {
    processor = new aapmidideviceservice::AAPMidiProcessorAndroid(driver_type);
}

aapmidideviceservice::AAPMidiProcessorAndroid* getDeviceInstance() {
    if (!processor)
        startNewDeviceInstance();
    return processor;
}

#define AAPMIDIDEVICE_INSTANCE getDeviceInstance()

JNIEXPORT void JNICALL Java_org_androidaudioplugin_midideviceservice_AudioPluginMidiDeviceInstance_initializeMidiProcessor(
        JNIEnv *env, jobject midiReceiver, jobject applicationContext, jobjectArray plugins, jint sampleRate, jint oboeFrameSize, jint audioOutChannelCount, jint aapFrameSize) {
    aap::set_application_context(env, applicationContext);
    ((aap::AndroidPluginHostPAL*) aap::getPluginHostPAL())->initializeKnownPlugins(plugins);

    startNewDeviceInstance();

    AAPMIDIDEVICE_INSTANCE->initialize(sampleRate, oboeFrameSize, audioOutChannelCount, aapFrameSize);
}

JNIEXPORT void JNICALL Java_org_androidaudioplugin_midideviceservice_AudioPluginMidiDeviceInstance_registerPluginService(
        JNIEnv *env, jobject midiReceiver, jobject binder, jstring packageName, jstring className) {
    auto packageNamePtr = dupFromJava(env, packageName);
    std::string packageNameString{packageNamePtr};
    auto classNamePtr = dupFromJava(env, className);
    std::string classNameString{classNamePtr};
    auto aiBinder = AIBinder_fromJavaBinder(env, binder);

    AAPMIDIDEVICE_INSTANCE->registerPluginService(
            std::make_unique<aap::AudioPluginServiceConnection>(packageNameString, classNameString, aiBinder));

    free((void *) classNamePtr);
    free((void *) packageNamePtr);
}

JNIEXPORT void JNICALL Java_org_androidaudioplugin_midideviceservice_AudioPluginMidiDeviceInstance_terminateMidiProcessor(
        JNIEnv *env, jobject midiReceiver) {
    AAPMIDIDEVICE_INSTANCE->terminate();

    processor = nullptr;

    aap::aprintf("AudioPluginMidiReceiver terminated.");
}

JNIEXPORT void JNICALL Java_org_androidaudioplugin_midideviceservice_AudioPluginMidiDeviceInstance_activate(
        JNIEnv *env, jobject midiReceiver) {
    AAPMIDIDEVICE_INSTANCE->activate();
}

JNIEXPORT void JNICALL Java_org_androidaudioplugin_midideviceservice_AudioPluginMidiDeviceInstance_deactivate(
        JNIEnv *env, jobject midiReceiver) {
    AAPMIDIDEVICE_INSTANCE->deactivate();
}

JNIEXPORT void JNICALL Java_org_androidaudioplugin_midideviceservice_AudioPluginMidiDeviceInstance_instantiatePlugin(
        JNIEnv *env, jobject midiReceiver, jstring pluginId) {
    auto pluginIdPtr = dupFromJava(env, pluginId);
    std::string pluginIdString = pluginIdPtr;

    AAPMIDIDEVICE_INSTANCE->instantiatePlugin(pluginIdString);

    free((void *) pluginIdPtr);
}

jbyte jni_midi_buffer[1024]{};

JNIEXPORT void JNICALL Java_org_androidaudioplugin_midideviceservice_AudioPluginMidiDeviceInstance_processMessage(
        JNIEnv *env, jobject midiReceiver, jbyteArray bytes, jint offset, jint length,
        jlong timestampInNanoseconds) {
    env->GetByteArrayRegion(bytes, offset, length, jni_midi_buffer);
    AAPMIDIDEVICE_INSTANCE->processMidiInput(
            reinterpret_cast<uint8_t *>(jni_midi_buffer), 0, length, timestampInNanoseconds);
}

} // extern "C"

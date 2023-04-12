
#include "state-service.h"

namespace aap {

    template<typename T>
    void StatePluginServiceExtension::withStateExtension(AndroidAudioPlugin* plugin,
                                                            T defaultValue,
                                                            std::function<void(aap_state_extension_t*, AndroidAudioPlugin*)> func) {
        // This instance->getExtension() should return an extension from the loaded plugin.
        assert(plugin);
        auto stateExtension = (aap_state_extension_t *) plugin->get_extension(plugin, AAP_PRESETS_EXTENSION_URI);
        assert(stateExtension);
        // We don't need context for service side.
        func(stateExtension, plugin);
    }

    void StatePluginServiceExtension::onInvoked(AndroidAudioPlugin* plugin, AAPXSServiceInstance *extensionInstance,
                                                  int32_t opcode) {
        switch (opcode) {
            case OPCODE_GET_STATE_SIZE:
                withStateExtension<int32_t>(plugin, 0, [=](aap_state_extension_t *ext,
                                                              AndroidAudioPlugin* plugin) {
                    *((int32_t *) extensionInstance->data) = ext->get_state_size(ext, plugin);
                });
                break;
            case OPCODE_GET_STATE:
                withStateExtension<int32_t>(plugin, 0, [=](aap_state_extension_t *ext,
                                                           AndroidAudioPlugin* plugin) {
                    aap_state_t state;
                    state.data = extensionInstance->data;
                    ext->get_state(ext, plugin, &state);
                });
                break;
            case OPCODE_SET_STATE:
                withStateExtension<int32_t>(plugin, 0, [=](aap_state_extension_t *ext,
                                                           AndroidAudioPlugin* plugin) {
                    aap_state_t state;
                    state.data = extensionInstance->data;
                    ext->set_state(ext, plugin, &state);
                });
            default:
                assert(0); // should not reach here
                break;
        }
    }
} // namespace aap

#include "port-config-service.h"

template<typename T>
void aap::PortConfigPluginServiceExtension::withPortConfigExtension(AndroidAudioPlugin* plugin, T defaultValue,
                                                                    std::function<void(aap_port_config_extension_t *,
                                                                                       AndroidAudioPlugin*)> func) {
    // This instance->getExtension() should return an extension from the loaded plugin.
    assert(plugin);
    auto portConfigExtension = (aap_port_config_extension_t *) plugin->get_extension(plugin, AAP_PORT_CONFIG_EXTENSION_URI);
    assert(portConfigExtension);
    // We don't need context for service side.
    func(portConfigExtension, plugin);
}

void aap::PortConfigPluginServiceExtension::onInvoked(AndroidAudioPlugin *plugin,
                                                      AAPXSServiceInstance *extensionInstance,
                                                      int32_t opcode) {
    switch (opcode) {
        case OPCODE_PORT_CONFIG_GET_OPTIONS:
            withPortConfigExtension<int32_t>(plugin, 0, [=](aap_port_config_extension_t *ext,
                                                        AndroidAudioPlugin* plugin) {
                auto destination = (aap_port_config_t *)  extensionInstance->data;
                ext->get_options(ext, plugin, destination);
                return 0;
            });
            break;
        case OPCODE_PORT_CONFIG_SELECT:
            withPortConfigExtension<int32_t>(plugin, 0, [=](aap_port_config_extension_t *ext,
                                                            AndroidAudioPlugin* plugin) {
                int32_t size = *((int32_t *) extensionInstance->data);
                char config[size + 1];
                strncpy(config, (char*) extensionInstance->data + sizeof(int32_t), size);
                config[size] = 0;
                ext->select(ext, plugin, config);
                return 0;
            });
            break;
    }
}


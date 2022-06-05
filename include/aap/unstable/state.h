#ifndef AAP_STATE_H_INCLUDED
#define AAP_STATE_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

#include "aap-core.h"
#include <stdint.h>

#define AAP_STATE_EXTENSION_URI "urn://androidaudioplugin.org/extensions/state/v1"

typedef struct {
    void* data;
    int32_t data_size;
} aap_state_t;

typedef int32_t (*state_extension_get_state_size_t) (AndroidAudioPlugin* plugin);
typedef void (*state_extension_get_state_t) (AndroidAudioPlugin* plugin, aap_state_t* destination);
typedef void (*state_extension_set_state_t) (AndroidAudioPlugin* plugin, aap_state_t* source);

typedef struct aap_state_extension_t {
    state_extension_get_state_size_t get_state_size;
    state_extension_get_state_t get_state;
    state_extension_set_state_t set_state;
} aap_state_extension_t;

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* AAP_STATE_H_INCLUDED */

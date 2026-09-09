#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct wd_mqtt_cfg {
    const char *uri;       /* e.g. mqtt://192.168.1.10:1883 */
    const char *device_id; /* topic leaf wd/{id}/... */
    int buffer_size;       /* esp-mqtt buffer; >= inline_max */
    int inline_max;        /* advertised in status */
} wd_mqtt_cfg_t;

/** Start MQTT client (call after Wi-Fi STA got IP). */
void wd_mqtt_start(const wd_mqtt_cfg_t *cfg);

bool wd_mqtt_is_connected(void);

#ifdef __cplusplus
}
#endif

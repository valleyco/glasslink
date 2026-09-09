#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/** Bring up NVS cfg + Wi-Fi STA + MQTT (blocks until IP). */
void wd_net_start(void);

#ifdef __cplusplus
}
#endif

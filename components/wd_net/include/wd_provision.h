#pragma once

#include "wd_config.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef WD_PROVISION_AP_SSID
#define WD_PROVISION_AP_SSID "glasslink-setup"
#endif

/** SoftAP IP served by esp-netif SoftAP default. */
#define WD_PROVISION_URL "http://192.168.4.1/"

/**
 * Optional UI hook (e.g. paint AP SSID + URL on glass). Set from main before
 * wd_net_start. May be NULL.
 */
typedef void (*wd_provision_ui_fn)(const char *ap_ssid, const char *url);

void wd_provision_set_ui(wd_provision_ui_fn fn);

/**
 * Start open SoftAP + HTTP config portal. Does not return (blocks until
 * POST /config saves NVS and reboots).
 */
void wd_provision_run(const wd_cfg_t *seed) __attribute__((noreturn));

#ifdef __cplusplus
}
#endif

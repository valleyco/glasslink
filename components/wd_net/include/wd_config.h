#pragma once

#include "esp_err.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    WD_CFG_SSID_MAX = 32,
    WD_CFG_PASS_MAX = 64,
    WD_CFG_URI_MAX = 128,
    WD_CFG_ID_MAX = 32
};

typedef struct wd_cfg {
    char wifi_ssid[WD_CFG_SSID_MAX + 1];
    char wifi_pass[WD_CFG_PASS_MAX + 1];
    char mqtt_uri[WD_CFG_URI_MAX + 1];
    char device_id[WD_CFG_ID_MAX + 1];
} wd_cfg_t;

/**
 * Load config: NVS namespace "wd" first; missing keys filled from Kconfig defaults.
 * If NVS was empty/incomplete and defaults are usable, persist the merged cfg.
 */
esp_err_t wd_cfg_load(wd_cfg_t *out);

/** Write all fields to NVS (namespace "wd"). */
esp_err_t wd_cfg_save(const wd_cfg_t *cfg);

#ifdef __cplusplus
}
#endif

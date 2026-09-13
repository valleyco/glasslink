/*
 * Device config in NVS (ESP32 flash KV — not classic EEPROM).
 * Namespace "wd": wifi_ssid, wifi_pass, mqtt_uri, device_id.
 */

#include "wd_config.h"

#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "sdkconfig.h"
#include "wd_form.h"

#include <string.h>

static const char *TAG = "wd_cfg";
static const char *NS = "wd";

#ifndef CONFIG_WD_WIFI_SSID
#define CONFIG_WD_WIFI_SSID "CHANGE_ME"
#endif
#ifndef CONFIG_WD_WIFI_PASS
#define CONFIG_WD_WIFI_PASS "CHANGE_ME"
#endif
#ifndef CONFIG_WD_MQTT_URI
#define CONFIG_WD_MQTT_URI "mqtt://192.168.1.1:1883"
#endif
#ifndef CONFIG_WD_DEVICE_ID
#define CONFIG_WD_DEVICE_ID "cyd1"
#endif

static void copy_str(char *dst, size_t dst_sz, const char *src)
{
    if (!dst || dst_sz == 0) {
        return;
    }
    if (!src) {
        dst[0] = '\0';
        return;
    }
    strncpy(dst, src, dst_sz - 1);
    dst[dst_sz - 1] = '\0';
}

static bool usable_ssid(const char *s)
{
    return s && s[0] && strcmp(s, "CHANGE_ME") != 0;
}

int wd_cfg_needs_provision(const wd_cfg_t *cfg)
{
    if (!cfg) {
        return 1;
    }
    return usable_ssid(cfg->wifi_ssid) ? 0 : 1;
}

int wd_cfg_apply_form(wd_cfg_t *cfg, const char *body, size_t body_len)
{
    char buf[WD_CFG_PASS_MAX + 1];
    int rc;

    if (!cfg || !body) {
        return -1;
    }

    rc = wd_form_get_field(body, body_len, "wifi_ssid", buf, sizeof(buf));
    if (rc < 0) {
        return -1;
    }
    if (rc == 0) {
        if (strlen(buf) > WD_CFG_SSID_MAX) {
            return -1;
        }
        copy_str(cfg->wifi_ssid, sizeof(cfg->wifi_ssid), buf);
    }

    rc = wd_form_get_field(body, body_len, "wifi_pass", buf, sizeof(buf));
    if (rc < 0) {
        return -1;
    }
    if (rc == 0) {
        copy_str(cfg->wifi_pass, sizeof(cfg->wifi_pass), buf);
    }

    rc = wd_form_get_field(body, body_len, "mqtt_uri", buf, sizeof(buf));
    if (rc < 0) {
        return -1;
    }
    if (rc == 0) {
        if (strlen(buf) > WD_CFG_URI_MAX) {
            return -1;
        }
        if (strncmp(buf, "mqtt://", 7) != 0) {
            return -1;
        }
        copy_str(cfg->mqtt_uri, sizeof(cfg->mqtt_uri), buf);
    }

    rc = wd_form_get_field(body, body_len, "device_id", buf, sizeof(buf));
    if (rc < 0) {
        return -1;
    }
    if (rc == 0) {
        if (strlen(buf) == 0 || strlen(buf) > WD_CFG_ID_MAX) {
            return -1;
        }
        copy_str(cfg->device_id, sizeof(cfg->device_id), buf);
    }

    if (!usable_ssid(cfg->wifi_ssid)) {
        return -1;
    }
    if (!cfg->mqtt_uri[0] || !cfg->device_id[0]) {
        return -1;
    }
    return 0;
}

static esp_err_t nvs_get_str_key(nvs_handle_t h, const char *key, char *out,
                                 size_t out_sz)
{
    size_t len = out_sz;
    esp_err_t err = nvs_get_str(h, key, out, &len);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        out[0] = '\0';
        return ESP_ERR_NVS_NOT_FOUND;
    }
    return err;
}

esp_err_t wd_cfg_save(const wd_cfg_t *cfg)
{
    nvs_handle_t h;
    esp_err_t err;
    if (!cfg) {
        return ESP_ERR_INVALID_ARG;
    }
    err = nvs_open(NS, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_set_str(h, "wifi_ssid", cfg->wifi_ssid);
    if (err == ESP_OK) {
        err = nvs_set_str(h, "wifi_pass", cfg->wifi_pass);
    }
    if (err == ESP_OK) {
        err = nvs_set_str(h, "mqtt_uri", cfg->mqtt_uri);
    }
    if (err == ESP_OK) {
        err = nvs_set_str(h, "device_id", cfg->device_id);
    }
    if (err == ESP_OK) {
        err = nvs_commit(h);
    }
    nvs_close(h);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "saved NVS wd cfg (ssid=%s device=%s)", cfg->wifi_ssid,
                 cfg->device_id);
    } else {
        ESP_LOGW(TAG, "NVS save failed: %s", esp_err_to_name(err));
    }
    return err;
}

esp_err_t wd_cfg_load(wd_cfg_t *out)
{
    nvs_handle_t h;
    esp_err_t err;
    int from_nvs = 0;
    wd_cfg_t cfg;

    if (!out) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(&cfg, 0, sizeof(cfg));

    err = nvs_open(NS, NVS_READONLY, &h);
    if (err == ESP_OK) {
        if (nvs_get_str_key(h, "wifi_ssid", cfg.wifi_ssid, sizeof(cfg.wifi_ssid)) ==
            ESP_OK) {
            from_nvs++;
        }
        if (nvs_get_str_key(h, "wifi_pass", cfg.wifi_pass, sizeof(cfg.wifi_pass)) ==
            ESP_OK) {
            from_nvs++;
        }
        if (nvs_get_str_key(h, "mqtt_uri", cfg.mqtt_uri, sizeof(cfg.mqtt_uri)) ==
            ESP_OK) {
            from_nvs++;
        }
        if (nvs_get_str_key(h, "device_id", cfg.device_id, sizeof(cfg.device_id)) ==
            ESP_OK) {
            from_nvs++;
        }
        nvs_close(h);
    }

    if (!cfg.wifi_ssid[0]) {
        copy_str(cfg.wifi_ssid, sizeof(cfg.wifi_ssid), CONFIG_WD_WIFI_SSID);
    }
    if (!cfg.wifi_pass[0]) {
        copy_str(cfg.wifi_pass, sizeof(cfg.wifi_pass), CONFIG_WD_WIFI_PASS);
    }
    if (!cfg.mqtt_uri[0]) {
        copy_str(cfg.mqtt_uri, sizeof(cfg.mqtt_uri), CONFIG_WD_MQTT_URI);
    }
    if (!cfg.device_id[0]) {
        copy_str(cfg.device_id, sizeof(cfg.device_id), CONFIG_WD_DEVICE_ID);
    }

    /* Seed NVS once from Kconfig so later firmware can use CHANGE_ME defaults. */
    if (from_nvs < 4 && usable_ssid(cfg.wifi_ssid)) {
        (void)wd_cfg_save(&cfg);
    }

    ESP_LOGI(TAG, "cfg ssid=%s mqtt=%s device=%s (nvs_keys=%d)", cfg.wifi_ssid,
             cfg.mqtt_uri, cfg.device_id, from_nvs);
    *out = cfg;
    return ESP_OK;
}

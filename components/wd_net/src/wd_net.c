/*
 * Wi-Fi STA + MQTT bring-up (device-only). SoftAP provision on missing/fail.
 */

#include "wd_net.h"

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "nvs_flash.h"
#include "wd_config.h"
#include "wd_http.h"
#include "wd_mqtt.h"
#include "wd_provision.h"

#include "bind.h"
#include "contract.h"

#include <string.h>

static const char *TAG = "wd_net";
static EventGroupHandle_t s_wifi;
static volatile int s_sta_trying;
#define WIFI_OK BIT0

#ifndef WD_STA_CONNECT_MS
#define WD_STA_CONNECT_MS 25000
#endif

static void on_wifi(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    (void)data;
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        if (s_sta_trying) {
            esp_wifi_connect();
        }
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_sta_trying) {
            ESP_LOGW(TAG, "wifi retry");
            esp_wifi_connect();
        }
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(s_wifi, WIFI_OK);
    }
}

static void wifi_stack_init(void)
{
    s_wifi = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    esp_netif_create_default_wifi_ap();
    wifi_init_config_t wcfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wcfg));
    ESP_ERROR_CHECK(
        esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, on_wifi, NULL));
    ESP_ERROR_CHECK(
        esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, on_wifi, NULL));
}

static int wifi_sta_connect(const wd_cfg_t *cfg, TickType_t timeout)
{
    wifi_config_t w = {0};
    EventBits_t bits;

    strncpy((char *)w.sta.ssid, cfg->wifi_ssid, sizeof(w.sta.ssid) - 1);
    strncpy((char *)w.sta.password, cfg->wifi_pass, sizeof(w.sta.password) - 1);
    s_sta_trying = 1;
    xEventGroupClearBits(s_wifi, WIFI_OK);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &w));
    ESP_ERROR_CHECK(esp_wifi_start());
    bits = xEventGroupWaitBits(s_wifi, WIFI_OK, pdFALSE, pdTRUE, timeout);
    if ((bits & WIFI_OK) == 0) {
        ESP_LOGW(TAG, "STA connect timeout (%u ms)", (unsigned)WD_STA_CONNECT_MS);
        s_sta_trying = 0;
        return 0;
    }
    /* Keep auto-reconnect for runtime drops. */
    return 1;
}

void wd_net_start(void)
{
    wd_cfg_t cfg;
    esp_err_t nvs_err = nvs_flash_init();
    if (nvs_err == ESP_ERR_NVS_NO_FREE_PAGES ||
        nvs_err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    } else {
        ESP_ERROR_CHECK(nvs_err);
    }

    ESP_ERROR_CHECK(wd_cfg_load(&cfg));
    wifi_stack_init();

    if (wd_cfg_needs_provision(&cfg)) {
        ESP_LOGW(TAG, "unprovisioned — SoftAP setup");
        wd_provision_run(&cfg);
    }
    if (!wifi_sta_connect(&cfg, pdMS_TO_TICKS(WD_STA_CONNECT_MS))) {
        ESP_LOGW(TAG, "STA failed — SoftAP setup");
        wd_provision_run(&cfg);
    }

    contract_set_fetch(wd_http_fetch, NULL);
    contract_set_fetch_release(wd_http_release);
    bind_reset();
    contract_group_reset();

    wd_mqtt_cfg_t m = {
        .uri = cfg.mqtt_uri,
        .device_id = cfg.device_id,
        .buffer_size = 8192,
        .inline_max = 6144,
    };
    wd_mqtt_start(&m);
    ESP_LOGI(TAG, "mqtt started; waiting cmd on wd/%s/cmd", cfg.device_id);
}

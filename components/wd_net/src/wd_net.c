/*
 * Wi-Fi STA + MQTT bring-up (device-only). Keeps IDF net deps inside `net`.
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

#include "bind.h"
#include "contract.h"

#include <stdio.h>

static const char *TAG = "wd_net";
static EventGroupHandle_t s_wifi;
#define WIFI_OK BIT0

static void on_wifi(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    (void)data;
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "wifi retry");
        esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(s_wifi, WIFI_OK);
    }
}

static void wifi_start(const wd_cfg_t *cfg)
{
    s_wifi = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t wcfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wcfg));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, on_wifi, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, on_wifi, NULL));
    wifi_config_t w = {0};
    strncpy((char *)w.sta.ssid, cfg->wifi_ssid, sizeof(w.sta.ssid) - 1);
    strncpy((char *)w.sta.password, cfg->wifi_pass, sizeof(w.sta.password) - 1);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &w));
    ESP_ERROR_CHECK(esp_wifi_start());
    xEventGroupWaitBits(s_wifi, WIFI_OK, pdFALSE, pdTRUE, portMAX_DELAY);
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
    wifi_start(&cfg);

    contract_set_fetch(wd_http_fetch, NULL);
    bind_reset();

    wd_mqtt_cfg_t m = {
        .uri = cfg.mqtt_uri,
        .device_id = cfg.device_id,
        .buffer_size = 8192,
        .inline_max = 6144,
    };
    wd_mqtt_start(&m);
    ESP_LOGI(TAG, "mqtt started; waiting cmd on wd/%s/cmd", cfg.device_id);
}

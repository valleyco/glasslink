/*
#include <stdio.h>
 * MQTT-enabled main (flash E2E later). Build: idf.py -D MQTT_MAIN=1 ...
 * Wi-Fi/broker from Kconfig (placeholders until NVS).
 */
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "hal_display.h"
#include "nvs_flash.h"
#include "render.h"
#include "wd_mqtt.h"

static const char *TAG = "main_mqtt";
static EventGroupHandle_t s_wifi;
#define WIFI_OK BIT0

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

static void on_wifi(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg; (void)data;
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "wifi retry");
        esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(s_wifi, WIFI_OK);
    }
}

static void wifi_start(void)
{
    s_wifi = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, on_wifi, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, on_wifi, NULL));
    wifi_config_t w = {0};
    snprintf((char *)w.sta.ssid, sizeof(w.sta.ssid), "%s", CONFIG_WD_WIFI_SSID);
    snprintf((char *)w.sta.password, sizeof(w.sta.password), "%s", CONFIG_WD_WIFI_PASS);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &w));
    ESP_ERROR_CHECK(esp_wifi_start());
    xEventGroupWaitBits(s_wifi, WIFI_OK, pdFALSE, pdTRUE, portMAX_DELAY);
}

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    hal_display_init();
    render_clear(0x0000);
    render_fill_rect(0, 0, 320, 20, 0x001F);
    wifi_start();
    wd_mqtt_cfg_t m = {
        .uri = CONFIG_WD_MQTT_URI,
        .device_id = CONFIG_WD_DEVICE_ID,
        .buffer_size = 8192,
        .inline_max = 6144,
    };
    wd_mqtt_start(&m);
    ESP_LOGI(TAG, "mqtt started; waiting cmd on wd/%s/cmd", CONFIG_WD_DEVICE_ID);
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

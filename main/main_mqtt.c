/*
 * MQTT-enabled main. Build: idf.py -D MQTT_MAIN=1 ...
 * Wi-Fi/broker via components/net (NVS + SoftAP provision).
 */
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hal_display.h"
#include "render.h"
#include "wd_net.h"
#include "wd_provision.h"

#include <string.h>

static const char *TAG = "main_mqtt";

static void provision_ui(const char *ap_ssid, const char *url)
{
    render_clear(0x0011);
    render_fill_rect(0, 0, 320, 28, 0xF800); /* red = setup */
    render_draw_text(8, 8, (const uint8_t *)"SETUP", 5, 0xFFFF, 2);
    render_draw_text(8, 48, (const uint8_t *)"join Wi-Fi AP:", 13, 0x07FF, 1);
    render_draw_text(8, 68, (const uint8_t *)ap_ssid, strlen(ap_ssid), 0xFFE0, 2);
    render_draw_text(8, 110, (const uint8_t *)"open in browser:", 16, 0x07FF, 1);
    render_draw_text(8, 130, (const uint8_t *)url, strlen(url), 0xFFFF, 1);
    render_draw_text(8, 170, (const uint8_t *)"save Wi-Fi + MQTT", 17, 0xC618, 1);
}

void app_main(void)
{
    hal_display_init();
    render_clear(0x0000);
    render_fill_rect(0, 0, 320, 20, 0x001F); /* blue = booted */
    wd_provision_set_ui(provision_ui);
    wd_net_start();
    render_fill_rect(0, 0, 320, 20, 0x07E0); /* green = wifi+mqtt up */
    ESP_LOGI(TAG, "running");
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

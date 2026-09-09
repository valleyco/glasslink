/*
 * MQTT-enabled main. Build: idf.py -D MQTT_MAIN=1 ...
 * Wi-Fi/broker via components/net (NVS + Kconfig seed).
 */
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hal_display.h"
#include "render.h"
#include "wd_net.h"

static const char *TAG = "main_mqtt";

void app_main(void)
{
    hal_display_init();
    render_clear(0x0000);
    render_fill_rect(0, 0, 320, 20, 0x001F); /* blue = booted */
    wd_net_start();
    render_fill_rect(0, 0, 320, 20, 0x07E0); /* green = wifi+mqtt up */
    ESP_LOGI(TAG, "running");
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

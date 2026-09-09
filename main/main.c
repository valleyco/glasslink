/*
 * Default firmware entry (Step 4). Static bars until MQTT lands.
 */

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hal_display.h"
#include "render.h"

static const char *TAG = "main";

void app_main(void)
{
    ESP_LOGI(TAG, "esp32-wl-display boot (Step 4 — no MQTT yet)");
    hal_display_init();
    render_clear(0x0000);
    render_fill_rect(0, 0, 320, 40, 0x001F);
    render_fill_rect(0, 100, 320, 40, 0xF800);
    render_fill_rect(0, 200, 320, 40, 0x07E0);
    ESP_LOGI(TAG, "static bars drawn; idle");
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

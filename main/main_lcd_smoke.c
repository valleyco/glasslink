/*
 * Step 4 visual smoke: clear + static rects via render/contract (no Wi-Fi).
 * Flash: make flash-lcd-smoke && make monitor
 */

#include "codec.h"
#include "contract.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hal_display.h"
#include "render.h"

static const char *TAG = "lcd_smoke";

static void draw_pattern_direct(void)
{
    render_clear(0x0000);
    render_fill_rect(0, 0, 80, 240, 0xF800);
    render_fill_rect(80, 0, 80, 240, 0x07E0);
    render_fill_rect(160, 0, 80, 240, 0x001F);
    render_fill_rect(240, 0, 80, 240, 0xFFFF);

    uint16_t block[16 * 16];
    for (int i = 0; i < 16 * 16; i++) {
        block[i] = 0xFFE0;
    }
    render_blit_rect(152, 112, 16, 16, block);
}

static void draw_via_contract(void)
{
    uint8_t msg[256];
    uint16_t px[8 * 8];
    size_t n;

    for (int i = 0; i < 64; i++) {
        px[i] = 0xF81F;
    }

    n = contract_pack_clear(msg, sizeof(msg), 1, 1, 0x2104);
    if (n) {
        (void)contract_dispatch(msg, n);
    }

    n = contract_pack_rect(msg, sizeof(msg), 2, 2, 20, 20, 8, 8,
                           CODEC_ENC_RAW_RGB565, (const uint8_t *)px,
                           sizeof(px));
    if (n) {
        (void)contract_dispatch(msg, n);
    }

    {
        uint16_t solid[32 * 16];
        uint8_t enc[256];
        size_t elen = 0;
        for (int i = 0; i < 32 * 16; i++) {
            solid[i] = 0x07FF;
        }
        if (codec_encode(CODEC_ENC_RAW_RGB565, solid, 32, 16, enc, sizeof(enc),
                         &elen) == CODEC_OK) {
            uint8_t big[512];
            n = contract_pack_rect(big, sizeof(big), 3, 3, 40, 180, 32, 16,
                                   CODEC_ENC_RAW_RGB565, enc, (uint32_t)elen);
            if (n) {
                (void)contract_dispatch(big, n);
            }
        }
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "wl-display LCD smoke start");
    hal_display_init();

    while (1) {
        ESP_LOGI(TAG, "pattern via render_*");
        draw_pattern_direct();
        vTaskDelay(pdMS_TO_TICKS(2500));

        ESP_LOGI(TAG, "pattern via contract_dispatch");
        draw_via_contract();
        vTaskDelay(pdMS_TO_TICKS(2500));
    }
}

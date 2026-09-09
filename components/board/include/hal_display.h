#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Primary CYD landscape profile (W1). */
enum {
    HAL_DISPLAY_WIDTH = 320,
    HAL_DISPLAY_HEIGHT = 240
};

void hal_display_init(void);

void hal_display_get_size(int *w, int *h);

/**
 * Fill rectangle in panel coords. Clipped to panel.
 * rgb565 is host-endian (native uint16_t); device SPI backend byteswaps on TX.
 */
void hal_display_fill_rect(int x, int y, int w, int h, uint16_t rgb565);

/**
 * Blit row-major RGB565 rect (w * h pixels). Clipped to panel.
 * Source is tightly packed for the unclipped logical rect; only the
 * intersection with the panel is written (src stepped accordingly).
 */
void hal_display_blit_rect_rgb565(int x, int y, int w, int h,
                                  const uint16_t *rgb565);

#ifdef __cplusplus
}
#endif

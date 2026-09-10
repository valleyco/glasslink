#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Fill entire panel with solid color. */
void render_clear(uint16_t rgb565);

/**
 * Blit a tightly packed RGB565 rect at (x,y).
 * Returns 0 on success, -1 if w/h invalid or rgb565 is NULL when w,h > 0.
 * Clipping is handled by the display backend.
 */
int render_blit_rect(int x, int y, int w, int h, const uint16_t *rgb565);

/**
 * Solid fill rect. Same clipping rules as HAL.
 * Returns 0 on success, -1 if w/h <= 0.
 */
int render_fill_rect(int x, int y, int w, int h, uint16_t rgb565);

/**
 * Draw ASCII (bytes 0x20–0x7E) with built-in 5×7 font.
 * scale is 1 or 2. Non-ASCII bytes skipped. Transparent background (set pixels only).
 * Returns 0 on success, -1 if text is NULL / scale invalid.
 */
int render_draw_text(int x, int y, const uint8_t *text, size_t len, uint16_t color,
                     int scale);

#ifdef __cplusplus
}
#endif

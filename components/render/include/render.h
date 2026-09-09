#pragma once

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
 * Solid fill rect (helper for tests / later L1). Same clipping rules as HAL.
 * Returns 0 on success, -1 if w/h <= 0 after interpreting args as empty-only invalid when w or h <= 0.
 */
int render_fill_rect(int x, int y, int w, int h, uint16_t rgb565);

#ifdef __cplusplus
}
#endif

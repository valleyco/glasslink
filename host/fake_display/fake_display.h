#pragma once

#include <stddef.h>
#include <stdint.h>

#include "hal_display.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Reset panel to 0x0000 and call hal_display_init semantics. */
void fake_display_reset(void);

/** Read one pixel (0 if out of range). */
uint16_t fake_display_get_pixel(int x, int y);

/** Direct panel buffer (row-major), length HAL_DISPLAY_WIDTH * HAL_DISPLAY_HEIGHT. */
const uint16_t *fake_display_panel(void);

size_t fake_display_panel_len(void);

/** Count pixels equal to color (full panel). */
size_t fake_display_count_color(uint16_t rgb565);

#ifdef __cplusplus
}
#endif

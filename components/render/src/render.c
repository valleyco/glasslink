#include "render.h"
#include "hal_display.h"

void render_clear(uint16_t rgb565)
{
    int w = 0;
    int h = 0;
    hal_display_get_size(&w, &h);
    hal_display_fill_rect(0, 0, w, h, rgb565);
}

int render_blit_rect(int x, int y, int w, int h, const uint16_t *rgb565)
{
    if (w <= 0 || h <= 0) {
        return -1;
    }
    if (!rgb565) {
        return -1;
    }
    hal_display_blit_rect_rgb565(x, y, w, h, rgb565);
    return 0;
}

int render_fill_rect(int x, int y, int w, int h, uint16_t rgb565)
{
    if (w <= 0 || h <= 0) {
        return -1;
    }
    hal_display_fill_rect(x, y, w, h, rgb565);
    return 0;
}

#include "fake_display.h"

#include <string.h>

static uint16_t s_panel[HAL_DISPLAY_WIDTH * HAL_DISPLAY_HEIGHT];
static int s_inited;

static int clip_rect(int *x, int *y, int *w, int *h, int *src_ox, int *src_oy)
{
    int x0 = *x;
    int y0 = *y;
    int rw = *w;
    int rh = *h;

    *src_ox = 0;
    *src_oy = 0;

    if (rw <= 0 || rh <= 0) {
        return 0;
    }

    if (x0 < 0) {
        *src_ox = -x0;
        rw += x0;
        x0 = 0;
    }
    if (y0 < 0) {
        *src_oy = -y0;
        rh += y0;
        y0 = 0;
    }
    if (x0 + rw > HAL_DISPLAY_WIDTH) {
        rw = HAL_DISPLAY_WIDTH - x0;
    }
    if (y0 + rh > HAL_DISPLAY_HEIGHT) {
        rh = HAL_DISPLAY_HEIGHT - y0;
    }
    if (rw <= 0 || rh <= 0) {
        return 0;
    }

    *x = x0;
    *y = y0;
    *w = rw;
    *h = rh;
    return 1;
}

void fake_display_reset(void)
{
    memset(s_panel, 0, sizeof(s_panel));
    s_inited = 1;
}

void hal_display_init(void)
{
    fake_display_reset();
}

void hal_display_get_size(int *w, int *h)
{
    if (w) {
        *w = HAL_DISPLAY_WIDTH;
    }
    if (h) {
        *h = HAL_DISPLAY_HEIGHT;
    }
}

void hal_display_fill_rect(int x, int y, int w, int h, uint16_t rgb565)
{
    int src_ox = 0;
    int src_oy = 0;
    if (!s_inited) {
        hal_display_init();
    }
    if (!clip_rect(&x, &y, &w, &h, &src_ox, &src_oy)) {
        return;
    }
    (void)src_ox;
    (void)src_oy;

    for (int row = 0; row < h; row++) {
        uint16_t *dst = s_panel + (y + row) * HAL_DISPLAY_WIDTH + x;
        for (int col = 0; col < w; col++) {
            dst[col] = rgb565;
        }
    }
}

void hal_display_blit_rect_rgb565(int x, int y, int w, int h,
                                  const uint16_t *rgb565)
{
    int src_ox = 0;
    int src_oy = 0;
    int orig_w = w;

    if (!rgb565 || w <= 0 || h <= 0) {
        return;
    }
    if (!s_inited) {
        hal_display_init();
    }
    if (!clip_rect(&x, &y, &w, &h, &src_ox, &src_oy)) {
        return;
    }

    for (int row = 0; row < h; row++) {
        const uint16_t *src =
            rgb565 + (src_oy + row) * orig_w + src_ox;
        uint16_t *dst = s_panel + (y + row) * HAL_DISPLAY_WIDTH + x;
        memcpy(dst, src, (size_t)w * sizeof(uint16_t));
    }
}

uint16_t fake_display_get_pixel(int x, int y)
{
    if (x < 0 || y < 0 || x >= HAL_DISPLAY_WIDTH || y >= HAL_DISPLAY_HEIGHT) {
        return 0;
    }
    return s_panel[y * HAL_DISPLAY_WIDTH + x];
}

const uint16_t *fake_display_panel(void)
{
    return s_panel;
}

size_t fake_display_panel_len(void)
{
    return (size_t)HAL_DISPLAY_WIDTH * (size_t)HAL_DISPLAY_HEIGHT;
}

size_t fake_display_count_color(uint16_t rgb565)
{
    size_t n = 0;
    size_t len = fake_display_panel_len();
    for (size_t i = 0; i < len; i++) {
        if (s_panel[i] == rgb565) {
            n++;
        }
    }
    return n;
}

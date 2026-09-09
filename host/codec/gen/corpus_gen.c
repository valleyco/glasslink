#include "corpus_gen.h"

#include <string.h>

static const char *const k_names[CORPUS_CLASS_COUNT] = {
    "solid",
    "h_runs",
    "v_runs",
    "grid",
    "gradient_h",
    "gradient_v",
    "gradient_2d",
    "ui_panel",
    "ui_text",
    "ui_gauge",
    "noise_white",
    "noise_1bit",
    "photo_soft",
    "sparse_dirty",
    "partial_tl",
    "partial_br",
    "adversarial_rle",
};

const char *corpus_class_name(corpus_class_t cls)
{
    if (cls < 0 || cls >= CORPUS_CLASS_COUNT) {
        return "?";
    }
    return k_names[cls];
}

int corpus_class_from_name(const char *name)
{
    if (!name) {
        return -1;
    }
    for (int i = 0; i < CORPUS_CLASS_COUNT; i++) {
        if (strcmp(name, k_names[i]) == 0) {
            return i;
        }
    }
    return -1;
}

static unsigned lcg_next(unsigned *s)
{
    *s = (*s) * 1664525u + 1013904223u;
    return *s;
}

static uint16_t rgb565(unsigned r5, unsigned g6, unsigned b5)
{
    return (uint16_t)(((r5 & 0x1fu) << 11) | ((g6 & 0x3fu) << 5) | (b5 & 0x1fu));
}

static void fill_solid(uint16_t *rgb, int n, uint16_t c)
{
    for (int i = 0; i < n; i++) {
        rgb[i] = c;
    }
}

static void fill_noise(uint16_t *rgb, int n, unsigned *s)
{
    for (int i = 0; i < n; i++) {
        rgb[i] = (uint16_t)(lcg_next(s) >> 16);
    }
}

static void fill_photo_soft(uint16_t *rgb, int w, int h, unsigned seed)
{
    int n = w * h;
    unsigned s = seed ? seed : 1u;
    fill_noise(rgb, n, &s);
    for (int pass = 0; pass < 2; pass++) {
        for (int y = 1; y < h - 1; y++) {
            for (int x = 1; x < w - 1; x++) {
                unsigned sum = 0;
                for (int dy = -1; dy <= 1; dy++) {
                    for (int dx = -1; dx <= 1; dx++) {
                        sum += rgb[(y + dy) * w + (x + dx)];
                    }
                }
                rgb[y * w + x] = (uint16_t)(sum / 9u);
            }
        }
    }
    for (int b = 0; b < 4; b++) {
        int cx = (int)(lcg_next(&s) % (unsigned)(w > 0 ? w : 1));
        int cy = (int)(lcg_next(&s) % (unsigned)(h > 0 ? h : 1));
        int rad = 3 + (int)(lcg_next(&s) % 8u);
        uint16_t col = (uint16_t)(lcg_next(&s) >> 16);
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                int dx = x - cx, dy = y - cy;
                if (dx * dx + dy * dy <= rad * rad) {
                    rgb[y * w + x] = col;
                }
            }
        }
    }
}

static void stamp_block(uint16_t *rgb, int w, int h, int x0, int y0, int bw,
                        int bh, uint16_t c)
{
    for (int y = y0; y < y0 + bh && y < h; y++) {
        if (y < 0) {
            continue;
        }
        for (int x = x0; x < x0 + bw && x < w; x++) {
            if (x < 0) {
                continue;
            }
            rgb[y * w + x] = c;
        }
    }
}

static void stamp_glyph(uint16_t *rgb, int w, int h, int x0, int y0, int id,
                        uint16_t fg, uint16_t bg)
{
    static const uint8_t bits[4][7] = {
        {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11},
        {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E},
        {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E},
        {0x1C, 0x12, 0x11, 0x11, 0x11, 0x12, 0x1C},
    };
    const uint8_t *g = bits[id & 3];
    for (int row = 0; row < 7; row++) {
        for (int col = 0; col < 5; col++) {
            uint16_t c = (g[row] & (1u << (4 - col))) ? fg : bg;
            int x = x0 + col;
            int y = y0 + row;
            if (x >= 0 && x < w && y >= 0 && y < h) {
                rgb[y * w + x] = c;
            }
        }
    }
}

int corpus_fill(corpus_class_t cls, uint16_t *rgb, int w, int h, unsigned seed)
{
    int n;
    unsigned s;

    if (!rgb || w <= 0 || h <= 0) {
        return -1;
    }
    n = w * h;
    s = seed ? seed : 0xC0FFEEu;

    switch (cls) {
    case CORPUS_SOLID:
        fill_solid(rgb, n, rgb565(0x1f, 0, 0));
        break;

    case CORPUS_H_RUNS:
        for (int y = 0; y < h; y++) {
            uint16_t c =
                ((y / 4) & 1) ? rgb565(0, 0x3f, 0) : rgb565(0, 0, 0x1f);
            for (int x = 0; x < w; x++) {
                rgb[y * w + x] = c;
            }
        }
        break;

    case CORPUS_V_RUNS:
        for (int x = 0; x < w; x++) {
            uint16_t c = ((x / 4) & 1) ? 0xFFFF : 0x0000;
            for (int y = 0; y < h; y++) {
                rgb[y * w + x] = c;
            }
        }
        break;

    case CORPUS_GRID:
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                rgb[y * w + x] = ((x + y) & 1) ? 0xFFFF : 0x0000;
            }
        }
        break;

    case CORPUS_GRADIENT_H:
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                unsigned t =
                    w > 1 ? (unsigned)x * 31u / (unsigned)(w - 1) : 0;
                rgb[y * w + x] = rgb565(t, t * 2, 31u - t);
            }
        }
        break;

    case CORPUS_GRADIENT_V:
        for (int y = 0; y < h; y++) {
            unsigned t = h > 1 ? (unsigned)y * 31u / (unsigned)(h - 1) : 0;
            for (int x = 0; x < w; x++) {
                rgb[y * w + x] = rgb565(t, 20, 31u - t);
            }
        }
        break;

    case CORPUS_GRADIENT_2D:
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                unsigned rx =
                    w > 1 ? (unsigned)x * 31u / (unsigned)(w - 1) : 0;
                unsigned ry =
                    h > 1 ? (unsigned)y * 63u / (unsigned)(h - 1) : 0;
                rgb[y * w + x] = rgb565(rx, ry, (rx + ry) & 0x1fu);
            }
        }
        break;

    case CORPUS_UI_PANEL: {
        fill_solid(rgb, n, rgb565(4, 8, 12));
        stamp_block(rgb, w, h, 0, 0, w, h > 12 ? 12 : h / 3, rgb565(2, 4, 20));
        stamp_block(rgb, w, h, 2, h > 16 ? 16 : h / 2, w > 4 ? w - 4 : w,
                    h > 24 ? h - 24 : h / 3, rgb565(8, 16, 8));
        stamp_block(rgb, w, h, 0, 0, w, 1, 0xFFFF);
        stamp_block(rgb, w, h, 0, h - 1, w, 1, 0xFFFF);
        stamp_block(rgb, w, h, 0, 0, 1, h, 0xFFFF);
        stamp_block(rgb, w, h, w - 1, 0, 1, h, 0xFFFF);
        break;
    }

    case CORPUS_UI_TEXT: {
        fill_solid(rgb, n, rgb565(0, 0, 0));
        int col = 2, row = 2, gid = 0;
        while (row + 8 < h) {
            stamp_glyph(rgb, w, h, col, row, gid++, 0xFFFF, 0x0000);
            col += 6;
            if (col + 5 >= w) {
                col = 2;
                row += 9;
            }
            if (gid > 40) {
                break;
            }
        }
        break;
    }

    case CORPUS_UI_GAUGE: {
        fill_solid(rgb, n, rgb565(2, 2, 2));
        int cx = w / 2, cy = h / 2;
        int rad = (w < h ? w : h) / 2 - 1;
        if (rad < 1) {
            rad = 1;
        }
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                int dx = x - cx, dy = y - cy;
                int d2 = dx * dx + dy * dy;
                if (d2 <= rad * rad && d2 >= (rad - 3) * (rad - 3)) {
                    rgb[y * w + x] = rgb565(0x1f, 0x20, 0);
                }
                if (d2 <= 4) {
                    rgb[y * w + x] = 0xFFFF;
                }
            }
        }
        for (int i = 0; i < rad; i++) {
            int x = cx + i / 2;
            int y = cy - i / 2;
            if (x >= 0 && x < w && y >= 0 && y < h) {
                rgb[y * w + x] = rgb565(0x1f, 0, 0);
            }
        }
        break;
    }

    case CORPUS_NOISE_WHITE:
        fill_noise(rgb, n, &s);
        break;

    case CORPUS_NOISE_1BIT:
        for (int i = 0; i < n; i++) {
            rgb[i] = (lcg_next(&s) & 1u) ? 0xFFFF : 0x0000;
        }
        break;

    case CORPUS_PHOTO_SOFT:
        fill_photo_soft(rgb, w, h, s);
        break;

    case CORPUS_SPARSE_DIRTY: {
        fill_solid(rgb, n, rgb565(6, 12, 6));
        stamp_block(rgb, w, h, w / 8, h / 8, w / 5 + 1, h / 6 + 1,
                    rgb565(0x1f, 0, 0));
        stamp_block(rgb, w, h, w / 2, h / 3, 8, 8, 0xFFFF);
        stamp_glyph(rgb, w, h, w > 10 ? w - 10 : 0, h > 10 ? h - 10 : 0, 1,
                    rgb565(0, 0x3f, 0), rgb565(6, 12, 6));
        break;
    }

    case CORPUS_PARTIAL_TL:
        fill_solid(rgb, n, 0);
        stamp_block(rgb, w, h, 0, 0, w > 4 ? w / 2 : w, h > 4 ? h / 2 : h,
                    rgb565(0x1f, 0x3f, 0));
        break;

    case CORPUS_PARTIAL_BR:
        fill_solid(rgb, n, 0);
        stamp_block(rgb, w, h, w / 2, h / 2, w - w / 2, h - h / 2,
                    rgb565(0, 0, 0x1f));
        break;

    case CORPUS_ADVERSARIAL_RLE:
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                rgb[y * w + x] =
                    (uint16_t)(((x + y) & 1) ? 0xAAAA : 0x5555);
            }
        }
        break;

    default:
        return -1;
    }

    return 0;
}

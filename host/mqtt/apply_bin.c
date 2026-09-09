/*
 * Apply one L0 binary envelope to fake_display (host MQTT sim / loopback).
 * Usage:
 *   apply_bin file.bin [--expect-clear 0xF800] [--expect-px x y color]...
 *   apply_bin file.bin --expect-fail
 */

#include "contract.h"
#include "fake_display.h"
#include "render.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_EXPECT_PX 16

static int parse_u16(const char *s, uint16_t *out)
{
    char *end = NULL;
    unsigned long v = strtoul(s, &end, 0);
    if (!end || *end != '\0' || v > 0xfffful) {
        return -1;
    }
    *out = (uint16_t)v;
    return 0;
}

static uint8_t *read_file(const char *path, size_t *len_out)
{
    FILE *f = fopen(path, "rb");
    long sz;
    uint8_t *buf;
    if (!f) {
        perror(path);
        return NULL;
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }
    sz = ftell(f);
    if (sz < 0) {
        fclose(f);
        return NULL;
    }
    rewind(f);
    buf = (uint8_t *)malloc((size_t)sz);
    if (!buf) {
        fclose(f);
        return NULL;
    }
    if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
        free(buf);
        fclose(f);
        return NULL;
    }
    fclose(f);
    *len_out = (size_t)sz;
    return buf;
}

int main(int argc, char **argv)
{
    const char *path = NULL;
    int expect_clear = 0;
    uint16_t clear_color = 0;
    int expect_fail = 0;
    int n_px = 0;
    int px_x[MAX_EXPECT_PX];
    int px_y[MAX_EXPECT_PX];
    uint16_t px_c[MAX_EXPECT_PX];
    uint8_t *buf;
    size_t len = 0;
    int rc;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--expect-clear") == 0 && i + 1 < argc) {
            expect_clear = 1;
            if (parse_u16(argv[++i], &clear_color) != 0) {
                fprintf(stderr, "bad color\n");
                return 2;
            }
        } else if (strcmp(argv[i], "--expect-fail") == 0) {
            expect_fail = 1;
        } else if (strcmp(argv[i], "--expect-px") == 0 && i + 3 < argc) {
            if (n_px >= MAX_EXPECT_PX) {
                fprintf(stderr, "too many --expect-px\n");
                return 2;
            }
            px_x[n_px] = atoi(argv[++i]);
            px_y[n_px] = atoi(argv[++i]);
            if (parse_u16(argv[++i], &px_c[n_px]) != 0) {
                fprintf(stderr, "bad px color\n");
                return 2;
            }
            n_px++;
        } else if (argv[i][0] != '-') {
            path = argv[i];
        } else {
            fprintf(stderr,
                    "usage: %s file.bin [--expect-clear C] [--expect-px x y C]... "
                    "[--expect-fail]\n",
                    argv[0]);
            return 2;
        }
    }
    if (!path) {
        fprintf(stderr, "need file.bin\n");
        return 2;
    }

    buf = read_file(path, &len);
    if (!buf) {
        return 2;
    }

    fake_display_reset();
    rc = contract_dispatch(buf, len);
    free(buf);

    if (expect_fail) {
        if (rc == CONTRACT_OK) {
            fprintf(stderr, "expect-fail but dispatch OK\n");
            return 1;
        }
        printf("expect-fail OK (dispatch rc=%d)\n", rc);
        return 0;
    }

    if (rc != CONTRACT_OK) {
        fprintf(stderr, "dispatch rc=%d\n", rc);
        return 1;
    }

    printf("ok bytes_applied panel=%dx%d\n", HAL_DISPLAY_WIDTH, HAL_DISPLAY_HEIGHT);

    if (expect_clear) {
        size_t n = fake_display_count_color(clear_color);
        size_t all = (size_t)HAL_DISPLAY_WIDTH * (size_t)HAL_DISPLAY_HEIGHT;
        if (n != all) {
            fprintf(stderr, "expect clear 0x%04x count=%zu want=%zu\n", clear_color, n,
                    all);
            return 1;
        }
        printf("expect-clear 0x%04x OK\n", clear_color);
    }
    for (int i = 0; i < n_px; i++) {
        uint16_t g = fake_display_get_pixel(px_x[i], px_y[i]);
        if (g != px_c[i]) {
            fprintf(stderr, "expect-px (%d,%d) want 0x%04x got 0x%04x\n", px_x[i],
                    px_y[i], px_c[i], g);
            return 1;
        }
        printf("expect-px (%d,%d)=0x%04x OK\n", px_x[i], px_y[i], px_c[i]);
    }
    return 0;
}

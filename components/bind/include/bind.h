#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    BIND_SLOT_MAX = 8,
    BIND_TEXT_MAX = 64
};

enum {
    BIND_OK = 0,
    BIND_ERR_ARG = -1,
    BIND_ERR_SLOT = -2
};

typedef struct bind_slot {
    uint8_t used;
    uint8_t scale;
    uint8_t max_chars;
    int16_t x;
    int16_t y;
    uint16_t fg;
    uint16_t bg;
    char text[BIND_TEXT_MAX + 1];
} bind_slot_t;

void bind_reset(void);

/** Define/replace a text slot and redraw. text may be NULL / len 0. */
int bind_define(uint8_t slot, int16_t x, int16_t y, uint16_t fg, uint16_t bg,
                uint8_t scale, uint8_t max_chars, const uint8_t *text,
                size_t text_len);

/** Update text and redraw. Truncates to max_chars. */
int bind_set_text(uint8_t slot, const uint8_t *text, size_t text_len);

const bind_slot_t *bind_get(uint8_t slot);

#ifdef __cplusplus
}
#endif

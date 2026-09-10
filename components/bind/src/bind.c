#include "bind.h"

#include "render.h"

#include <string.h>

static bind_slot_t s_slots[BIND_SLOT_MAX];

void bind_reset(void)
{
    memset(s_slots, 0, sizeof(s_slots));
}

static int erase_and_draw(const bind_slot_t *s)
{
    int scale = s->scale ? (int)s->scale : 1;
    int tw = (int)s->max_chars * 6 * scale;
    int th = 7 * scale;
    if (tw <= 0 || th <= 0) {
        return BIND_ERR_ARG;
    }
    (void)render_fill_rect((int)s->x, (int)s->y, tw, th, s->bg);
    if (s->text[0]) {
        (void)render_draw_text((int)s->x, (int)s->y, (const uint8_t *)s->text,
                               strlen(s->text), s->fg, scale);
    }
    return BIND_OK;
}

static void copy_text(bind_slot_t *s, const uint8_t *text, size_t text_len)
{
    size_t n = text_len;
    size_t cap = s->max_chars;
    if (cap > BIND_TEXT_MAX) {
        cap = BIND_TEXT_MAX;
    }
    if (n > cap) {
        n = cap;
    }
    if (text && n) {
        memcpy(s->text, text, n);
    }
    s->text[n] = '\0';
}

int bind_define(uint8_t slot, int16_t x, int16_t y, uint16_t fg, uint16_t bg,
                uint8_t scale, uint8_t max_chars, const uint8_t *text,
                size_t text_len)
{
    bind_slot_t *s;
    if (slot >= BIND_SLOT_MAX) {
        return BIND_ERR_SLOT;
    }
    if (max_chars == 0 || max_chars > BIND_TEXT_MAX) {
        return BIND_ERR_ARG;
    }
    if (scale > 2) {
        return BIND_ERR_ARG;
    }
    s = &s_slots[slot];
    s->used = 1;
    s->x = x;
    s->y = y;
    s->fg = fg;
    s->bg = bg;
    s->scale = scale ? scale : 1;
    s->max_chars = max_chars;
    copy_text(s, text, text_len);
    return erase_and_draw(s);
}

int bind_set_text(uint8_t slot, const uint8_t *text, size_t text_len)
{
    bind_slot_t *s;
    if (slot >= BIND_SLOT_MAX) {
        return BIND_ERR_SLOT;
    }
    s = &s_slots[slot];
    if (!s->used) {
        return BIND_ERR_SLOT;
    }
    copy_text(s, text, text_len);
    return erase_and_draw(s);
}

const bind_slot_t *bind_get(uint8_t slot)
{
    if (slot >= BIND_SLOT_MAX) {
        return NULL;
    }
    return &s_slots[slot];
}

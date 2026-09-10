#include "contract.h"

#include "bind.h"
#include "codec.h"
#include "render.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
    int16_t x;
    int16_t y;
} row_blit_ctx_t;

typedef struct {
    uint8_t used;
    uint16_t len;
    uint8_t bytes[CONTRACT_BATCH_BYTES_MAX];
} group_slot_t;

static contract_fetch_fn s_fetch;
static contract_fetch_release_fn s_fetch_release;
static void *s_fetch_user;
static group_slot_t s_groups[CONTRACT_GROUP_SLOTS];

void contract_set_fetch(contract_fetch_fn fn, void *user)
{
    s_fetch = fn;
    s_fetch_user = user;
}

void contract_set_fetch_release(contract_fetch_release_fn fn)
{
    s_fetch_release = fn;
}

void contract_group_reset(void)
{
    memset(s_groups, 0, sizeof(s_groups));
}

static void release_fetch_body(uint8_t *body)
{
    if (!body) {
        return;
    }
    if (s_fetch_release) {
        s_fetch_release(body, s_fetch_user);
    } else {
        free(body);
    }
}

static uint16_t rd_u16(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static int16_t rd_i16(const uint8_t *p)
{
    return (int16_t)rd_u16(p);
}

static void blit_row_cb(int y, const uint16_t *row, int width, void *user)
{
    row_blit_ctx_t *c = (row_blit_ctx_t *)user;
    (void)render_blit_rect((int)c->x, (int)c->y + y, width, 1, row);
}

static int apply_rect_pixels(const contract_msg_t *msg, const uint8_t *pix,
                             size_t pix_len)
{
    row_blit_ctx_t ctx;
    int rc;

    if (msg->w > (uint16_t)CODEC_MAX_WIDTH) {
        return CONTRACT_ERR_ARG;
    }
    if (!pix && pix_len > 0) {
        return CONTRACT_ERR_ARG;
    }
    if (msg->enc == (uint8_t)CODEC_ENC_RAW_RGB565) {
        uint64_t need = (uint64_t)msg->w * (uint64_t)msg->h * 2ull;
        if (need != (uint64_t)pix_len) {
            return CONTRACT_ERR_PAYLOAD;
        }
    } else if (pix_len == 0) {
        return CONTRACT_ERR_PAYLOAD;
    }

    ctx.x = msg->x;
    ctx.y = msg->y;
    rc = codec_decode_rows((codec_enc_t)msg->enc, (int)msg->w, (int)msg->h, pix,
                           pix_len, blit_row_cb, &ctx);
    if (rc == CODEC_OK) {
        return CONTRACT_OK;
    }
    if (rc == CODEC_ERR_NOSPACE) {
        return CONTRACT_ERR_NOSPACE;
    }
    if (rc == CODEC_ERR_TRUNC) {
        return CONTRACT_ERR_TRUNC;
    }
    return CONTRACT_ERR_PAYLOAD;
}

int contract_apply_batch(const uint8_t *ops, size_t len)
{
    size_t i = 0;
    unsigned nops = 0;

    if (!ops || len == 0 || len > (size_t)CONTRACT_BATCH_BYTES_MAX) {
        return CONTRACT_ERR_PAYLOAD;
    }
    while (i < len) {
        uint8_t op;
        if (nops >= (unsigned)CONTRACT_BATCH_OPS_MAX) {
            return CONTRACT_ERR_PAYLOAD;
        }
        op = ops[i++];
        if (op == CONTRACT_BATCH_OP_FILL) {
            int16_t x, y;
            uint16_t w, h, color;
            if (i + 10 > len) {
                return CONTRACT_ERR_TRUNC;
            }
            x = rd_i16(ops + i);
            y = rd_i16(ops + i + 2);
            w = rd_u16(ops + i + 4);
            h = rd_u16(ops + i + 6);
            color = rd_u16(ops + i + 8);
            i += 10;
            if (w == 0 || h == 0) {
                return CONTRACT_ERR_ARG;
            }
            if (render_fill_rect((int)x, (int)y, (int)w, (int)h, color) != 0) {
                return CONTRACT_ERR_ARG;
            }
        } else if (op == CONTRACT_BATCH_OP_TEXT) {
            int16_t x, y;
            uint16_t color;
            uint8_t scale, tlen;
            int sc;
            if (i + 8 > len) {
                return CONTRACT_ERR_TRUNC;
            }
            x = rd_i16(ops + i);
            y = rd_i16(ops + i + 2);
            color = rd_u16(ops + i + 4);
            scale = ops[i + 6];
            tlen = ops[i + 7];
            i += 8;
            if (tlen == 0 || tlen > (uint8_t)CONTRACT_TEXT_MAX || scale > 2) {
                return CONTRACT_ERR_ARG;
            }
            if (i + (size_t)tlen > len) {
                return CONTRACT_ERR_TRUNC;
            }
            sc = scale ? (int)scale : 1;
            if (render_draw_text((int)x, (int)y, ops + i, (size_t)tlen, color,
                                 sc) != 0) {
                return CONTRACT_ERR_ARG;
            }
            i += (size_t)tlen;
        } else {
            return CONTRACT_ERR_TYPE;
        }
        nops++;
    }
    return CONTRACT_OK;
}

static int group_define_apply(uint8_t gid, const uint8_t *ops, size_t len)
{
    group_slot_t *g;
    int rc;
    if (gid >= CONTRACT_GROUP_SLOTS) {
        return CONTRACT_ERR_ARG;
    }
    if (!ops || len == 0 || len > (size_t)CONTRACT_BATCH_BYTES_MAX) {
        return CONTRACT_ERR_PAYLOAD;
    }
    rc = contract_apply_batch(ops, len);
    if (rc != CONTRACT_OK) {
        return rc;
    }
    g = &s_groups[gid];
    memcpy(g->bytes, ops, len);
    g->len = (uint16_t)len;
    g->used = 1;
    return CONTRACT_OK;
}

static int group_draw_apply(uint8_t gid)
{
    group_slot_t *g;
    if (gid >= CONTRACT_GROUP_SLOTS) {
        return CONTRACT_ERR_ARG;
    }
    g = &s_groups[gid];
    if (!g->used || g->len == 0) {
        return CONTRACT_ERR_ARG;
    }
    return contract_apply_batch(g->bytes, (size_t)g->len);
}

int contract_apply(const contract_msg_t *msg)
{
    if (!msg) {
        return CONTRACT_ERR_ARG;
    }
    switch (msg->type) {
    case CONTRACT_TYPE_DISPLAY_CLEAR:
        render_clear(msg->color);
        return CONTRACT_OK;

    case CONTRACT_TYPE_RASTER_RECT:
        if (msg->flags & CONTRACT_FLAG_URI) {
            uint8_t *body = NULL;
            size_t body_len = 0;
            int frc;
            int rc;

            if (!s_fetch) {
                return CONTRACT_ERR_FETCH;
            }
            if (!msg->payload || msg->payload_len == 0) {
                return CONTRACT_ERR_PAYLOAD;
            }
            frc = s_fetch(msg->payload, (size_t)msg->payload_len, &body,
                          &body_len, s_fetch_user);
            if (frc != 0 || !body) {
                release_fetch_body(body);
                return CONTRACT_ERR_FETCH;
            }
            rc = apply_rect_pixels(msg, body, body_len);
            release_fetch_body(body);
            return rc;
        }
        return apply_rect_pixels(msg, msg->payload, (size_t)msg->payload_len);

    case CONTRACT_TYPE_FILL_RECT:
        if (render_fill_rect((int)msg->x, (int)msg->y, (int)msg->w, (int)msg->h,
                             msg->color) != 0) {
            return CONTRACT_ERR_ARG;
        }
        return CONTRACT_OK;

    case CONTRACT_TYPE_DRAW_TEXT: {
        int scale = (msg->enc == 0) ? 1 : (int)msg->enc;
        if (!msg->payload || msg->payload_len == 0) {
            return CONTRACT_ERR_PAYLOAD;
        }
        if (render_draw_text((int)msg->x, (int)msg->y, msg->payload,
                             (size_t)msg->payload_len, msg->color, scale) != 0) {
            return CONTRACT_ERR_ARG;
        }
        return CONTRACT_OK;
    }

    case CONTRACT_TYPE_BIND_DEFINE: {
        uint16_t bg;
        const uint8_t *text;
        size_t tlen;
        int brc;
        uint8_t scale;

        if (!msg->payload || msg->payload_len < 2) {
            return CONTRACT_ERR_PAYLOAD;
        }
        bg = (uint16_t)msg->payload[0] | ((uint16_t)msg->payload[1] << 8);
        text = msg->payload + 2;
        tlen = (size_t)msg->payload_len - 2;
        scale = msg->enc ? msg->enc : 1;
        brc = bind_define((uint8_t)msg->id, msg->x, msg->y, msg->color, bg, scale,
                          (uint8_t)msg->w, text, tlen);
        if (brc == BIND_OK) {
            return CONTRACT_OK;
        }
        if (brc == BIND_ERR_SLOT) {
            return CONTRACT_ERR_ARG;
        }
        return CONTRACT_ERR_ARG;
    }

    case CONTRACT_TYPE_BIND_SET: {
        int brc;
        if (!msg->payload || msg->payload_len == 0) {
            return CONTRACT_ERR_PAYLOAD;
        }
        brc = bind_set_text((uint8_t)msg->id, msg->payload,
                            (size_t)msg->payload_len);
        if (brc == BIND_OK) {
            return CONTRACT_OK;
        }
        return CONTRACT_ERR_ARG;
    }

    case CONTRACT_TYPE_DRAW_BATCH:
        if (!msg->payload) {
            return CONTRACT_ERR_PAYLOAD;
        }
        return contract_apply_batch(msg->payload, (size_t)msg->payload_len);

    case CONTRACT_TYPE_GROUP_DEFINE:
        if (!msg->payload) {
            return CONTRACT_ERR_PAYLOAD;
        }
        return group_define_apply((uint8_t)msg->id, msg->payload,
                                  (size_t)msg->payload_len);

    case CONTRACT_TYPE_GROUP_DRAW:
        return group_draw_apply((uint8_t)msg->id);

    default:
        return CONTRACT_ERR_TYPE;
    }
}

int contract_dispatch(const uint8_t *buf, size_t len)
{
    contract_msg_t msg;
    int rc = contract_parse(buf, len, &msg);
    if (rc != CONTRACT_OK) {
        return rc;
    }
    return contract_apply(&msg);
}

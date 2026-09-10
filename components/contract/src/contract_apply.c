#include "contract.h"

#include "bind.h"
#include "codec.h"
#include "render.h"

#include <stdlib.h>

typedef struct {
    int16_t x;
    int16_t y;
} row_blit_ctx_t;

static contract_fetch_fn s_fetch;
static void *s_fetch_user;

void contract_set_fetch(contract_fetch_fn fn, void *user)
{
    s_fetch = fn;
    s_fetch_user = user;
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
                free(body);
                return CONTRACT_ERR_FETCH;
            }
            rc = apply_rect_pixels(msg, body, body_len);
            free(body);
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

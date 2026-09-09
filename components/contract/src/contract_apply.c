#include "contract.h"

#include "codec.h"
#include "render.h"

typedef struct {
    int16_t x;
    int16_t y;
} row_blit_ctx_t;

static void blit_row_cb(int y, const uint16_t *row, int width, void *user)
{
    row_blit_ctx_t *c = (row_blit_ctx_t *)user;
    (void)render_blit_rect((int)c->x, (int)c->y + y, width, 1, row);
}

int contract_apply(const contract_msg_t *msg)
{
    row_blit_ctx_t ctx;
    int rc;

    if (!msg) {
        return CONTRACT_ERR_ARG;
    }
    switch (msg->type) {
    case CONTRACT_TYPE_DISPLAY_CLEAR:
        render_clear(msg->color);
        return CONTRACT_OK;

    case CONTRACT_TYPE_RASTER_RECT:
        if (msg->w > (uint16_t)CODEC_MAX_WIDTH) {
            return CONTRACT_ERR_ARG;
        }
        if (!msg->payload && msg->payload_len > 0) {
            return CONTRACT_ERR_ARG;
        }
        ctx.x = msg->x;
        ctx.y = msg->y;
        rc = codec_decode_rows((codec_enc_t)msg->enc, (int)msg->w, (int)msg->h,
                               msg->payload, (size_t)msg->payload_len,
                               blit_row_cb, &ctx);
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

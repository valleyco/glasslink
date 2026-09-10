#include "contract.h"

#include "codec.h"

#include <string.h>

static uint16_t rd_u16(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t rd_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static int16_t rd_i16(const uint8_t *p)
{
    return (int16_t)rd_u16(p);
}

static void wr_u16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xffu);
    p[1] = (uint8_t)((v >> 8) & 0xffu);
}

static void wr_u32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xffu);
    p[1] = (uint8_t)((v >> 8) & 0xffu);
    p[2] = (uint8_t)((v >> 16) & 0xffu);
    p[3] = (uint8_t)((v >> 24) & 0xffu);
}

static void wr_i16(uint8_t *p, int16_t v)
{
    wr_u16(p, (uint16_t)v);
}

static void hdr_common(uint8_t *h, uint8_t type, uint16_t id, uint16_t seq)
{
    h[0] = CONTRACT_MAGIC0;
    h[1] = CONTRACT_MAGIC1;
    h[2] = CONTRACT_MAGIC2;
    h[3] = CONTRACT_MAGIC3;
    h[4] = CONTRACT_VER;
    h[5] = type;
    h[6] = 0;
    h[7] = 0;
    wr_u16(h + 8, id);
    wr_u16(h + 10, seq);
}

int contract_parse(const uint8_t *buf, size_t len, contract_msg_t *out)
{
    const uint8_t *h;
    uint32_t plen;
    uint8_t flags;

    if (!buf || !out) {
        return CONTRACT_ERR_ARG;
    }
    if (len < (size_t)CONTRACT_HDR_SIZE) {
        return CONTRACT_ERR_TRUNC;
    }
    h = buf;
    if (h[0] != CONTRACT_MAGIC0 || h[1] != CONTRACT_MAGIC1 ||
        h[2] != CONTRACT_MAGIC2 || h[3] != CONTRACT_MAGIC3) {
        return CONTRACT_ERR_MAGIC;
    }
    if (h[4] != CONTRACT_VER) {
        return CONTRACT_ERR_VER;
    }

    flags = h[6];
    /* Only FLAG_URI is defined; any other bit is an error. */
    if ((flags & (uint8_t)~CONTRACT_FLAG_URI) != 0) {
        return CONTRACT_ERR_FLAGS;
    }

    out->type = h[5];
    out->flags = flags;
    out->id = rd_u16(h + 8);
    out->seq = rd_u16(h + 10);
    out->x = rd_i16(h + 12);
    out->y = rd_i16(h + 14);
    out->w = rd_u16(h + 16);
    out->h = rd_u16(h + 18);
    out->enc = h[20];
    out->fmt = h[21];
    out->color = rd_u16(h + 22);
    plen = rd_u32(h + 24);
    out->payload_len = plen;

    if (len < (size_t)CONTRACT_HDR_SIZE + (size_t)plen) {
        return CONTRACT_ERR_TRUNC;
    }
    out->payload = (plen > 0) ? (buf + CONTRACT_HDR_SIZE) : NULL;

    switch (out->type) {
    case CONTRACT_TYPE_DISPLAY_CLEAR:
        if (flags != 0) {
            return CONTRACT_ERR_FLAGS;
        }
        if (plen != 0) {
            return CONTRACT_ERR_ARG;
        }
        break;
    case CONTRACT_TYPE_RASTER_RECT:
        if (out->w == 0 || out->h == 0) {
            return CONTRACT_ERR_ARG;
        }
        if (out->fmt != CONTRACT_FMT_RGB565) {
            return CONTRACT_ERR_ARG;
        }
        if (out->enc != (uint8_t)CODEC_ENC_RAW_RGB565 &&
            out->enc != (uint8_t)CODEC_ENC_DELTA_RLE_V1) {
            return CONTRACT_ERR_ARG;
        }
        if (flags & CONTRACT_FLAG_URI) {
            if (plen == 0 || plen > (uint32_t)CONTRACT_URI_MAX) {
                return CONTRACT_ERR_PAYLOAD;
            }
            break;
        }
        if (out->enc == (uint8_t)CODEC_ENC_RAW_RGB565) {
            uint64_t need = (uint64_t)out->w * (uint64_t)out->h * 2ull;
            if (need != (uint64_t)plen) {
                return CONTRACT_ERR_PAYLOAD;
            }
        } else if (plen == 0) {
            return CONTRACT_ERR_PAYLOAD;
        }
        break;
    case CONTRACT_TYPE_FILL_RECT:
        if (flags != 0) {
            return CONTRACT_ERR_FLAGS;
        }
        if (plen != 0) {
            return CONTRACT_ERR_ARG;
        }
        if (out->w == 0 || out->h == 0) {
            return CONTRACT_ERR_ARG;
        }
        break;
    case CONTRACT_TYPE_DRAW_TEXT:
        if (flags != 0) {
            return CONTRACT_ERR_FLAGS;
        }
        if (plen == 0 || plen > (uint32_t)CONTRACT_TEXT_MAX) {
            return CONTRACT_ERR_PAYLOAD;
        }
        if (out->enc > 2) {
            return CONTRACT_ERR_ARG;
        }
        break;
    case CONTRACT_TYPE_BIND_DEFINE:
        if (flags != 0) {
            return CONTRACT_ERR_FLAGS;
        }
        if (out->id >= (uint16_t)CONTRACT_BIND_SLOTS) {
            return CONTRACT_ERR_ARG;
        }
        if (out->w == 0 || out->w > (uint16_t)CONTRACT_TEXT_MAX) {
            return CONTRACT_ERR_ARG;
        }
        if (out->enc > 2) {
            return CONTRACT_ERR_ARG;
        }
        /* payload: bg_u16 + optional text */
        if (plen < 2 || plen > 2u + (uint32_t)CONTRACT_TEXT_MAX) {
            return CONTRACT_ERR_PAYLOAD;
        }
        break;
    case CONTRACT_TYPE_BIND_SET:
        if (flags != 0) {
            return CONTRACT_ERR_FLAGS;
        }
        if (out->id >= (uint16_t)CONTRACT_BIND_SLOTS) {
            return CONTRACT_ERR_ARG;
        }
        if (plen == 0 || plen > (uint32_t)CONTRACT_TEXT_MAX) {
            return CONTRACT_ERR_PAYLOAD;
        }
        break;
    case CONTRACT_TYPE_DRAW_BATCH:
        if (flags != 0) {
            return CONTRACT_ERR_FLAGS;
        }
        if (plen == 0 || plen > (uint32_t)CONTRACT_BATCH_BYTES_MAX) {
            return CONTRACT_ERR_PAYLOAD;
        }
        break;
    case CONTRACT_TYPE_GROUP_DEFINE:
        if (flags != 0) {
            return CONTRACT_ERR_FLAGS;
        }
        if (out->id >= (uint16_t)CONTRACT_GROUP_SLOTS) {
            return CONTRACT_ERR_ARG;
        }
        if (plen == 0 || plen > (uint32_t)CONTRACT_BATCH_BYTES_MAX) {
            return CONTRACT_ERR_PAYLOAD;
        }
        break;
    case CONTRACT_TYPE_GROUP_DRAW:
        if (flags != 0) {
            return CONTRACT_ERR_FLAGS;
        }
        if (out->id >= (uint16_t)CONTRACT_GROUP_SLOTS) {
            return CONTRACT_ERR_ARG;
        }
        if (plen != 0) {
            return CONTRACT_ERR_ARG;
        }
        break;
    default:
        return CONTRACT_ERR_TYPE;
    }
    return CONTRACT_OK;
}

size_t contract_pack_clear(uint8_t *out, size_t out_cap, uint16_t id,
                           uint16_t seq, uint16_t color)
{
    if (!out || out_cap < (size_t)CONTRACT_HDR_SIZE) {
        return 0;
    }
    memset(out, 0, CONTRACT_HDR_SIZE);
    hdr_common(out, CONTRACT_TYPE_DISPLAY_CLEAR, id, seq);
    wr_u16(out + 22, color);
    wr_u32(out + 24, 0);
    return (size_t)CONTRACT_HDR_SIZE;
}

size_t contract_pack_rect_flags(uint8_t *out, size_t out_cap, uint16_t id,
                                uint16_t seq, int16_t x, int16_t y, uint16_t w,
                                uint16_t h, uint8_t enc, uint8_t flags,
                                const uint8_t *payload, uint32_t payload_len)
{
    size_t total = (size_t)CONTRACT_HDR_SIZE + (size_t)payload_len;

    if (!out || w == 0 || h == 0) {
        return 0;
    }
    if ((flags & (uint8_t)~CONTRACT_FLAG_URI) != 0) {
        return 0;
    }
    if (payload_len > 0 && !payload) {
        return 0;
    }
    if (out_cap < total) {
        return 0;
    }
    memset(out, 0, CONTRACT_HDR_SIZE);
    hdr_common(out, CONTRACT_TYPE_RASTER_RECT, id, seq);
    out[6] = flags;
    wr_i16(out + 12, x);
    wr_i16(out + 14, y);
    wr_u16(out + 16, w);
    wr_u16(out + 18, h);
    out[20] = enc;
    out[21] = CONTRACT_FMT_RGB565;
    wr_u16(out + 22, 0);
    wr_u32(out + 24, payload_len);
    if (payload_len) {
        memcpy(out + CONTRACT_HDR_SIZE, payload, payload_len);
    }
    return total;
}

size_t contract_pack_rect(uint8_t *out, size_t out_cap, uint16_t id,
                          uint16_t seq, int16_t x, int16_t y, uint16_t w,
                          uint16_t h, uint8_t enc, const uint8_t *payload,
                          uint32_t payload_len)
{
    return contract_pack_rect_flags(out, out_cap, id, seq, x, y, w, h, enc, 0,
                                    payload, payload_len);
}

size_t contract_pack_fill_rect(uint8_t *out, size_t out_cap, uint16_t id,
                               uint16_t seq, int16_t x, int16_t y, uint16_t w,
                               uint16_t h, uint16_t color)
{
    if (!out || out_cap < (size_t)CONTRACT_HDR_SIZE || w == 0 || h == 0) {
        return 0;
    }
    memset(out, 0, CONTRACT_HDR_SIZE);
    hdr_common(out, CONTRACT_TYPE_FILL_RECT, id, seq);
    wr_i16(out + 12, x);
    wr_i16(out + 14, y);
    wr_u16(out + 16, w);
    wr_u16(out + 18, h);
    wr_u16(out + 22, color);
    wr_u32(out + 24, 0);
    return (size_t)CONTRACT_HDR_SIZE;
}

size_t contract_pack_text(uint8_t *out, size_t out_cap, uint16_t id,
                          uint16_t seq, int16_t x, int16_t y, uint16_t color,
                          uint8_t scale, const uint8_t *utf8, uint32_t len)
{
    size_t total = (size_t)CONTRACT_HDR_SIZE + (size_t)len;

    if (!out || !utf8 || len == 0 || len > (uint32_t)CONTRACT_TEXT_MAX) {
        return 0;
    }
    if (scale > 2) {
        return 0;
    }
    if (out_cap < total) {
        return 0;
    }
    memset(out, 0, CONTRACT_HDR_SIZE);
    hdr_common(out, CONTRACT_TYPE_DRAW_TEXT, id, seq);
    wr_i16(out + 12, x);
    wr_i16(out + 14, y);
    out[20] = scale; /* 0/1 → scale 1 at apply */
    wr_u16(out + 22, color);
    wr_u32(out + 24, len);
    memcpy(out + CONTRACT_HDR_SIZE, utf8, len);
    return total;
}

size_t contract_pack_bind_define(uint8_t *out, size_t out_cap, uint16_t slot,
                                 uint16_t seq, int16_t x, int16_t y,
                                 uint16_t fg, uint16_t bg, uint8_t scale,
                                 uint8_t max_chars, const uint8_t *utf8,
                                 uint32_t text_len)
{
    uint32_t plen = 2u + text_len;
    size_t total = (size_t)CONTRACT_HDR_SIZE + (size_t)plen;

    if (!out || slot >= (uint16_t)CONTRACT_BIND_SLOTS) {
        return 0;
    }
    if (max_chars == 0 || max_chars > (uint8_t)CONTRACT_TEXT_MAX || scale > 2) {
        return 0;
    }
    if (text_len > 0 && !utf8) {
        return 0;
    }
    if (text_len > (uint32_t)CONTRACT_TEXT_MAX || out_cap < total) {
        return 0;
    }
    memset(out, 0, CONTRACT_HDR_SIZE);
    hdr_common(out, CONTRACT_TYPE_BIND_DEFINE, slot, seq);
    wr_i16(out + 12, x);
    wr_i16(out + 14, y);
    wr_u16(out + 16, max_chars);
    out[20] = scale;
    wr_u16(out + 22, fg);
    wr_u32(out + 24, plen);
    wr_u16(out + CONTRACT_HDR_SIZE, bg);
    if (text_len) {
        memcpy(out + CONTRACT_HDR_SIZE + 2, utf8, text_len);
    }
    return total;
}

size_t contract_pack_bind_set(uint8_t *out, size_t out_cap, uint16_t slot,
                              uint16_t seq, const uint8_t *utf8, uint32_t len)
{
    size_t total = (size_t)CONTRACT_HDR_SIZE + (size_t)len;

    if (!out || !utf8 || len == 0 || len > (uint32_t)CONTRACT_TEXT_MAX) {
        return 0;
    }
    if (slot >= (uint16_t)CONTRACT_BIND_SLOTS || out_cap < total) {
        return 0;
    }
    memset(out, 0, CONTRACT_HDR_SIZE);
    hdr_common(out, CONTRACT_TYPE_BIND_SET, slot, seq);
    wr_u32(out + 24, len);
    memcpy(out + CONTRACT_HDR_SIZE, utf8, len);
    return total;
}

size_t contract_batch_put_fill(uint8_t *dst, size_t dst_cap, int16_t x,
                               int16_t y, uint16_t w, uint16_t h,
                               uint16_t color)
{
    if (!dst || w == 0 || h == 0 || dst_cap < 11) {
        return 0;
    }
    dst[0] = CONTRACT_BATCH_OP_FILL;
    wr_i16(dst + 1, x);
    wr_i16(dst + 3, y);
    wr_u16(dst + 5, w);
    wr_u16(dst + 7, h);
    wr_u16(dst + 9, color);
    return 11;
}

size_t contract_batch_put_text(uint8_t *dst, size_t dst_cap, int16_t x,
                               int16_t y, uint16_t color, uint8_t scale,
                               const uint8_t *utf8, uint8_t len)
{
    size_t need = 9u + (size_t)len;
    if (!dst || !utf8 || len == 0 || len > (uint8_t)CONTRACT_TEXT_MAX) {
        return 0;
    }
    if (scale > 2 || dst_cap < need) {
        return 0;
    }
    dst[0] = CONTRACT_BATCH_OP_TEXT;
    wr_i16(dst + 1, x);
    wr_i16(dst + 3, y);
    wr_u16(dst + 5, color);
    dst[7] = scale;
    dst[8] = len;
    memcpy(dst + 9, utf8, len);
    return need;
}

static size_t pack_with_payload(uint8_t *out, size_t out_cap, uint8_t type,
                                uint16_t id, uint16_t seq, const uint8_t *payload,
                                uint32_t payload_len)
{
    size_t total = (size_t)CONTRACT_HDR_SIZE + (size_t)payload_len;
    if (!out || (payload_len > 0 && !payload) || out_cap < total) {
        return 0;
    }
    memset(out, 0, CONTRACT_HDR_SIZE);
    hdr_common(out, type, id, seq);
    wr_u32(out + 24, payload_len);
    if (payload_len) {
        memcpy(out + CONTRACT_HDR_SIZE, payload, payload_len);
    }
    return total;
}

size_t contract_pack_batch(uint8_t *out, size_t out_cap, uint16_t id,
                           uint16_t seq, const uint8_t *ops, uint32_t ops_len)
{
    if (ops_len == 0 || ops_len > (uint32_t)CONTRACT_BATCH_BYTES_MAX) {
        return 0;
    }
    return pack_with_payload(out, out_cap, CONTRACT_TYPE_DRAW_BATCH, id, seq, ops,
                             ops_len);
}

size_t contract_pack_group_define(uint8_t *out, size_t out_cap, uint16_t group,
                                  uint16_t seq, const uint8_t *ops,
                                  uint32_t ops_len)
{
    if (group >= (uint16_t)CONTRACT_GROUP_SLOTS) {
        return 0;
    }
    if (ops_len == 0 || ops_len > (uint32_t)CONTRACT_BATCH_BYTES_MAX) {
        return 0;
    }
    return pack_with_payload(out, out_cap, CONTRACT_TYPE_GROUP_DEFINE, group, seq,
                             ops, ops_len);
}

size_t contract_pack_group_draw(uint8_t *out, size_t out_cap, uint16_t group,
                                uint16_t seq)
{
    if (!out || group >= (uint16_t)CONTRACT_GROUP_SLOTS ||
        out_cap < (size_t)CONTRACT_HDR_SIZE) {
        return 0;
    }
    memset(out, 0, CONTRACT_HDR_SIZE);
    hdr_common(out, CONTRACT_TYPE_GROUP_DRAW, group, seq);
    wr_u32(out + 24, 0);
    return (size_t)CONTRACT_HDR_SIZE;
}

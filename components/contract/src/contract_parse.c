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
        /* enc = scale: 0 or 1 → 1; 2 → 2; else bad */
        if (out->enc > 2) {
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

#include "codec_internal.h"

#include <string.h>

size_t codec_raw_bound(int w, int h)
{
    if (w <= 0 || h <= 0) {
        return 0;
    }
    return (size_t)w * (size_t)h * sizeof(uint16_t);
}

int codec_raw_encode(const uint16_t *rgb, int w, int h, uint8_t *out,
                     size_t out_cap, size_t *out_len)
{
    size_t need = codec_raw_bound(w, h);
    if (!rgb || !out || !out_len || w <= 0 || h <= 0) {
        return CODEC_ERR_ARG;
    }
    if (out_cap < need) {
        return CODEC_ERR_NOSPACE;
    }
    memcpy(out, rgb, need);
    *out_len = need;
    return CODEC_OK;
}

int codec_raw_decode(int w, int h, const uint8_t *in, size_t in_len,
                     uint16_t *rgb_out)
{
    size_t need = codec_raw_bound(w, h);
    if (!in || !rgb_out || w <= 0 || h <= 0) {
        return CODEC_ERR_ARG;
    }
    if (in_len < need) {
        return CODEC_ERR_TRUNC;
    }
    if (in_len != need) {
        return CODEC_ERR_FORMAT;
    }
    memcpy(rgb_out, in, need);
    return CODEC_OK;
}

int codec_raw_decode_rows(int w, int h, const uint8_t *in, size_t in_len,
                          codec_row_fn fn, void *ctx)
{
    size_t need = codec_raw_bound(w, h);
    size_t row_bytes;
    if (!in || !fn || w <= 0 || h <= 0 || w > CODEC_MAX_WIDTH) {
        return CODEC_ERR_ARG;
    }
    if (in_len != need) {
        return in_len < need ? CODEC_ERR_TRUNC : CODEC_ERR_FORMAT;
    }
    row_bytes = (size_t)w * sizeof(uint16_t);
    for (int y = 0; y < h; y++) {
        const uint16_t *row =
            (const uint16_t *)(const void *)(in + (size_t)y * row_bytes);
        fn(y, row, w, ctx);
    }
    return CODEC_OK;
}

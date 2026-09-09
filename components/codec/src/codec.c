#include "codec_internal.h"

size_t codec_encode_bound(codec_enc_t enc, int w, int h)
{
    switch (enc) {
    case CODEC_ENC_RAW_RGB565:
        return codec_raw_bound(w, h);
    case CODEC_ENC_DELTA_RLE_V1:
        return codec_delta_rle_bound(w, h);
    default:
        return 0;
    }
}

int codec_encode(codec_enc_t enc, const uint16_t *rgb, int w, int h,
                 uint8_t *out, size_t out_cap, size_t *out_len)
{
    switch (enc) {
    case CODEC_ENC_RAW_RGB565:
        return codec_raw_encode(rgb, w, h, out, out_cap, out_len);
    case CODEC_ENC_DELTA_RLE_V1:
        return codec_delta_rle_encode(rgb, w, h, out, out_cap, out_len);
    default:
        return CODEC_ERR_ARG;
    }
}

int codec_decode(codec_enc_t enc, int w, int h, const uint8_t *in, size_t in_len,
                 uint16_t *rgb_out)
{
    switch (enc) {
    case CODEC_ENC_RAW_RGB565:
        return codec_raw_decode(w, h, in, in_len, rgb_out);
    case CODEC_ENC_DELTA_RLE_V1:
        return codec_delta_rle_decode(w, h, in, in_len, rgb_out);
    default:
        return CODEC_ERR_ARG;
    }
}

int codec_decode_rows(codec_enc_t enc, int w, int h, const uint8_t *in,
                      size_t in_len, codec_row_fn fn, void *ctx)
{
    switch (enc) {
    case CODEC_ENC_RAW_RGB565:
        return codec_raw_decode_rows(w, h, in, in_len, fn, ctx);
    case CODEC_ENC_DELTA_RLE_V1:
        return codec_delta_rle_decode_rows(w, h, in, in_len, fn, ctx);
    default:
        return CODEC_ERR_ARG;
    }
}

int codec_encode_auto(const uint16_t *rgb, int w, int h, uint8_t *out,
                      size_t out_cap, size_t *out_len, codec_enc_t *chosen)
{
    size_t raw_bytes;
    size_t delta_len = 0;
    int rc;

    if (!rgb || !out || !out_len || w <= 0 || h <= 0) {
        return CODEC_ERR_ARG;
    }
    raw_bytes = (size_t)w * (size_t)h * sizeof(uint16_t);
    rc = codec_encode(CODEC_ENC_DELTA_RLE_V1, rgb, w, h, out, out_cap, &delta_len);
    if (rc != CODEC_OK) {
        return rc;
    }
    /* Prefer raw when delta is not a clear win (≥98% of raw size). */
    if (delta_len * 100u >= raw_bytes * 98u) {
        rc = codec_encode(CODEC_ENC_RAW_RGB565, rgb, w, h, out, out_cap, out_len);
        if (rc != CODEC_OK) {
            return rc;
        }
        if (chosen) {
            *chosen = CODEC_ENC_RAW_RGB565;
        }
        return CODEC_OK;
    }
    *out_len = delta_len;
    if (chosen) {
        *chosen = CODEC_ENC_DELTA_RLE_V1;
    }
    return CODEC_OK;
}

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

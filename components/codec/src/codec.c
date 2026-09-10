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

/**
 * Sparse probe: many distinct RGB565 samples ⇒ photo/noise (delta won't win).
 * Flat UI / solid / few-tone checkers stay on the delta path.
 */
static int looks_noisy(const uint16_t *rgb, int w, int h)
{
    enum { PROBES = 48, SLOTS = 40 };
    uint16_t seen[SLOTS];
    int n_seen = 0;
    size_t n = (size_t)w * (size_t)h;
    size_t step;
    size_t probes = 0;

    if (n < 16) {
        return 0;
    }
    step = n / (size_t)PROBES;
    if (step < 1) {
        step = 1;
    }
    for (size_t i = 0; i < n && probes < (size_t)PROBES; i += step, probes++) {
        uint16_t v = rgb[i];
        int j;
        for (j = 0; j < n_seen; j++) {
            if (seen[j] == v) {
                break;
            }
        }
        if (j == n_seen) {
            if (n_seen >= SLOTS) {
                return 1;
            }
            seen[n_seen++] = v;
        }
    }
    return n_seen >= SLOTS;
}

static int encode_raw_chosen(const uint16_t *rgb, int w, int h, uint8_t *out,
                             size_t out_cap, size_t *out_len, codec_enc_t *chosen)
{
    int rc = codec_encode(CODEC_ENC_RAW_RGB565, rgb, w, h, out, out_cap, out_len);
    if (rc != CODEC_OK) {
        return rc;
    }
    if (chosen) {
        *chosen = CODEC_ENC_RAW_RGB565;
    }
    return CODEC_OK;
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

    /* Skip delta attempt on continuous-tone / noise (host policy). */
    if (looks_noisy(rgb, w, h)) {
        return encode_raw_chosen(rgb, w, h, out, out_cap, out_len, chosen);
    }

    rc = codec_encode(CODEC_ENC_DELTA_RLE_V1, rgb, w, h, out, out_cap, &delta_len);
    if (rc != CODEC_OK) {
        return rc;
    }
    /* Prefer raw when delta is not a clear win (≥98% of raw size). */
    if (delta_len * 100u >= raw_bytes * 98u) {
        return encode_raw_chosen(rgb, w, h, out, out_cap, out_len, chosen);
    }
    *out_len = delta_len;
    if (chosen) {
        *chosen = CODEC_ENC_DELTA_RLE_V1;
    }
    return CODEC_OK;
}

#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum codec_enc {
    CODEC_ENC_RAW_RGB565 = 0
} codec_enc_t;

enum {
    CODEC_OK = 0,
    CODEC_ERR_ARG = -1,
    CODEC_ERR_NOSPACE = -2,
    CODEC_ERR_FORMAT = -3,
    CODEC_ERR_TRUNC = -4
};

/**
 * Encode RGB565 row-major image into out buffer.
 * Product wire: raw only (delta_rle lives in ../delta-rle-lab).
 */
int codec_encode(codec_enc_t enc, const uint16_t *rgb, int w, int h,
                 uint8_t *out, size_t out_cap, size_t *out_len);

/**
 * Decode into tightly packed RGB565 buffer (w*h pixels).
 */
int codec_decode(codec_enc_t enc, int w, int h, const uint8_t *in, size_t in_len,
                 uint16_t *rgb_out);

/**
 * Streaming decode: one full row (w pixels) per callback, y = 0..h-1.
 * Raw path: no heap; rows point into `in`.
 */
typedef void (*codec_row_fn)(int y, const uint16_t *row, int width, void *ctx);

enum { CODEC_MAX_WIDTH = 320 };

int codec_decode_rows(codec_enc_t enc, int w, int h, const uint8_t *in,
                      size_t in_len, codec_row_fn fn, void *ctx);

/** Upper bound on encoded size for worst case (safe buffer sizing). */
size_t codec_encode_bound(codec_enc_t enc, int w, int h);

#ifdef __cplusplus
}
#endif

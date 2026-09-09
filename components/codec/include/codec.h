#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum codec_enc {
    CODEC_ENC_RAW_RGB565 = 0,
    /** Vertical-then-horizontal delta on RGB565, then byte RLE (see codec_delta_rle.c). */
    CODEC_ENC_DELTA_RLE_V1 = 1
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
 * @return CODEC_OK or negative error; *out_len set on success.
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
 * Does not allocate a full-frame buffer inside the codec (may use 1–2 line scratch on stack/heap of caller-provided... 
 * actually we use stack lines of max width - document max width).
 */
typedef void (*codec_row_fn)(int y, const uint16_t *row, int width, void *ctx);

enum { CODEC_MAX_WIDTH = 320 };

int codec_decode_rows(codec_enc_t enc, int w, int h, const uint8_t *in,
                      size_t in_len, codec_row_fn fn, void *ctx);

/** Upper bound on encoded size for worst case (safe buffer sizing). */
size_t codec_encode_bound(codec_enc_t enc, int w, int h);

/** Prefer delta_rle_v1; fall back to raw if delta is not clearly smaller.
 *  Threshold: use raw when `delta_len * 100 >= raw_len * 98` (harness T3).
 *  @param chosen optional; receives the enc actually written.
 */
int codec_encode_auto(const uint16_t *rgb, int w, int h, uint8_t *out,
                      size_t out_cap, size_t *out_len, codec_enc_t *chosen);

#ifdef __cplusplus
}
#endif

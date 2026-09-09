#pragma once

#include "codec.h"

size_t codec_raw_bound(int w, int h);
int codec_raw_encode(const uint16_t *rgb, int w, int h, uint8_t *out,
                     size_t out_cap, size_t *out_len);
int codec_raw_decode(int w, int h, const uint8_t *in, size_t in_len,
                     uint16_t *rgb_out);
int codec_raw_decode_rows(int w, int h, const uint8_t *in, size_t in_len,
                          codec_row_fn fn, void *ctx);

size_t codec_delta_rle_bound(int w, int h);
int codec_delta_rle_encode(const uint16_t *rgb, int w, int h, uint8_t *out,
                           size_t out_cap, size_t *out_len);
int codec_delta_rle_decode(int w, int h, const uint8_t *in, size_t in_len,
                           uint16_t *rgb_out);
int codec_delta_rle_decode_rows(int w, int h, const uint8_t *in, size_t in_len,
                                codec_row_fn fn, void *ctx);

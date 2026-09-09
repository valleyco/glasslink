/*
 * delta_rle_v1 (initial freeze candidate)
 *
 * Predict (encode forward):
 *   1) Vertical: for y=1..h-1: p[y][x] = rgb[y][x] - rgb[y-1][x]  (uint16 wrap)
 *      row 0 unchanged from rgb
 *   2) Horizontal DPCM on vertical residual: p[x] = v[x] - v[x-1]
 *      (right→left in-place so left is still pre-horizontal)
 *
 * Decode: undo horizontal L→R per row, then vertical T→B.
 *
 * Byte stream: little-endian residual uint16s, RLE'd as bytes.
 *
 * RLE control byte:
 *   ctrl <  0x80: (ctrl+1) literal bytes follow
 *   ctrl >= 0x80: repeat next byte (ctrl - 0x80 + 1) times
 * Max run/literal length per opcode: 128.
 */

#include "codec_internal.h"

#include <stdlib.h>
#include <string.h>

enum { RLE_MAX = 128 };

static void predict_forward(const uint16_t *rgb, int w, int h, uint16_t *pred)
{
    /* Vertical DPCM against original RGB (row 0 unchanged). */
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            uint16_t v = rgb[y * w + x];
            if (y > 0) {
                v = (uint16_t)(v - rgb[(y - 1) * w + x]);
            }
            pred[y * w + x] = v;
        }
    }
    /* Horizontal DPCM against pre-horizontal left (not already-delta'd).
     * Walk right→left so left neighbor is still the vertical residual. */
    for (int y = 0; y < h; y++) {
        for (int x = w - 1; x >= 1; x--) {
            pred[y * w + x] =
                (uint16_t)(pred[y * w + x] - pred[y * w + (x - 1)]);
        }
    }
}

static void predict_inverse_row_h(uint16_t *row, int w)
{
    for (int x = 1; x < w; x++) {
        row[x] = (uint16_t)(row[x] + row[x - 1]);
    }
}

static void predict_inverse_v_from_prev(uint16_t *row, const uint16_t *prev,
                                        int w, int y)
{
    if (y == 0 || !prev) {
        return;
    }
    for (int x = 0; x < w; x++) {
        row[x] = (uint16_t)(row[x] + prev[x]);
    }
}

static int rle_encode(const uint8_t *in, size_t in_len, uint8_t *out,
                      size_t out_cap, size_t *out_len)
{
    size_t o = 0;
    size_t i = 0;
    while (i < in_len) {
        size_t run = 1;
        while (i + run < in_len && run < (size_t)RLE_MAX &&
               in[i + run] == in[i]) {
            run++;
        }
        if (run >= 3) {
            if (o + 2 > out_cap) {
                return CODEC_ERR_NOSPACE;
            }
            out[o++] = (uint8_t)(0x80u | (run - 1));
            out[o++] = in[i];
            i += run;
            continue;
        }
        size_t lit_start = i;
        size_t lit = 0;
        while (i < in_len && lit < (size_t)RLE_MAX) {
            size_t r = 1;
            while (i + r < in_len && r < (size_t)RLE_MAX &&
                   in[i + r] == in[i]) {
                r++;
            }
            if (r >= 3) {
                break;
            }
            /* Do not let lit exceed RLE_MAX — (lit-1) must fit in 7 bits. */
            if (lit + r > (size_t)RLE_MAX) {
                size_t take = (size_t)RLE_MAX - lit;
                i += take;
                lit += take;
                break;
            }
            i += r;
            lit += r;
        }
        if (lit == 0) {
            return CODEC_ERR_FORMAT;
        }
        if (o + 1 + lit > out_cap) {
            return CODEC_ERR_NOSPACE;
        }
        out[o++] = (uint8_t)(lit - 1);
        memcpy(out + o, in + lit_start, lit);
        o += lit;
    }
    *out_len = o;
    return CODEC_OK;
}

static int rle_decode(const uint8_t *in, size_t in_len, uint8_t *out,
                      size_t out_cap, size_t *out_len)
{
    size_t i = 0;
    size_t o = 0;
    while (i < in_len) {
        uint8_t ctrl = in[i++];
        if (ctrl < 0x80) {
            size_t lit = (size_t)ctrl + 1;
            if (i + lit > in_len) {
                return CODEC_ERR_TRUNC;
            }
            if (o + lit > out_cap) {
                return CODEC_ERR_NOSPACE;
            }
            memcpy(out + o, in + i, lit);
            o += lit;
            i += lit;
        } else {
            size_t run = (size_t)(ctrl - 0x80) + 1;
            if (i >= in_len) {
                return CODEC_ERR_TRUNC;
            }
            uint8_t v = in[i++];
            if (o + run > out_cap) {
                return CODEC_ERR_NOSPACE;
            }
            memset(out + o, v, run);
            o += run;
        }
    }
    *out_len = o;
    return CODEC_OK;
}

size_t codec_delta_rle_bound(int w, int h)
{
    size_t raw;
    if (w <= 0 || h <= 0) {
        return 0;
    }
    raw = (size_t)w * (size_t)h * sizeof(uint16_t);
    return raw * 2 + 16;
}

int codec_delta_rle_encode(const uint16_t *rgb, int w, int h, uint8_t *out,
                           size_t out_cap, size_t *out_len)
{
    size_t px;
    size_t raw_bytes;
    uint16_t *pred;
    uint8_t *tmp;
    size_t bound;
    size_t rle_len = 0;
    int rc;

    if (!rgb || !out || !out_len || w <= 0 || h <= 0 || w > CODEC_MAX_WIDTH) {
        return CODEC_ERR_ARG;
    }
    px = (size_t)w * (size_t)h;
    raw_bytes = px * sizeof(uint16_t);
    pred = (uint16_t *)malloc(raw_bytes);
    if (!pred) {
        return CODEC_ERR_NOSPACE;
    }
    predict_forward(rgb, w, h, pred);

    bound = codec_delta_rle_bound(w, h);
    tmp = (uint8_t *)malloc(bound);
    if (!tmp) {
        free(pred);
        return CODEC_ERR_NOSPACE;
    }
    rc = rle_encode((const uint8_t *)pred, raw_bytes, tmp, bound, &rle_len);
    free(pred);
    if (rc != CODEC_OK) {
        free(tmp);
        return rc;
    }
    if (rle_len > out_cap) {
        free(tmp);
        return CODEC_ERR_NOSPACE;
    }
    memcpy(out, tmp, rle_len);
    free(tmp);
    *out_len = rle_len;
    return CODEC_OK;
}

int codec_delta_rle_decode(int w, int h, const uint8_t *in, size_t in_len,
                           uint16_t *rgb_out)
{
    size_t raw_bytes;
    size_t dec_len = 0;
    uint8_t *raw;
    int rc;

    if (!in || !rgb_out || w <= 0 || h <= 0 || w > CODEC_MAX_WIDTH) {
        return CODEC_ERR_ARG;
    }
    raw_bytes = (size_t)w * (size_t)h * sizeof(uint16_t);
    raw = (uint8_t *)malloc(raw_bytes);
    if (!raw) {
        return CODEC_ERR_NOSPACE;
    }
    rc = rle_decode(in, in_len, raw, raw_bytes, &dec_len);
    if (rc != CODEC_OK) {
        free(raw);
        return rc;
    }
    if (dec_len != raw_bytes) {
        free(raw);
        return CODEC_ERR_FORMAT;
    }
    memcpy(rgb_out, raw, raw_bytes);
    free(raw);

    for (int y = 0; y < h; y++) {
        predict_inverse_row_h(rgb_out + y * w, w);
    }
    for (int y = 1; y < h; y++) {
        predict_inverse_v_from_prev(rgb_out + y * w, rgb_out + (y - 1) * w, w,
                                    y);
    }
    return CODEC_OK;
}

int codec_delta_rle_decode_rows(int w, int h, const uint8_t *in, size_t in_len,
                                codec_row_fn fn, void *ctx)
{
    size_t raw_bytes;
    size_t dec_len = 0;
    uint8_t *raw;
    uint16_t *pred;
    uint16_t prev[CODEC_MAX_WIDTH];
    uint16_t row[CODEC_MAX_WIDTH];
    int rc;

    if (!in || !fn || w <= 0 || h <= 0 || w > CODEC_MAX_WIDTH) {
        return CODEC_ERR_ARG;
    }
    raw_bytes = (size_t)w * (size_t)h * sizeof(uint16_t);
    raw = (uint8_t *)malloc(raw_bytes);
    if (!raw) {
        return CODEC_ERR_NOSPACE;
    }
    rc = rle_decode(in, in_len, raw, raw_bytes, &dec_len);
    if (rc != CODEC_OK) {
        free(raw);
        return rc;
    }
    if (dec_len != raw_bytes) {
        free(raw);
        return CODEC_ERR_FORMAT;
    }
    pred = (uint16_t *)(void *)raw;

    for (int y = 0; y < h; y++) {
        memcpy(row, pred + y * w, (size_t)w * sizeof(uint16_t));
        predict_inverse_row_h(row, w);
        predict_inverse_v_from_prev(row, y ? prev : NULL, w, y);
        fn(y, row, w, ctx);
        memcpy(prev, row, (size_t)w * sizeof(uint16_t));
    }
    free(raw);
    return CODEC_OK;
}

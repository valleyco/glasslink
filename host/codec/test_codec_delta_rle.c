#include "codec.h"
#include "test_assert.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int roundtrip(codec_enc_t enc, const uint16_t *src, int w, int h)
{
    size_t bound = codec_encode_bound(enc, w, h);
    uint8_t *buf = (uint8_t *)malloc(bound);
    uint16_t *dst = (uint16_t *)malloc((size_t)w * (size_t)h * sizeof(uint16_t));
    size_t len = 0;
    int rc;

    if (!buf || !dst) {
        free(buf);
        free(dst);
        return -99;
    }
    rc = codec_encode(enc, src, w, h, buf, bound, &len);
    if (rc != CODEC_OK) {
        free(buf);
        free(dst);
        return rc;
    }
    memset(dst, 0xA5, (size_t)w * (size_t)h * sizeof(uint16_t));
    rc = codec_decode(enc, w, h, buf, len, dst);
    if (rc != CODEC_OK) {
        free(buf);
        free(dst);
        return rc;
    }
    rc = memcmp(src, dst, (size_t)w * (size_t)h * sizeof(uint16_t)) == 0
             ? CODEC_OK
             : -100;
    free(buf);
    free(dst);
    return rc;
}

static void test_solid(void)
{
    uint16_t src[8 * 8];
    for (int i = 0; i < 64; i++) {
        src[i] = 0xF800;
    }
    ASSERT_EQ_INT(CODEC_OK, roundtrip(CODEC_ENC_DELTA_RLE_V1, src, 8, 8));
}

static void test_gradient(void)
{
    uint16_t src[16 * 8];
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 16; x++) {
            src[y * 16 + x] = (uint16_t)((x * 3) | (y << 8));
        }
    }
    ASSERT_EQ_INT(CODEC_OK, roundtrip(CODEC_ENC_DELTA_RLE_V1, src, 16, 8));
}

static void test_checker(void)
{
    uint16_t src[7 * 5];
    for (int y = 0; y < 5; y++) {
        for (int x = 0; x < 7; x++) {
            src[y * 7 + x] = ((x + y) & 1) ? 0xFFFF : 0x0000;
        }
    }
    ASSERT_EQ_INT(CODEC_OK, roundtrip(CODEC_ENC_DELTA_RLE_V1, src, 7, 5));
}

static void test_odd_sizes(void)
{
    uint16_t a[1] = {0x1234};
    uint16_t b[3 * 1];
    uint16_t c[1 * 4];
    b[0] = 1;
    b[1] = 2;
    b[2] = 3;
    for (int i = 0; i < 4; i++) {
        c[i] = (uint16_t)(0x10 * i);
    }
    ASSERT_EQ_INT(CODEC_OK, roundtrip(CODEC_ENC_DELTA_RLE_V1, a, 1, 1));
    ASSERT_EQ_INT(CODEC_OK, roundtrip(CODEC_ENC_DELTA_RLE_V1, b, 3, 1));
    ASSERT_EQ_INT(CODEC_OK, roundtrip(CODEC_ENC_DELTA_RLE_V1, c, 1, 4));
}

static void test_wraparound(void)
{
    uint16_t src[4] = {0x0000, 0xFFFF, 0x0001, 0xFFFE};
    ASSERT_EQ_INT(CODEC_OK, roundtrip(CODEC_ENC_DELTA_RLE_V1, src, 2, 2));
}

typedef struct {
    int w;
    int h;
    int rows_seen;
    uint16_t *dst;
} row_ctx_t;

static void on_row(int y, const uint16_t *row, int width, void *user)
{
    row_ctx_t *c = (row_ctx_t *)user;
    ASSERT_EQ_INT(c->w, width);
    ASSERT_TRUE(y >= 0 && y < c->h);
    memcpy(c->dst + y * c->w, row, (size_t)width * sizeof(uint16_t));
    c->rows_seen++;
}

static void test_streaming_rows(void)
{
    const int w = 6, h = 4;
    uint16_t src[24];
    uint16_t dst[24];
    uint8_t buf[256];
    size_t len = 0;
    row_ctx_t ctx = {.w = w, .h = h, .rows_seen = 0, .dst = dst};

    for (int i = 0; i < 24; i++) {
        src[i] = (uint16_t)(i * 7 + 3);
    }
    ASSERT_EQ_INT(CODEC_OK, codec_encode(CODEC_ENC_DELTA_RLE_V1, src, w, h, buf,
                                         sizeof(buf), &len));
    memset(dst, 0, sizeof(dst));
    ASSERT_EQ_INT(CODEC_OK, codec_decode_rows(CODEC_ENC_DELTA_RLE_V1, w, h, buf,
                                              len, on_row, &ctx));
    ASSERT_EQ_INT(h, ctx.rows_seen);
    ASSERT_TRUE(memcmp(src, dst, sizeof(src)) == 0);
}

/* Solid frame: one long RLE run spans many row boundaries (Step 13a). */
static void test_streaming_solid_cross_row_rle(void)
{
    const int w = 64, h = 48;
    uint16_t *src = (uint16_t *)malloc((size_t)w * (size_t)h * sizeof(uint16_t));
    uint16_t *dst = (uint16_t *)malloc((size_t)w * (size_t)h * sizeof(uint16_t));
    size_t bound;
    uint8_t *buf;
    size_t len = 0;
    row_ctx_t ctx;

    ASSERT_TRUE(src && dst);
    for (int i = 0; i < w * h; i++) {
        src[i] = 0xF800;
    }
    bound = codec_encode_bound(CODEC_ENC_DELTA_RLE_V1, w, h);
    buf = (uint8_t *)malloc(bound);
    ASSERT_TRUE(buf != NULL);
    ASSERT_EQ_INT(CODEC_OK, codec_encode(CODEC_ENC_DELTA_RLE_V1, src, w, h, buf,
                                         bound, &len));
    ASSERT_TRUE(len < (size_t)w * 4); /* heavily compressed */
    ctx.w = w;
    ctx.h = h;
    ctx.rows_seen = 0;
    ctx.dst = dst;
    memset(dst, 0, (size_t)w * (size_t)h * sizeof(uint16_t));
    ASSERT_EQ_INT(CODEC_OK, codec_decode_rows(CODEC_ENC_DELTA_RLE_V1, w, h, buf,
                                              len, on_row, &ctx));
    ASSERT_EQ_INT(h, ctx.rows_seen);
    ASSERT_TRUE(memcmp(src, dst, (size_t)w * (size_t)h * sizeof(uint16_t)) ==
                0);
    free(buf);
    free(src);
    free(dst);
}

static void test_streaming_max_width(void)
{
    const int w = CODEC_MAX_WIDTH, h = 2;
    uint16_t *src = (uint16_t *)malloc((size_t)w * (size_t)h * sizeof(uint16_t));
    uint16_t *dst = (uint16_t *)malloc((size_t)w * (size_t)h * sizeof(uint16_t));
    size_t bound;
    uint8_t *buf;
    size_t len = 0;
    row_ctx_t ctx;

    ASSERT_TRUE(src && dst);
    for (int i = 0; i < w * h; i++) {
        src[i] = (uint16_t)(i * 13u);
    }
    bound = codec_encode_bound(CODEC_ENC_DELTA_RLE_V1, w, h);
    buf = (uint8_t *)malloc(bound);
    ASSERT_TRUE(buf != NULL);
    ASSERT_EQ_INT(CODEC_OK, codec_encode(CODEC_ENC_DELTA_RLE_V1, src, w, h, buf,
                                         bound, &len));
    ctx.w = w;
    ctx.h = h;
    ctx.rows_seen = 0;
    ctx.dst = dst;
    ASSERT_EQ_INT(CODEC_OK, codec_decode_rows(CODEC_ENC_DELTA_RLE_V1, w, h, buf,
                                              len, on_row, &ctx));
    ASSERT_EQ_INT(h, ctx.rows_seen);
    ASSERT_TRUE(memcmp(src, dst, (size_t)w * (size_t)h * sizeof(uint16_t)) ==
                0);
    free(buf);
    free(src);
    free(dst);
}

static void test_trunc_stream(void)
{
    uint16_t src[16];
    uint8_t buf[128];
    size_t len = 0;
    uint16_t dst[16];
    for (int i = 0; i < 16; i++) {
        src[i] = 0xABCD;
    }
    ASSERT_EQ_INT(CODEC_OK, codec_encode(CODEC_ENC_DELTA_RLE_V1, src, 4, 4, buf,
                                         sizeof(buf), &len));
    ASSERT_TRUE(len > 2);
    ASSERT_EQ_INT(CODEC_ERR_TRUNC,
                  codec_decode(CODEC_ENC_DELTA_RLE_V1, 4, 4, buf, 1, dst));
}

static void test_fuzz_seeds(void)
{
    /* deterministic LCG fuzz */
    for (unsigned seed = 1; seed <= 200; seed++) {
        unsigned s = seed * 2654435761u;
        int w = 1 + (int)(s % 17);
        int h = 1 + (int)((s >> 4) % 11);
        int n = w * h;
        uint16_t *src = (uint16_t *)malloc((size_t)n * sizeof(uint16_t));
        ASSERT_TRUE(src != NULL);
        for (int i = 0; i < n; i++) {
            s = s * 1664525u + 1013904223u;
            src[i] = (uint16_t)(s >> 16);
        }
        int rc = roundtrip(CODEC_ENC_DELTA_RLE_V1, src, w, h);
        if (rc != CODEC_OK) {
            fprintf(stderr, "fuzz fail seed=%u w=%d h=%d rc=%d\n", seed, w, h,
                    rc);
        }
        ASSERT_EQ_INT(CODEC_OK, rc);
        free(src);
    }
}

static void test_raw_also_roundtrip_same_buf(void)
{
    uint16_t src[9] = {0};
    src[4] = 0x07E0;
    ASSERT_EQ_INT(CODEC_OK, roundtrip(CODEC_ENC_RAW_RGB565, src, 3, 3));
}

/* Stress RLE literal packing near 128-byte opcode limit (noise-like). */
static void test_rle_literal_boundary(void)
{
    uint16_t src[32 * 32];
    unsigned s = 42u;
    for (int i = 0; i < 32 * 32; i++) {
        s = s * 1664525u + 1013904223u;
        src[i] = (uint16_t)(s >> 16);
    }
    ASSERT_EQ_INT(CODEC_OK, roundtrip(CODEC_ENC_DELTA_RLE_V1, src, 32, 32));
}

int main(void)
{
    test_solid();
    test_gradient();
    test_checker();
    test_odd_sizes();
    test_wraparound();
    test_streaming_rows();
    test_streaming_solid_cross_row_rle();
    test_streaming_max_width();
    test_trunc_stream();
    test_fuzz_seeds();
    test_raw_also_roundtrip_same_buf();
    test_rle_literal_boundary();
    return test_report();
}

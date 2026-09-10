#include "codec.h"
#include "test_assert.h"

#include <stdint.h>
#include <string.h>

static void fill(uint16_t *p, int n, uint16_t c)
{
    for (int i = 0; i < n; i++) {
        p[i] = c;
    }
}

static void noise(uint16_t *p, int n, unsigned seed)
{
    unsigned s = seed;
    for (int i = 0; i < n; i++) {
        s = s * 1664525u + 1013904223u;
        p[i] = (uint16_t)(s >> 16);
    }
}

int main(void)
{
    enum { W = 32, H = 32, N = W * H };
    uint16_t rgb[N];
    uint8_t out[N * 2 + 64];
    size_t out_len = 0;
    codec_enc_t chosen = CODEC_ENC_RAW_RGB565;

    fill(rgb, N, 0x07E0);
    ASSERT_EQ_INT(CODEC_OK,
                  codec_encode_auto(rgb, W, H, out, sizeof(out), &out_len, &chosen));
    ASSERT_EQ_INT(CODEC_ENC_DELTA_RLE_V1, (int)chosen);
    ASSERT_TRUE(out_len < (size_t)N * 2);

    noise(rgb, N, 42);
    ASSERT_EQ_INT(CODEC_OK,
                  codec_encode_auto(rgb, W, H, out, sizeof(out), &out_len, &chosen));
    ASSERT_EQ_INT(CODEC_ENC_RAW_RGB565, (int)chosen);
    ASSERT_EQ_INT((int)((size_t)N * 2), (int)out_len);

    /* Two-tone checker: few unique colors → try delta (may still size-fallback). */
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            rgb[y * W + x] = ((x + y) & 1) ? 0xFFFF : 0x0000;
        }
    }
    ASSERT_EQ_INT(CODEC_OK,
                  codec_encode_auto(rgb, W, H, out, sizeof(out), &out_len, &chosen));
    ASSERT_TRUE(chosen == CODEC_ENC_RAW_RGB565 ||
                chosen == CODEC_ENC_DELTA_RLE_V1);

    return test_report();
}

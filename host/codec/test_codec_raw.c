#include "codec.h"
#include "test_assert.h"

#include <stdint.h>
#include <string.h>

static void test_raw_roundtrip(void)
{
    const int w = 5, h = 3;
    uint16_t src[15];
    uint16_t dst[15];
    uint8_t buf[64];
    size_t len = 0;

    for (int i = 0; i < 15; i++) {
        src[i] = (uint16_t)(0x100 + i);
    }
    ASSERT_EQ_INT(CODEC_OK, codec_encode(CODEC_ENC_RAW_RGB565, src, w, h, buf,
                                         sizeof(buf), &len));
    ASSERT_EQ_INT(30, (int)len);
    memset(dst, 0, sizeof(dst));
    ASSERT_EQ_INT(CODEC_OK,
                  codec_decode(CODEC_ENC_RAW_RGB565, w, h, buf, len, dst));
    ASSERT_TRUE(memcmp(src, dst, sizeof(src)) == 0);
}

static void test_raw_trunc(void)
{
    uint16_t src[4] = {1, 2, 3, 4};
    uint16_t dst[4];
    uint8_t buf[16];
    size_t len = 0;
    ASSERT_EQ_INT(CODEC_OK, codec_encode(CODEC_ENC_RAW_RGB565, src, 2, 2, buf,
                                         sizeof(buf), &len));
    ASSERT_EQ_INT(CODEC_ERR_TRUNC, codec_decode(CODEC_ENC_RAW_RGB565, 2, 2, buf,
                                                len - 1, dst));
}

static void test_raw_bad_args(void)
{
    uint16_t px = 1;
    uint8_t buf[8];
    size_t len = 0;
    ASSERT_EQ_INT(CODEC_ERR_ARG, codec_encode(CODEC_ENC_RAW_RGB565, NULL, 1, 1,
                                              buf, sizeof(buf), &len));
    ASSERT_EQ_INT(CODEC_ERR_ARG, codec_encode(CODEC_ENC_RAW_RGB565, &px, 0, 1,
                                              buf, sizeof(buf), &len));
}

int main(void)
{
    test_raw_roundtrip();
    test_raw_trunc();
    test_raw_bad_args();
    return test_report();
}

#include "codec.h"
#include "contract.h"
#include "fake_display.h"
#include "render.h"
#include "test_assert.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static void test_dispatch_clear(void)
{
    uint8_t buf[64];
    size_t n;

    fake_display_reset();
    /* dirty one pixel first */
    {
        uint16_t one = 0xFFFF;
        render_blit_rect(0, 0, 1, 1, &one);
    }
    n = contract_pack_clear(buf, sizeof(buf), 1, 1, 0xF800);
    ASSERT_EQ_INT(CONTRACT_OK, contract_dispatch(buf, n));
    ASSERT_EQ_U16(0xF800, fake_display_get_pixel(0, 0));
    ASSERT_EQ_U16(0xF800, fake_display_get_pixel(319, 239));
    ASSERT_TRUE(fake_display_count_color(0xF800) ==
                (size_t)HAL_DISPLAY_WIDTH * HAL_DISPLAY_HEIGHT);
}

static void test_dispatch_rect_raw(void)
{
    uint8_t msg[256];
    uint16_t px[4] = {0x001F, 0x07E0, 0xF800, 0xFFFF};
    size_t n;

    fake_display_reset();
    n = contract_pack_rect(msg, sizeof(msg), 2, 0, 10, 20, 2, 2,
                           CODEC_ENC_RAW_RGB565, (const uint8_t *)px,
                           sizeof(px));
    ASSERT_TRUE(n > 0);
    ASSERT_EQ_INT(CONTRACT_OK, contract_dispatch(msg, n));
    ASSERT_EQ_U16(0x001F, fake_display_get_pixel(10, 20));
    ASSERT_EQ_U16(0x07E0, fake_display_get_pixel(11, 20));
    ASSERT_EQ_U16(0xF800, fake_display_get_pixel(10, 21));
    ASSERT_EQ_U16(0xFFFF, fake_display_get_pixel(11, 21));
    ASSERT_EQ_U16(0x0000, fake_display_get_pixel(9, 20));
}

static void test_dispatch_rect_delta(void)
{
    const int w = 8, h = 4;
    uint16_t src[32];
    uint8_t enc[256];
    uint8_t msg[512];
    size_t elen = 0;
    size_t n;
    int rc;

    for (int i = 0; i < w * h; i++) {
        src[i] = (uint16_t)(0x1000 + i * 3);
    }
    rc = codec_encode(CODEC_ENC_DELTA_RLE_V1, src, w, h, enc, sizeof(enc),
                      &elen);
    ASSERT_EQ_INT(CODEC_OK, rc);

    fake_display_reset();
    n = contract_pack_rect(msg, sizeof(msg), 5, 6, 3, 4, (uint16_t)w,
                           (uint16_t)h, CODEC_ENC_DELTA_RLE_V1, enc,
                           (uint32_t)elen);
    ASSERT_TRUE(n > 0);
    ASSERT_EQ_INT(CONTRACT_OK, contract_dispatch(msg, n));
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            ASSERT_EQ_U16(src[y * w + x],
                          fake_display_get_pixel(3 + x, 4 + y));
        }
    }
}

static void test_clear_then_rect(void)
{
    uint8_t buf[128];
    uint16_t solid[16];
    size_t n;
    int i;

    for (i = 0; i < 16; i++) {
        solid[i] = 0x07E0;
    }
    fake_display_reset();
    n = contract_pack_clear(buf, sizeof(buf), 0, 0, 0x001F);
    ASSERT_EQ_INT(CONTRACT_OK, contract_dispatch(buf, n));
    n = contract_pack_rect(buf, sizeof(buf), 0, 1, 0, 0, 4, 4,
                           CODEC_ENC_RAW_RGB565, (const uint8_t *)solid,
                           sizeof(solid));
    ASSERT_EQ_INT(CONTRACT_OK, contract_dispatch(buf, n));
    ASSERT_EQ_U16(0x07E0, fake_display_get_pixel(0, 0));
    ASSERT_EQ_U16(0x001F, fake_display_get_pixel(10, 10));
}

static void test_bad_delta_payload(void)
{
    uint8_t junk[4] = {0x01, 0x02, 0x03, 0x04};
    uint8_t msg[64];
    size_t n = contract_pack_rect(msg, sizeof(msg), 0, 0, 0, 0, 4, 4,
                                  CODEC_ENC_DELTA_RLE_V1, junk, 4);
    fake_display_reset();
    ASSERT_TRUE(n > 0);
    {
        int rc = contract_dispatch(msg, n);
        ASSERT_TRUE(rc == CONTRACT_ERR_PAYLOAD || rc == CONTRACT_ERR_TRUNC);
    }
}

int main(void)
{
    test_dispatch_clear();
    test_dispatch_rect_raw();
    test_dispatch_rect_delta();
    test_clear_then_rect();
    test_bad_delta_payload();
    return test_report();
}

#include "codec.h"
#include "contract.h"
#include "bind.h"
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

static void test_reject_delta_enc(void)
{
    uint8_t junk[8] = {0};
    uint8_t msg[64];
    size_t n;
    /* enc=1 no longer accepted (W21) */
    n = contract_pack_rect(msg, sizeof(msg), 0, 0, 0, 0, 2, 2, 1, junk, 8);
    ASSERT_TRUE(n > 0);
    fake_display_reset();
    ASSERT_EQ_INT(CONTRACT_ERR_ARG, contract_dispatch(msg, n));
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

static void test_bad_raw_payload(void)
{
    uint8_t junk[4] = {0x01, 0x02, 0x03, 0x04};
    uint8_t msg[64];
    size_t n = contract_pack_rect(msg, sizeof(msg), 0, 0, 0, 0, 4, 4,
                                  CODEC_ENC_RAW_RGB565, junk, 4);
    fake_display_reset();
    ASSERT_TRUE(n > 0);
    ASSERT_EQ_INT(CONTRACT_ERR_PAYLOAD, contract_dispatch(msg, n));
}

static uint8_t s_mock_body[64];
static size_t s_mock_body_len;
static int s_mock_fetch_rc;

static int mock_fetch(const uint8_t *url, size_t url_len, uint8_t **body_out,
                      size_t *body_len_out, void *user)
{
    (void)user;
    if (s_mock_fetch_rc != 0) {
        return s_mock_fetch_rc;
    }
    if (url_len < 4 || memcmp(url, "http", 4) != 0) {
        return -1;
    }
    *body_out = (uint8_t *)malloc(s_mock_body_len);
    if (!*body_out) {
        return -1;
    }
    memcpy(*body_out, s_mock_body, s_mock_body_len);
    *body_len_out = s_mock_body_len;
    return 0;
}

static void test_dispatch_uri_rect(void)
{
    uint16_t px[4] = {0x001F, 0x07E0, 0xF800, 0xFFFF};
    const char *url = "http://host/test.raw";
    uint8_t msg[128];
    size_t n;

    memcpy(s_mock_body, px, sizeof(px));
    s_mock_body_len = sizeof(px);
    s_mock_fetch_rc = 0;
    contract_set_fetch(mock_fetch, NULL);

    fake_display_reset();
    n = contract_pack_rect_flags(
        msg, sizeof(msg), 9, 1, 10, 20, 2, 2, CODEC_ENC_RAW_RGB565,
        CONTRACT_FLAG_URI, (const uint8_t *)url, (uint32_t)strlen(url));
    ASSERT_TRUE(n > 0);
    ASSERT_EQ_INT(CONTRACT_OK, contract_dispatch(msg, n));
    ASSERT_EQ_U16(0x001F, fake_display_get_pixel(10, 20));
    ASSERT_EQ_U16(0x07E0, fake_display_get_pixel(11, 20));
    ASSERT_EQ_U16(0xF800, fake_display_get_pixel(10, 21));
    ASSERT_EQ_U16(0xFFFF, fake_display_get_pixel(11, 21));

    contract_set_fetch(NULL, NULL);
}

static void test_uri_without_fetch_fails(void)
{
    const char *url = "http://host/x";
    uint8_t msg[128];
    size_t n;

    contract_set_fetch(NULL, NULL);
    n = contract_pack_rect_flags(
        msg, sizeof(msg), 1, 1, 0, 0, 2, 2, CODEC_ENC_RAW_RGB565,
        CONTRACT_FLAG_URI, (const uint8_t *)url, (uint32_t)strlen(url));
    fake_display_reset();
    ASSERT_EQ_INT(CONTRACT_ERR_FETCH, contract_dispatch(msg, n));
}

static void test_dispatch_fill_rect(void)
{
    uint8_t buf[64];
    size_t n;

    fake_display_reset();
    n = contract_pack_clear(buf, sizeof(buf), 0, 0, 0x0000);
    ASSERT_EQ_INT(CONTRACT_OK, contract_dispatch(buf, n));
    n = contract_pack_fill_rect(buf, sizeof(buf), 1, 1, 10, 20, 5, 3, 0xF800);
    ASSERT_EQ_INT(CONTRACT_OK, contract_dispatch(buf, n));
    ASSERT_EQ_U16(0xF800, fake_display_get_pixel(10, 20));
    ASSERT_EQ_U16(0xF800, fake_display_get_pixel(14, 22));
    ASSERT_EQ_U16(0x0000, fake_display_get_pixel(9, 20));
    ASSERT_EQ_U16(0x0000, fake_display_get_pixel(15, 20));
}

static void test_dispatch_text(void)
{
    const char *s = "A";
    uint8_t buf[64];
    size_t n;
    int lit = 0;
    int x, y;

    fake_display_reset();
    n = contract_pack_clear(buf, sizeof(buf), 0, 0, 0x0000);
    ASSERT_EQ_INT(CONTRACT_OK, contract_dispatch(buf, n));
    n = contract_pack_text(buf, sizeof(buf), 1, 1, 0, 0, 0xFFFF, 1,
                           (const uint8_t *)s, 1);
    ASSERT_EQ_INT(CONTRACT_OK, contract_dispatch(buf, n));
    /* 'A' glyph should light some pixels in 5×7 box */
    for (y = 0; y < 7; y++) {
        for (x = 0; x < 5; x++) {
            if (fake_display_get_pixel(x, y) == 0xFFFF) {
                lit++;
            }
        }
    }
    ASSERT_TRUE(lit > 5);
}

static void test_dispatch_bind(void)
{
    uint8_t buf[128];
    size_t n;
    const char *init = "12.3";
    const char *upd = "99";

    fake_display_reset();
    bind_reset();
    n = contract_pack_bind_define(buf, sizeof(buf), 0, 1, 8, 16, 0xFFFF, 0x0010,
                                  1, 8, (const uint8_t *)init, 4);
    ASSERT_TRUE(n > 0);
    ASSERT_EQ_INT(CONTRACT_OK, contract_dispatch(buf, n));
    ASSERT_EQ_INT(0, strcmp(bind_get(0)->text, "12.3"));

    n = contract_pack_bind_set(buf, sizeof(buf), 0, 2, (const uint8_t *)upd, 2);
    ASSERT_EQ_INT(CONTRACT_OK, contract_dispatch(buf, n));
    ASSERT_EQ_INT(0, strcmp(bind_get(0)->text, "99"));
}

static void test_dispatch_batch(void)
{
    uint8_t ops[64];
    uint8_t msg[128];
    size_t o = 0;
    size_t n;
    size_t w;

    fake_display_reset();
    w = contract_batch_put_fill(ops + o, sizeof(ops) - o, 10, 20, 4, 2, 0xF800);
    ASSERT_TRUE(w > 0);
    o += w;
    w = contract_batch_put_text(ops + o, sizeof(ops) - o, 0, 0, 0xFFFF, 1,
                                (const uint8_t *)"Hi", 2);
    ASSERT_TRUE(w > 0);
    o += w;
    n = contract_pack_batch(msg, sizeof(msg), 1, 1, ops, (uint32_t)o);
    ASSERT_TRUE(n > 0);
    ASSERT_EQ_INT(CONTRACT_OK, contract_dispatch(msg, n));
    ASSERT_EQ_U16(0xF800, fake_display_get_pixel(10, 20));
    ASSERT_EQ_U16(0xF800, fake_display_get_pixel(13, 21));
    ASSERT_TRUE(fake_display_get_pixel(0, 0) == 0xFFFF ||
                fake_display_get_pixel(1, 1) == 0xFFFF);
}

static void test_dispatch_group(void)
{
    uint8_t ops[32];
    uint8_t msg[96];
    size_t o = 0;
    size_t n;
    size_t w;

    fake_display_reset();
    contract_group_reset();
    w = contract_batch_put_fill(ops + o, sizeof(ops) - o, 5, 5, 3, 3, 0x07E0);
    ASSERT_TRUE(w > 0);
    o += w;
    n = contract_pack_group_define(msg, sizeof(msg), 0, 1, ops, (uint32_t)o);
    ASSERT_TRUE(n > 0);
    ASSERT_EQ_INT(CONTRACT_OK, contract_dispatch(msg, n));
    ASSERT_EQ_U16(0x07E0, fake_display_get_pixel(5, 5));

    /* clear panel then redraw group */
    n = contract_pack_clear(msg, sizeof(msg), 0, 2, 0x0000);
    ASSERT_EQ_INT(CONTRACT_OK, contract_dispatch(msg, n));
    ASSERT_EQ_U16(0x0000, fake_display_get_pixel(5, 5));
    n = contract_pack_group_draw(msg, sizeof(msg), 0, 3);
    ASSERT_EQ_INT(CONTRACT_OK, contract_dispatch(msg, n));
    ASSERT_EQ_U16(0x07E0, fake_display_get_pixel(5, 5));
    ASSERT_EQ_U16(0x07E0, fake_display_get_pixel(7, 7));
}

static void test_group_draw_undefined_fails(void)
{
    uint8_t msg[64];
    size_t n;

    contract_group_reset();
    n = contract_pack_group_draw(msg, sizeof(msg), 1, 0);
    ASSERT_EQ_INT(CONTRACT_ERR_ARG, contract_dispatch(msg, n));
}

int main(void)
{
    test_dispatch_clear();
    test_dispatch_rect_raw();
    test_reject_delta_enc();
    test_clear_then_rect();
    test_bad_raw_payload();
    test_dispatch_uri_rect();
    test_uri_without_fetch_fails();
    test_dispatch_fill_rect();
    test_dispatch_text();
    test_dispatch_bind();
    test_dispatch_batch();
    test_dispatch_group();
    test_group_draw_undefined_fails();
    return test_report();
}

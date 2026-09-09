#include "contract.h"
#include "codec.h"
#include "test_assert.h"

#include <stdint.h>
#include <string.h>

static void test_pack_parse_clear(void)
{
    uint8_t buf[64];
    contract_msg_t m;
    size_t n = contract_pack_clear(buf, sizeof(buf), 7, 9, 0xF800);
    ASSERT_EQ_INT(CONTRACT_HDR_SIZE, (int)n);
    ASSERT_EQ_INT(CONTRACT_OK, contract_parse(buf, n, &m));
    ASSERT_EQ_INT(CONTRACT_TYPE_DISPLAY_CLEAR, m.type);
    ASSERT_EQ_INT(7, m.id);
    ASSERT_EQ_INT(9, m.seq);
    ASSERT_EQ_U16(0xF800, m.color);
    ASSERT_EQ_INT(0, (int)m.payload_len);
}

static void test_bad_magic(void)
{
    uint8_t buf[64];
    contract_msg_t m;
    size_t n = contract_pack_clear(buf, sizeof(buf), 0, 0, 0);
    buf[0] = 'X';
    ASSERT_EQ_INT(CONTRACT_ERR_MAGIC, contract_parse(buf, n, &m));
}

static void test_bad_ver(void)
{
    uint8_t buf[64];
    contract_msg_t m;
    size_t n = contract_pack_clear(buf, sizeof(buf), 0, 0, 0);
    buf[4] = 99;
    ASSERT_EQ_INT(CONTRACT_ERR_VER, contract_parse(buf, n, &m));
}

static void test_bad_type(void)
{
    uint8_t buf[64];
    contract_msg_t m;
    size_t n = contract_pack_clear(buf, sizeof(buf), 0, 0, 0);
    buf[5] = 0x7F;
    ASSERT_EQ_INT(CONTRACT_ERR_TYPE, contract_parse(buf, n, &m));
}

static void test_uri_flag_on_clear_rejected(void)
{
    uint8_t buf[64];
    contract_msg_t m;
    size_t n = contract_pack_clear(buf, sizeof(buf), 0, 0, 0);
    buf[6] = CONTRACT_FLAG_URI;
    ASSERT_EQ_INT(CONTRACT_ERR_FLAGS, contract_parse(buf, n, &m));
}

static void test_unknown_flag_rejected(void)
{
    uint8_t buf[64];
    contract_msg_t m;
    size_t n = contract_pack_clear(buf, sizeof(buf), 0, 0, 0);
    buf[6] = 0x02; /* reserved bit */
    ASSERT_EQ_INT(CONTRACT_ERR_FLAGS, contract_parse(buf, n, &m));
}

static void test_parse_uri_rect(void)
{
    const char *url = "http://192.168.0.1:8000/a.bin";
    uint8_t buf[128];
    contract_msg_t m;
    size_t n = contract_pack_rect_flags(
        buf, sizeof(buf), 1, 2, 10, 20, 8, 8, CODEC_ENC_DELTA_RLE_V1,
        CONTRACT_FLAG_URI, (const uint8_t *)url, (uint32_t)strlen(url));
    ASSERT_TRUE(n == CONTRACT_HDR_SIZE + strlen(url));
    ASSERT_EQ_INT(CONTRACT_OK, contract_parse(buf, n, &m));
    ASSERT_EQ_INT(CONTRACT_FLAG_URI, m.flags);
    ASSERT_EQ_INT((int)strlen(url), (int)m.payload_len);
    ASSERT_TRUE(memcmp(m.payload, url, strlen(url)) == 0);
    /* URI path does not require payload_len == w*h*2 */
    ASSERT_EQ_INT(8, m.w);
    ASSERT_EQ_INT(8, m.h);
}

static void test_trunc_header(void)
{
    uint8_t buf[64];
    contract_msg_t m;
    size_t n = contract_pack_clear(buf, sizeof(buf), 0, 0, 1);
    ASSERT_EQ_INT(CONTRACT_ERR_TRUNC, contract_parse(buf, 10, &m));
    ASSERT_TRUE(n == CONTRACT_HDR_SIZE);
}

static void test_trunc_payload(void)
{
    uint8_t buf[128];
    uint8_t pix[8] = {0};
    contract_msg_t m;
    size_t n = contract_pack_rect(buf, sizeof(buf), 1, 2, 0, 0, 2, 2, 0, pix, 8);
    ASSERT_TRUE(n == CONTRACT_HDR_SIZE + 8);
    ASSERT_EQ_INT(CONTRACT_ERR_TRUNC,
                  contract_parse(buf, CONTRACT_HDR_SIZE + 3, &m));
}

static void test_raw_len_mismatch(void)
{
    uint8_t buf[128];
    uint8_t pix[4] = {0};
    contract_msg_t m;
    size_t n =
        contract_pack_rect(buf, sizeof(buf), 0, 0, 0, 0, 2, 2, 0, pix, 4);
    ASSERT_TRUE(n > 0);
    ASSERT_EQ_INT(CONTRACT_ERR_PAYLOAD, contract_parse(buf, n, &m));
}

static void test_rect_zero_wh(void)
{
    uint8_t buf[64];
    contract_msg_t m;
    /* pack rejects w=0 */
    ASSERT_EQ_INT(0, (int)contract_pack_rect(buf, sizeof(buf), 0, 0, 0, 0, 0, 1,
                                             0, NULL, 0));
    /* craft header manually */
    memset(buf, 0, sizeof(buf));
    buf[0] = 'W';
    buf[1] = 'L';
    buf[2] = 'D';
    buf[3] = '1';
    buf[4] = 1;
    buf[5] = CONTRACT_TYPE_RASTER_RECT;
    buf[16] = 0;
    buf[17] = 0; /* w=0 */
    buf[18] = 1;
    buf[19] = 0; /* h=1 */
    ASSERT_EQ_INT(CONTRACT_ERR_ARG, contract_parse(buf, CONTRACT_HDR_SIZE, &m));
}

static void test_clear_with_payload(void)
{
    uint8_t buf[64];
    contract_msg_t m;
    size_t n = contract_pack_clear(buf, sizeof(buf), 0, 0, 0x001F);
    buf[24] = 1; /* payload_len = 1 */
    buf[28] = 0xAA;
    ASSERT_EQ_INT(CONTRACT_ERR_ARG,
                  contract_parse(buf, CONTRACT_HDR_SIZE + 1, &m));
    ASSERT_TRUE(n == CONTRACT_HDR_SIZE);
}

static void test_parse_rect_fields(void)
{
    uint8_t buf[128];
    uint8_t pix[8];
    contract_msg_t m;
    memset(pix, 0x11, sizeof(pix));
    size_t n = contract_pack_rect(buf, sizeof(buf), 3, 4, -2, 5, 2, 2, 0, pix, 8);
    ASSERT_EQ_INT(CONTRACT_OK, contract_parse(buf, n, &m));
    ASSERT_EQ_INT(CONTRACT_TYPE_RASTER_RECT, m.type);
    ASSERT_EQ_INT(3, m.id);
    ASSERT_EQ_INT(4, m.seq);
    ASSERT_EQ_INT(-2, m.x);
    ASSERT_EQ_INT(5, m.y);
    ASSERT_EQ_INT(2, m.w);
    ASSERT_EQ_INT(2, m.h);
    ASSERT_EQ_INT(0, m.enc);
    ASSERT_EQ_INT(8, (int)m.payload_len);
    ASSERT_TRUE(m.payload != NULL && m.payload[0] == 0x11);
}

int main(void)
{
    test_pack_parse_clear();
    test_bad_magic();
    test_bad_ver();
    test_bad_type();
    test_uri_flag_on_clear_rejected();
    test_unknown_flag_rejected();
    test_parse_uri_rect();
    test_trunc_header();
    test_trunc_payload();
    test_raw_len_mismatch();
    test_rect_zero_wh();
    test_clear_with_payload();
    test_parse_rect_fields();
    return test_report();
}

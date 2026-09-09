#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    CONTRACT_VER = 1,
    CONTRACT_HDR_SIZE = 28,
    CONTRACT_URI_MAX = 256
};

/** Wire magic: 'W' 'L' 'D' '1' */
enum {
    CONTRACT_MAGIC0 = 0x57,
    CONTRACT_MAGIC1 = 0x4C,
    CONTRACT_MAGIC2 = 0x44,
    CONTRACT_MAGIC3 = 0x31
};

typedef enum contract_type {
    CONTRACT_TYPE_DISPLAY_CLEAR = 0x01,
    CONTRACT_TYPE_RASTER_RECT = 0x02
} contract_type_t;

enum {
    CONTRACT_FLAG_URI = 1u << 0
};

enum {
    CONTRACT_FMT_RGB565 = 0
};

enum {
    CONTRACT_OK = 0,
    CONTRACT_ERR_ARG = -1,
    CONTRACT_ERR_MAGIC = -2,
    CONTRACT_ERR_VER = -3,
    CONTRACT_ERR_TYPE = -4,
    CONTRACT_ERR_FLAGS = -5,
    CONTRACT_ERR_TRUNC = -6,
    CONTRACT_ERR_PAYLOAD = -7,
    CONTRACT_ERR_NOSPACE = -8,
    CONTRACT_ERR_FETCH = -9
};

typedef struct contract_msg {
    uint8_t type;
    uint8_t flags;
    uint16_t id;
    uint16_t seq;
    int16_t x;
    int16_t y;
    uint16_t w;
    uint16_t h;
    uint8_t enc;
    uint8_t fmt;
    uint16_t color;
    uint32_t payload_len;
    const uint8_t *payload; /* points into caller buffer; not copied */
} contract_msg_t;

/**
 * Optional HTTP(S) body fetch for FLAG_URI rects.
 * On success: *body_out is malloc'd; contract_apply frees it.
 * url is not NUL-terminated; length is url_len.
 * @return 0 on success, negative on failure.
 */
typedef int (*contract_fetch_fn)(const uint8_t *url, size_t url_len,
                                 uint8_t **body_out, size_t *body_len_out,
                                 void *user);

void contract_set_fetch(contract_fetch_fn fn, void *user);

/**
 * Parse one envelope. Does not copy payload; out->payload points into buf.
 * @return CONTRACT_OK or negative error.
 */
int contract_parse(const uint8_t *buf, size_t len, contract_msg_t *out);

/**
 * Apply a parsed message: clear → render_clear; rect → decode + blit rows.
 * FLAG_URI rects call the registered fetch hook first.
 * @return CONTRACT_OK or negative (maps codec failures to PAYLOAD/NOSPACE).
 */
int contract_apply(const contract_msg_t *msg);

/** parse + apply */
int contract_dispatch(const uint8_t *buf, size_t len);

/**
 * Pack helpers (host tools / tests). Write into out[0..out_cap).
 * @return total bytes written, or 0 on failure (too small / bad args).
 */
size_t contract_pack_clear(uint8_t *out, size_t out_cap, uint16_t id,
                           uint16_t seq, uint16_t color);

size_t contract_pack_rect(uint8_t *out, size_t out_cap, uint16_t id,
                          uint16_t seq, int16_t x, int16_t y, uint16_t w,
                          uint16_t h, uint8_t enc, const uint8_t *payload,
                          uint32_t payload_len);

/** Like pack_rect but sets header flags (e.g. CONTRACT_FLAG_URI). */
size_t contract_pack_rect_flags(uint8_t *out, size_t out_cap, uint16_t id,
                                uint16_t seq, int16_t x, int16_t y, uint16_t w,
                                uint16_t h, uint8_t enc, uint8_t flags,
                                const uint8_t *payload, uint32_t payload_len);

#ifdef __cplusplus
}
#endif

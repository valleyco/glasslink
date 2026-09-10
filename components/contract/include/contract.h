#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    CONTRACT_VER = 1,
    CONTRACT_HDR_SIZE = 28,
    CONTRACT_URI_MAX = 256,
    CONTRACT_TEXT_MAX = 64,
    CONTRACT_BIND_SLOTS = 8,
    CONTRACT_GROUP_SLOTS = 4,
    CONTRACT_BATCH_BYTES_MAX = 1024,
    CONTRACT_BATCH_OPS_MAX = 32
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
    CONTRACT_TYPE_RASTER_RECT = 0x02,
    CONTRACT_TYPE_FILL_RECT = 0x03,
    CONTRACT_TYPE_DRAW_TEXT = 0x04,
    CONTRACT_TYPE_BIND_DEFINE = 0x05,
    CONTRACT_TYPE_BIND_SET = 0x06,
    CONTRACT_TYPE_DRAW_BATCH = 0x07,
    CONTRACT_TYPE_GROUP_DEFINE = 0x08,
    CONTRACT_TYPE_GROUP_DRAW = 0x09,
    CONTRACT_TYPE_DRAW_POLY = 0x0A,
    CONTRACT_TYPE_MOVE_TO = 0x0B,
    CONTRACT_TYPE_LINE_TO = 0x0C,
    CONTRACT_TYPE_CUBIC_TO = 0x0D
} contract_type_t;

/** Sub-opcodes inside draw.batch / group.define payloads (reuse type ids). */
enum {
    CONTRACT_BATCH_OP_FILL = 0x03,
    CONTRACT_BATCH_OP_TEXT = 0x04,
    CONTRACT_BATCH_OP_POLY = 0x0A,
    CONTRACT_BATCH_OP_MOVE_TO = 0x0B,
    CONTRACT_BATCH_OP_LINE_TO = 0x0C,
    CONTRACT_BATCH_OP_CUBIC_TO = 0x0D
};

enum {
    CONTRACT_POLY_MAX = 16
};


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

typedef int (*contract_fetch_fn)(const uint8_t *url, size_t url_len,
                                 uint8_t **body_out, size_t *body_len_out,
                                 void *user);

/** Optional body release; if unset, contract_apply calls free(body). */
typedef void (*contract_fetch_release_fn)(uint8_t *body, void *user);

void contract_set_fetch(contract_fetch_fn fn, void *user);
void contract_set_fetch_release(contract_fetch_release_fn fn);

int contract_parse(const uint8_t *buf, size_t len, contract_msg_t *out);
int contract_apply(const contract_msg_t *msg);
int contract_dispatch(const uint8_t *buf, size_t len);

size_t contract_pack_clear(uint8_t *out, size_t out_cap, uint16_t id,
                           uint16_t seq, uint16_t color);

size_t contract_pack_rect(uint8_t *out, size_t out_cap, uint16_t id,
                          uint16_t seq, int16_t x, int16_t y, uint16_t w,
                          uint16_t h, uint8_t enc, const uint8_t *payload,
                          uint32_t payload_len);

size_t contract_pack_rect_flags(uint8_t *out, size_t out_cap, uint16_t id,
                                uint16_t seq, int16_t x, int16_t y, uint16_t w,
                                uint16_t h, uint8_t enc, uint8_t flags,
                                const uint8_t *payload, uint32_t payload_len);

size_t contract_pack_fill_rect(uint8_t *out, size_t out_cap, uint16_t id,
                               uint16_t seq, int16_t x, int16_t y, uint16_t w,
                               uint16_t h, uint16_t color);

/** UTF-8 / ASCII text; enc = pixel scale (1 or 2; 0 → 1). */
size_t contract_pack_text(uint8_t *out, size_t out_cap, uint16_t id,
                          uint16_t seq, int16_t x, int16_t y, uint16_t color,
                          uint8_t scale, const uint8_t *utf8, uint32_t len);

/** id = slot (0..7). w = max_chars. payload = bg_u16_le + optional UTF-8. */
size_t contract_pack_bind_define(uint8_t *out, size_t out_cap, uint16_t slot,
                                 uint16_t seq, int16_t x, int16_t y,
                                 uint16_t fg, uint16_t bg, uint8_t scale,
                                 uint8_t max_chars, const uint8_t *utf8,
                                 uint32_t text_len);

/** id = slot. payload = UTF-8 value. */
size_t contract_pack_bind_set(uint8_t *out, size_t out_cap, uint16_t slot,
                              uint16_t seq, const uint8_t *utf8, uint32_t len);

/** Append fill_rect sub-op to batch buffer. Returns bytes written or 0. */
size_t contract_batch_put_fill(uint8_t *dst, size_t dst_cap, int16_t x,
                               int16_t y, uint16_t w, uint16_t h,
                               uint16_t color);

/** Append text sub-op. Returns bytes written or 0. */
size_t contract_batch_put_text(uint8_t *dst, size_t dst_cap, int16_t x,
                               int16_t y, uint16_t color, uint8_t scale,
                               const uint8_t *utf8, uint8_t len);

size_t contract_pack_batch(uint8_t *out, size_t out_cap, uint16_t id,
                           uint16_t seq, const uint8_t *ops, uint32_t ops_len);

/** id = group 0..3; payload = batch ops; stores + draws. */
size_t contract_pack_group_define(uint8_t *out, size_t out_cap, uint16_t group,
                                  uint16_t seq, const uint8_t *ops,
                                  uint32_t ops_len);

/** id = group; empty payload; redraws stored batch. */
size_t contract_pack_group_draw(uint8_t *out, size_t out_cap, uint16_t group,
                                uint16_t seq);

/** enc = n verts (3..16); color; payload = n×(x:i16,y:i16) LE. */
size_t contract_pack_poly(uint8_t *out, size_t out_cap, uint16_t id,
                          uint16_t seq, uint16_t color, uint8_t n,
                          const int16_t *xy /* n*2 */);

size_t contract_pack_move_to(uint8_t *out, size_t out_cap, uint16_t id,
                             uint16_t seq, int16_t x, int16_t y);

/** Stroke pen→(x,y) with color; advances pen. */
size_t contract_pack_line_to(uint8_t *out, size_t out_cap, uint16_t id,
                             uint16_t seq, int16_t x, int16_t y,
                             uint16_t color);

/** Cubic from pen through (x1,y1),(x2,y2) to (x3,y3); payload 6×i16; advances pen. */
size_t contract_pack_cubic_to(uint8_t *out, size_t out_cap, uint16_t id,
                              uint16_t seq, uint16_t color, int16_t x1,
                              int16_t y1, int16_t x2, int16_t y2, int16_t x3,
                              int16_t y3);

size_t contract_batch_put_poly(uint8_t *dst, size_t dst_cap, uint16_t color,
                               uint8_t n, const int16_t *xy);
size_t contract_batch_put_move_to(uint8_t *dst, size_t dst_cap, int16_t x,
                                  int16_t y);
size_t contract_batch_put_line_to(uint8_t *dst, size_t dst_cap, int16_t x,
                                  int16_t y, uint16_t color);
size_t contract_batch_put_cubic_to(uint8_t *dst, size_t dst_cap, uint16_t color,
                                   int16_t x1, int16_t y1, int16_t x2,
                                   int16_t y2, int16_t x3, int16_t y3);

/** Execute packed batch ops (fill/text). Used by draw.batch and groups. */
int contract_apply_batch(const uint8_t *ops, size_t len);

void contract_group_reset(void);
void contract_pen_reset(void);

#ifdef __cplusplus
}
#endif

# L0 binary command envelope v1 (W9)

Normative wire format for MQTT `cmd` payloads (and host tools).  
**Little-endian** multi-byte fields. No CBOR/JSON on device v1.

## Header (28 bytes, fixed)

| Off | Size | Field | Notes |
|-----|------|-------|-------|
| 0 | 4 | `magic` | ASCII `WLD1` (`0x57 0x4C 0x44 0x31`) |
| 4 | 1 | `ver` | **1** |
| 5 | 1 | `type` | see types |
| 6 | 1 | `flags` | bit0 reserved **URI** (must be 0 in v1) |
| 7 | 1 | `pad0` | 0 |
| 8 | 2 | `id` | command / widget id (host-defined) |
| 10 | 2 | `seq` | monotonic seq (debug / ack later) |
| 12 | 2 | `x` | int16 panel X |
| 14 | 2 | `y` | int16 panel Y |
| 16 | 2 | `w` | uint16 width |
| 18 | 2 | `h` | uint16 height |
| 20 | 1 | `enc` | `0=raw_rgb565`, `1=delta_rle_v1` (same as `codec_enc_t`) |
| 21 | 1 | `fmt` | `0=rgb565` only in v1 |
| 22 | 2 | `color` | `display.clear` fill color; unused for rect |
| 24 | 4 | `payload_len` | bytes following header |
| 28 | `payload_len` | `payload` | type-specific |

Total message size = `28 + payload_len`.  
Truncation / `payload_len` past buffer → `CONTRACT_ERR_TRUNC`.

## Types

| `type` | Name | Payload | Geometry / color |
|--------|------|---------|------------------|
| `0x01` | `display.clear` | **empty** (`payload_len=0`) | `color` = RGB565; `x,y,w,h,enc` ignored |
| `0x02` | `raster.rect` | encoded pixels | `x,y,w,h` required (`w,h≥1`); `enc` selects codec; `color` ignored |

Reserved for later (parser rejects unknown): `0x10` draw.batch, `0x20` asset.fetch, etc.

## Flags

| Bit | Name | v1 |
|-----|------|----|
| 0 | `FLAG_URI` | **must be 0** — HTTP body comes in a later phase (W7) |
| 1–7 | reserved | 0 |

## `raster.rect` payload rules

- `fmt` must be `0` (RGB565).
- `enc=0` (raw): `payload_len == w * h * 2`, host-endian RGB565 packed row-major (LE bytes on wire = native on LE hosts).
- `enc=1` (`delta_rle_v1`): bitstream per [`delta_rle_v1.md`](delta_rle_v1.md); `payload_len ≥ 1`.
- Decoder streams **rows** into `hal_display_blit` (no full-frame buffer required on device path).
- Panel clip is HAL’s job; out-of-range origin is OK if intersection non-empty.

## Reject conditions (non-exhaustive)

| Code | When |
|------|------|
| `CONTRACT_ERR_MAGIC` | magic ≠ `WLD1` |
| `CONTRACT_ERR_VER` | `ver ≠ 1` |
| `CONTRACT_ERR_TYPE` | unknown type |
| `CONTRACT_ERR_FLAGS` | URI or other forbidden bits |
| `CONTRACT_ERR_ARG` | clear with payload; rect with w/h=0; bad fmt/enc |
| `CONTRACT_ERR_TRUNC` | buffer shorter than header or header+payload |
| `CONTRACT_ERR_PAYLOAD` | raw length mismatch / codec failure |
| `CONTRACT_ERR_NOSPACE` | scratch alloc fail (host) |

## Host API surface

```c
int contract_parse(const uint8_t *buf, size_t len, contract_msg_t *out);
int contract_apply(const contract_msg_t *msg);   /* → render + codec */
int contract_dispatch(const uint8_t *buf, size_t len);
size_t contract_pack_clear(...);
size_t contract_pack_rect(...);
```

See `components/contract/include/contract.h`.

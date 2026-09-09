# L0 binary command envelope v1 (W9) + URI pull (W15 / Step 10-B)

Normative wire format for MQTT `cmd` payloads (and host tools).  
**Little-endian** multi-byte fields. No CBOR/JSON on device v1.

## Header (28 bytes, fixed)

| Off | Size | Field | Notes |
|-----|------|-------|-------|
| 0 | 4 | `magic` | ASCII `WLD1` (`0x57 0x4C 0x44 0x31`) |
| 4 | 1 | `ver` | **1** |
| 5 | 1 | `type` | see types |
| 6 | 1 | `flags` | bit0 = `FLAG_URI`; other bits **must be 0** |
| 7 | 1 | `pad0` | 0 |
| 8 | 2 | `id` | command / widget id (host-defined) |
| 10 | 2 | `seq` | monotonic seq (ack matching) |
| 12 | 2 | `x` | int16 panel X |
| 14 | 2 | `y` | int16 panel Y |
| 16 | 2 | `w` | uint16 width |
| 18 | 2 | `h` | uint16 height |
| 20 | 1 | `enc` | `0=raw_rgb565`, `1=delta_rle_v1` |
| 21 | 1 | `fmt` | `0=rgb565` only in v1 |
| 22 | 2 | `color` | `display.clear` fill color; unused for rect |
| 24 | 4 | `payload_len` | bytes following header |
| 28 | `payload_len` | `payload` | type-specific |

Total message size = `28 + payload_len`.  
Truncation / `payload_len` past buffer → `CONTRACT_ERR_TRUNC`.

## Types

| `type` | Name | Payload | Geometry / color |
|--------|------|---------|------------------|
| `0x01` | `display.clear` | **empty** (`payload_len=0`) | `color` = RGB565; flags must be 0 |
| `0x02` | `raster.rect` | encoded pixels **or** URI (see flags) | `x,y,w,h` required (`w,h≥1`); `enc` selects codec |

## Flags

| Bit | Name | Meaning |
|-----|------|---------|
| 0 | `FLAG_URI` | MQTT payload is UTF-8 `http://…` URL (1…256 bytes). Device HTTP GETs the body; body is the same codec bytes that would have been inline. |
| 1–7 | reserved | must be 0 → `CONTRACT_ERR_FLAGS` |

## `raster.rect` payload rules

### Inline (`FLAG_URI` clear)

- `fmt` must be `0` (RGB565).
- `enc=0` (raw): `payload_len == w * h * 2`.
- `enc=1` (`delta_rle_v1`): bitstream per [`delta_rle_v1.md`](delta_rle_v1.md); `payload_len ≥ 1`.

### URI (`FLAG_URI` set)

- Payload = URL bytes (not NUL-terminated); `1 ≤ payload_len ≤ 256`.
- URL scheme **`http://` only** (no TLS — W10).
- HTTP response body = encoded pixels for `enc` / `w` / `h` (raw length check applies to the **body**, not MQTT).
- Device body cap: **`http_max`** (default 65536) advertised on `status`. Prefer `delta_rle_v1` for large rects; full-frame raw (≈150 KiB) will not fit.

Decoder streams **rows** into `hal_display_blit`. Fetch hook: `contract_set_fetch` (device: `wd_http_fetch`).

## Reject conditions (non-exhaustive)

| Code | When |
|------|------|
| `CONTRACT_ERR_MAGIC` | magic ≠ `WLD1` |
| `CONTRACT_ERR_VER` | `ver ≠ 1` |
| `CONTRACT_ERR_TYPE` | unknown type |
| `CONTRACT_ERR_FLAGS` | reserved flag bits; URI on clear |
| `CONTRACT_ERR_ARG` | clear with payload; rect with w/h=0; bad fmt/enc |
| `CONTRACT_ERR_TRUNC` | buffer shorter than header or header+payload |
| `CONTRACT_ERR_PAYLOAD` | raw length mismatch / codec failure / bad URI length |
| `CONTRACT_ERR_NOSPACE` | scratch alloc fail |
| `CONTRACT_ERR_FETCH` | no fetch hook / HTTP failure / oversized body |

## Host API surface

```c
int contract_parse(const uint8_t *buf, size_t len, contract_msg_t *out);
int contract_apply(const contract_msg_t *msg);
int contract_dispatch(const uint8_t *buf, size_t len);
void contract_set_fetch(contract_fetch_fn fn, void *user);
size_t contract_pack_clear(...);
size_t contract_pack_rect(...);
size_t contract_pack_rect_flags(..., uint8_t flags, ...);
```

See `components/contract/include/contract.h`.

MQTT topics: [`topics-v1.md`](topics-v1.md).

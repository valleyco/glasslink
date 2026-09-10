# L0/L1 binary command envelope (W9 + W15 URI + W16 early L1)

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
| 20 | 1 | `enc` | codec id **or** text scale (see type) |
| 21 | 1 | `fmt` | `0=rgb565` for raster; unused for fill/text |
| 22 | 2 | `color` | clear / fill / text fg |
| 24 | 4 | `payload_len` | bytes following header |
| 28 | `payload_len` | `payload` | type-specific |

Total message size = `28 + payload_len`.

## Types

| `type` | Name | Payload | Geometry / color / enc |
|--------|------|---------|------------------|
| `0x01` | `display.clear` | empty | `color` = RGB565; flags 0 |
| `0x02` | `raster.rect` | pixels **or** URI | `x,y,w,h`; `enc` = codec |
| `0x03` | `display.fill_rect` | empty | `x,y,w,h` + `color`; flags 0 |
| `0x04` | `draw.text` | UTF-8 (1…64) | `x,y` origin; `color` = fg; `enc` = scale (0/1→1, 2→2) |

## Flags

| Bit | Name | Meaning |
|-----|------|---------|
| 0 | `FLAG_URI` | On `raster.rect` only: MQTT payload is `http://…` URL |
| 1–7 | reserved | must be 0 |

## `raster.rect`

**Inline:** `enc=0` raw (`payload_len == w*h*2`); `enc=1` delta_rle_v1.  
**URI:** payload = URL (≤256); device GET body = encoded pixels; `http_max` default 65536.

## `draw.text`

Device 5×7 ASCII (`0x20`–`0x7E`); transparent bg; scale 1 or 2; advance `6*scale` px.

## Errors

`CONTRACT_ERR_*` as before, plus fetch failures for URI.

## Pack API

`contract_pack_clear`, `contract_pack_rect[_flags]`, `contract_pack_fill_rect`, `contract_pack_text`.

MQTT topics: [`topics-v1.md`](topics-v1.md).

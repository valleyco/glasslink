# Research brief — ESP32-suitable image codecs

Status: absorbed into `PLAN.md` / architecture (v1 unchanged)  
Scope: CYD 320×240 RGB565, no PSRAM, L0 partial rects, decode-on-device

## Ranking for live L0 UI (partial rects)

| Rank | Codec | Role |
|------|-------|------|
| 1 | **Custom Δx/Δy + RLE (RGB565)** | **v1 primary** — streaming, partial-friendly |
| 2 | **Raw RGB565** | v1 baseline / tiny widgets |
| 3 | **mini-qoi** (or QOI-565 variant) | bench opponent (stock QOI is RGB888) |
| 4 | LVGL-style block RLE (format only) | optional second simple `enc` |
| 5 | Heatshrink / LZ4 as **byte wrap** | v1.5 optional on large blobs |
| 6 | **TJpgDec** JPEG | full-bleed photos / wallpaper via HTTP only |
| 7 | Tamp / uzlib | generic assets later |
| — | PNG, full-FB QOI ref, MJPEG/H.264 | **avoid** live path |

## Why delta_rle_v1 wins here

- Same family as QOI/LVGL RLE, but **native RGB565** (no 888 expand)
- Vertical delta = one previous line; horizontal delta = registers
- RLE on residuals; line-stream into TFT window
- Damage rect = natural unit (unlike JPEG 8×8 MCU tax on small widgets)

## JPEG / PNG notes

- **TJpgDec**: ~3.5 KiB work, MCU callbacks → TFT OK; **weak for tiny UI rects**
- HW JPEG is **ESP32-P4**, not this CYD
- **PNG**: full RGBA buffer mentality — tooling only, not device live path

## Sequence model (v1.5+, not v1 ship)

TFT **is** the framebuffer. No device P-frame state beyond glass:

```
keyframe: large/full raster.rect (delta_rle | jpeg)
p-frame:  sparse raster.rect list (+ later fill_rect)
header:   frame_id, base_key
```

Steal RFB *ideas* (RRE/Hextile), not a full VNC stack.

## v1 freeze (aligned with PLAN)

| Item | Action |
|------|--------|
| `enc=raw_rgb565` | required |
| `enc=delta_rle_v1` | primary streaming decoder |
| Bench | raw, plain RLE, delta_rle, mini-qoi, TJpgDec full frame |
| Non-goals | PNG on device, mandatory Huffman/YUV, sequence ship, LVGL |

## Decoder constraints

- No heap in decoder; max width from display profile (320)
- Scratch: 1–2 lines (+ RLE state) steady
- Normative bit layout + `testdata/` vectors before firmware polish

## Primary sources

- goal.md stages; architecture draft L0 path
- QOI / mini-qoi; TJpgDec (elm-chan + IDF); LVGL rle/lz4/tjpgd docs
- heatshrink, LZ4, Tamp, uzlib, bitbank JPEGDEC/PNGdec

*Full agent write-up retained in session history; this file is the durable digest.*

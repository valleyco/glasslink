# ESP32 Wireless Display — Architecture Draft (v0)

Status: working draft for interactive refinement  
Source intent: `goal.md`  
Related hardware/HAL experience: `../esp32-invaders` (primary board + display stack)

## 0. Related project: esp32-invaders (ground truth)

We already own a working **ESP32-2432S028R CYD V3 USB-C** bring-up in
`/home/davidl/Projects/esp32-invaders`. Treat it as the **hardware + IDF
baseline** for this repo unless we deliberately retarget.

| Fact | Value (verified in invaders) | Arch implication |
|------|-------------------------------|------------------|
| MCU | Classic **ESP32-WROOM** (not S3) | No PSRAM assumed; tight internal RAM |
| Panel | **ST7789** SPI, landscape **320×240** | Not ILI9341 on this unit — wrong driver = stripes |
| SPI LCD | MOSI13 SCLK14 CS15 DC2 BL21 @ ~20 MHz | RGB565 **byte-swap on TX** |
| Touch | XPT2046 on **SPI3** (separate bus) | Optional for wl-display v1 |
| Stack | **ESP-IDF** + `esp_lcd`, Make targets | Prefer IDF over Arduino/PlatformIO for reuse |
| Blit style | Strip DMA (`BLIT_ROWS=8`), dual strip buffers | Line/strip decode fits MQTT raster path |
| Dirty path | 1bpp shadow → row bands → partial SPI | Same *idea* for RGB565 rect bands later |
| HAL API | `hal_display_fill_rect`, `blit_rows`, `blit_panel_rgb565` | Ideal sink for L0 `raster.rect` |
| Process | Interactive `PLAN.md` steps | Mirror that workflow here |

**Reuse options (decide later):**

1. **Copy** `components/board` display pieces into this repo (fast, forks drift)
2. **Shared component repo** / submodule (clean long-term)
3. **Vendored files** with explicit “synced from invaders @ date” note

Do **not** pull invaders’ 8080/machine cores — only board/display patterns (and maybe touch later).

## 1. Product intent

A **wireless thin client display** (same CYD as invaders):

- Host(s) push visual updates and/or bind live values
- Device renders to local TFT with tight RAM/CPU budget
- Primary control plane: **MQTT**
- Optional data plane for large objects: **HTTP(S) pull** triggered by MQTT

Non-goals for v1 (unless we reopen):

- Full general-purpose browser/HTML UI
- On-device UI authoring
- Multi-display video streaming at high FPS
- Replacing invaders (separate product; shared glass)

## 2. Design principles

1. **Control plane ≠ bulk plane** — MQTT for small commands/events; HTTP for blobs when needed.
2. **Partial updates first** — never require full-frame over the wire for every change.
3. **Layered display languages** — start narrow (raster rect), keep hooks for draw cmds + high-level UI.
4. **Decode cheaper than encode** — heavy work on host; device is a constrained decoder/renderer.
5. **Explicit memory budget** — every feature states peak RAM and whether PSRAM is required.
6. **Versioned contracts** — topics + payload `ver` field; forward-compatible enums.
7. **Host-first TDD** — pure C (`codec`, `contract`, `render`) tested with gcc + fake_display before IDF; device adapters stay thin.

## 3. Logical architecture

```
┌──────────────────────────┐         MQTT          ┌─────────────────────────────┐
│  Host / Orchestrator     │◄─────────────────────►│  Device Runtime (ESP32)     │
│  - scene composer        │                       │  - net + mqtt               │
│  - encoder (raster/cmd)  │   optional HTTP GET   │  - contract dispatcher      │
│  - asset store           │◄──────────────────────│  - asset cache (opt)        │
│  - value publishers      │                       │  - render pipeline          │
└──────────────────────────┘                       │  - display driver           │
                                                   │  - telemetry                │
                                                   └─────────────────────────────┘
```

### 3.1 Device runtime modules

| Module | Responsibility |
|--------|----------------|
| `boot/config` | NVS: Wi-Fi, broker, device id, display profile |
| `net` | Wi-Fi reconnect, time (SNTP) if needed |
| `mqtt` | connect, subscribe, publish telemetry/acks |
| `contract` | parse envelopes, version gate, route by `type` |
| `assets` | HTTP fetch, size limits, optional flash/spiffs cache |
| `codec` | raster decoders (custom delta-RLE, jpeg later…) |
| `render` | apply rects / commands to FB or direct-to-TFT |
| `display` | panel init, `setAddrWindow` + burst write |
| `bind` (later) | map topic values → named draw slots |
| `ui` (later) | panels/gauges tree |

### 3.2 Host modules (reference, not required in-repo day 1)

- Scene/state → damage regions
- Encoder per region
- MQTT publisher + optional static asset HTTP server
- Dev tools: frame dump, codec A/B, topic spy

## 4. Display language roadmap (3 tiers)

Aligned with `goal.md`. **They are not mutually exclusive** — treat as protocol layers.

| Tier | Name | Wire content | Device work | v1? |
|------|------|--------------|-------------|-----|
| L0 | **Raster rect** | compressed pixels for `x,y,w,h` | decode + TFT blit | **Yes** |
| L1 | **Draw commands** | ops: fill, line, text, image-ref, group redraw | command interpreter; groups by id | v1.5 / v2 |
| L2 | **UI structure** | panels, gauges, lists; style + bindings | widget tree + dirty layout | later |

**v1 recommendation:** ship **L0 solid** + envelope that can carry L1 later without topic redesign.  
Optionally a tiny L1 subset (`clear`, `fill_rect`, `text` with literal string) if needed for boot/status screens — still secondary.

### 4.1 Why L0 first

- Matches compression research goal directly
- Easiest correctness story (pixels in → pixels out)
- Host can still render complex UI and push dirty rects
- Avoids early commitment to LVGL vs custom widgets on-device

### 4.2 L1 hooks (design now, implement later)

- `group_id` for batched redraw
- `value_ref` for “text comes from bound MQTT topic” without resending geometry
- Idempotent full-group replace vs patch ops

### 4.3 L2 later

- Evaluate **LVGL** only if L1 becomes painful; LVGL costs RAM/Flash and pulls the product toward on-device UI.
- Alternative: keep device dumb; host sends L0/L1 only (thin client forever).

## 5. Communication architecture

### 5.1 Roles

| Channel | Use |
|---------|-----|
| MQTT | session, commands, small rects, bindings, telemetry, acks |
| HTTP(S) | large images, fonts, command blobs, codec dictionaries |
| (future) | raw TCP/WS only if MQTT framing overhead becomes a proven bottleneck |

### 5.2 Topic sketch (device-centric)

Research digest: `docs/research-mqtt.md` (openHASP/Tasmota/HA/ESPHome patterns).

```
wd/{device_id}/lwt             # retained online/offline (will)
wd/{device_id}/status          # retained caps: codecs, inline_max, disp, heap
wd/{device_id}/telemetry       # fps, decode ms, rssi, errors
wd/{device_id}/ack             # {id, ok, err, ms}
wd/{device_id}/cmd             # envelope (NOT retained)
wd/{device_id}/bind/+/set      # live values (v1.5+)
wd/{device_id}/event           # touch/buttons [later]
wd/group/{group_id}/cmd        # optional fan-out
wd/broadcast/cmd               # optional
```

### 5.3 Envelope (all `cmd` payloads)

**v1 wire: custom binary header + payload** (W9). Host may pretty-print; device parses fixed layout only. **Never base64 pixels.**

Conceptual (normative layout in `docs/contract/cmd-v1.md` at Step 3):

```text
magic ver type flags id seq
x y w h   fmt enc   payload_len
payload…
```

Types v1: `display.clear`, `raster.rect` (inline data). **No `uri` until HTTP phase.**

`inline_max` in `status` must be **< esp-mqtt buffer** (e.g. 6 KiB / 8 KiB). HTTP offload remains architectural phase 2 (W7).

**Types (planned):**

| type | body gist |
|------|-----------|
| `sys.hello` / config push | display profile, codec caps |
| `display.clear` | color |
| `raster.rect` | geom + fmt + encoding + data or `uri` |
| `asset.fetch` | uri, sha256, ttl, where to store |
| `draw.batch` | list of ops (L1) |
| `ui.patch` | L2 later |
| `bind.upsert` | name → topic/path (L1/L2) |

### 5.4 Large payload policy

1. If payload ≤ threshold (e.g. 4–16 KiB): inline MQTT binary/base64  
2. Else: MQTT carries metadata + `uri`; device HTTP GETs  
3. Always prefer **damage rects** over full 320×240 frames  
4. Chunking over MQTT only if HTTP unavailable; keep as escape hatch

### 5.5 QoS / reliability

- Commands that mutate screen: QoS 1, optional `ack` with `id`
- Telemetry: QoS 0
- Status retained for discovery
- Host should tolerate dupes (idempotent `id` or full-rect replace)

## 6. Raster pipeline (v1 core)

```
Host framebuffer damage
    → crop rect(s)
    → optional color transform (RGB565 / plane split / YUV)
    → custom filter (delta-x, delta-y) + RLE (+ later Huffman)
    → package envelope (inline or uri)
        → device decode into strip/line scratch (not full FB)
        → esp_lcd draw_bitmap window (HAL byteswap) + SPI DMA
```

Map onto invaders HAL:

| wl-display op | invaders-style sink |
|---------------|---------------------|
| `display.clear` / solid | `hal_display_fill_rect` full panel |
| raw/compressed rect | decode → strip buffer(s) → `draw_bitmap(x,y,x+w,y+h)` |
| full test pattern | `hal_display_blit_rows` / lcdtest BMP technique |

### 6.1 Partial updates

- Host coalesces dirty regions (max N rects/frame or merge)
- Device applies in order; **no full RGB565 FB** on classic CYD (150 KiB)
- Invaders proved partial SPI bands work well; here bands come from **network rects**, not 1bpp VRAM diff
- Keep dual strip buffers (~`PANEL_W * N_rows`) like `cyd_display.c` for DMA overlap

### 6.2 Memory stances (this board)

| Mode | FB | When |
|------|----|------|
| **Direct / strip** | line or N-row strips only | **v1 default** (ESP32-WROOM CYD, no PSRAM) |
| **Single FB** | one RGB565 150 KiB | only if we move to S3+PSRAM or prove heap headroom after Wi-Fi+MQTT |
| **Double FB** | two × 150 KiB | **out** on this silicon |

Wi-Fi + MQTT + TLS will eat heap invaders never needed — strip path is mandatory until measured otherwise.

### 6.3 Codec strategy (from goal.md)

**Custom path (explicit project goal):**

- Stage A: vertical delta (line vs previous), horizontal delta within line  
- Stage B: RLE on deltas  
- Experiments: per-plane, YUV, fixed Huffman  
- Compare vs: JPEG (block, poor tiny partials), QOI, raw RLE RGB565, LZ4 if RAM allows  

**v1 engineering plan:**

1. Implement **raw RGB565 rect** (correctness baseline)  
2. Implement **delta+RLE** decoder on device + encoder on host (Python ref)  
3. Benchmark vs plain RLE, **mini-qoi**, **TJpgDec** (photos) — see `docs/research-codecs.md`  
4. Keep `fmt`/`enc` enums so wire format stays stable  

**Research rank (live partial UI):** delta_rle > raw > mini-qoi > byte-wrap LZ4/Heatshrink > JPEG(full) ≫ PNG/video.

## 7. Stack recommendation (firmware)

**Revised after invaders review — match the stack that already lights this glass.**

| Choice | Recommendation | Rationale |
|--------|-----------------|-----------|
| Build | **ESP-IDF** + root `Makefile` (invaders style) | Same board, same tooling, host-testable pure C |
| Framework | IDF 5.x, `esp_lcd` ST7789 vendor panel | Already verified; avoid TFT_eSPI pin folklore |
| Display | Port/adapt `hal_display_*` from invaders `components/board` | fill_rect / strip blit / byteswap known-good |
| MQTT | **esp-mqtt** (IDF component) | Buffer control, QoS, event loop fit FreeRTOS |
| HTTP | `esp_http_client` | Asset pull |
| Envelope | Start **binary or CBOR**; JSON only for debug host | Heap after Wi-Fi is scarce |
| Codec core | **Pure C** in `components/codec` — gcc host tests + IDF | Same portability rule as invaders `machine` |
| UI lib | **None in v1** | Thin client; no LVGL |

**Board profile v1 (frozen candidate):**

```text
slug:     esp32-2432s028r
mcu:      ESP32-WROOM
panel:    ST7789 320x240 landscape
psram:    no
touch:    XPT2046 optional (out of v1 scope unless needed)
manual:   copy or submodule docs/boards/esp32-2432s028r from invaders
```

**Portability rule (steal from invaders PLAN):**  
`codec` + protocol parsers compile on **Linux gcc and IDF**. Wi-Fi, MQTT, SPI, NVS live in `board` / `main` only.

## 8. v1 scope (proposed freeze)

### In

- Device: Wi-Fi + MQTT connect, status/telemetry  
- `display.clear`, `raster.rect` with `enc=raw_rgb565` and `enc=delta_rle_v1`  
- Inline payloads + HTTP `uri` fetch for larger rects  
- Direct-to-TFT path with line/scratch buffer  
- Host Python tool: send clear + rects, encode delta_rle_v1  
- Docs: contract + memory budget + bench notes  

### Out

- Touch UI, L2 widgets, LVGL  
- Full L1 command language (maybe 2–3 ops max if needed)  
- Image sequence / video codec (design spike OK, not ship)  
- OTA (nice-to-have once baseline works)  
- Encryption beyond what broker/TLS offers (TLS optional phase 1.1)

### Success criteria

1. End-to-end dirty rect appears on panel < target latency (define: e.g. p95 < 200 ms LAN)  
2. delta_rle_v1 beats raw on UI-like frames; documented vs JPEG for partials  
3. Peak RAM documented; runs on **no-PSRAM** profile  
4. Contract versioned; second encoder can be added without breaking device

## 9. v1.5 / v2 spikes (backlog)

- Draw command batch + groups  
- MQTT value bindings for text/gauges  
- Sequence/delta frames (P-frames between key rects)  
- Huffman / YUV plane modes  
- PSRAM FB path + simple on-device widgets  
- Touch events upward  

## 10. Repo layout (proposed — host-TDD + IDF)

```
PLAN.md
goal.md
docs/                      architecture, research-*, contract/, boards/, benches/
components/
  render/                  pure apply clear/rect → hal_display ops
  codec/                   raw + delta_rle_v1 (no IDF includes)
  contract/                envelope parse + dispatch (no IDF)
  board/                   hal_display.h + esp32 SPI impl (and/or host fake)
  net/                     mqtt/http adapters (IDF only; thin)
main/                      wire adapters
host/
  common/test_assert.h
  fake_display/            in-memory panel backend
  render/  codec/  contract/   gcc test binaries (invaders style)
  tools/                   Python inject / corpus / MQTT
testdata/                  goldens for codec + contract + render snapshots
Makefile                   make test | test-codec | build | flash …
sdkconfig.defaults*
```

**Test pyramid:** many host unit/stream tests → fewer contract→fake_display paths → few device visual/MQTT checks.

## 11. Decisions (see root `PLAN.md` for status)

| ID | Topic | Choice |
|----|-------|--------|
| W1 | Board | CYD ST7789 primary; **S3+PSRAM = profile #2** (doc only) |
| W2 | Stack | **ESP-IDF** + esp_lcd + esp-mqtt |
| W3/W13 | HAL | Copy minimal; **shared header + two backends** (no vtable) |
| W4 | Touch v1 | **No** |
| W5 | Language | **Pure L0** + clear only |
| W6 | Codecs | raw + **delta_rle_v1** |
| W7 | Bulk | **Inline MQTT only** first; HTTP later |
| W8 | Process | **PLAN.md** |
| W9 | Envelope | **Custom binary header** (not CBOR v1) |
| W10 | TLS | **No** day one (LAN) |
| W11 | Buffers | Raise mqtt buf; no retain rasters |
| W12 | TDD | Host-first + fake_display |

## 12. Next working sessions (suggested)

1. Freeze W1–W3 + v1 scope in a root `PLAN.md`  
2. Bring board manual + minimal `hal_display` (clear + fill_rect + rect blit)  
3. Normative `docs/contract/cmd-v1.md`  
4. `components/codec` raw + `delta_rle_v1` with host tests  
5. Wi-Fi + esp-mqtt hello + one inline rect E2E  
6. HTTP uri path + bench corpus vs JPEG/QOI  

---

*This file is a draft for discussion, not a freeze.*


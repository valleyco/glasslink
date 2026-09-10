# ESP32 Wireless Display — living plan

**This file:** `/home/davidl/Projects/esp32-wl-display/PLAN.md` (repo root).

Work it interactively. **Do not start a step until we agree it.**
After each discussion, update *Decisions* and the step status.

Statuses: `todo` · `discuss` · `agreed` · `in progress` · `done`

Related:

- Product intent: [`goal.md`](goal.md)
- Architecture draft: [`docs/architecture-draft.md`](docs/architecture-draft.md)
- Hardware/HAL baseline: `../esp32-invaders` (CYD ST7789 bring-up)
- Agent pack: [`AGENTS.md`](AGENTS.md), [`.cursor/`](.cursor/) (rules / commands / skill; mirror of zigbee process)

---

## High-level design (target)

A **wireless thin-client display** on the same glass as invaders:

```
  HOST (Python tools + later scene composer)
       │  MQTT cmd / bind / small rects
       │  HTTP GET large assets (optional)
       ▼
  DEVICE (ESP-IDF firmware)
       net (Wi-Fi) → mqtt → contract dispatch
                              ├─ codec (pure C) → RGB565 strips
                              └─ board HAL → ST7789 SPI
```

**Display languages (layers, not forks):**

| Layer | Name | v1 |
|-------|------|----|
| L0 | Raster rect (raw + compressed) | **yes** |
| L1 | Draw commands + groups + value binds | later |
| L2 | High-level UI (panels/gauges; maybe LVGL) | later |

**Portability rule (from invaders):**  
`codec`, `contract`, fake display, and most policy compile on **Linux gcc and IDF**.  
Wi-Fi, MQTT, SPI, NVS live only in thin adapters under `board` / `main` / `net`.

**Memory rule (this silicon):**  
Classic ESP32-WROOM, **no PSRAM**, full RGB565 frame ≈ **150 KiB**.  
v1 = **direct-to-panel strip/rect buffers only** — no full FB, no double FB.

**TDD rule (W12):**  
**Red → green → refactor on the host first.** Almost all logic is host-tested.  
Hardware/MQTT are integration shells over already-green pure cores.  
No production feature lands without a failing host test written first (except pure wiring probes explicitly marked “manual QA”).

---

## Host-first architecture (testability)

```
                    ┌──────────────────────────────────────────┐
  HOST TESTS        │  host/codec  host/contract  host/render  │
  (gcc, no IDF)     │  fake_display · golden bitstreams        │
                    │  make test  /  make test-codec …         │
                    └──────────────────┬───────────────────────┘
                                       │ same C sources
                    ┌──────────────────┴───────────────────────┐
  HOST TOOLS        │  host/tools (Python): encode, MQTT inject│
                    │  benches, corpus — may call C via CLI/.so│
                    └──────────────────┬───────────────────────┘
                                       │ same C sources
                    ┌──────────────────┴───────────────────────┐
  FIRMWARE          │  main + board + net adapters             │
                    │  SPI ST7789, esp-mqtt, http_client       │
                    └──────────────────────────────────────────┘
```

### What is host-testable vs device-only

| Module | Host TDD? | How |
|--------|-----------|-----|
| `codec` encode/decode | **Yes — primary** | gcc + golden vectors; round-trip; streaming API |
| `contract` parse/dispatch | **Yes** | feed byte envelopes → assert routed ops / errors |
| Render policy (apply clear/rect to FB) | **Yes** | **fake_display**: in-memory RGB565 panel; pixel asserts |
| Dirty/coalesce helpers (host side later) | **Yes** | pure C or Python |
| delta_rle bit layout / RLE edge cases | **Yes** | table-driven tests |
| Bench metrics (size, encode µs) | **Yes** | host benches; device decode µs optional later |
| ST7789 init / SPI / byteswap on wire | **Device** | lcdtest / visual probe |
| Wi-Fi join | **Device** | + optional host mock only for state machine if extracted |
| esp-mqtt I/O | **Device** | contract parser still host-tested with captured payloads |
| HTTP fetch | **Device** + host with local http.server for tools | pure URL/policy host-tested |
| Heap under Wi-Fi | **Device measure** | not unit-tested |

**Fake display (key enabler):**  
`hal_display_*` has a **host backend** (`components/board` split or `host/fake_display`) implementing fill/blit into `uint16_t panel[320*240]`. Firmware uses SPI backend. Same render/codec call path.

---

## Decisions

| ID | Topic | Choice | Status |
|----|-------|--------|--------|
| **W1** | First board | **Primary:** ESP32-2432S028R CYD V3 — ST7789, 320×240, WROOM, 4 MB flash, **no PSRAM**. **Profile #2 (documented only for v1):** ESP32-S3 + PSRAM capable panel — same contract/`display_profile`; optional full-FB path later. No S3 hardware required to proceed. | **agreed** |
| **W2** | Firmware stack | **ESP-IDF** + Make (invaders-style), `esp_lcd` ST7789, **esp-mqtt**. HTTP client **not** in first E2E (see W7). Not Arduino/PlatformIO for v1. | **agreed** |
| **W3** | Display HAL reuse | **Copy minimal display-only** HAL + provenance note (“from invaders @ date”). Shared `hal_display.h`; **two link backends** (SPI device / host fake) — **no vtable**. Submodule later if drift hurts. | **agreed** |
| **W4** | Touch in v1 | **No** | **agreed** |
| **W5** | v1 display language | **Simplest: pure L0** + `display.clear` only. No fill/text draw ops in v1. L1/L2 names may appear in docs as reserved; not implemented. | **agreed** |
| **W6** | v1 codecs | `raw_rgb565` baseline + **`delta_rle_v1`**. TDD order: raw → RLE → delta+RLE. Bench vs QOI/JPEG later. | **agreed** |
| **W7** | Large payloads | **Simplest first: MQTT inline only.** Raise esp-mqtt buffer; stay under `inline_max`. **HTTP URI pull = later phase** (still in architecture; not first E2E). No MQTT chunking. | **agreed** |
| **W8** | Process | This **PLAN.md** interactive step discipline. | **agreed** |
| **W12** | **TDD / host tests** | **Host-first TDD**: pure C + gcc; fake_display; red→green→refactor. HAL port may use characterization + lcdtest. Device = integration after green. | **agreed** |
| **W9** | Envelope format | **Simplest: custom binary L0 header** + raw payload bytes (magic, ver, type, id, geom, enc, len, …). **No CBOR/JSON on device v1.** Host tools may dump hex/JSON for humans. Pixels never base64. Evolve to CBOR when L1/bind need it. | **agreed** |
| **W10** | TLS day one | **No** — LAN trusted network only until rect path works. | **agreed** |
| **W11** | Broker / buffers | LAN Mosquitto OK. **Raise esp-mqtt buffer** (e.g. 8 KiB); `inline_max` < buffer. Never retain rasters. | **agreed** |
| **W13** | HAL test seam | **Shared `hal_display.h` + two `.c` backends** (link-time choice). Not vtable/function-pointer inject. | **agreed** |
| **W14** | Post-v1 order | **A → D → (B or C):** (A) product polish, (D) thin Python scene/host composer, then either **B-http** (bigger pictures) or **L1 start** (fill/text + later binds) — pick at Step 10. No codec-v2 / LVGL until scheduled. | **agreed** |
| **W15** | Step 10 path | **B-http**: `FLAG_URI` on `raster.rect`; MQTT carries URL; device `esp_http_client` GET → decode → blit. Cap body (no full-FB raw). L1 deferred. | **agreed** |
| **W16** | Early L1 slice | **fill_rect** + **draw.text** (device 5×7 ASCII bitmap, fg color, no binds yet). No draw.batch / groups / LVGL. Binds = B2 later. | **agreed** |
| **W17** | Value binds | Hybrid: **bind.define** (geometry once) + live UTF-8 on `wd/{id}/bind/{slot}/set` (and cmd `bind.set`). Erase via stored bg. Max 8 slots. No CBOR. | **agreed** |
| **W18** | Heap / fragmentation | Step **13**: no residual malloc in delta decode; static HTTP RX pool; `heap_free`/`heap_largest` on status. | **done** |
| **W19** | Showcase demo | After feature set is in place (not before): one **full** host-driven demo that exercises the product end-to-end on glass (and SDL). Builds on `wd_demo` / `wd_scene` — not a new stack. | **agreed** |
| **W20** | L1 batch + groups (B1) | **draw.batch** + **group** slots (define/draw). No CBOR/LVGL; fill+text sub-ops only. | **done** |
| **W21** | Delta codec exit | Lab extract (Step 16) + **removed from product** (Step 17): wire `enc=0` raw only. | **done** |

---

## Current snapshot

Steps **0–13, 15–17 done**. Flash deferred for glass QA.
**Next:** glass QA when board free; **Step 14 showcase** when ready; other backlog.


---

## Steps (TDD-ordered)

### Step 0 — Freeze intent, board, stack, TDD
**Status:** `done` (2026-09-09)  
**Agreed:** W1–W13 (see Decisions). Simplicity: L0-only, binary cmd header, inline MQTT, dual-backend HAL, S3 profile documented.

Deliverables:

- [x] `goal.md`, architecture, research briefs
- [x] `PLAN.md` with TDD / host-test matrix + decision freeze

---

### Step 1 — Host test harness + fake_display + render core
**Status:** `done` (2026-09-09) · **Depends on:** Step 0  
**Note:** `hal_display.h` + host fake backend; `render_clear` / `fill_rect` / `blit_rect`; `make test-render` — 38 asserts green. No IDF.

```
host/common/test_assert.h
host/fake_display/                 # implements hal_display_* + inspect helpers
components/board/include/hal_display.h
components/render/
host/render/test_render_basic.c
Makefile → make test / test-render
```

---

### Step 2 — `components/codec` TDD + corpus harness design
**Status:** `done` (2a+2b) · **Depends on:** Step 1  
**Design:** [`docs/codec-harness.md`](docs/codec-harness.md) — Track A correctness vs Track B bench/tune.

#### Step 2a — Correctness (blocks pipeline)
**Status:** `done` · Host green: raw 8 + delta_rle 423 asserts. Spec: [`docs/contract/delta_rle_v1.md`](docs/contract/delta_rle_v1.md).

| Test group | Asserts |
|------------|---------|
| raw round-trip | pixels in = out |
| delta_rle_dev | round-trip + goldens (one initial profile) |
| RLE / delta edges | runs, odd sizes, wrap |
| streaming | row callback, no full FB |
| reject | truncated / bad streams |
| fuzz | N random small rects, fixed seeds |

```
components/codec/
host/codec/test_*.c
testdata/codec/golden/
make test-codec
```

**Done when:** `make test-codec` green; decode API usable from render.

#### Step 2b — Corpus generators + bench MVP (tune foundation)
**Status:** `done`

- Synthetic classes in `host/codec/gen/` (solid, runs, grid, gradients, ui_*, sparse, noise, …)  
- `make bench-codec` → ratio / enc_us / dec_us / expand; writes [`docs/benches/codec-bench-mvp.md`](docs/benches/codec-bench-mvp.md)  
- Manifest: [`testdata/corpus/manifest.json`](testdata/corpus/manifest.json)  
- First numbers (host): delta geo-mean **~37×** on ui/sparse/solid/runs; noise/grid often expand → prefer raw  
- Encoder **profile knobs** / `TUNE=sweep` still deferred to Step 7

**Done when:** bench runs on core set; not a full freeze of delta_rle_v1 yet. ✓

#### Step 7 — Full tune + freeze (see below)
Kill/keep criteria, Pareto, lock `delta_rle_v1` bitstreams.

---

### Step 3 — Contract parse/dispatch TDD (binary v1)
**Status:** `done` · **Depends on:** Step 0; enums from Step 2  
**Spec:** [`docs/contract/cmd-v1.md`](docs/contract/cmd-v1.md) — 28-byte LE header, magic `WLD1`.

- Types: `display.clear`, `raster.rect` (inline only)
- `components/contract` parse/pack + apply (row-stream decode → blit)
- Host: parse rejects + apply goldens on fake_display (raw + delta_rle)
- URI flag rejected (W7)

```
make test-contract   # 32 parse + 52 apply asserts
```

**Done when:** docs + `make test-contract` green (no MQTT). ✓

---

### Step 4 — IDF skeleton + real ST7789 HAL (integration)
**Status:** `done` (build green; device eyeball pending) · **Depends on:** Step 1 interface stable  

- SPI backend: `components/board/src/cyd_display_spi.c` (from invaders; see `PROVENANCE.md`)
- Board notes: [`docs/boards/cyd-2432s028r.md`](docs/boards/cyd-2432s028r.md)
- IDF components: board / render / codec / contract + `main`
- `make build-esp32` / `make build-lcd-smoke` → bins OK on host IDF 6.1
- Flash: `make flash` or `make flash-lcd-smoke` then `make monitor` (`PORT=/dev/ttyUSB0`)

**Done when:** glass matches host fake snapshot of same ops (eyeball OK).  
**Manual QA:** bars (default) or alternating render/contract patterns (lcd-smoke).

---

### Step 5 — Wi-Fi + MQTT adapter (thin, inline only)
**Status:** `done` (host loopback + device skeleton; glass E2E pending flash) · **Depends on:** Steps 3–4  

- Broker: **local Mosquitto** `127.0.0.1:1883` (`allow_anonymous`, LAN listener)
- Topics: [`docs/contract/mqtt-topics-v1.md`](docs/contract/mqtt-topics-v1.md)
- Host: `tools/wd_mqtt.py` inject/sim/loopback; `host/mqtt/apply_bin` → fake_display
- `make mqtt-loopback` green (clear via broker → apply)
- Device: `components/net` (`wd_mqtt_start` → `contract_dispatch`); `main_mqtt.c` + Kconfig placeholders
- Build: `idf.py -D MQTT_MAIN=1 …` (no flash yet)

**Done when:** broker message clears/draws panel; host tests still green.  
**Remaining:** flash + real Wi-Fi SSID/broker URI on LAN.

---

### Step 6 — L0 E2E + host inject tools (split host / device)

Rationale (2026-09-09): finish everything host-provable before flash. Wi‑Fi/SPI are the unknowns; broker→contract→pixels is not.

#### Step 6h — Host MQTT L0 complete (no flash)
**Status:** `done` (2026-09-09) · **Depends on:** Steps 2–5  

| Deliverable | Notes |
|-------------|--------|
| Loopback: clear + **raw rect** + **delta rect** + checker | Broker → `apply_bin` → fake_display pixel expects ✓ |
| URI flag reject + `inline_max` inject guard | W7 / mqtt-topics ✓ |
| Inject: `--pattern` / `--rgb-file` / `--payload-file` | encode via `encode_rect` ✓ |
| `make mqtt-loopback` green | gate for host MQTT path ✓ |

**Done when:** host can exercise the full L0 cmd path over MQTT without a device. ✓

#### Step 6s — Host CYD SDL simulator (visible)
**Status:** `done` (2026-09-09) · **Depends on:** Steps 1, 3, 6h  
Invaders-style `host/sim/wd-sim`: SDL2 window over `fake_display` 320×240 RGB565 (scale 2). Same contract/codec/render path — no IDF, no touch.

| Mode | What |
|------|------|
| `make sim` / `--demo` | Scripted clear + raw/delta/bars ✓ |
| `--apply file.bin…` | Replay packed envelopes ✓ |
| `make sim-mqtt` / `wd_mqtt.py visual` | MQTT → stdin framing → window ✓ |

**Done when:** demo + MQTT visual path work on Linux (`libsdl2-dev`). ✓

#### Step 6d — Device glass E2E
**Status:** `done` (2026-09-09) · **Depends on:** Step 6h + 4–5  
- Flash: `make build-mqtt && make flash-mqtt` then `make flash-nvs` (secrets in gitignored `tools/nvs.csv`)
- NVS namespace `wd` via `wd_cfg_*` / `wd_net_start` in `components/wd_net`
- Eyeball OK: blue clear + red rect via MQTT inject
- Boot heap band: ~145 KiB DRAM free at init
- **Still no HTTP** → B-http if inline forced

**Done when:** LAN dirty rects reliable on glass; heap noted. ✓

---

### Step 7 — Compression tune, compare, freeze v1
**Status:** `done` (2026-09-09) · **Depends on:** Step 2a–2b  

**Freeze:** KEEP `raw_rgb565` + `delta_rle_v1` (v_then_h + byte RLE). Primary geo-mean **~37.7×** on UI/sparse/solid/runs. Doc: [`docs/benches/codec-v1.md`](docs/benches/codec-v1.md).  
**Host policy:** `codec_encode_auto` / `--enc auto` (noisy→raw; else raw if `comp ≥ 0.98×raw`).  
**Deferred:** QOI/JPEG/plain-RLE/predict knobs → backlog / `delta_rle_v2` host experiments.

**Done when:** `docs/benches/codec-v1.md` has chosen profile + tables + keep/kill decision. ✓

---

### Step 8 — Product polish (theme A)
**Status:** `done` (2026-09-09) · **Depends on:** Steps 0–7 · **Decision:** W14  
Make demos and day-to-day MQTT use trustworthy. Still L0-only on device.

#### Step 8a — Readable host demo text
**Status:** `done`  
Fix **B-demo-text**: replace Pillow `load_default` banners with a readable embedded bitmap (or TTF) font → RGB565 L0 rects. No device font / L1 yet.

#### Step 8b — Ack / status / LWT hygiene
**Status:** `done`  
Device already publishes `wd/{id}/ack|status|lwt`. Harden: ack includes `seq`/`id`/`rc`; status includes `device_id` + `inline_max`; document topics; host inject/demo can optionally wait for ack. Rely on esp-mqtt auto-reconnect; republish online+status on `CONNECTED` (already).

#### Step 8c — Device-id story
**Status:** `done`  
**Freeze:** configured name via NVS/`CONFIG_WD_DEVICE_ID` (e.g. `cyd1`) — **not** MAC suffix for v1. Document in README + open questions closed. Env `WD_DEVICE` for host tools.

**Done when:** B-demo-text resolved or closed; topic/ack contract noted in docs; device-id decision written; host can observe ack on inject.

---

### Step 9 — Thin scene host (theme D)
**Status:** `done` (2026-09-09) · **Depends on:** Step 8  
Python composer: dirty rects / scene steps, auto encode (`codec_encode_auto` / CLI), multi-device publish. Device stays dumb L0. Builds on `wd_demo.py` / `wd_mqtt.py` — not a full UI toolkit.

**Done when:** `tools/` can drive a multi-rect “scene” from a small script/YAML without hand-packing each inject; documented in README.

**Delivered:** `tools/wd_scene.py` + `tools/scenes/hello.yaml`; `make scene`. ✓

---

### Step 10 — Bigger pictures via HTTP (theme B)
**Status:** `done` (2026-09-09) · **Depends on:** Step 9 · **Picked:** B-http / **W15**

MQTT `raster.rect` + `FLAG_URI` → device HTTP GET → decode → blit. Body capped (`http_max` 65536). L1 (theme C) remains backlog.

**Delivered:** fetch hook + `wd_http`; host URI tests; `asset` / `--uri` / `http_rect.yaml`; docs. Host tests + IDF build green. ✓

---

### Step 11 — Early L1: fill_rect + text (theme C)
**Status:** `done` (2026-09-10) · **Depends on:** Step 10 · **Decision:** W16

| Op | Type | Notes |
|----|------|-------|
| `display.fill_rect` | `0x03` | x,y,w,h + `color`; empty payload |
| `draw.text` | `0x04` | x,y + fg `color`; UTF-8 payload (≤64); device 5×7 font |

Host TDD first. No value binds (B2), no groups/batch, no LVGL.

**Delivered:** contract `0x03`/`0x04`, 5×7 font in render, inject/scene `fill`+`text`, `l1_hello.yaml`. ✓

---

### Step 12 — MQTT value binds → text slots (B2)
**Status:** `done` (2026-09-10) · **Depends on:** Step 11 · **Decision:** W17

- `0x05` `bind.define` — slot id, x,y, fg, scale, max_chars, bg+optional initial text
- `0x06` `bind.set` — slot id + UTF-8 value (cmd path)
- MQTT `wd/{device}/bind/{slot}/set` — UTF-8 value (live path)
- Host-testable `components/bind`; erase bg then redraw 5×7 text

**Delivered:** bind component + `0x05`/`0x06` + MQTT `bind/+/set`; tools/scene. ✓

---

### Step 13 — Heap / fragmentation harden (B-mem)
**Status:** `done` · **Depends on:** Step 12 · **Decision:** W18  

| Slice | Intent | Result |
|-------|--------|--------|
| 13a | `delta_rle_decode_rows` without `malloc(w*h*2)` | Stream RLE→row predict→callback; stack 2×`CODEC_MAX_WIDTH`. Host +537 asserts. |
| 13b | Fixed HTTP body buffer | Static `WD_HTTP_MAX_BODY` pool + `wd_http_release` / `contract_set_fetch_release` |
| 13c | `status` heap fields | `heap_free`, `heap_largest` on retained status (measure after flash) |

**Done when:** host tests prove decode path peak RAM bounded; docs updated; device status exposes heap stats. ✓ (glass measure deferred)

---

### Step 14 — Showcase demo (B-showcase)
**Status:** `todo` · **Depends on:** feature backlog largely done (glass QA + at least L0/L1/binds/HTTP stable) · **Decision:** W19  
**Do not start until explicit `go`** — this is the *after we have the features* victory lap, not a substitute for missing capabilities.

**Intent:** one **fantastic** end-to-end demo script (host) that feels like a product trailer on CYD (+ SDL):

| Beat | Show |
|------|------|
| Boot / clear | Color washes, L0 confidence |
| UI chrome | `fill_rect` + `draw.text` layout |
| Live values | Bind slots ticking (clock, fake sensors, MQTT `bind/+/set`) |
| Big art | HTTP `uri` panels (auto-encoded assets) — not endless MQTT photo tiles |
| Motion | Dirty-rect animation / scene pacing that reads as intentional |
| Multi-device | Optional second id if available |

**Deliverables (when scheduled):** `tools/wd_showcase.py` and/or `tools/scenes/showcase.yaml` + short README “run this”; `make showcase` / `showcase-sim`. Prefer composing existing inject/scene/asset paths over a parallel stack.

**Done when:** one command demos the story on sim and on glass; docs point to it as the flagship path.

---

### Step 15 — L1 draw.batch + groups (B1)
**Status:** `done` · **Depends on:** Step 12 · **Decision:** W20  

| Slice | Intent | Result |
|-------|--------|--------|
| 15a | `draw.batch` (`0x07`) fill+text packed ops | Host apply + parse; caps 1024 B / 32 ops |
| 15b | Groups 0..3: `0x08` define / `0x09` draw | Static store in contract; redraw after clear |
| 15c | Tools/docs/status | inject batch/group-*; scene `batch_group.yaml`; status `batch`+`groups` |

**Done when:** host tests green; one scene draws via batch and redraws via group; IDF builds. ✓ (glass QA deferred)

---

### Step 16 — Extract `delta_rle` lab project (W21)
**Status:** `done` · **Decision:** W21  

**Lab:** [`../delta-rle-lab`](../delta-rle-lab) (`/home/davidl/Projects/delta-rle-lab`) — host `make test` / `make bench` green.  
Copied: `components/codec`, `host/codec` (+ corpus), benches, `delta_rle_v1.md`, codec-harness. Product stripped of delta in Step 17.

**Done when:** new repo builds `make test` / `make bench`; wl-display still unchanged. ✓

---

### Step 17 — Remove delta from wl-display (W21)
**Status:** `done` · **Depends on:** Step 16 · **Decision:** W21  

Product wire: **`enc=0` raw only**. Delta encode/decode/tools/docs removed; status `codecs:["raw_rgb565"]`. Lab: [`../delta-rle-lab`](../delta-rle-lab).

**Done when:** host tests green without delta suite; IDF builds; docs raw-only; lab pointer. ✓

---

### Step 7+ — Backlog (not scheduled)

| ID | Item |
|----|------|
| B1 | L1 draw.batch + groups — **done** Step 15 (W20) |
| B2 | MQTT value binds → text slots — **done** Step 12 |
| B3 | TLS / credentials story (**W10**) |
| B4 | Touch events upward (reuse invaders touch later) |
| B5 | Image sequence / P-frame compression |
| B6 | Huffman / YUV plane modes on delta_rle → **lab repo** (Step 16), not product |
| B-codec-lab | Extract delta_rle lab — **done** Step 16 → `../delta-rle-lab` |
| B-codec-drop | Remove delta from wl-display — **done** Step 17 |
| B7 | Shared HAL component repo (if copy drifts) |
| B8 | OTA |
| B9 | Home Assistant discovery flavor |
| B-http | HTTP `uri` pull — **done** Step 10-B (`FLAG_URI` / `wd_http`) |
| B-cbor | CBOR/generic envelope when L1/bind need it (replace or sit beside binary) |
| B-s3 | Build/verify profile #2 (S3+PSRAM) when hardware appears |
| B-demo-text | Demo banner font — **fixed** Step 8a (`wd_text.py`) |
| B-mem | Heap harden — **done** Step 13 (stream decode, static HTTP pool, status heap) |
| B-showcase | Full fantastic demo script — **Step 14** (W19); after features, not instead of them |

---

## Memory / heap budget (planning band)

From CYD research + invaders experience (order-of-magnitude; **measure on device**):

| Item | Ballpark |
|------|----------|
| Usable internal SRAM class | ~300 KB before app |
| Full RGB565 FB | **150 KiB — avoid** |
| Strip buffers (e.g. 2× 320×8) | ~10 KiB |
| Wi-Fi + MQTT (plaintext) | tens of KB + RX buffer we choose (e.g. 4–16 KiB) |
| TLS (if enabled) | often needs ~40 KB **contiguous** — risky on this board |
| Arduino-class reports | ~190 KB free (2.x) / ~90 KB (3.x) with UI — IDF will differ; still tight |

v1 success: steady operation **without** PSRAM and **without** full FB after Wi-Fi+MQTT.

**Step 13 (done):** decode_rows streams RLE (no residual `malloc`); HTTP URI uses a fixed static body pool; status advertises `heap_free` / `heap_largest`. Encoder scratch still heap on host. Remaining risk: 64 KiB static HTTP pool + Wi‑Fi/MQTT concurrent pressure — watch `heap_largest` on glass.

### Const → flash vs mutable → DRAM

| Kind | Placement | How |
|------|-----------|-----|
| Tables / fonts (`static const`, e.g. `FONT5X7`) | **Flash** `.rodata` (DROM) | Plain C `const` — IDF default; **no** Arduino `PROGMEM` / IDF attrs in shared code |
| HTTP body, groups, binds, SPI strips | **DRAM** `.bss` | Mutable `static` pools by design |
| Host gcc builds | process r/o / BSS | Same sources; no `ESP_PLATFORM` placement macros |

**Check after device link:** `make audit-mem` → [`tools/audit_elf_mem.py`](tools/audit_elf_mem.py) (needs `build-mqtt` map). Doc: [`docs/memory-placement.md`](docs/memory-placement.md).

---

## Working agreement

- **TDD:** failing host test before production code for pure modules (W12).
- **Host green before device** for codec, contract, render.
- One step at a time; agree → implement → mark `done`.
- No LVGL until L2 is an explicit step.
- No IDF headers inside `codec` / `contract` / `render`.
- Board gotchas stay in `docs/boards/.../BOARD.md`.
- Parallelism OK: host Steps 1–3 without hardware; Step 4 when glass is free.

### Default make targets (intent)

```
make test            # all host suites
make test-render
make test-codec
make test-contract
make bench-codec     # host benches
make build flash monitor   # IDF device (later)
```

---

## Open questions (short list)

1. Dev Wi-Fi/MQTT credentials: **NVS namespace `wd`** (agreed 2026-09-09) — load first; seed from Kconfig once; `make flash-nvs` for CSV provision. Secrets never in git.
2. Device id: **configured name** (NVS / `CONFIG_WD_DEVICE_ID`); host `WD_DEVICE` — **not** MAC suffix in v1 (Step 8c).
3. Exact binary header layout — **settled** in Step 3 / [`docs/contract/cmd-v1.md`](docs/contract/cmd-v1.md).
4. Step 10 path: **B-http** (locked). Step 11 early L1: **W16** (fill_rect + text).
5. Step 13 (W18 heap) — **done** (host + IDF build; glass heap measure when flashable).
6. Showcase demo (W19 / Step 14 / B-showcase) — **after** features; wait for explicit `go`.
7. B1 draw.batch + groups (W20 / Step 15) — **done**.
8. Delta exit (W21): Steps **16–17** **done** (lab `../delta-rle-lab`; product raw-only).

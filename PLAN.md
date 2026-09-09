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

---

## Current snapshot

Steps **0–5 host path done**. Local Mosquitto `127.0.0.1:1883` used for loopback. Device MQTT firmware builds (`MQTT_MAIN=1`); **flash E2E pending**.

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

### Step 6 — L0 E2E + host inject tools
**Status:** `in progress` (inject tool exists; device flash E2E open) · **Depends on:** Steps 2–5  
- Tool: `wd_mqtt.py inject` (raw + delta via `encode_rect`) ✓ host
- Device path on glass; heap notes
- **Still no HTTP** unless rects force it → then backlog B-http

**Done when:** LAN dirty rects reliable; heap noted.

---

### Step 7 — Compression tune, compare, freeze v1
**Status:** `todo` · **Depends on:** Step 2a–2b  
See [`docs/codec-harness.md`](docs/codec-harness.md) + [`docs/research-codecs.md`](docs/research-codecs.md).

- Knob sweeps (predict order, RLE style, planes/YUV experiments)
- Compare: raw, plain RLE, delta_rle profiles, mini-qoi, optional JPEG full-frame
- Encoder policy: auto-raw if no gain
- Primary score: geo-mean ratio on **UI/sparse** classes under scratch budget
- Freeze `delta_rle_v1` goldens + bitstream doc; archive sweep notes

**Done when:** `docs/benches/codec-v1.md` has chosen profile + tables + keep/kill decision.

---

### Step 7+ — Backlog (not scheduled)

| ID | Item |
|----|------|
| B1 | L1 draw.batch + groups |
| B2 | MQTT value binds → text slots |
| B3 | TLS / credentials story (**W10**) |
| B4 | Touch events upward (reuse invaders touch later) |
| B5 | Image sequence / P-frame compression |
| B6 | Huffman / YUV plane modes on delta_rle |
| B7 | Shared HAL component repo (if copy drifts) |
| B8 | OTA |
| B9 | Home Assistant discovery flavor |
| B-http | HTTP `uri` pull when inline_max insufficient (W7 phase 2) |
| B-cbor | CBOR/generic envelope when L1/bind need it (replace or sit beside binary) |
| B-s3 | Build/verify profile #2 (S3+PSRAM) when hardware appears |

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

1. Dev Wi-Fi/MQTT credentials: compile-time vs NVS provisioning.
2. Device id: MAC suffix vs configured name.
3. Exact binary header layout (settle in Step 3 with tests) — magic/`ver`/`type`/`id`/geom/`enc`/`len`.

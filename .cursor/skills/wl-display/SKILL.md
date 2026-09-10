---
name: wl-display
description: >-
  Architecture and frozen decisions for esp32-wl-display (CYD wireless thin-client
  display, MQTT, L0 raster, host TDD). Use when planning steps, scaffolding
  components, or checking v1 scope.
---

# Wireless ESP32 display (wl-display)

## Architecture

```
Host tools / tests (gcc + Python)
    codec | contract | render | fake_display
                │ same C
Device (ESP-IDF)
    net/mqtt → contract → codec → render → hal_display (ST7789 SPI)
```

- **Control plane:** MQTT binary commands (L0 + early L1).
- **Bulk HTTP `uri`:** Step 10-B — `FLAG_URI` + static body pool (`http_max` cap)
- **Display language:** L0 rects/clear + L1 fill/text/batch + groups (W16/W20) + binds (W17)
- **Heap:** Step 13 — stream/raw decode; static HTTP pool; status `heap_free`/`heap_largest`
- **Codec:** product **raw only** (W21); delta lab `../delta-rle-lab`
- **Showcase:** Step 14 — `tools/wd_showcase.py` / `make showcase[-sim]` (`--dry-run` OK offline)
- **Panel:** clock/weather daemon — `tools/wd_panel.py` / `make panel[-sim]` · plan `docs/plans/weather-clock-panel.md`

## Frozen decisions (see PLAN.md)

| ID | Choice |
|----|--------|
| W1 | CYD 2432S028R ST7789 primary; S3+PSRAM = profile #2 doc |
| W2 | ESP-IDF + esp-mqtt |
| W3/W13 | Copy minimal HAL; shared `hal_display.h` + two link backends |
| W4 | No touch v1 |
| W5 | Pure L0 + clear |
| W6 | raw RGB565 on product; delta_rle → lab (W21) |
| W7 | Inline MQTT first |
| W9 | Custom binary header (not CBOR) |
| W10 | No TLS (LAN); WireGuard = backlog B-wg after heap measure |
| W12 | Host-first TDD |

## Layout (target)

```
components/render|codec|contract|board|net/
host/common|fake_display|render|codec|contract|tools/
testdata/
docs/  PLAN.md  goal.md
main/
```

## Make targets (intent)

```bash
make test              # all host suites
make test-render
make test-codec
make test-contract
make build flash monitor   # after IDF skeleton
```

## Codec harness

- Design: `docs/codec-harness.md` — Track A `make test-codec` vs Track B `make bench-codec`
- Tune via **profiles/sweeps** on host; freeze one `enc` id for device (no runtime knobs on MCU)
- Corpus: synthetic classes (ui/sparse/noise/gradients) + optional real PNGs

## Out of scope for v1

LVGL/L2, touch, TLS, CBOR on device, full-frame FB on classic CYD, Arduino-first stack.

## Related trees

- HAL/board reference: `../esp32-invaders` (ST7789 bring-up)
- Process pack mirror: `../esp32-zigbee/.cursor/`

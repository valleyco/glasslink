# esp32-wl-display

Wireless **thin-client** display on an ESP32 CYD (ESP32-2432S028R, ST7789 320×240).  
A host sends **L0** commands over **MQTT** (binary envelopes): `display.clear` and raw `raster.rect` (plus L1 chrome). No touch / LVGL / TLS in v1.

Living plan: [`PLAN.md`](PLAN.md) · Intent: [`goal.md`](goal.md)

---

## Quick start (host)

```bash
# Python tools
python3 -m venv tools/.venv
tools/.venv/bin/pip install -r tools/requirements.txt

# Unit tests (gcc, no IDF)
make test

# Visible CYD simulator (SDL2 — needs libsdl2-dev)
make sim

# MQTT loopback vs local Mosquitto
make mqtt-loopback
```

Broker default: `127.0.0.1:1883` (device must use your PC’s **LAN IP**, e.g. `mqtt://192.168.0.216:1883`).

---

## Demo

**Flagship showcase** (Step 14): color washes → L1 batch/groups → live binds → HTTP URI art → motion.

```bash
./tools/wd_showcase.py --dry-run          # pack + asset only (no broker)
make showcase                             # glass (WD_DEVICE=cyd1); needs Mosquitto
make showcase-sim                         # SDL + local HTTP (URI expanded host-side)
```

Glass HTTP: device must reach this host — set `WD_HTTP_HOST` / `--http-host` to your LAN IP if auto-detect is wrong.

**Clock + weather panel** (long-running): dark instrument UI — 1 Hz clock binds, Open-Meteo or `--fake`, L1 icon groups. Plan: [`docs/plans/weather-clock-panel.md`](docs/plans/weather-clock-panel.md).

```bash
./tools/wd_panel.py --dry-run
make panel                                # glass; Open-Meteo for Rehovot, IL (default)
make panel-sim                            # SDL + fake weather (offline demo)
# override: WD_LAT/WD_LON/WD_PLACE or --lat/--lon/--place
```

**Older photo demo** (NASA JPEG tiles over MQTT):

**Pre-download originals** (JPEG in `tools/.cache/demo/` — nothing converted yet).  
**At play time** Python (Pillow) does JPEG→RGB565, tiles under `inline_max`, and orchestrates MQTT.

```bash
make demo-fetch   # optional; caches NASA PD stills only
make demo         # glass (device id from NVS, often cyd1)
make demo-sim     # SDL window (no hardware)
```

Manual:

```bash
./tools/wd_demo.py fetch
./tools/wd_demo.py --device cyd1
./tools/wd_demo.py --device sim1 --visual
```

---

## Scenes (YAML → MQTT)

Thin host composer (`tools/wd_scene.py`): multi-step L0 scenes without hand-packing injects. Ops: `clear`, `fill`, `checker`, `banner`, `image`, `sleep`. Multi-device via YAML `devices:` or repeated `--device`.

```bash
make scene                                    # tools/scenes/hello.yaml → cyd1
make scene SCENE=tools/scenes/hello.yaml WD_DEVICE=cyd1
./tools/wd_scene.py tools/scenes/hello.yaml --device cyd1
```

### Early L1 (fill + device text)

```bash
./tools/wd_scene.py tools/scenes/l1_hello.yaml --device cyd1
./tools/wd_mqtt.py inject fill --device cyd1 --x 10 --y 10 --w 100 --h 40 --color 0xF800
./tools/wd_mqtt.py inject text --device cyd1 --x 16 --y 60 --text "hello" --scale 2
```

### Batch + groups (W20)

```bash
./tools/wd_scene.py tools/scenes/batch_group.yaml --device cyd1
```

### Value binds

```bash
./tools/wd_mqtt.py inject bind-define --device cyd1 --slot 0 --x 16 --y 80 \
  --fg 0xFFFF --bg 0x0011 --scale 2 --max-chars 8 --text "0.0"
./tools/wd_mqtt.py inject bind-pub --device cyd1 --slot 0 --text "42.5"
./tools/wd_scene.py tools/scenes/bind_demo.yaml --device cyd1
```

### HTTP URI (large rects)

Encode → serve on LAN → MQTT carries URL only (`FLAG_URI`):

```bash
mkdir -p tools/.cache/http
./tools/wd_mqtt.py asset --w 160 --h 120 --pattern checker --enc raw \
  --out tools/.cache/http/panel.bin
# in another terminal (use a directory the device can GET):
python3 -m http.server 8000 --directory tools/.cache/http
# edit URL in tools/scenes/http_rect.yaml to this PC's LAN IP, then:
./tools/wd_scene.py tools/scenes/http_rect.yaml --device cyd1
```

---

## Device (CYD)

Device id is a **configured name** (NVS key `device_id` / `CONFIG_WD_DEVICE_ID`, e.g. `cyd1`) — not derived from MAC. Host tools use `--device` or env `WD_DEVICE`. Topics: [`docs/contract/topics-v1.md`](docs/contract/topics-v1.md).

```bash
# ESP-IDF env sourced (e.g. source ~/Projects/esp-idf/export.sh)
cp tools/nvs.example.csv tools/nvs.csv   # edit Wi-Fi + mqtt://<LAN-IP>:1883 + device_id
make build-mqtt
make flash-mqtt
make flash-nvs                           # credentials → NVS (not EEPROM)
```

Glass should show a blue boot bar, then green when MQTT is up. Inject:

```bash
./tools/wd_mqtt.py inject clear --device cyd1 --color 0x001F
./tools/wd_mqtt.py inject clear --device cyd1 --color 0x001F --wait-ack
./tools/wd_mqtt.py inject clear --device cyd1 --color 0xF800
./tools/wd_mqtt.py inject rect --device cyd1 --solid 0x07E0 --w 48 --h 32 --x 20 --y 40
# product: --enc raw only (delta experiments → ../delta-rle-lab)
# large photos: asset --enc raw + inject rect --uri http://…
```

---

## Layout

| Path | Role |
|------|------|
| `components/codec` | raw RGB565 only (delta → `../delta-rle-lab`) |
| `components/contract` | binary L0 parse/apply |
| `components/render` / `board` | blit + ST7789 HAL |
| `components/wd_net` | Wi-Fi + MQTT + NVS config |
| `host/` | gcc tests, fake_display, SDL sim |
| `tools/wd_mqtt.py` | inject / loopback / visual |
| `tools/wd_panel.py` | clock + weather panel daemon |
| `tools/wd_showcase.py` | Step 14 flagship demo |
| `tools/wd_demo.py` | orchestrated photo demo |
| `tools/wd_scene.py` | YAML scene composer |
| `tools/scenes/` | example scenes (`showcase.yaml`, …) |
| `docs/contract/` | wire formats + MQTT topics |

Codec notes: [`docs/benches/codec-v1.md`](docs/benches/codec-v1.md) (raw-only; lab link).  
Flash vs DRAM placement: [`docs/memory-placement.md`](docs/memory-placement.md) (`make audit-mem` after `build-mqtt`).

---

## v1 scope

**In:** L0 raw raster + clear, L1 fill/text/batch + groups, binds, MQTT inline + `FLAG_URI` HTTP, host TDD, LAN plaintext MQTT.
**Out:** CBOR, touch, TLS, LVGL, value binds (see backlog in `PLAN.md`).

---

## Bugs / known issues

| ID | Area | Notes |
|----|------|-------|
| — | — | B-mirror fixed (`mirror_x`). |

---

## License notes (demo assets)

Demo images are **fetched** (not vendored) from NASA image assets (U.S. government works, public domain). Cache keeps **JPEG originals** only; RGB565 is produced in memory at play time. See URLs in `tools/wd_demo.py`.

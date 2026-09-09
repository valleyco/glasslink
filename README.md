# esp32-wl-display

Wireless **thin-client** display on an ESP32 CYD (ESP32-2432S028R, ST7789 320×240).  
A host sends **L0** commands over **MQTT** (binary envelopes): `display.clear` and compressed/raw `raster.rect`. No touch / LVGL / TLS in v1.

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

## Device (CYD)

```bash
# ESP-IDF env sourced (e.g. source ~/Projects/esp-idf/export.sh)
cp tools/nvs.example.csv tools/nvs.csv   # edit Wi-Fi + mqtt://<LAN-IP>:1883
make build-mqtt
make flash-mqtt
make flash-nvs                           # credentials → NVS (not EEPROM)
```

Glass should show a blue boot bar, then green when MQTT is up. Inject:

```bash
./tools/wd_mqtt.py inject clear --device cyd1 --color 0xF800
./tools/wd_mqtt.py inject rect --device cyd1 --enc auto --solid 0x07E0 --w 48 --h 32 --x 20 --y 40
```

---

## Layout

| Path | Role |
|------|------|
| `components/codec` | raw + `delta_rle_v1` (frozen) |
| `components/contract` | binary L0 parse/apply |
| `components/render` / `board` | blit + ST7789 HAL |
| `components/wd_net` | Wi-Fi + MQTT + NVS config |
| `host/` | gcc tests, fake_display, SDL sim |
| `tools/wd_mqtt.py` | inject / loopback / visual |
| `tools/wd_demo.py` | orchestrated demo |
| `docs/contract/` | wire formats + MQTT topics |

Codec freeze notes: [`docs/benches/codec-v1.md`](docs/benches/codec-v1.md).

---

## v1 scope

**In:** L0 raster + clear, binary MQTT inline payloads, `raw_rgb565` + `delta_rle_v1`, host TDD, LAN plaintext MQTT.  
**Out:** HTTP URI pull, CBOR, touch, TLS, LVGL (see backlog in `PLAN.md`).

---

## Bugs / known issues

| ID | Area | Notes |
|----|------|-------|
| B-demo-text | `tools/wd_demo.py` banners | Host-drawn banner/title text (Pillow `ImageFont.load_default`) is hard to read / poorly sized on the 320×240 panel. Demo images and solid rects are fine; fix later (truetype font or bitmap glyphs), not blocking L0 path. |

---

## License notes (demo assets)

Demo images are **fetched** (not vendored) from NASA image assets (U.S. government works, public domain). Cache keeps **JPEG originals** only; RGB565 is produced in memory at play time. See URLs in `tools/wd_demo.py`.

# MQTT topics v1

Broker default for host tools: **`127.0.0.1:1883`** (local Mosquitto, anonymous LAN).

## Topics

| Topic | Dir | Retain | QoS | Payload |
|-------|-----|--------|-----|---------|
| `wd/{device_id}/cmd` | host → device | **no** | 1 | binary L0 envelope ([cmd-v1.md](cmd-v1.md)) |
| `wd/{device_id}/ack` | device → host | no | 0 | JSON `{"rc":0,"n":N}` |
| `wd/{device_id}/status` | device → host | **yes** | 1 | JSON caps |
| `wd/{device_id}/lwt` | device | **yes** | 1 | `online` / `offline` |

## Limits

| Name | Default |
|------|---------|
| `mqtt` buffer (device) | **8192** |
| `inline_max` | **6144** (< buffer) |

Never retain rasters. URI/HTTP not in v1.

## Host tools

Needs a local venv with paho-mqtt:

```bash
python3 -m venv tools/.venv
tools/.venv/bin/pip install -r tools/requirements.txt
```

```bash
# one-shot against local broker + fake_display apply
# covers: clear, raw rect, delta rect, delta checker, URI reject, inline_max guard
make mqtt-loopback

# inject
./tools/wd_mqtt.py inject clear --device dev1 --color 0xF800
./tools/wd_mqtt.py inject rect --device dev1 --x 10 --y 10 --w 16 --h 16 --solid 0x07E0
./tools/wd_mqtt.py inject rect --device dev1 --pattern checker --enc delta --w 16 --h 8
./tools/wd_mqtt.py inject rect --device dev1 --rgb-file pixels.rgb --w 8 --h 8 --enc raw --out cmd.bin

# long-running host simulator (no device flash)
./tools/wd_mqtt.py sim --device dev1
```

`inline_max` = **6144**; inject refuses oversized envelopes.

### Visible host CYD sim (Step 6s)

```bash
# scripted clear / raw / delta / bars in an SDL window
make sim
# keys: space=pause  n=step  q=quit

# live MQTT → same window
make sim-mqtt
# other terminal:
./tools/wd_mqtt.py inject clear --device sim1 --color 0xF800
./tools/wd_mqtt.py inject rect --device sim1 --pattern checker --enc delta --w 32 --h 24 --x 40 --y 40
```

Device firmware (`components/wd_net`) uses the same topics once provisioned.

### Credentials (NVS, not EEPROM)

ESP32 stores config in **NVS** flash (namespace `wd`). Do not commit secrets.

```bash
cp tools/nvs.example.csv tools/nvs.csv   # edit SSID/pass/broker
# IDF env active:
make flash-nvs                           # writes NVS @ 0x9000
make build-mqtt && make flash-mqtt       # app (seeds NVS from Kconfig if empty)
```

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

```bash
# one-shot against local broker + fake_display apply
make mqtt-loopback

# inject
./tools/wd_mqtt.py inject clear --device dev1 --color 0xF800
./tools/wd_mqtt.py inject rect --device dev1 --x 10 --y 10 --w 16 --h 16 --solid 0x07E0

# long-running host simulator (no device flash)
./tools/wd_mqtt.py sim --device dev1
```

Device firmware (`components/net`) uses the same topics once Wi-Fi is configured (Step 5 flash E2E).

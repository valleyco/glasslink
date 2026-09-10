# MQTT topics v1 (wl-display)

LAN plaintext MQTT (W10). Device id is a **configured name** (NVS / `CONFIG_WD_DEVICE_ID`), not a MAC suffix.

## Topic map

| Topic | Dir | Retain | Payload |
|-------|-----|--------|---------|
| `wd/{device_id}/cmd` | host → device | no | binary L0 envelope ([`cmd-v1.md`](cmd-v1.md)) |
| `wd/{device_id}/ack` | device → host | no | JSON: `{"rc":N,"n":bytes,"id":U,"seq":U}` after each cmd |
| `wd/{device_id}/status` | device → host | **yes** | JSON capabilities (see below) |
| `wd/{device_id}/bind/{slot}/set` | host → device | no | UTF-8 live value for text slot (0..7) |

`rc` matches `contract` status (`0` = `CONTRACT_OK`). `id`/`seq` are peeked from the cmd header (0 if truncated).

## Status JSON (retained)

```json
{
  "fw": "wl-display",
  "device_id": "cyd1",
  "disp": { "w": 320, "h": 240 },
  "inline_max": 6144,
  "http": true,
  "http_max": 65536,
  "l1": ["fill_rect", "text", "batch"],
  "bind_slots": 8,
  "groups": 4,
  "heap_free": 120000,
  "heap_largest": 90000,
  "codecs": ["raw_rgb565", "delta_rle_v1"]
}
```

`heap_free` / `heap_largest` are live DRAM stats (`esp_get_free_heap_size`, largest 8-bit capable block). Republished on every MQTT `CONNECTED` (covers reconnect).

## Host tooling

- `WD_DEVICE` / `--device` selects `{device_id}` (default often `cyd1`).
- `./tools/wd_mqtt.py inject … --wait-ack` publishes then waits for matching `seq` on `ack`.
- `./tools/wd_mqtt.py asset … --out FILE` encodes a body for HTTP serve.
- `./tools/wd_mqtt.py inject rect --uri http://HOST/FILE …` sends `FLAG_URI` cmd.

# MQTT topics v1 (wl-display)

LAN plaintext MQTT (W10). Device id is a **configured name** (NVS / `CONFIG_WD_DEVICE_ID`), not a MAC suffix.

## Topic map

| Topic | Dir | Retain | Payload |
|-------|-----|--------|---------|
| `wd/{device_id}/cmd` | host → device | no | binary L0 envelope ([`cmd-v1.md`](cmd-v1.md)) |
| `wd/{device_id}/ack` | device → host | no | JSON: `{"rc":N,"n":bytes,"id":U,"seq":U}` after each cmd |
| `wd/{device_id}/status` | device → host | **yes** | JSON capabilities (see below) |
| `wd/{device_id}/lwt` | broker/device | **yes** | `online` on connect; LWT `offline` on unclean disconnect |

`rc` matches `contract` status (`0` = `CONTRACT_OK`). `id`/`seq` are peeked from the cmd header (0 if truncated).

## Status JSON (retained)

```json
{
  "fw": "wl-display",
  "device_id": "cyd1",
  "disp": { "w": 320, "h": 240 },
  "inline_max": 6144,
  "codecs": ["raw_rgb565", "delta_rle_v1"]
}
```

Republished on every MQTT `CONNECTED` (covers reconnect).

## Host tooling

- `WD_DEVICE` / `--device` selects `{device_id}` (default often `cyd1`).
- `./tools/wd_mqtt.py inject … --wait-ack` publishes then waits for matching `seq` on `ack`.

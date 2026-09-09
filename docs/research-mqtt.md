# Research brief — MQTT remote display patterns

Status: absorbed into architecture + PLAN Step 3  
Context: ESP32 thin client; L0 raster first (unlike most OSS “MQTT displays”)

## Key insight

openHASP / Tasmota / ESPHome are mostly **on-device UI** + MQTT as property/command bus.  
This project is rarer: **remote framebuffer / dirty-rect thin client** — differentiating, fits compression + no-PSRAM.

| Tier | OSS reference | Our use |
|------|---------------|---------|
| L0 rects | HA MQTT image (full frame only); ad-hoc epaper | **v1 primary** |
| L1 draw | Tasmota `DisplayText` | envelope hook later |
| L2 widgets | openHASP jsonl + `p1b2.val` attrs | later |

## Topic consensus

- **Device-centric** tree; encode direction in path (`cmd` vs `status`/`ack`)
- **LWT/status retained**; **never retain** frame/raster payloads
- Group/broadcast optional later
- openHASP-style **attribute in topic leaf** is a good L2 bind pattern later

Proposed (aligned with architecture):

```
wd/{device_id}/lwt|status|telemetry|ack|event
wd/{device_id}/cmd
wd/{device_id}/bind/{name}/set     # v1.5+
wd/group/{group_id}/cmd            # optional
wd/broadcast/cmd                   # optional
```

## Payload reality check

| Limit | Typical |
|-------|---------|
| PubSubClient default | **256 B** (irrelevant — we use esp-mqtt) |
| esp-mqtt default buffer | **1024 B** — **raise deliberately** (e.g. 4–16 KiB) |
| openHASP jsonl practical | ~2 KiB |
| Mosquitto `message_size_limit` | default **0 = unlimited** |

**Bottleneck = device RX buffer + heap**, not broker.  
Inline threshold must track **configured `mqtt_buf`**, not Mosquitto max.

| Format | v1 lean |
|--------|---------|
| JSON + base64 pixels | **avoid** on device |
| JSON small cmds | debug/host OK |
| **CBOR** or **custom binary** | **preferred cmd envelope** |
| Raw image on bare topic | HA-style; OK only for small JPEG |

## Large assets

1. **HTTP offload** (ESPHome online_image, HA url_topic, openHASP screenshot HTTP) — **recommended**
2. Raw MQTT file bytes — small JPEG only; **don’t retain**
3. MQTT chunking — escape hatch only (≈ reinvent HTTP)
4. Flash path refs — L2 asset style later

Policy: ≤ inline_max → MQTT bytes; else `uri` + optional `sha256`/`ttl`.

## Live value binds

| Pattern | Who | When |
|---------|-----|------|
| Host pushes attrs (openHASP) | host | L2 |
| Device subscribes (ESPHome) | device | autonomous |
| **Hybrid bind table** (`value_ref`) | both | **v1.5** — matches goal.md |

v1: literals only; no bind table.

## Contract skeleton (Step 3 starting point)

**Envelope:** `ver`, `id`, `type`, `ts`, `ack`, `body` — CBOR normative lean; JSON dev twin optional.

**v1 types:**

- `display.clear` — `{ color }`
- `raster.rect` — `{ x,y,w,h, fmt, enc, data|uri, seq? }`
- `sys`/status caps — `codecs[]`, `inline_max`, `mqtt_buf`, `disp`, `http`

**Later types (define names now):** `draw.batch`, `bind.upsert`, `asset.fetch`, `ui.patch`

**QoS:** mutating cmd QoS1, not retained; telemetry QoS0; LWT/status retained.

**status example fields:** `fw`, `model`, `disp{w,h,fmt,psram}`, `mqtt_buf`, `inline_max`, `codecs`, `features`, `http`.

## Sources

openHASP MQTT/commands; Tasmota Displays; HA MQTT Image/Camera; ESPHome display + online_image + mqtt_subscribe; esp-mqtt Kconfig; Mosquitto manpage; PubSubClient defaults.

*Full agent write-up in session history; this is the durable digest.*

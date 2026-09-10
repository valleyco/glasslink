# Weather / clock panel — living plan

**File:** `docs/plans/weather-clock-panel.md`  
**Product:** esp32-wl-display (CYD 320×240, L0 raw + L1 fill/text/batch/groups + binds)  
**Status:** `done` (host path) · Eyeball: `make panel-sim` / glass when available.

Related: product [`PLAN.md`](../../PLAN.md), showcase [`tools/wd_showcase.py`](../../tools/wd_showcase.py).

---

## Intent

A **long-running host daemon** that drives one (or more) devices as a **dark clock + date + weather** instrument panel. It should look as good as L1 allows, exercise the full feature set we have, and **surface missing product capabilities** in a Gaps section (not invent them mid-demo unless scheduled in product PLAN).

Not a one-shot scene; not a replacement for Step 14 showcase.

---

## Decisions (locked 2026-09-10)

| ID | Choice |
|----|--------|
| **P1** | Host daemon over MQTT (same path as showcase-sim / glass). |
| **P2** | Deliverable names: `tools/wd_panel.py`, `make panel` / `panel-sim`, this plan file. |
| **P3** | Time: **24h**; date: **`dd/mm/yyyy`**. |
| **P4** | Weather: **Open-Meteo** (no API token). Aggressive caching; default refresh **60 min** (CLI-configurable). Never poll faster than configured; skip HTTP if cache fresh. |
| **P5** | Clock updates: host publishes **every 1 s** via bind slots (colon can blink by alternating strings). Device-side “internal clock” bind = **gap / future** — not in first cut. |
| **P6** | Look: **dark** instrument panel (navy / charcoal + amber or cool accent). Exact layout owned by implementer within beats below. |
| **P7** | Weather icons: **compose from L1 fills** only (batch / groups). No HTTP bitmaps for icons in v1 of this panel. |
| **P8** | Motion: **second tick only** (time bind). No chrome animation beyond optional blinking colon. |
| **P9** | Feature gaps discovered while building/running → recorded **in this file** (Gaps); promote to product `PLAN.md` steps only when we agree. |

---

## Weather API (Open-Meteo)

- **No key.** Example: current weather + WMO weather code + temperature.  
  `https://api.open-meteo.com/v1/forecast?latitude=…&longitude=…&current=temperature_2m,weather_code&timezone=auto`
- Host flags: `--lat` / `--lon` (defaults: document a sensible home default or require flags; prefer env `WD_LAT` / `WD_LON`).
- **`--fake`:** canned rotating conditions for offline / CI / no network (still runs clock).
- On-disk cache under `tools/.cache/panel/weather.json` with fetch timestamp; honor `--weather-min` (default **60**).
- Map WMO codes → small icon set: e.g. `clear`, `cloudy`, `overcast`, `fog`, `drizzle`, `rain`, `snow`, `thunder` (compose each from fills).

Abuse rule: one GET per refresh window per process; failures keep last good cache + show `—` / `ERR` on binds.

---

## Layout (320×240, dark) — target composition

Implementer may tweak pixels; structure is locked:

```
┌──────────────────────────────────────┐
│  (thin top bar / brand accent)       │
│                                      │
│           HH:MM:SS   (large binds)   │
│           dd/mm/yyyy (bind)          │
│                                      │
│   [icon 64×64]   23.4°C              │
│   fills/group    condition text      │
│                                      │
└──────────────────────────────────────┘
```

- **Time / date / temp / condition** → **bind slots** (erase+redraw).  
- **Static chrome** → one `draw.batch` (or clear + batch) at start / on weather chrome redraw.  
- **Icon** → `group.define` for current condition (reuse group slot 0); on condition change, redefine group 0 and redraw. Other groups free for chrome if needed (max **4** total).

Second tick: update time bind(s) only — do **not** clear the whole panel every second.

---

## Feature use matrix (must exercise)

| Feature | Use |
|---------|-----|
| `display.clear` | Boot / rare full redraw |
| `draw.batch` | Dark chrome, labels |
| `group.define` / `group.draw` | Weather icon set |
| `bind.define` / `bind/+/set` | Clock, date, temp, condition |
| L1 `fill_rect` / `draw.text` | Inside batch/groups |
| MQTT long session | Reconnect-friendly; republish chrome if needed after broker blip |
| Open-Meteo / `--fake` | Weather path |

**Out of scope for panel v1:** HTTP URI photos, NASA demo assets, WireGuard, TLS, multi-panel layouts, touch.

---

## Deliverables (when `go`)

1. `tools/wd_panel.py` — daemon: MQTT connect → paint chrome → bind loop 1 Hz → weather refresh on interval.  
2. `make panel` / `make panel-sim` (mirror showcase).  
3. Short README blurb (or section under Demo) pointing here.  
4. Update **Gaps** below with anything that hurt the look or forced hacks.  
5. Host-only sanity: `--dry-run` packs + fake weather map without broker.

Glass eyeball when board free; SDL via `panel-sim`.

---

## Gaps / maybe-missing features (living)

Record findings here while implementing or after first run. Seed list (expected):

| Gap | Why it hurts this panel | Idea |
|-----|-------------------------|------|
| G1 | Host must MQTT time every 1 s | Device **internal clock bind** (NTP/SNTP → auto text) — user interest; product step later |
| G2 | Only **4** group slots | Many icon variants force redefine-in-place; can’t keep all icons resident |
| G3 | Batch/group = **fill + text only** | No circle/arc → blocky sun/cloud; no 1px diagonals as first-class |
| G4 | 5×7 font, scale 1–2 | Large clock is scale-2 bind only (~14 px tall); limited typographic polish |
| G5 | Bind = **text only** | Can’t bind an icon id; weather icon needs group redefine |
| G6 | No partial dirty except bind bg | Icon groups start with a 64×64 bg fill to erase prior glyph |
| G7 | No anti-aliased / bitmap icon in L1 | Could add small raw RGB565 glyph atlas later (L0) if fills look too crude |
| G8 | Blinking colon via space | `"HH MM SS"` shifts glyph advance vs `:` — slight jitter |
| G9 | No `°` in 5×7 ASCII | Temp shown as `23.4C` |
| G10 | Fake demo vs API cadence | `panel-sim` uses `--weather-min 2` so icons rotate; live default remains 60 |

Promote any **G\*** into product `PLAN.md` only after explicit agree.

---

## Done when

- `panel-sim` shows dark clock ticking + date + icon/temp from fake or live weather.  
- Live path respects 60 min (configurable) cache; `--fake` needs no network.  
- Gaps section updated with real pain points from the build.  
- No new device firmware features required for panel v1 (host-only), unless we later schedule G1 etc.

---

## Open (minor — implementer default OK)

- Default lat/lon if env unset (document in `--help`; fail soft to `--fake` or require coords).  
- Exact RGB565 palette and icon pixel art.  
- One device vs `--device` repeatable (same as showcase).

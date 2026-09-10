#!/usr/bin/env python3
"""
Dark clock + date + weather instrument panel (docs/plans/weather-clock-panel.md).

Long-running MQTT host: 1 Hz time binds, hourly Open-Meteo (cached), L1 icon groups.

Examples:
  ./tools/wd_panel.py --dry-run --fake
  ./tools/wd_panel.py --device sim1 --visual --fake
  ./tools/wd_panel.py --device cyd1                 # default: Rehovot, Israel
  ./tools/wd_panel.py --device cyd1 --lat 32.08 --lon 34.78
  ./tools/wd_panel.py --fake                        # canned cycle (offline)
  make panel-sim   # SDL + fake weather
"""

from __future__ import annotations

import argparse
import json
import os
import sys
import time
import urllib.error
import urllib.parse
import urllib.request
from datetime import datetime
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
VENV_PY = Path(__file__).resolve().parent / ".venv" / "bin" / "python"
CACHE = Path(__file__).resolve().parent / ".cache" / "panel"

try:
    import paho.mqtt.client as mqtt
except ImportError:
    if VENV_PY.exists() and os.path.realpath(sys.executable) != os.path.realpath(
        VENV_PY
    ):
        os.execv(str(VENV_PY), [str(VENV_PY), str(Path(__file__).resolve()), *sys.argv[1:]])
    print("Need paho-mqtt (tools/.venv + requirements.txt).", file=sys.stderr)
    sys.exit(1)

sys.path.insert(0, str(Path(__file__).resolve().parent))
import wd_mqtt as wm  # noqa: E402

# --- palette (RGB565) ---
COL_BG = 0x1082  # dark navy
COL_BAR = 0x0410
COL_ACCENT = 0xFEA0  # amber
COL_MUTED = 0x8410
COL_TEXT = 0xFFFF
COL_TEMP = 0x07FF
COL_SUN = 0xFFE0
COL_CLOUD = 0xC618
COL_CLOUD_DK = 0x8410
COL_RAIN = 0x5D7F
COL_SNOW = 0xFFFF
COL_THUNDER = 0xFE60
COL_FOG = 0xA514

ICON_X, ICON_Y = 20, 148
ICON_S = 64

# bind slots — HH/MM/SS split so only seconds erase each tick
SLOT_HH = 0
SLOT_MM = 1
SLOT_SS = 2
SLOT_DATE = 3
SLOT_TEMP = 4
SLOT_COND = 5

# scale-2 clock geometry (glyph advance = 6*scale = 12)
TIME_Y = 48
TIME_SCALE = 2
_ADV = 6 * TIME_SCALE  # 12
TIME_X0 = 112  # HH
COLON1_X = TIME_X0 + 2 * _ADV
TIME_X1 = COLON1_X + _ADV  # MM
COLON2_X = TIME_X1 + 2 * _ADV
TIME_X2 = COLON2_X + _ADV  # SS
DATE_X, DATE_Y = 118, 88
PLACE_Y = 108  # under date

GROUP_ICON = 0

# Default place: Rehovot, Israel (override WD_LAT/WD_LON or --lat/--lon)
DEFAULT_PLACE = "Rehovot, IL"
DEFAULT_LAT = 31.8948
DEFAULT_LON = 34.8113

FAKE_CYCLE = [
    ("clear", 24.0, "CLEAR"),
    ("cloudy", 21.5, "CLOUDY"),
    ("overcast", 18.0, "OVERCAST"),
    ("fog", 12.0, "FOG"),
    ("drizzle", 15.2, "DRIZZLE"),
    ("rain", 14.0, "RAIN"),
    ("snow", -1.0, "SNOW"),
    ("thunder", 17.0, "THUNDER"),
]


def icon_ops(kind: str) -> list:
    """L1 fill-only weather glyph in ICON_* box; first fill clears bbox."""
    x, y, s = ICON_X, ICON_Y, ICON_S
    ops: list = [{"fill": {"x": x, "y": y, "w": s, "h": s, "color": COL_BG}}]
    cx, cy = x + s // 2, y + s // 2

    def f(ox: int, oy: int, w: int, h: int, c: int) -> None:
        ops.append({"fill": {"x": x + ox, "y": y + oy, "w": w, "h": h, "color": c}})

    if kind == "clear":
        f(22, 22, 20, 20, COL_SUN)
        for ox, oy, w, h in (
            (30, 6, 4, 10),
            (30, 48, 4, 10),
            (6, 30, 10, 4),
            (48, 30, 10, 4),
            (12, 12, 8, 4),
            (44, 12, 8, 4),
            (12, 48, 8, 4),
            (44, 48, 8, 4),
        ):
            f(ox, oy, w, h, COL_SUN)
    elif kind == "cloudy":
        f(8, 28, 36, 18, COL_CLOUD)
        f(28, 22, 28, 24, COL_CLOUD)
        f(18, 18, 20, 16, COL_CLOUD_DK)
    elif kind == "overcast":
        f(4, 20, 56, 14, COL_CLOUD_DK)
        f(8, 32, 48, 14, COL_CLOUD)
        f(12, 44, 40, 10, COL_CLOUD_DK)
    elif kind == "fog":
        for i, yy in enumerate((18, 28, 38, 48)):
            f(8, yy, 48 - (i % 2) * 8, 4, COL_FOG)
    elif kind in ("drizzle", "rain"):
        f(10, 14, 32, 16, COL_CLOUD)
        f(28, 10, 24, 20, COL_CLOUD)
        step = 5 if kind == "drizzle" else 7
        for i in range(5):
            f(14 + i * 8, 36, 2, step + (i % 2) * 4, COL_RAIN)
    elif kind == "snow":
        f(10, 12, 32, 16, COL_CLOUD)
        f(28, 8, 24, 20, COL_CLOUD)
        for ox, oy in ((16, 40), (28, 48), (40, 40), (22, 52), (34, 36)):
            f(ox, oy, 4, 4, COL_SNOW)
    elif kind == "thunder":
        f(10, 10, 36, 18, COL_CLOUD_DK)
        f(26, 6, 28, 22, COL_CLOUD)
        # stepped bolt
        f(30, 28, 10, 6, COL_THUNDER)
        f(26, 34, 10, 6, COL_THUNDER)
        f(32, 40, 8, 6, COL_THUNDER)
        f(28, 46, 8, 10, COL_THUNDER)
    else:
        f(20, 20, 24, 24, COL_MUTED)
    _ = (cx, cy)
    return ops


def wmo_to_kind(code: int) -> str:
    if code == 0:
        return "clear"
    if code in (1, 2):
        return "cloudy"
    if code == 3:
        return "overcast"
    if code in (45, 48):
        return "fog"
    if code in (51, 53, 55, 56, 57):
        return "drizzle"
    if code in (61, 63, 65, 66, 67, 80, 81, 82):
        return "rain"
    if code in (71, 73, 75, 77, 85, 86):
        return "snow"
    if code in (95, 96, 99):
        return "thunder"
    return "cloudy"


def kind_label(kind: str) -> str:
    return {
        "clear": "CLEAR",
        "cloudy": "CLOUDY",
        "overcast": "OVERCAST",
        "fog": "FOG",
        "drizzle": "DRIZZLE",
        "rain": "RAIN",
        "snow": "SNOW",
        "thunder": "THUNDER",
    }.get(kind, "---")[:10]


def place_display(place: str) -> str:
    """ASCII label for 5×7 (no fancy commas required)."""
    s = place.strip() or DEFAULT_PLACE
    # Prefer "Rehovot IL" style if comma present
    if "," in s:
        city, rest = s.split(",", 1)
        s = f"{city.strip()} {rest.strip()}"
    return s[:18]


def chrome_ops(place: str = DEFAULT_PLACE) -> list:
    loc = place_display(place)
    # Center-ish under clock (approx glyph width 6 at scale 1)
    loc_x = max(8, (320 - len(loc) * 6) // 2)
    return [
        {"fill": {"x": 0, "y": 0, "w": 320, "h": 240, "color": COL_BG}},
        {"fill": {"x": 0, "y": 0, "w": 320, "h": 22, "color": COL_BAR}},
        {"fill": {"x": 0, "y": 22, "w": 320, "h": 2, "color": COL_ACCENT}},
        {"text": {"x": 8, "y": 4, "color": COL_ACCENT, "scale": 1, "text": "wl-panel"}},
        {
            "text": {
                "x": COLON1_X,
                "y": TIME_Y,
                "color": COL_ACCENT,
                "scale": TIME_SCALE,
                "text": ":",
            }
        },
        {
            "text": {
                "x": COLON2_X,
                "y": TIME_Y,
                "color": COL_ACCENT,
                "scale": TIME_SCALE,
                "text": ":",
            }
        },
        {"text": {"x": loc_x, "y": PLACE_Y, "color": COL_MUTED, "scale": 1, "text": loc}},
        {"fill": {"x": 16, "y": 130, "w": 288, "h": 1, "color": COL_MUTED}},
        {"text": {"x": 110, "y": 136, "color": COL_MUTED, "scale": 1, "text": "WEATHER"}},
    ]


def publish(client, devices: list[str], payload: bytes) -> None:
    for d in devices:
        info = client.publish(wm.topic_cmd(d), payload, qos=1)
        info.wait_for_publish(timeout=5)
        if not info.is_published():
            raise TimeoutError(f"MQTT publish timeout → {d}")


def bind_set(client, devices: list[str], slot: int, text: str) -> None:
    raw = text.encode("utf-8")
    if len(raw) > wm.TEXT_MAX:
        text = text[: wm.TEXT_MAX]
    for d in devices:
        t = wm.topic_bind_set(d, slot)
        info = client.publish(t, text.encode("utf-8"), qos=1)
        info.wait_for_publish(timeout=5)
        if not info.is_published():
            raise TimeoutError(f"bind timeout → {t}")


class WeatherState:
    def __init__(self) -> None:
        self.kind = "clear"
        self.temp_c: float | None = 20.0
        self.label = "CLEAR"
        self.fake_i = 0
        self.last_fetch_mono = 0.0


def load_cache() -> dict | None:
    path = CACHE / "weather.json"
    if not path.is_file():
        return None
    try:
        return json.loads(path.read_text())
    except (OSError, json.JSONDecodeError):
        return None


def save_cache(data: dict) -> None:
    CACHE.mkdir(parents=True, exist_ok=True)
    (CACHE / "weather.json").write_text(json.dumps(data, indent=2))


def fetch_open_meteo(lat: float, lon: float) -> tuple[str, float, str]:
    q = urllib.parse.urlencode(
        {
            "latitude": f"{lat:.4f}",
            "longitude": f"{lon:.4f}",
            "current": "temperature_2m,weather_code",
            "timezone": "auto",
        }
    )
    url = f"https://api.open-meteo.com/v1/forecast?{q}"
    req = urllib.request.Request(url, headers={"User-Agent": "esp32-wl-display-panel/1.0"})
    with urllib.request.urlopen(req, timeout=20) as r:
        body = json.loads(r.read().decode())
    cur = body["current"]
    code = int(cur["weather_code"])
    temp = float(cur["temperature_2m"])
    kind = wmo_to_kind(code)
    return kind, temp, kind_label(kind)


def refresh_weather(args: argparse.Namespace, state: WeatherState, *, force: bool = False) -> bool:
    """Update state from cache/API/fake. Returns True if icon/label/temp changed."""
    interval = max(1.0, float(args.weather_min) * 60.0)
    now = time.monotonic()
    if not force and state.last_fetch_mono and (now - state.last_fetch_mono) < interval:
        return False

    prev = (state.kind, state.temp_c, state.label)

    if args.fake:
        kind, temp, label = FAKE_CYCLE[state.fake_i % len(FAKE_CYCLE)]
        state.fake_i += 1
        state.kind, state.temp_c, state.label = kind, temp, label
        state.last_fetch_mono = now
        print(f"weather fake → {label} {temp:.1f}C")
        return prev != (state.kind, state.temp_c, state.label)

    # Live: honor on-disk cache age (wall clock) before network
    cached = load_cache()
    if cached and not force:
        age = time.time() - float(cached.get("fetched_at", 0))
        if age < interval:
            state.kind = str(cached.get("kind", "cloudy"))
            state.temp_c = float(cached["temp_c"]) if cached.get("temp_c") is not None else None
            state.label = str(cached.get("label", "---"))[:10]
            state.last_fetch_mono = now
            print(f"weather cache hit ({age / 60:.0f} min old)")
            return prev != (state.kind, state.temp_c, state.label)

    try:
        kind, temp, label = fetch_open_meteo(args.lat, args.lon)
        state.kind, state.temp_c, state.label = kind, temp, label
        save_cache(
            {
                "fetched_at": time.time(),
                "kind": kind,
                "temp_c": temp,
                "label": label,
                "lat": args.lat,
                "lon": args.lon,
            }
        )
        print(f"weather fetch → {label} {temp:.1f}C ({args.place})")
    except (urllib.error.URLError, TimeoutError, KeyError, ValueError, OSError) as e:
        print(f"weather fetch failed: {e}", file=sys.stderr)
        if cached:
            state.kind = str(cached.get("kind", "cloudy"))
            state.temp_c = float(cached["temp_c"]) if cached.get("temp_c") is not None else None
            state.label = str(cached.get("label", "ERR"))[:10]
        else:
            state.label = "ERR"
            state.temp_c = None
    state.last_fetch_mono = now
    return prev != (state.kind, state.temp_c, state.label)


class Panel:
    def __init__(
        self, devices: list[str], client: mqtt.Client | None, place: str = DEFAULT_PLACE
    ):
        self.devices = devices
        self.client = client
        self.place = place
        self.seq = 1
        self._need_full = True
        self._hh = self._mm = self._ss = self._date = ""
        self._last_full_mono = 0.0
        self._last_request_mono = 0.0

    def request_full(self, reason: str = "") -> None:
        """Queue a full chrome+binds repaint (e.g. device rebooted)."""
        now = time.monotonic()
        if self._need_full and (now - self._last_request_mono) < 2.0:
            return
        self._need_full = True
        self._last_request_mono = now
        if reason:
            print(f"re-paint queued ({reason})")

    def _pub(self, payload: bytes) -> None:
        if self.client is None:
            return
        publish(self.client, self.devices, payload)
        self.seq = (self.seq + 1) & 0xFFFF

    def paint_full(self, weather: WeatherState) -> None:
        self._pub(wm.pack_clear(1, self.seq, COL_BG))
        self._pub(wm.pack_batch(1, self.seq, chrome_ops(self.place)))
        self._hh = self._mm = self._ss = self._date = ""
        self._pub(
            wm.pack_bind_define(
                SLOT_HH, self.seq, TIME_X0, TIME_Y, COL_ACCENT, COL_BG, TIME_SCALE, 2, "00"
            )
        )
        self._pub(
            wm.pack_bind_define(
                SLOT_MM, self.seq, TIME_X1, TIME_Y, COL_ACCENT, COL_BG, TIME_SCALE, 2, "00"
            )
        )
        self._pub(
            wm.pack_bind_define(
                SLOT_SS, self.seq, TIME_X2, TIME_Y, COL_ACCENT, COL_BG, TIME_SCALE, 2, "00"
            )
        )
        self._pub(
            wm.pack_bind_define(
                SLOT_DATE, self.seq, DATE_X, DATE_Y, COL_TEXT, COL_BG, 1, 10, "01/01/1970"
            )
        )
        self._pub(
            wm.pack_bind_define(
                SLOT_TEMP, self.seq, 110, 160, COL_TEMP, COL_BG, 2, 8, "--.-C"
            )
        )
        self._pub(
            wm.pack_bind_define(
                SLOT_COND, self.seq, 110, 196, COL_TEXT, COL_BG, 1, 10, "----------"
            )
        )
        self.paint_weather(weather, force_icon=True)
        self.tick_clock(force=True)
        self._need_full = False
        self._last_full_mono = time.monotonic()
        print("full paint done")

    def paint_weather(self, weather: WeatherState, *, force_icon: bool = False) -> None:
        ops = icon_ops(weather.kind)
        self._pub(wm.pack_group_define(GROUP_ICON, self.seq, ops))
        if self.client:
            temp = (
                f"{weather.temp_c:.1f}C"
                if weather.temp_c is not None
                else "--.-C"
            )
            bind_set(self.client, self.devices, SLOT_TEMP, temp[:8])
            bind_set(self.client, self.devices, SLOT_COND, weather.label[:10])
        _ = force_icon

    def tick_clock(self, *, force: bool = False) -> None:
        """Update only fields that changed (seconds each tick; date once/day)."""
        if self.client is None:
            return
        now = datetime.now()
        hh, mm, ss = now.strftime("%H"), now.strftime("%M"), now.strftime("%S")
        date = now.strftime("%d/%m/%Y")
        if force or hh != self._hh:
            bind_set(self.client, self.devices, SLOT_HH, hh)
            self._hh = hh
        if force or mm != self._mm:
            bind_set(self.client, self.devices, SLOT_MM, mm)
            self._mm = mm
        if force or ss != self._ss:
            bind_set(self.client, self.devices, SLOT_SS, ss)
            self._ss = ss
        if force or date != self._date:
            bind_set(self.client, self.devices, SLOT_DATE, date)
            self._date = date


def dry_run() -> int:
    print("== dry-run packs ==")
    msgs = [
        ("clear", wm.pack_clear(1, 1, COL_BG)),
        ("chrome", wm.pack_batch(1, 2, chrome_ops(DEFAULT_PLACE))),
        (
            "bind HH",
            wm.pack_bind_define(
                SLOT_HH, 3, TIME_X0, TIME_Y, COL_ACCENT, COL_BG, TIME_SCALE, 2, "00"
            ),
        ),
        (
            "bind SS",
            wm.pack_bind_define(
                SLOT_SS, 4, TIME_X2, TIME_Y, COL_ACCENT, COL_BG, TIME_SCALE, 2, "00"
            ),
        ),
    ]
    for kind, _, _ in FAKE_CYCLE:
        body = wm.batch_ops_bytes(icon_ops(kind))
        msgs.append((f"icon {kind}", wm.pack_group_define(0, 4, icon_ops(kind))))
        print(f"  icon {kind}: {len(icon_ops(kind))} ops, payload {len(body)} B")
    for label, payload in msgs:
        if len(payload) > wm.INLINE_MAX:
            raise SystemExit(f"{label} exceeds inline_max")
        print(f"  pack ok {label}: {len(payload)} B")
    # fake weather map
    for code in (0, 2, 3, 45, 51, 61, 71, 95, 999):
        print(f"  WMO {code} → {wmo_to_kind(code)} / {kind_label(wmo_to_kind(code))}")
    print("dry-run done.")
    return 0


def run(args: argparse.Namespace) -> int:
    if args.dry_run:
        return dry_run()

    devices = list(args.device) if args.device else [os.environ.get("WD_DEVICE", "cyd1")]
    devices = [str(d) for d in devices]

    use_fake = bool(args.fake)
    if args.lat is None:
        args.lat = DEFAULT_LAT
    if args.lon is None:
        args.lon = DEFAULT_LON
    if not getattr(args, "place", None):
        args.place = DEFAULT_PLACE
    args.fake = use_fake

    weather = WeatherState()
    refresh_weather(args, weather, force=True)

    visual_proc = None
    if args.visual:
        import subprocess

        sim_bin = ROOT / "host" / "sim" / "wd-sim"
        if not sim_bin.exists():
            subprocess.check_call(["make", "-C", str(ROOT / "host" / "sim"), "all"], cwd=ROOT)
        visual_proc = subprocess.Popen(
            [
                str(VENV_PY if VENV_PY.exists() else sys.executable),
                str(ROOT / "tools" / "wd_mqtt.py"),
                "visual",
                "--device",
                devices[0],
                "--host",
                args.host,
                "--port",
                str(args.port),
                "--scale",
                str(args.scale),
            ],
            cwd=ROOT,
        )
        time.sleep(1.0)

    panel_holder: dict = {"panel": None}

    def on_connect(client, userdata, flags, reason_code, properties=None):  # noqa: ARG001
        print(f"MQTT connected rc={reason_code}")
        for d in devices:
            client.subscribe(wm.topic_lwt(d), qos=1)
            client.subscribe(wm.topic_status(d), qos=1)
            print(f"  watch {wm.topic_lwt(d)} + status")
        p = panel_holder.get("panel")
        if p is not None:
            p.request_full("host mqtt connect")

    def on_message(client, userdata, msg):  # noqa: ARG001
        p = panel_holder.get("panel")
        if p is None:
            return
        topic = msg.topic
        payload = msg.payload
        for d in devices:
            if topic == wm.topic_lwt(d):
                text = payload.decode("utf-8", errors="replace").strip().lower()
                if text == "online":
                    p.request_full(f"{d} lwt online")
                elif text == "offline":
                    print(f"{d} offline")
                return
            if topic == wm.topic_status(d):
                # Device republishes retained status on every MQTT CONNECT (reboot)
                p.request_full(f"{d} status")
                return

    client = mqtt.Client(
        mqtt.CallbackAPIVersion.VERSION2, client_id=f"wd-panel-{os.getpid()}"
    )
    client.on_connect = on_connect
    client.on_message = on_message
    client.connect(args.host, args.port, keepalive=30)
    client.loop_start()

    panel = Panel(devices, client, place=str(args.place))
    panel_holder["panel"] = panel

    try:
        print(
            f"panel running devices={devices} place={args.place} "
            f"lat={args.lat} lon={args.lon} fake={args.fake} Ctrl+C to stop"
        )
        while True:
            if panel._need_full:
                panel.paint_full(weather)
            if refresh_weather(args, weather):
                panel.paint_weather(weather)
            panel.tick_clock()
            # Align to next wall-clock second (avoids drift / double-tick)
            delay = 1.0 - (time.time() % 1.0)
            if delay < 0.05:
                delay += 1.0
            time.sleep(delay)
    except KeyboardInterrupt:
        print("\npanel stop")
        return 0
    finally:
        client.loop_stop()
        client.disconnect()
        if visual_proc and visual_proc.poll() is None:
            visual_proc.terminate()
            try:
                visual_proc.wait(timeout=3)
            except Exception:  # noqa: BLE001
                visual_proc.kill()


def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description="wl-display clock/weather panel daemon")
    p.add_argument("--host", default=os.environ.get("MQTT_HOST", "127.0.0.1"))
    p.add_argument("--port", type=int, default=int(os.environ.get("MQTT_PORT", "1883")))
    p.add_argument("--device", action="append", help="device id (repeatable)")
    p.add_argument("--visual", action="store_true", help="SDL visual for first device")
    p.add_argument("--scale", type=int, default=2)
    p.add_argument("--dry-run", action="store_true", help="pack checks only")
    p.add_argument("--fake", action="store_true", help="canned weather (no API)")
    p.add_argument(
        "--weather-min",
        type=float,
        default=float(os.environ.get("WD_WEATHER_MIN", "60")),
        help="minutes between weather refreshes (default 60)",
    )
    p.add_argument(
        "--place",
        default=os.environ.get("WD_PLACE", DEFAULT_PLACE),
        help=f"label for logs (default {DEFAULT_PLACE})",
    )
    p.add_argument(
        "--lat",
        type=float,
        default=float(os.environ["WD_LAT"]) if os.environ.get("WD_LAT") else DEFAULT_LAT,
        help=f"Open-Meteo latitude (default {DEFAULT_LAT} = {DEFAULT_PLACE})",
    )
    p.add_argument(
        "--lon",
        type=float,
        default=float(os.environ["WD_LON"]) if os.environ.get("WD_LON") else DEFAULT_LON,
        help=f"Open-Meteo longitude (default {DEFAULT_LON} = {DEFAULT_PLACE})",
    )
    return p


def main() -> int:
    return run(build_parser().parse_args())


if __name__ == "__main__":
    sys.exit(main())

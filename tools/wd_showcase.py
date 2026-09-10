#!/usr/bin/env python3
"""
Flagship product showcase (Step 14 / W19).

Beats: color wash → L1 chrome (batch + groups) → live binds → HTTP URI art
→ dirty-rect motion → finale. Composes wd_mqtt pack/publish helpers.

Examples:
  ./tools/wd_showcase.py --dry-run              # pack + asset only (no broker)
  ./tools/wd_showcase.py --device cyd1          # glass (needs Mosquitto + HTTP reachability)
  ./tools/wd_showcase.py --device sim1 --visual # SDL (URI expanded host-side)
  make showcase / make showcase-sim
"""

from __future__ import annotations

import argparse
import http.server
import os
import socket
import socketserver
import subprocess
import sys
import threading
import time
from datetime import datetime
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
VENV_PY = Path(__file__).resolve().parent / ".venv" / "bin" / "python"
CACHE = Path(__file__).resolve().parent / ".cache" / "showcase"

try:
    import paho.mqtt.client as mqtt  # noqa: F401
except ImportError:
    if VENV_PY.exists() and os.path.realpath(sys.executable) != os.path.realpath(
        VENV_PY
    ):
        os.execv(str(VENV_PY), [str(VENV_PY), str(Path(__file__).resolve()), *sys.argv[1:]])
    print(
        f"Need paho-mqtt (pip install -r tools/requirements.txt; venv={VENV_PY}).",
        file=sys.stderr,
    )
    sys.exit(1)

sys.path.insert(0, str(Path(__file__).resolve().parent))
import wd_mqtt as wm  # noqa: E402

PANEL_W, PANEL_H = 320, 240
ART_W, ART_H = 160, 120
ART_X, ART_Y = 80, 72


def lan_ipv4() -> str:
    """Best-effort LAN address for device HTTP GET (not 127.0.0.1)."""
    env = os.environ.get("WD_HTTP_HOST", "").strip()
    if env:
        return env
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(("8.8.8.8", 80))
        ip = s.getsockname()[0]
        s.close()
        if ip and not ip.startswith("127."):
            return ip
    except OSError:
        pass
    return "127.0.0.1"


def publish(client, devices: list[str], payload: bytes) -> None:
    for d in devices:
        info = client.publish(wm.topic_cmd(d), payload, qos=1)
        info.wait_for_publish(timeout=5)
        if not info.is_published():
            raise TimeoutError(f"MQTT publish timeout → {d}")


def bind_topic(client, devices: list[str], slot: int, text: str) -> None:
    for d in devices:
        t = wm.topic_bind_set(d, slot)
        info = client.publish(t, text.encode("utf-8"), qos=1)
        info.wait_for_publish(timeout=5)
        if not info.is_published():
            raise TimeoutError(f"bind publish timeout → {t}")


class _QuietHandler(http.server.SimpleHTTPRequestHandler):
    def log_message(self, fmt: str, *args) -> None:  # noqa: A003
        pass


def start_http(directory: Path, port: int) -> tuple[socketserver.TCPServer, threading.Thread]:
    directory.mkdir(parents=True, exist_ok=True)
    handler = lambda *a, **k: _QuietHandler(*a, directory=str(directory), **k)  # noqa: E731
    httpd = socketserver.ThreadingTCPServer(("0.0.0.0", port), handler)
    httpd.daemon_threads = True
    th = threading.Thread(target=httpd.serve_forever, daemon=True)
    th.start()
    return httpd, th


def write_art_asset(path: Path) -> None:
    """Synthetic UI-ish panel (checker + color bars) — raw RGB565."""
    path.parent.mkdir(parents=True, exist_ok=True)
    # Top: horizontal runs; bottom: checker — looks intentional on 160×120.
    top = wm.rgb565_h_runs(ART_W, ART_H // 2, 0x0410, 0x0011)
    bot = wm.rgb565_checker(ART_W, ART_H - ART_H // 2, 0xFFE0, 0xF81F)
    path.write_bytes(top + bot)
    print(f"asset {path} ({path.stat().st_size} B raw)")


def beat(name: str) -> None:
    print(f"\n== {name} ==")


def run_showcase(args: argparse.Namespace) -> int:
    devices = list(args.device) if args.device else [os.environ.get("WD_DEVICE", "cyd1")]
    devices = [str(d) for d in devices]
    pause = float(args.pause)
    seq = 1
    cmd_id = 1

    CACHE.mkdir(parents=True, exist_ok=True)
    art_path = CACHE / "panel.bin"
    write_art_asset(art_path)

    http_port = int(args.http_port)
    # Glass needs LAN IP; SDL visual expands URI via host GET → prefer loopback.
    if args.http_host:
        http_host = args.http_host
    elif args.visual or any(d.startswith("sim") for d in devices):
        http_host = "127.0.0.1"
    else:
        http_host = lan_ipv4()
    art_url = f"http://{http_host}:{http_port}/panel.bin"

    # Pre-build messages for dry-run validation
    msgs: list[tuple[str, bytes]] = []

    def add(label: str, payload: bytes) -> None:
        if len(payload) > wm.INLINE_MAX and not (
            len(payload) >= wm.HDR_SIZE
            and payload[6] & wm.FLAG_URI
        ):
            # URI envelopes are small; others must fit inline.
            raise ValueError(f"{label}: envelope {len(payload)} > inline_max")
        msgs.append((label, payload))

    # --- Beat 1: color washes ---
    for color, name in (
        (0x0011, "navy"),
        (0xF800, "red"),
        (0x07E0, "green"),
        (0x001F, "blue"),
        (0x0000, "black"),
    ):
        add(f"clear {name}", wm.pack_clear(cmd_id, seq, color))
        seq += 1

    # --- Beat 2: UI chrome via batch ---
    batch_ops = [
        {"fill": {"x": 0, "y": 0, "w": 320, "h": 36, "color": 0x0410}},
        {"text": {"x": 8, "y": 10, "color": 0xFFE0, "scale": 2, "text": "wl-display"}},
        {"fill": {"x": 8, "y": 48, "w": 100, "h": 12, "color": 0x07E0}},
        {"fill": {"x": 116, "y": 48, "w": 100, "h": 12, "color": 0xF800}},
        {"fill": {"x": 224, "y": 48, "w": 88, "h": 12, "color": 0x001F}},
        {"text": {"x": 8, "y": 68, "color": 0xFFFF, "scale": 1, "text": "L1 batch chrome"}},
    ]
    add("batch chrome", wm.pack_batch(cmd_id, seq, batch_ops))
    seq += 1

    # --- Beat 3: group define → clear → redraw ---
    group_ops = [
        {"fill": {"x": 8, "y": 180, "w": 304, "h": 48, "color": 0x2104}},
        {"text": {"x": 16, "y": 194, "color": 0x07FF, "scale": 2, "text": "GROUP0"}},
    ]
    add("group define", wm.pack_group_define(0, seq, group_ops))
    seq += 1
    add("clear before group", wm.pack_clear(cmd_id, seq, 0x0000))
    seq += 1
    add("group draw", wm.pack_group_draw(0, seq))
    seq += 1

    # --- Beat 4: bind slots ---
    add(
        "bind0 define",
        wm.pack_bind_define(0, seq, 16, 100, 0xFFFF, 0x0000, 2, 10, "--:--"),
    )
    seq += 1
    add(
        "bind1 define",
        wm.pack_bind_define(1, seq, 16, 132, 0x07E0, 0x0000, 2, 10, "----"),
    )
    seq += 1

    # --- Beat 5: HTTP URI art ---
    add(
        "uri art",
        wm.pack_rect(
            cmd_id,
            seq,
            ART_X,
            ART_Y,
            ART_W,
            ART_H,
            wm.ENC_RAW,
            art_url.encode("utf-8"),
            flags=wm.FLAG_URI,
        ),
    )
    seq += 1

    # --- Beat 6: motion strips (prepared templates; positions vary at play) ---
    # Built at play time.

    # --- Beat 7: finale ---
    finale_ops = [
        {"fill": {"x": 0, "y": 0, "w": 320, "h": 240, "color": 0x0011}},
        {"text": {"x": 40, "y": 100, "color": 0xFFE0, "scale": 2, "text": "SHOWCASE"}},
        {"text": {"x": 70, "y": 140, "color": 0x07FF, "scale": 1, "text": "L0+L1+bind+HTTP"}},
    ]
    add("finale batch", wm.pack_batch(cmd_id, seq, finale_ops))
    seq += 1

    print(f"showcase devices={devices} msgs={len(msgs)} art_url={art_url}")
    for label, payload in msgs:
        print(f"  pack ok {label}: {len(payload)} B")

    if args.dry_run:
        print("dry-run done (no MQTT / HTTP / SDL).")
        return 0

    httpd = None
    visual_proc = None
    client = None
    try:
        httpd, _th = start_http(CACHE, http_port)
        print(f"HTTP serving {CACHE} on 0.0.0.0:{http_port} (URL {art_url})")

        if args.visual:
            sim_bin = ROOT / "host" / "sim" / "wd-sim"
            if not sim_bin.exists():
                subprocess.check_call(
                    ["make", "-C", str(ROOT / "host" / "sim"), "all"], cwd=ROOT
                )
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

        client = wm.mqtt_client(args.host, args.port, f"wd-showcase-{os.getpid()}")
        client.loop_start()

        # Play beats with pacing (rebuild seq for live path)
        seq = 1

        beat("1 color washes")
        for color in (0x0011, 0xF800, 0x07E0, 0x001F, 0x0000):
            publish(client, devices, wm.pack_clear(cmd_id, seq, color))
            seq += 1
            time.sleep(0.35)

        beat("2 L1 batch chrome")
        publish(client, devices, wm.pack_batch(cmd_id, seq, batch_ops))
        seq += 1
        time.sleep(pause)

        beat("3 group define / clear / redraw")
        publish(client, devices, wm.pack_group_define(0, seq, group_ops))
        seq += 1
        time.sleep(0.4)
        publish(client, devices, wm.pack_clear(cmd_id, seq, 0x0000))
        seq += 1
        time.sleep(0.25)
        publish(client, devices, wm.pack_group_draw(0, seq))
        seq += 1
        time.sleep(0.35)
        publish(client, devices, wm.pack_group_draw(0, seq))
        seq += 1
        time.sleep(pause)

        beat("4 live binds")
        # Restore chrome under binds
        publish(client, devices, wm.pack_clear(cmd_id, seq, 0x0011))
        seq += 1
        publish(
            client,
            devices,
            wm.pack_batch(
                cmd_id,
                seq,
                [
                    {"fill": {"x": 0, "y": 0, "w": 320, "h": 28, "color": 0x0410}},
                    {
                        "text": {
                            "x": 8,
                            "y": 6,
                            "color": 0xFFE0,
                            "scale": 2,
                            "text": "LIVE BINDS",
                        }
                    },
                ],
            ),
        )
        seq += 1
        publish(
            client,
            devices,
            wm.pack_bind_define(0, seq, 16, 80, 0xFFFF, 0x0011, 2, 10, "--:--"),
        )
        seq += 1
        publish(
            client,
            devices,
            wm.pack_bind_define(1, seq, 16, 120, 0x07E0, 0x0011, 2, 10, "temp"),
        )
        seq += 1
        for i in range(8):
            clock = datetime.now().strftime("%H:%M:%S")[:8]
            temp = f"{20.0 + (i % 5) * 0.7:.1f}C"
            bind_topic(client, devices, 0, clock)
            bind_topic(client, devices, 1, temp)
            print(f"  bind tick {clock} {temp}")
            time.sleep(0.45)

        beat("5 HTTP URI art")
        publish(client, devices, wm.pack_clear(cmd_id, seq, 0x0000))
        seq += 1
        publish(
            client,
            devices,
            wm.pack_text(cmd_id, seq, 8, 8, 0xFFFF, "HTTP URI pull", scale=2),
        )
        seq += 1
        publish(
            client,
            devices,
            wm.pack_rect(
                cmd_id,
                seq,
                ART_X,
                ART_Y,
                ART_W,
                ART_H,
                wm.ENC_RAW,
                art_url.encode("utf-8"),
                flags=wm.FLAG_URI,
            ),
        )
        seq += 1
        time.sleep(pause + 0.5)

        beat("6 dirty-rect motion")
        publish(client, devices, wm.pack_clear(cmd_id, seq, 0x10A2))
        seq += 1
        publish(
            client,
            devices,
            wm.pack_text(cmd_id, seq, 8, 8, 0xFFE0, "MOTION", scale=2),
        )
        seq += 1
        for i in range(24):
            x = 8 + (i * 12) % 280
            publish(
                client,
                devices,
                wm.pack_fill_rect(cmd_id, seq, x, 100, 40, 24, 0x07E0),
            )
            seq += 1
            # erase previous trail lightly
            if i > 0:
                px = 8 + ((i - 1) * 12) % 280
                publish(
                    client,
                    devices,
                    wm.pack_fill_rect(cmd_id, seq, px, 100, 40, 24, 0x10A2),
                )
                seq += 1
            time.sleep(0.06)

        beat("7 finale")
        publish(client, devices, wm.pack_batch(cmd_id, seq, finale_ops))
        seq += 1
        time.sleep(pause)

        print(f"\nshowcase done seq={seq}")
        if args.visual:
            print("SDL open — quit with q / Esc in the window.")
            if visual_proc:
                visual_proc.wait()
        return 0
    finally:
        if client:
            client.loop_stop()
            client.disconnect()
        if httpd:
            httpd.shutdown()
        if visual_proc and visual_proc.poll() is None and not args.visual:
            visual_proc.terminate()


def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description="wl-display Step 14 flagship showcase")
    p.add_argument("--host", default=os.environ.get("MQTT_HOST", "127.0.0.1"))
    p.add_argument("--port", type=int, default=int(os.environ.get("MQTT_PORT", "1883")))
    p.add_argument(
        "--device",
        action="append",
        help="device id (repeatable; default WD_DEVICE or cyd1)",
    )
    p.add_argument("--pause", type=float, default=0.9, help="seconds between major beats")
    p.add_argument(
        "--visual",
        action="store_true",
        help="start wd_mqtt.py visual (SDL) for first --device",
    )
    p.add_argument("--scale", type=int, default=2, help="SDL scale when --visual")
    p.add_argument(
        "--dry-run",
        action="store_true",
        help="build asset + pack envelopes only (no broker/HTTP/SDL)",
    )
    p.add_argument("--http-port", type=int, default=int(os.environ.get("WD_HTTP_PORT", "8765")))
    p.add_argument(
        "--http-host",
        default="",
        help="host in URI (default: 127.0.0.1 for sim/visual, else LAN IP)",
    )
    return p


def main() -> int:
    return run_showcase(build_parser().parse_args())


if __name__ == "__main__":
    sys.exit(main())

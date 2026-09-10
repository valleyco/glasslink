#!/usr/bin/env python3
"""
Orchestrated demo for esp32-wl-display.

Cache stores **original** JPEG/PNG only (no pre-converted RGB565).
At play time: Pillow → RGB565 → tile under inline_max → MQTT.

Examples:
  ./tools/wd_demo.py fetch                         # pre-download originals
  ./tools/wd_demo.py --device cyd1                 # convert on the fly + MQTT
  ./tools/wd_demo.py --device sim1 --visual         # SDL + inject
  ./tools/wd_demo.py --device cyd1 --skip-download  # cache only (no network)
"""

from __future__ import annotations

import argparse
import hashlib
import os
import struct
import subprocess
import sys
import time
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
VENV_PY = Path(__file__).resolve().parent / ".venv" / "bin" / "python"
CACHE = Path(__file__).resolve().parent / ".cache" / "demo"

# Re-exec under tools venv if deps missing
try:
    import paho.mqtt.client as mqtt  # noqa: F401
    from PIL import Image
except ImportError:
    if VENV_PY.exists() and os.path.realpath(sys.executable) != os.path.realpath(
        VENV_PY
    ):
        os.execv(str(VENV_PY), [str(VENV_PY), str(Path(__file__).resolve()), *sys.argv[1:]])
    print(
        f"Need paho-mqtt + Pillow (pip install -r tools/requirements.txt; venv={VENV_PY}).",
        file=sys.stderr,
    )
    sys.exit(1)

# Import pack/publish helpers from sibling module
sys.path.insert(0, str(Path(__file__).resolve().parent))
import wd_mqtt as wm  # noqa: E402
import wd_text as wt  # noqa: E402

PANEL_W, PANEL_H = 320, 240
INLINE_PAYLOAD = wm.INLINE_MAX - wm.HDR_SIZE

# Public domain NASA stills (cached as JPEG; RGB565 only at publish time).
ASSETS = [
    {
        "id": "nasa_earth",
        "url": "https://images-assets.nasa.gov/image/PIA18033/PIA18033~small.jpg",
        "license": "NASA public domain",
        "fit": (200, 150),
        "pos": (60, 30),
    },
    {
        "id": "nasa_mars",
        "url": "https://images-assets.nasa.gov/image/PIA04921/PIA04921~small.jpg",
        "license": "NASA public domain",
        "fit": (180, 140),
        "pos": (70, 40),
    },
]


def rgb888_to_rgb565(r: int, g: int, b: int) -> int:
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def image_to_rgb565(img: Image.Image) -> bytes:
    """RGB/RGBA PIL image → little-endian RGB565 bytes (row-major)."""
    rgba = img.convert("RGBA")
    px = rgba.load()
    w, h = rgba.size
    out = bytearray(w * h * 2)
    i = 0
    for y in range(h):
        for x in range(w):
            r, g, b, a = px[x, y]
            if a < 128:
                r = g = b = 0
            c = rgb888_to_rgb565(r, g, b)
            out[i] = c & 0xFF
            out[i + 1] = (c >> 8) & 0xFF
            i += 2
    return bytes(out)


def download(url: str, dest: Path) -> None:
    dest.parent.mkdir(parents=True, exist_ok=True)
    if dest.exists() and dest.stat().st_size > 0:
        return
    req = urllib.request.Request(
        url,
        headers={"User-Agent": "esp32-wl-display-demo/1.0 (local demo; NASA PD assets)"},
    )
    print(f"download {url}")
    with urllib.request.urlopen(req, timeout=60) as r, open(dest, "wb") as f:
        f.write(r.read())


def load_asset(asset: dict, skip_download: bool) -> tuple[int, int, bytes]:
    path = CACHE / f"{asset['id']}.jpg"
    if not skip_download:
        download(asset["url"], path)
    elif not path.exists():
        raise FileNotFoundError(f"missing cache {path}; run without --skip-download")
    img = Image.open(path)
    img = img.convert("RGB")
    fw, fh = asset["fit"]
    img.thumbnail((fw, fh), Image.Resampling.LANCZOS)
    # letterbox onto exact fit box with black
    canvas = Image.new("RGB", (fw, fh), (0, 0, 0))
    ox = (fw - img.width) // 2
    oy = (fh - img.height) // 2
    canvas.paste(img, (ox, oy))
    return fw, fh, image_to_rgb565(canvas)


def make_banner(text: str, w: int = 320, h: int = 28, bg=(0, 40, 80), fg=(255, 220, 80)) -> bytes:
    return image_to_rgb565(wt.draw_banner_rgb(text, w, h, bg=bg, fg=fg))


def encode_auto(w: int, h: int, raw: bytes) -> tuple[int, bytes]:
    """Product: raw only (delta lab is ../delta-rle-lab)."""
    return wm.ENC_RAW, raw


def tile_and_publish(
    client,
    device: str,
    x0: int,
    y0: int,
    w: int,
    h: int,
    raw: bytes,
    seq_start: int,
) -> int:
    """Publish image as horizontal strips that fit inline_max."""
    assert len(raw) == w * h * 2
    if w * h * 2 > 24 * 1024:
        print(
            f"  hint: {w}x{h} image is large for MQTT tiles — "
            "prefer HTTP uri + asset --enc auto for photos",
            file=sys.stderr,
        )
    seq = seq_start
    y = 0
    while y < h:
        strip_h = min(16, h - y)  # start modest; shrink if needed
        while strip_h >= 1:
            off = y * w * 2
            chunk = raw[off : off + w * strip_h * 2]
            enc, payload = encode_auto(w, strip_h, chunk)
            msg = wm.pack_rect(10, seq, x0, y0 + y, w, strip_h, enc, payload)
            if len(msg) <= wm.INLINE_MAX:
                break
            strip_h //= 2
        else:
            raise RuntimeError(f"cannot fit strip at y={y} under inline_max")
        t = wm.topic_cmd(device)
        info = client.publish(t, msg, qos=1)
        info.wait_for_publish(timeout=5)
        if not info.is_published():
            raise TimeoutError(f"MQTT publish timeout strip y={y0 + y}")
        print(f"  strip y={y0 + y} {w}x{strip_h} enc={enc} {len(msg)}B")
        seq += 1
        y += strip_h
        time.sleep(0.05)
    return seq


def publish(client, device: str, payload: bytes) -> None:
    info = client.publish(wm.topic_cmd(device), payload, qos=1)
    info.wait_for_publish(timeout=5)
    if not info.is_published():
        raise TimeoutError("MQTT publish timeout")


def run_demo(args: argparse.Namespace) -> int:
    wm.ensure_encode_rect()
    CACHE.mkdir(parents=True, exist_ok=True)

    visual_proc = None
    if args.visual:
        sim_bin = ROOT / "host" / "sim" / "wd-sim"
        if not sim_bin.exists():
            subprocess.check_call(["make", "-C", str(ROOT / "host" / "sim"), "all"], cwd=ROOT)
        visual_proc = subprocess.Popen(
            [
                str(VENV_PY if VENV_PY.exists() else sys.executable),
                str(ROOT / "tools" / "wd_mqtt.py"),
                "visual",
                "--device",
                args.device,
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

    client = wm.mqtt_client(args.host, args.port, f"wd-demo-{os.getpid()}")
    client.loop_start()
    seq = 1

    try:
        print("== clear navy ==")
        publish(client, args.device, wm.pack_clear(1, seq, 0x0011))
        seq += 1
        time.sleep(args.pause)

        print("== banner ==")
        ban = make_banner("esp32-wl-display  MQTT L0 demo")
        enc, payload = encode_auto(PANEL_W, 28, ban)
        publish(
            client,
            args.device,
            wm.pack_rect(2, seq, 0, 0, PANEL_W, 28, enc, payload),
        )
        seq += 1
        time.sleep(args.pause)

        print("== color bars ==")
        for i, color in enumerate((0xF800, 0x07E0, 0x001F, 0xFFE0)):
            raw = wm.rgb565_fill(70, 40, color)
            enc, payload = encode_auto(70, 40, raw)
            publish(
                client,
                args.device,
                wm.pack_rect(3, seq, 20 + i * 74, 50, 70, 40, enc, payload),
            )
            seq += 1
            time.sleep(0.2)
        time.sleep(args.pause)

        print("== checker ==")
        chk = wm.rgb565_checker(64, 48, 0xFFFF, 0x001F)
        enc, payload = encode_auto(64, 48, chk)
        publish(
            client,
            args.device,
            wm.pack_rect(4, seq, 128, 120, 64, 48, enc, payload),
        )
        seq += 1
        time.sleep(args.pause)

        for asset in ASSETS:
            print(f"== image {asset['id']} ({asset['license']}) ==")
            publish(client, args.device, wm.pack_clear(1, seq, 0x0000))
            seq += 1
            time.sleep(0.3)
            w, h, raw = load_asset(asset, args.skip_download)
            x, y = asset["pos"]
            # title strip
            title = make_banner(
                asset["id"], w=min(w, 200), h=16, bg=(20, 20, 20), fg=(180, 220, 255)
            )
            te, tp = encode_auto(min(w, 200), 16, title)
            publish(
                client,
                args.device,
                wm.pack_rect(5, seq, x, max(0, y - 18), min(w, 200), 16, te, tp),
            )
            seq += 1
            seq = tile_and_publish(client, args.device, x, y, w, h, raw, seq)
            time.sleep(args.pause + 0.5)

        print("== finale: raw UI panel-ish ==")
        publish(client, args.device, wm.pack_clear(1, seq, 0x10A2))
        seq += 1
        for box in (
            (10, 40, 300, 24, 0x0410),
            (10, 80, 140, 100, 0xF800),
            (170, 80, 140, 100, 0x07E0),
        ):
            x, y, bw, bh, c = box
            raw = wm.rgb565_fill(bw, bh, c)
            enc, payload = encode_auto(bw, bh, raw)
            publish(
                client,
                args.device,
                wm.pack_rect(6, seq, x, y, bw, bh, enc, payload),
            )
            seq += 1
            time.sleep(0.15)

        print("demo done.")
        if args.visual:
            print("SDL window open — quit with q / Esc in the window.")
            if visual_proc:
                visual_proc.wait()
        return 0
    finally:
        client.loop_stop()
        client.disconnect()
        if visual_proc and visual_proc.poll() is None and not args.visual:
            visual_proc.terminate()


def fetch_assets() -> int:
    """Pre-download original stills into tools/.cache/demo (no convert)."""
    CACHE.mkdir(parents=True, exist_ok=True)
    for asset in ASSETS:
        path = CACHE / f"{asset['id']}.jpg"
        download(asset["url"], path)
        print(f"cached {path} ({path.stat().st_size} B) — {asset['license']}")
    print("fetch done (JPEG only; RGB565 happens at demo play time).")
    return 0


def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description="wl-display orchestrated demo")
    p.add_argument(
        "command",
        nargs="?",
        choices=("fetch", "play"),
        default="play",
        help="fetch=download originals only; play=convert on the fly + MQTT (default)",
    )
    p.add_argument("--host", default=os.environ.get("MQTT_HOST", "127.0.0.1"))
    p.add_argument("--port", type=int, default=int(os.environ.get("MQTT_PORT", "1883")))
    p.add_argument("--device", default=os.environ.get("WD_DEVICE", "cyd1"))
    p.add_argument("--pause", type=float, default=1.2, help="seconds between scenes")
    p.add_argument(
        "--visual",
        action="store_true",
        help="start wd_mqtt.py visual (SDL) for this device id",
    )
    p.add_argument("--scale", type=int, default=2, help="SDL scale when --visual")
    p.add_argument(
        "--skip-download",
        action="store_true",
        help="use tools/.cache/demo only (no network during play)",
    )
    return p


def main() -> int:
    args = build_parser().parse_args()
    if args.command == "fetch":
        return fetch_assets()
    return run_demo(args)


if __name__ == "__main__":
    sys.exit(main())

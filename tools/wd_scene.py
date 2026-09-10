#!/usr/bin/env python3
"""
Thin L0 scene composer (Step 9).

Drive one or more devices from a YAML scene — clear / fill / checker / banner /
image — without hand-packing injects. Device stays dumb L0.

Examples:
  ./tools/wd_scene.py tools/scenes/hello.yaml
  ./tools/wd_scene.py tools/scenes/hello.yaml --device cyd1
  ./tools/wd_scene.py tools/scenes/hello.yaml --device cyd1 --device cyd2
"""

from __future__ import annotations

import argparse
import os
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
VENV_PY = Path(__file__).resolve().parent / ".venv" / "bin" / "python"

try:
    import yaml
except ImportError:
    if VENV_PY.exists() and os.path.realpath(sys.executable) != os.path.realpath(
        VENV_PY
    ):
        os.execv(str(VENV_PY), [str(VENV_PY), str(Path(__file__).resolve()), *sys.argv[1:]])
    print(
        "Need PyYAML (pip install -r tools/requirements.txt).",
        file=sys.stderr,
    )
    sys.exit(1)

try:
    from PIL import Image
except ImportError:
    if VENV_PY.exists() and os.path.realpath(sys.executable) != os.path.realpath(
        VENV_PY
    ):
        os.execv(str(VENV_PY), [str(VENV_PY), str(Path(__file__).resolve()), *sys.argv[1:]])
    print("Need Pillow.", file=sys.stderr)
    sys.exit(1)

sys.path.insert(0, str(Path(__file__).resolve().parent))
import wd_mqtt as wm  # noqa: E402
import wd_text as wt  # noqa: E402

INLINE_PAYLOAD = wm.INLINE_MAX - wm.HDR_SIZE


def parse_color(v) -> int:
    if isinstance(v, int):
        return v & 0xFFFF
    if isinstance(v, str):
        return int(v, 0) & 0xFFFF
    raise ValueError(f"bad color {v!r}")


def rgb888_to_rgb565(r: int, g: int, b: int) -> int:
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def image_to_rgb565(img: Image.Image) -> bytes:
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


def encode_auto(w: int, h: int, raw: bytes) -> tuple[int, bytes]:
    """Product: raw only (delta lab is ../delta-rle-lab)."""
    return wm.ENC_RAW, raw


def publish(client, device: str, payload: bytes) -> None:
    info = client.publish(wm.topic_cmd(device), payload, qos=1)
    info.wait_for_publish(timeout=5)
    if not info.is_published():
        raise TimeoutError(f"MQTT publish timeout → {device}")


def publish_all(client, devices: list[str], payload: bytes) -> None:
    for d in devices:
        publish(client, d, payload)


def tile_rect(
    client,
    devices: list[str],
    cmd_id: int,
    seq: int,
    x0: int,
    y0: int,
    w: int,
    h: int,
    raw: bytes,
) -> int:
    assert len(raw) == w * h * 2
    if w * h * 2 > 24 * 1024:
        print(
            f"  hint: {w}x{h} image is large for MQTT tiles — "
            "prefer `uri` + `wd_mqtt.py asset --enc auto` for photos",
            file=sys.stderr,
        )
    y = 0
    while y < h:
        strip_h = min(16, h - y)
        while strip_h >= 1:
            off = y * w * 2
            chunk = raw[off : off + w * strip_h * 2]
            enc, payload = encode_auto(w, strip_h, chunk)
            msg = wm.pack_rect(cmd_id, seq, x0, y0 + y, w, strip_h, enc, payload)
            if len(msg) <= wm.INLINE_MAX:
                break
            strip_h //= 2
        else:
            raise RuntimeError(f"cannot fit strip at y={y} under inline_max")
        publish_all(client, devices, msg)
        print(f"  strip y={y0 + y} {w}x{strip_h} enc={enc} {len(msg)}B → {devices}")
        seq += 1
        y += strip_h
        time.sleep(0.03)
    return seq


def load_image(path: Path, fit: tuple[int, int] | None) -> tuple[int, int, bytes]:
    img = Image.open(path).convert("RGB")
    if fit:
        fw, fh = fit
        img.thumbnail((fw, fh), Image.Resampling.LANCZOS)
        canvas = Image.new("RGB", (fw, fh), (0, 0, 0))
        canvas.paste(img, ((fw - img.width) // 2, (fh - img.height) // 2))
        img = canvas
    return img.width, img.height, image_to_rgb565(img)


def normalize_step(step) -> tuple[str, dict]:
    """Accept {'op': 'clear', ...} or {'clear': {...}} / {'sleep': 0.3}."""
    if not isinstance(step, dict) or not step:
        raise ValueError(f"bad step {step!r}")
    if "op" in step:
        op = str(step["op"])
        body = {k: v for k, v in step.items() if k != "op"}
        return op, body
    if len(step) != 1:
        raise ValueError(f"step needs one key or op=: {step!r}")
    op, body = next(iter(step.items()))
    if body is None:
        body = {}
    elif not isinstance(body, dict):
        # sleep: 0.3
        body = {"sec": body} if op in ("sleep", "pause") else {"value": body}
    return str(op), body


def run_scene(scene: dict, args: argparse.Namespace) -> int:
    wm.ensure_encode_rect()
    scene_dir = Path(args.scene).resolve().parent

    if args.device:
        devices = list(args.device)
    else:
        devices = list(scene.get("devices") or ["cyd1"])
    devices = [str(d) for d in devices]

    defaults = scene.get("defaults") or {}
    cmd_id = int(defaults.get("id", 1))
    pause = float(defaults.get("pause", 0.05))
    seq = int(args.seq)

    client = wm.mqtt_client(args.host, args.port, f"wd-scene-{os.getpid()}")
    client.loop_start()
    try:
        name = scene.get("name") or Path(args.scene).stem
        print(f"scene={name} devices={devices}")
        for i, raw_step in enumerate(scene.get("steps") or []):
            op, body = normalize_step(raw_step)
            if op in ("sleep", "pause"):
                sec = float(body.get("sec", body.get("value", 0)))
                print(f"[{i}] sleep {sec}s")
                time.sleep(sec)
                continue
            if op == "devices":
                devices = [str(d) for d in (body if isinstance(body, list) else body.get("list", []))]
                print(f"[{i}] devices → {devices}")
                continue

            print(f"[{i}] {op} {body}")
            if op == "clear":
                color = parse_color(body.get("color", 0))
                publish_all(client, devices, wm.pack_clear(cmd_id, seq, color))
                seq += 1
            elif op in ("fill", "solid", "fill_rect"):
                x = int(body.get("x", 0))
                y = int(body.get("y", 0))
                w = int(body["w"])
                h = int(body["h"])
                color = parse_color(body.get("color", body.get("solid", 0xF800)))
                if body.get("l0"):
                    raw = wm.rgb565_fill(w, h, color)
                    enc, payload = encode_auto(w, h, raw)
                    msg = wm.pack_rect(cmd_id, seq, x, y, w, h, enc, payload)
                    if len(msg) > wm.INLINE_MAX:
                        seq = tile_rect(client, devices, cmd_id, seq, x, y, w, h, raw)
                    else:
                        publish_all(client, devices, msg)
                        seq += 1
                else:
                    publish_all(
                        client,
                        devices,
                        wm.pack_fill_rect(cmd_id, seq, x, y, w, h, color),
                    )
                    seq += 1
            elif op in ("text", "draw_text"):
                text = str(body.get("text", ""))
                x = int(body.get("x", 0))
                y = int(body.get("y", 0))
                color = parse_color(body.get("color", 0xFFFF))
                scale = int(body.get("scale", 2))
                publish_all(
                    client,
                    devices,
                    wm.pack_text(cmd_id, seq, x, y, color, text, scale=scale),
                )
                seq += 1
            elif op in ("bind_define", "bind-define"):
                slot = int(body["slot"])
                x = int(body.get("x", 0))
                y = int(body.get("y", 0))
                fg = parse_color(body.get("fg", body.get("color", 0xFFFF)))
                bg = parse_color(body.get("bg", 0x0000))
                scale = int(body.get("scale", 2))
                max_chars = int(body.get("max_chars", body.get("max-chars", 8)))
                text = str(body.get("text", ""))
                publish_all(
                    client,
                    devices,
                    wm.pack_bind_define(
                        slot, seq, x, y, fg, bg, scale, max_chars, text
                    ),
                )
                seq += 1
            elif op in ("bind_set", "bind-set"):
                slot = int(body["slot"])
                text = str(body["text"])
                # Prefer live MQTT topic when use_topic: true
                if body.get("topic") or body.get("use_topic"):
                    for d in devices:
                        t = wm.topic_bind_set(d, slot)
                        info = client.publish(t, text.encode("utf-8"), qos=1)
                        info.wait_for_publish(timeout=5)
                        if not info.is_published():
                            raise TimeoutError(f"bind-pub timeout → {t}")
                    print(f"  bind topic slot={slot} {text!r} → {devices}")
                else:
                    publish_all(
                        client, devices, wm.pack_bind_set(slot, seq, text)
                    )
                    seq += 1
            elif op == "batch":
                ops = body.get("ops")
                if not isinstance(ops, list):
                    raise ValueError("batch needs ops: list")
                msg = wm.pack_batch(cmd_id, seq, ops)
                if len(msg) > wm.INLINE_MAX:
                    raise ValueError(f"batch envelope {len(msg)} > inline_max")
                publish_all(client, devices, msg)
                seq += 1
            elif op in ("group_define", "group-define"):
                group = int(body["group"])
                ops = body.get("ops")
                if not isinstance(ops, list):
                    raise ValueError("group_define needs ops: list")
                msg = wm.pack_group_define(group, seq, ops)
                if len(msg) > wm.INLINE_MAX:
                    raise ValueError(f"group envelope {len(msg)} > inline_max")
                publish_all(client, devices, msg)
                seq += 1
            elif op in ("group_draw", "group-draw"):
                group = int(body["group"])
                publish_all(client, devices, wm.pack_group_draw(group, seq))
                seq += 1
            elif op == "checker":
                x = int(body.get("x", 0))
                y = int(body.get("y", 0))
                w = int(body["w"])
                h = int(body["h"])
                c0 = parse_color(body.get("c0", 0xFFFF))
                c1 = parse_color(body.get("c1", 0x001F))
                raw = wm.rgb565_checker(w, h, c0, c1)
                enc, payload = encode_auto(w, h, raw)
                msg = wm.pack_rect(cmd_id, seq, x, y, w, h, enc, payload)
                if len(msg) > wm.INLINE_MAX:
                    seq = tile_rect(client, devices, cmd_id, seq, x, y, w, h, raw)
                else:
                    publish_all(client, devices, msg)
                    seq += 1
            elif op == "banner":
                text = str(body.get("text", ""))
                x = int(body.get("x", 0))
                y = int(body.get("y", 0))
                w = int(body.get("w", 320))
                h = int(body.get("h", 28))
                bg = tuple(body.get("bg", (0, 40, 80)))
                fg = tuple(body.get("fg", (255, 220, 80)))
                raw = image_to_rgb565(wt.draw_banner_rgb(text, w, h, bg=bg, fg=fg))
                enc, payload = encode_auto(w, h, raw)
                msg = wm.pack_rect(cmd_id, seq, x, y, w, h, enc, payload)
                if len(msg) > wm.INLINE_MAX:
                    seq = tile_rect(client, devices, cmd_id, seq, x, y, w, h, raw)
                else:
                    publish_all(client, devices, msg)
                    seq += 1
            elif op == "image":
                rel = body["path"]
                path = Path(rel)
                if not path.is_file():
                    path = (ROOT / rel).resolve()
                if not path.is_file():
                    path = (scene_dir / rel).resolve()
                if not path.is_file():
                    raise FileNotFoundError(rel)
                fit = body.get("fit")
                fit_t = (int(fit[0]), int(fit[1])) if fit else None
                x = int(body.get("x", 0))
                y = int(body.get("y", 0))
                w, h, raw = load_image(path, fit_t)
                seq = tile_rect(client, devices, cmd_id, seq, x, y, w, h, raw)
            elif op in ("uri", "uri_rect"):
                url = str(body["url"])
                if not url.startswith("http://"):
                    raise ValueError("uri must be http://")
                if len(url) > 256:
                    raise ValueError("uri too long")
                x = int(body.get("x", 0))
                y = int(body.get("y", 0))
                w = int(body["w"])
                h = int(body["h"])
                enc_name = str(body.get("enc", "")).strip()
                if enc_name != "raw":
                    raise ValueError(
                        "uri needs enc: raw matching the served body "
                        "(encode with: wd_mqtt.py asset --enc raw)"
                    )
                enc = wm.ENC_RAW
                msg = wm.pack_rect(
                    cmd_id,
                    seq,
                    x,
                    y,
                    w,
                    h,
                    enc,
                    url.encode("utf-8"),
                    flags=wm.FLAG_URI,
                )
                publish_all(client, devices, msg)
                seq += 1
            else:
                raise ValueError(f"unknown op {op!r}")
            time.sleep(float(body.get("pause", pause)))
        print(f"done seq={seq}")
        return 0
    finally:
        client.loop_stop()
        client.disconnect()


def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description="wl-display YAML scene composer")
    p.add_argument("scene", help="path to scene YAML")
    p.add_argument("--host", default=os.environ.get("MQTT_HOST", "127.0.0.1"))
    p.add_argument("--port", type=int, default=int(os.environ.get("MQTT_PORT", "1883")))
    p.add_argument(
        "--device",
        action="append",
        help="override scene devices (repeatable)",
    )
    p.add_argument("--seq", type=int, default=1, help="starting seq")
    return p


def main() -> int:
    args = build_parser().parse_args()
    path = Path(args.scene)
    if not path.is_file():
        print(f"missing scene {path}", file=sys.stderr)
        return 1
    scene = yaml.safe_load(path.read_text())
    if not isinstance(scene, dict):
        print("scene root must be a mapping", file=sys.stderr)
        return 1
    return run_scene(scene, args)


if __name__ == "__main__":
    sys.exit(main())

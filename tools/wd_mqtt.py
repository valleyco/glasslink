#!/usr/bin/env python3
"""
Host MQTT helpers for esp32-wl-display (Step 5/6).

Uses local broker by default (127.0.0.1:1883). Binary L0 envelope = docs/contract/cmd-v1.md.

Examples:
  ./tools/wd_mqtt.py inject clear --device dev1 --color 0xF800
  ./tools/wd_mqtt.py inject rect --device dev1 --x 10 --y 20 --w 8 --h 8 --enc raw
  ./tools/wd_mqtt.py inject rect --device dev1 --enc delta --solid 0x07E0 --w 32 --h 16
  ./tools/wd_mqtt.py sim --device dev1 --apply ./host/mqtt/apply_bin
  ./tools/wd_mqtt.py loopback --device loop1
"""

from __future__ import annotations

import argparse
import os
import struct
import subprocess
import sys
import tempfile
import threading
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
VENV_PY = Path(__file__).resolve().parent / ".venv" / "bin" / "python"

# Re-exec under tools venv if paho missing
try:
    import paho.mqtt.client as mqtt
except ImportError:
    if VENV_PY.exists() and os.path.realpath(sys.executable) != os.path.realpath(
        VENV_PY
    ):
        os.execv(str(VENV_PY), [str(VENV_PY), str(Path(__file__).resolve()), *sys.argv[1:]])
    print(
        f"Need paho-mqtt (venv={VENV_PY} exists={VENV_PY.exists()}).",
        file=sys.stderr,
    )
    sys.exit(1)

MAGIC = b"WLD1"
HDR_SIZE = 28
TYPE_CLEAR = 0x01
TYPE_RECT = 0x02
ENC_RAW = 0
ENC_DELTA = 1
FMT_RGB565 = 0


# LE header: magic[4] ver type flags pad id seq x y w h enc fmt color payload_len
_HDR = "<4sBBBBHHhhHHBBHI"


def pack_clear(id_: int, seq: int, color: int) -> bytes:
    return struct.pack(
        _HDR,
        MAGIC,
        1,  # ver
        TYPE_CLEAR,
        0,  # flags
        0,  # pad
        id_ & 0xFFFF,
        seq & 0xFFFF,
        0,  # x
        0,  # y
        0,  # w
        0,  # h
        0,  # enc
        FMT_RGB565,
        color & 0xFFFF,
        0,  # payload_len
    )


def pack_rect(
    id_: int,
    seq: int,
    x: int,
    y: int,
    w: int,
    h: int,
    enc: int,
    payload: bytes,
) -> bytes:
    hdr = struct.pack(
        _HDR,
        MAGIC,
        1,
        TYPE_RECT,
        0,
        0,
        id_ & 0xFFFF,
        seq & 0xFFFF,
        x,
        y,
        w & 0xFFFF,
        h & 0xFFFF,
        enc & 0xFF,
        FMT_RGB565,
        0,
        len(payload),
    )
    return hdr + payload


def rgb565_fill(w: int, h: int, color: int) -> bytes:
    px = struct.pack("<H", color & 0xFFFF)
    return px * (w * h)


def topic_cmd(device: str) -> str:
    return f"wd/{device}/cmd"


def topic_status(device: str) -> str:
    return f"wd/{device}/status"


def topic_lwt(device: str) -> str:
    return f"wd/{device}/lwt"


def topic_ack(device: str) -> str:
    return f"wd/{device}/ack"


def mqtt_client(host: str, port: int, client_id: str) -> mqtt.Client:
    c = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id=client_id)
    c.connect(host, port, keepalive=30)
    return c


def cmd_inject(args: argparse.Namespace) -> int:
    if args.subcmd == "clear":
        payload = pack_clear(args.id, args.seq, args.color)
    elif args.subcmd == "rect":
        if args.payload_file:
            data = Path(args.payload_file).read_bytes()
            enc = ENC_DELTA if args.enc == "delta" else ENC_RAW
        elif args.enc == "delta":
            # Host-side encode via apply helper is heavier; use solid raw then
            # shell out to a tiny encoder if available — for MVP solid uses C tool.
            data = encode_delta_solid(args.w, args.h, args.solid)
            enc = ENC_DELTA
        else:
            data = rgb565_fill(args.w, args.h, args.solid)
            enc = ENC_RAW
        payload = pack_rect(
            args.id, args.seq, args.x, args.y, args.w, args.h, enc, data
        )
    else:
        print("unknown inject subcmd", file=sys.stderr)
        return 2

    if args.out:
        Path(args.out).write_bytes(payload)
        print(f"wrote {len(payload)} bytes → {args.out}")
        if not args.publish:
            return 0

    if args.publish or not args.out:
        c = mqtt_client(args.host, args.port, f"wd-inject-{os.getpid()}")
        t = topic_cmd(args.device)
        info = c.publish(t, payload, qos=1, retain=False)
        info.wait_for_publish(timeout=5)
        c.disconnect()
        print(f"published {len(payload)} B → {t} @ {args.host}:{args.port}")
    return 0


def encode_delta_solid(w: int, h: int, color: int) -> bytes:
    """Call host codec CLI if present; else fall back to raw (caller sets enc)."""
    enc_bin = ROOT / "host" / "mqtt" / "encode_rect"
    raw = rgb565_fill(w, h, color)
    if not enc_bin.exists():
        # Build on demand
        subprocess.check_call(["make", "-C", str(ROOT / "host" / "mqtt"), "encode_rect"], cwd=ROOT)
    out = subprocess.check_output(
        [str(enc_bin), "--enc", "delta", "--w", str(w), "--h", str(h)],
        input=raw,
    )
    return out


def cmd_sim(args: argparse.Namespace) -> int:
    apply_bin = Path(args.apply)
    if not apply_bin.exists():
        subprocess.check_call(["make", "-C", str(ROOT / "host" / "mqtt"), "all"], cwd=ROOT)
    if not apply_bin.exists():
        print(f"missing {apply_bin}", file=sys.stderr)
        return 1

    got = {"n": 0}

    def on_message(client, userdata, msg):  # noqa: ARG001
        got["n"] += 1
        with tempfile.NamedTemporaryFile(delete=False, suffix=".bin") as f:
            f.write(msg.payload)
            path = f.name
        try:
            r = subprocess.run(
                [str(apply_bin), path],
                capture_output=True,
                text=True,
                check=False,
            )
            print(f"<< {msg.topic} {len(msg.payload)}B rc={r.returncode}")
            if r.stdout:
                print(r.stdout, end="")
            if r.stderr:
                print(r.stderr, end="", file=sys.stderr)
            # publish crude ack
            client.publish(
                topic_ack(args.device),
                f"rc={r.returncode} n={len(msg.payload)}".encode(),
                qos=0,
                retain=False,
            )
        finally:
            os.unlink(path)

    c = mqtt_client(args.host, args.port, f"wd-sim-{args.device}-{os.getpid()}")
    c.on_message = on_message
    c.subscribe(topic_cmd(args.device), qos=1)
    c.publish(topic_lwt(args.device), b"online", qos=1, retain=True)
    c.publish(
        topic_status(args.device),
        b'{"fw":"host-sim","disp":{"w":320,"h":240},"inline_max":6144,"codecs":["raw","delta_rle_v1"]}',
        qos=1,
        retain=True,
    )
    print(f"sim listening on {topic_cmd(args.device)} (broker {args.host}:{args.port})")
    c.loop_forever()
    return 0


def cmd_loopback(args: argparse.Namespace) -> int:
    """One-shot: start sim thread, inject clear + rect, check apply_bin exit."""
    subprocess.check_call(["make", "-C", str(ROOT / "host" / "mqtt"), "all"], cwd=ROOT)
    apply_bin = ROOT / "host" / "mqtt" / "apply_bin"
    results: list[int] = []
    ready = threading.Event()

    def on_message(client, userdata, msg):  # noqa: ARG001
        with tempfile.NamedTemporaryFile(delete=False, suffix=".bin") as f:
            f.write(msg.payload)
            path = f.name
        try:
            r = subprocess.run([str(apply_bin), path, "--expect-clear", "0xF800"], check=False)
            results.append(r.returncode)
        finally:
            os.unlink(path)
            if len(results) >= 1:
                client.disconnect()

    def on_connect(client, userdata, flags, reason_code, properties=None):  # noqa: ARG001
        client.subscribe(topic_cmd(args.device), qos=1)
        ready.set()

    c = mqtt_client(args.host, args.port, f"wd-loop-{os.getpid()}")
    c.on_message = on_message
    c.on_connect = on_connect
    c.loop_start()
    if not ready.wait(timeout=5):
        print("connect timeout", file=sys.stderr)
        return 1
    time.sleep(0.2)
    payload = pack_clear(1, 1, 0xF800)
    c.publish(topic_cmd(args.device), payload, qos=1).wait_for_publish(timeout=5)
    # wait for handler
    for _ in range(50):
        if results:
            break
        time.sleep(0.1)
    c.loop_stop()
    if not results:
        print("FAIL: no message received", file=sys.stderr)
        return 1
    if results[0] != 0:
        print(f"FAIL: apply_bin rc={results[0]}", file=sys.stderr)
        return 1
    print("loopback OK (clear 0xF800 via mosquitto → apply_bin)")
    return 0


def _add_broker_args(ap: argparse.ArgumentParser) -> None:
    ap.add_argument("--host", default=os.environ.get("MQTT_HOST", "127.0.0.1"))
    ap.add_argument(
        "--port", type=int, default=int(os.environ.get("MQTT_PORT", "1883"))
    )
    ap.add_argument("--device", default=os.environ.get("WD_DEVICE", "dev1"))


def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description="wd MQTT host tools")
    _add_broker_args(p)
    sp = p.add_subparsers(dest="cmd", required=True)

    inj = sp.add_parser("inject", help="pack + publish L0 command")
    isub = inj.add_subparsers(dest="subcmd", required=True)

    def _inj_common(ap: argparse.ArgumentParser) -> None:
        _add_broker_args(ap)
        ap.add_argument("--id", type=int, default=1)
        ap.add_argument("--seq", type=int, default=1)
        ap.add_argument("--out", help="write binary file instead/in addition")
        ap.add_argument(
            "--publish",
            action="store_true",
            help="force publish when --out set",
        )

    c = isub.add_parser("clear")
    _inj_common(c)
    c.add_argument("--color", type=lambda s: int(s, 0), default=0x0000)

    r = isub.add_parser("rect")
    _inj_common(r)
    r.add_argument("--x", type=int, default=0)
    r.add_argument("--y", type=int, default=0)
    r.add_argument("--w", type=int, default=8)
    r.add_argument("--h", type=int, default=8)
    r.add_argument("--enc", choices=("raw", "delta"), default="raw")
    r.add_argument("--solid", type=lambda s: int(s, 0), default=0xF800)
    r.add_argument("--payload-file", help="raw codec payload bytes")

    s = sp.add_parser("sim", help="host device simulator (subscribe → apply_bin)")
    _add_broker_args(s)
    s.add_argument(
        "--apply",
        default=str(ROOT / "host" / "mqtt" / "apply_bin"),
        help="path to apply_bin",
    )

    lb = sp.add_parser("loopback", help="one-shot broker round-trip test")
    _add_broker_args(lb)
    return p


def main() -> int:
    args = build_parser().parse_args()
    if args.cmd == "inject":
        return cmd_inject(args)
    if args.cmd == "sim":
        return cmd_sim(args)
    if args.cmd == "loopback":
        return cmd_loopback(args)
    return 2


if __name__ == "__main__":
    sys.exit(main())

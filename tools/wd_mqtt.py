#!/usr/bin/env python3
"""
Host MQTT helpers for esp32-wl-display (Step 5/6h).

Uses local broker by default (127.0.0.1:1883). Binary L0 envelope = docs/contract/cmd-v1.md.

Examples:
  ./tools/wd_mqtt.py inject clear --device dev1 --color 0xF800
  ./tools/wd_mqtt.py inject rect --device dev1 --x 10 --y 20 --w 8 --h 8 --enc raw
  ./tools/wd_mqtt.py inject rect --device dev1 --enc delta --solid 0x07E0 --w 32 --h 16
  ./tools/wd_mqtt.py inject rect --device dev1 --pattern checker --enc delta --w 16 --h 8
  ./tools/wd_mqtt.py inject rect --device dev1 --rgb-file pixels.rgb --w 8 --h 8 --enc raw
  ./tools/wd_mqtt.py sim --device dev1 --apply ./host/mqtt/apply_bin
  ./tools/wd_mqtt.py visual --device sim1
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
FLAG_URI = 1 << 0
INLINE_MAX = 6144  # docs/contract/mqtt-topics-v1.md

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
    flags: int = 0,
) -> bytes:
    hdr = struct.pack(
        _HDR,
        MAGIC,
        1,
        TYPE_RECT,
        flags & 0xFF,
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


def rgb565_checker(w: int, h: int, c0: int, c1: int) -> bytes:
    out = bytearray()
    for y in range(h):
        for x in range(w):
            c = c0 if ((x ^ y) & 1) == 0 else c1
            out += struct.pack("<H", c & 0xFFFF)
    return bytes(out)


def rgb565_h_runs(w: int, h: int, c0: int, c1: int) -> bytes:
    out = bytearray()
    for y in range(h):
        c = c0 if (y & 1) == 0 else c1
        out += struct.pack("<H", c & 0xFFFF) * w
    return bytes(out)


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


def ensure_encode_rect() -> Path:
    enc_bin = ROOT / "host" / "mqtt" / "encode_rect"
    if not enc_bin.exists():
        subprocess.check_call(
            ["make", "-C", str(ROOT / "host" / "mqtt"), "encode_rect"], cwd=ROOT
        )
    return enc_bin


def encode_rgb(enc_name: str, w: int, h: int, raw: bytes) -> bytes:
    """Encode raw RGB565 LE pixels via host encode_rect CLI."""
    need = w * h * 2
    if len(raw) != need:
        raise ValueError(f"rgb size {len(raw)} != {need} for {w}x{h}")
    enc_bin = ensure_encode_rect()
    return subprocess.check_output(
        [str(enc_bin), "--enc", enc_name, "--w", str(w), "--h", str(h)],
        input=raw,
    )


def enc_id(name: str) -> int:
    return ENC_DELTA if name == "delta" else ENC_RAW


def build_rect_payload(args: argparse.Namespace) -> tuple[int, bytes]:
    """Return (enc_id, codec_payload) for inject rect."""
    enc_name = args.enc
    if args.payload_file:
        data = Path(args.payload_file).read_bytes()
        return enc_id(enc_name), data

    if args.rgb_file:
        raw = Path(args.rgb_file).read_bytes()
    elif args.pattern == "checker":
        raw = rgb565_checker(args.w, args.h, args.solid, args.solid2)
    elif args.pattern == "h_runs":
        raw = rgb565_h_runs(args.w, args.h, args.solid, args.solid2)
    else:
        raw = rgb565_fill(args.w, args.h, args.solid)

    if enc_name == "raw":
        return ENC_RAW, raw
    return ENC_DELTA, encode_rgb("delta", args.w, args.h, raw)


def check_inline_max(msg: bytes, *, enforce: bool) -> None:
    if len(msg) > INLINE_MAX:
        msg_txt = f"envelope {len(msg)} B exceeds inline_max={INLINE_MAX}"
        if enforce:
            raise ValueError(msg_txt)
        print(f"warn: {msg_txt}", file=sys.stderr)


def cmd_inject(args: argparse.Namespace) -> int:
    try:
        if args.subcmd == "clear":
            payload = pack_clear(args.id, args.seq, args.color)
        elif args.subcmd == "rect":
            enc, data = build_rect_payload(args)
            payload = pack_rect(
                args.id, args.seq, args.x, args.y, args.w, args.h, enc, data
            )
            check_inline_max(payload, enforce=True)
        else:
            print("unknown inject subcmd", file=sys.stderr)
            return 2
    except (ValueError, subprocess.CalledProcessError) as e:
        print(f"inject error: {e}", file=sys.stderr)
        return 1

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


def write_framed(proc: subprocess.Popen, payload: bytes) -> None:
    """Send one envelope to wd-sim --stdin (u32 LE length + bytes)."""
    assert proc.stdin is not None
    proc.stdin.write(struct.pack("<I", len(payload)))
    proc.stdin.write(payload)
    proc.stdin.flush()


def cmd_visual(args: argparse.Namespace) -> int:
    """MQTT subscribe → SDL CYD sim via stdin framing."""
    sim_bin = ROOT / "host" / "sim" / "wd-sim"
    if not sim_bin.exists():
        subprocess.check_call(["make", "-C", str(ROOT / "host" / "sim"), "all"], cwd=ROOT)
    if not sim_bin.exists():
        print(f"missing {sim_bin}", file=sys.stderr)
        return 1

    proc = subprocess.Popen(
        [str(sim_bin), "--scale", str(args.scale), "--stdin"],
        stdin=subprocess.PIPE,
        cwd=ROOT,
    )

    def on_message(client, userdata, msg):  # noqa: ARG001
        try:
            write_framed(proc, msg.payload)
            print(f"<< {msg.topic} {len(msg.payload)}B → sim")
            client.publish(
                topic_ack(args.device),
                f'{{"rc":0,"n":{len(msg.payload)}}}'.encode(),
                qos=0,
                retain=False,
            )
        except BrokenPipeError:
            print("sim closed stdin", file=sys.stderr)
            client.disconnect()

    c = mqtt_client(args.host, args.port, f"wd-visual-{args.device}-{os.getpid()}")
    c.on_message = on_message
    c.subscribe(topic_cmd(args.device), qos=1)
    c.publish(topic_lwt(args.device), b"online", qos=1, retain=True)
    c.publish(
        topic_status(args.device),
        b'{"fw":"host-sdl-sim","disp":{"w":320,"h":240},"inline_max":6144,"codecs":["raw","delta_rle_v1"]}',
        qos=1,
        retain=True,
    )
    print(
        f"visual sim on {topic_cmd(args.device)} @ {args.host}:{args.port} "
        f"(inject then q in SDL window)"
    )
    try:
        c.loop_forever()
    finally:
        if proc.poll() is None:
            if proc.stdin:
                proc.stdin.close()
            proc.wait(timeout=3)
    return 0


def cmd_sim(args: argparse.Namespace) -> int:
    apply_bin = Path(args.apply)
    if not apply_bin.exists():
        subprocess.check_call(["make", "-C", str(ROOT / "host" / "mqtt"), "all"], cwd=ROOT)
    if not apply_bin.exists():
        print(f"missing {apply_bin}", file=sys.stderr)
        return 1

    def on_message(client, userdata, msg):  # noqa: ARG001
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


def _apply_once(apply_bin: Path, payload: bytes, expect_args: list[str]) -> int:
    with tempfile.NamedTemporaryFile(delete=False, suffix=".bin") as f:
        f.write(payload)
        path = f.name
    try:
        r = subprocess.run(
            [str(apply_bin), path, *expect_args],
            check=False,
        )
        return r.returncode
    finally:
        os.unlink(path)


def cmd_loopback(args: argparse.Namespace) -> int:
    """Broker round-trip: clear, raw rect, delta rect, checker, URI reject."""
    subprocess.check_call(["make", "-C", str(ROOT / "host" / "mqtt"), "all"], cwd=ROOT)
    apply_bin = ROOT / "host" / "mqtt" / "apply_bin"

    # Local encode checks (no broker) — oversize inject guard
    big_raw = rgb565_fill(64, 64, 0xF800)  # 8192 B payload + hdr > INLINE_MAX
    try:
        check_inline_max(
            pack_rect(1, 1, 0, 0, 64, 64, ENC_RAW, big_raw), enforce=True
        )
        print("FAIL: oversize should raise", file=sys.stderr)
        return 1
    except ValueError:
        print("inline_max guard OK (64x64 raw rejected)", flush=True)

    cases: list[tuple[str, bytes, list[str]]] = []

    # 1) clear
    cases.append(
        ("clear", pack_clear(1, 1, 0xF800), ["--expect-clear", "0xF800"])
    )

    # 2) raw solid rect
    rx, ry, rw, rh = 10, 20, 8, 8
    raw_green = rgb565_fill(rw, rh, 0x07E0)
    cases.append(
        (
            "raw-rect",
            pack_rect(2, 2, rx, ry, rw, rh, ENC_RAW, raw_green),
            ["--expect-px", str(rx), str(ry), "0x07E0",
             "--expect-px", str(rx + rw - 1), str(ry + rh - 1), "0x07E0"],
        )
    )

    # 3) delta solid rect
    dx, dy, dw, dh = 100, 50, 16, 8
    delta_blue = encode_rgb("delta", dw, dh, rgb565_fill(dw, dh, 0x001F))
    cases.append(
        (
            "delta-rect",
            pack_rect(3, 3, dx, dy, dw, dh, ENC_DELTA, delta_blue),
            ["--expect-px", str(dx), str(dy), "0x001F",
             "--expect-px", str(dx + dw - 1), str(dy + dh - 1), "0x001F"],
        )
    )

    # 4) checker via encode_rect (delta)
    cx, cy, cw, ch = 40, 80, 12, 6
    c0, c1 = 0xF800, 0x001F
    checker = rgb565_checker(cw, ch, c0, c1)
    delta_chk = encode_rgb("delta", cw, ch, checker)
    cases.append(
        (
            "delta-checker",
            pack_rect(4, 4, cx, cy, cw, ch, ENC_DELTA, delta_chk),
            [
                "--expect-px", str(cx), str(cy), hex(c0),
                "--expect-px", str(cx + 1), str(cy), hex(c1),
                "--expect-px", str(cx), str(cy + 1), hex(c1),
            ],
        )
    )

    # 5) URI flag must fail dispatch (W7)
    uri_payload = pack_rect(
        5, 5, 0, 0, 2, 2, ENC_RAW, rgb565_fill(2, 2, 0), flags=FLAG_URI
    )
    cases.append(("uri-reject", uri_payload, ["--expect-fail"]))

    results: list[tuple[str, int]] = []
    pending: list[tuple[str, list[str]]] = []
    lock = threading.Lock()
    ready = threading.Event()
    done = threading.Event()

    def on_message(client, userdata, msg):  # noqa: ARG001
        with lock:
            if not pending:
                return
            name, expect_args = pending.pop(0)
        rc = _apply_once(apply_bin, msg.payload, expect_args)
        with lock:
            results.append((name, rc))
            if len(results) >= len(cases):
                done.set()
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
        c.loop_stop()
        return 1
    time.sleep(0.15)

    for name, payload, expect_args in cases:
        with lock:
            pending.append((name, expect_args))
        c.publish(topic_cmd(args.device), payload, qos=1).wait_for_publish(timeout=5)
        # Wait for this case before publishing next (order + apply_bin isolation)
        for _ in range(50):
            with lock:
                if any(r[0] == name for r in results):
                    break
            time.sleep(0.05)
        else:
            print(f"FAIL: timeout waiting for {name}", file=sys.stderr)
            c.loop_stop()
            return 1

    done.wait(timeout=2)
    c.loop_stop()

    failed = [(n, rc) for n, rc in results if rc != 0]
    if len(results) != len(cases):
        print(f"FAIL: got {len(results)}/{len(cases)} results", file=sys.stderr)
        return 1
    if failed:
        for n, rc in failed:
            print(f"FAIL: {n} apply_bin rc={rc}", file=sys.stderr)
        return 1

    names = ", ".join(n for n, _ in results)
    print(f"loopback OK ({names} via mosquitto → apply_bin)")
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
    r.add_argument(
        "--solid2",
        type=lambda s: int(s, 0),
        default=0x001F,
        help="second color for checker/h_runs",
    )
    r.add_argument(
        "--pattern",
        choices=("solid", "checker", "h_runs"),
        default="solid",
        help="synthetic RGB565 source (ignored if --rgb-file/--payload-file)",
    )
    r.add_argument(
        "--rgb-file",
        help="raw RGB565 LE pixels (w*h*2); encoded per --enc",
    )
    r.add_argument(
        "--payload-file",
        help="pre-encoded codec payload bytes (skips encode)",
    )

    s = sp.add_parser("sim", help="host device simulator (subscribe → apply_bin)")
    _add_broker_args(s)
    s.add_argument(
        "--apply",
        default=str(ROOT / "host" / "mqtt" / "apply_bin"),
        help="path to apply_bin",
    )

    v = sp.add_parser("visual", help="MQTT → SDL CYD window (host/sim)")
    _add_broker_args(v)
    v.add_argument("--scale", type=int, default=2)

    lb = sp.add_parser("loopback", help="one-shot broker round-trip test")
    _add_broker_args(lb)
    return p


def main() -> int:
    args = build_parser().parse_args()
    if args.cmd == "inject":
        return cmd_inject(args)
    if args.cmd == "sim":
        return cmd_sim(args)
    if args.cmd == "visual":
        return cmd_visual(args)
    if args.cmd == "loopback":
        return cmd_loopback(args)
    return 2


if __name__ == "__main__":
    sys.exit(main())

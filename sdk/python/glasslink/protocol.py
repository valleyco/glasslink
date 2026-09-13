"""Binary cmd packers, topics, RGB565 helpers (glasslink host SDK)."""
from __future__ import annotations

import os
import struct
import subprocess
from pathlib import Path

_PKG = Path(__file__).resolve().parent
# glasslink/ → python/ → sdk/ → repo root
ROOT = Path(os.environ["GLASSLINK_ROOT"]).resolve() if os.environ.get("GLASSLINK_ROOT") else _PKG.parents[2]

MAGIC = b"WLD1"
HDR_SIZE = 28
TYPE_CLEAR = 0x01
TYPE_RECT = 0x02
TYPE_FILL = 0x03
TYPE_TEXT = 0x04
TYPE_BIND_DEFINE = 0x05
TYPE_BIND_SET = 0x06
TYPE_BATCH = 0x07
TYPE_GROUP_DEFINE = 0x08
TYPE_GROUP_DRAW = 0x09
TYPE_POLY = 0x0A
TYPE_MOVE_TO = 0x0B
TYPE_LINE_TO = 0x0C
TYPE_CUBIC_TO = 0x0D
ENC_RAW = 0
FMT_RGB565 = 0
FLAG_URI = 1 << 0
INLINE_MAX = 6144  # docs/contract/mqtt-topics-v1.md
TEXT_MAX = 64
BIND_SLOTS = 8
GROUP_SLOTS = 4
BATCH_BYTES_MAX = 1024

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


def pack_fill_rect(
    id_: int, seq: int, x: int, y: int, w: int, h: int, color: int
) -> bytes:
    return struct.pack(
        _HDR,
        MAGIC,
        1,
        TYPE_FILL,
        0,
        0,
        id_ & 0xFFFF,
        seq & 0xFFFF,
        x,
        y,
        w & 0xFFFF,
        h & 0xFFFF,
        0,
        FMT_RGB565,
        color & 0xFFFF,
        0,
    )


def pack_text(
    id_: int,
    seq: int,
    x: int,
    y: int,
    color: int,
    text: str,
    scale: int = 2,
) -> bytes:
    raw = text.encode("utf-8")
    if not raw or len(raw) > TEXT_MAX:
        raise ValueError(f"text length must be 1..{TEXT_MAX} bytes")
    if scale not in (0, 1, 2):
        raise ValueError("scale must be 0, 1, or 2")
    hdr = struct.pack(
        _HDR,
        MAGIC,
        1,
        TYPE_TEXT,
        0,
        0,
        id_ & 0xFFFF,
        seq & 0xFFFF,
        x,
        y,
        0,
        0,
        scale & 0xFF,
        FMT_RGB565,
        color & 0xFFFF,
        len(raw),
    )
    return hdr + raw


def pack_bind_define(
    slot: int,
    seq: int,
    x: int,
    y: int,
    fg: int,
    bg: int,
    scale: int,
    max_chars: int,
    text: str = "",
) -> bytes:
    if not (0 <= slot < BIND_SLOTS):
        raise ValueError("slot out of range")
    raw = text.encode("utf-8")
    if len(raw) > TEXT_MAX:
        raise ValueError("text too long")
    body = struct.pack("<H", bg & 0xFFFF) + raw
    hdr = struct.pack(
        _HDR,
        MAGIC,
        1,
        TYPE_BIND_DEFINE,
        0,
        0,
        slot & 0xFFFF,
        seq & 0xFFFF,
        x,
        y,
        max_chars & 0xFFFF,
        0,
        scale & 0xFF,
        FMT_RGB565,
        fg & 0xFFFF,
        len(body),
    )
    return hdr + body


def pack_bind_set(slot: int, seq: int, text: str) -> bytes:
    if not (0 <= slot < BIND_SLOTS):
        raise ValueError("slot out of range")
    raw = text.encode("utf-8")
    if not raw or len(raw) > TEXT_MAX:
        raise ValueError(f"text length must be 1..{TEXT_MAX}")
    hdr = struct.pack(
        _HDR,
        MAGIC,
        1,
        TYPE_BIND_SET,
        0,
        0,
        slot & 0xFFFF,
        seq & 0xFFFF,
        0,
        0,
        0,
        0,
        0,
        FMT_RGB565,
        0,
        len(raw),
    )
    return hdr + raw


def _parse_color(v) -> int:
    if isinstance(v, int):
        return v & 0xFFFF
    return int(str(v), 0) & 0xFFFF


def batch_ops_bytes(ops: list) -> bytes:
    """Build draw.batch / group.define payload from a list of op dicts."""
    out = bytearray()
    if not ops:
        raise ValueError("batch needs at least one op")
    if len(ops) > 32:
        raise ValueError("too many batch ops (max 32)")
    for item in ops:
        if not isinstance(item, dict) or len(item) != 1:
            # also accept {"op":"fill", ...}
            if isinstance(item, dict) and "op" in item:
                kind = str(item["op"])
                body = item
            else:
                raise ValueError(f"bad batch op {item!r}")
        else:
            kind, body = next(iter(item.items()))
            if not isinstance(body, dict):
                raise ValueError(f"bad batch op body {item!r}")
        kind = str(kind)
        if kind in ("fill", "fill_rect"):
            x = int(body["x"])
            y = int(body["y"])
            w = int(body["w"])
            h = int(body["h"])
            color = _parse_color(body.get("color", 0xF800))
            if w <= 0 or h <= 0:
                raise ValueError("fill w/h must be > 0")
            out += struct.pack("<BhhHHH", TYPE_FILL, x, y, w & 0xFFFF, h & 0xFFFF, color)
        elif kind == "text":
            text = str(body["text"])
            raw = text.encode("utf-8")
            if not raw or len(raw) > TEXT_MAX:
                raise ValueError(f"text length must be 1..{TEXT_MAX}")
            scale = int(body.get("scale", 1))
            if scale not in (0, 1, 2):
                raise ValueError("scale must be 0..2")
            x = int(body["x"])
            y = int(body["y"])
            color = _parse_color(body.get("color", 0xFFFF))
            out += struct.pack(
                "<BhhHBB", TYPE_TEXT, x, y, color, scale & 0xFF, len(raw)
            )
            out += raw
        elif kind in ("poly", "polygon"):
            color = _parse_color(body.get("color", 0xFFFF))
            pts = body.get("points") or body.get("xy")
            if not isinstance(pts, (list, tuple)) or len(pts) < 3:
                raise ValueError("poly needs points: [[x,y],…] (≥3)")
            if len(pts) > 16:
                raise ValueError("poly max 16 points")
            out += struct.pack("<BBH", TYPE_POLY, len(pts), color)
            for p in pts:
                out += struct.pack("<hh", int(p[0]), int(p[1]))
        elif kind in ("move_to", "move"):
            out += struct.pack(
                "<Bhh", TYPE_MOVE_TO, int(body["x"]), int(body["y"])
            )
        elif kind in ("line_to", "line"):
            color = _parse_color(body.get("color", 0xFFFF))
            out += struct.pack(
                "<BhhH",
                TYPE_LINE_TO,
                int(body["x"]),
                int(body["y"]),
                color,
            )
        elif kind in ("cubic_to", "cubic", "bezier"):
            color = _parse_color(body.get("color", 0xFFFF))
            out += struct.pack(
                "<BHhhhhhh",
                TYPE_CUBIC_TO,
                color,
                int(body["x1"]),
                int(body["y1"]),
                int(body["x2"]),
                int(body["y2"]),
                int(body["x3"]),
                int(body["y3"]),
            )
        else:
            raise ValueError(f"unknown batch op {kind!r}")
    if len(out) > BATCH_BYTES_MAX:
        raise ValueError(f"batch payload {len(out)} > {BATCH_BYTES_MAX}")
    return bytes(out)


def pack_batch(id_: int, seq: int, ops: list) -> bytes:
    body = batch_ops_bytes(ops)
    hdr = struct.pack(
        _HDR,
        MAGIC,
        1,
        TYPE_BATCH,
        0,
        0,
        id_ & 0xFFFF,
        seq & 0xFFFF,
        0,
        0,
        0,
        0,
        0,
        FMT_RGB565,
        0,
        len(body),
    )
    return hdr + body


def pack_group_define(group: int, seq: int, ops: list) -> bytes:
    if not (0 <= group < GROUP_SLOTS):
        raise ValueError("group out of range")
    body = batch_ops_bytes(ops)
    hdr = struct.pack(
        _HDR,
        MAGIC,
        1,
        TYPE_GROUP_DEFINE,
        0,
        0,
        group & 0xFFFF,
        seq & 0xFFFF,
        0,
        0,
        0,
        0,
        0,
        FMT_RGB565,
        0,
        len(body),
    )
    return hdr + body


def pack_group_draw(group: int, seq: int) -> bytes:
    if not (0 <= group < GROUP_SLOTS):
        raise ValueError("group out of range")
    return struct.pack(
        _HDR,
        MAGIC,
        1,
        TYPE_GROUP_DRAW,
        0,
        0,
        group & 0xFFFF,
        seq & 0xFFFF,
        0,
        0,
        0,
        0,
        0,
        FMT_RGB565,
        0,
        0,
    )


def pack_poly(id_: int, seq: int, color: int, points: list) -> bytes:
    n = len(points)
    if n < 3 or n > 16:
        raise ValueError("poly needs 3..16 points")
    body = b"".join(struct.pack("<hh", int(p[0]), int(p[1])) for p in points)
    hdr = struct.pack(
        _HDR,
        MAGIC,
        1,
        TYPE_POLY,
        0,
        0,
        id_ & 0xFFFF,
        seq & 0xFFFF,
        0,
        0,
        0,
        0,
        n & 0xFF,
        FMT_RGB565,
        color & 0xFFFF,
        len(body),
    )
    return hdr + body


def pack_move_to(id_: int, seq: int, x: int, y: int) -> bytes:
    return struct.pack(
        _HDR,
        MAGIC,
        1,
        TYPE_MOVE_TO,
        0,
        0,
        id_ & 0xFFFF,
        seq & 0xFFFF,
        int(x),
        int(y),
        0,
        0,
        0,
        FMT_RGB565,
        0,
        0,
    )


def pack_line_to(id_: int, seq: int, x: int, y: int, color: int) -> bytes:
    return struct.pack(
        _HDR,
        MAGIC,
        1,
        TYPE_LINE_TO,
        0,
        0,
        id_ & 0xFFFF,
        seq & 0xFFFF,
        int(x),
        int(y),
        0,
        0,
        0,
        FMT_RGB565,
        color & 0xFFFF,
        0,
    )


def pack_cubic_to(
    id_: int, seq: int, color: int, x1: int, y1: int, x2: int, y2: int, x3: int, y3: int
) -> bytes:
    body = struct.pack("<hhhhhh", x1, y1, x2, y2, x3, y3)
    hdr = struct.pack(
        _HDR,
        MAGIC,
        1,
        TYPE_CUBIC_TO,
        0,
        0,
        id_ & 0xFFFF,
        seq & 0xFFFF,
        0,
        0,
        0,
        0,
        0,
        FMT_RGB565,
        color & 0xFFFF,
        len(body),
    )
    return hdr + body


def topic_bind_set(device: str, slot: int) -> str:
    return f"wd/{device}/bind/{slot}/set"


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


def mqtt_client(host: str, port: int, client_id: str):
    try:
        import paho.mqtt.client as mqtt
    except ImportError as e:  # pragma: no cover
        raise ImportError("glasslink requires paho-mqtt") from e
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


def encode_rgb(enc_name: str, w: int, h: int, raw: bytes) -> tuple[str, bytes]:
    """Encode raw RGB565 LE via host encode_rect (product: raw only)."""
    need = w * h * 2
    if len(raw) != need:
        raise ValueError(f"rgb size {len(raw)} != {need} for {w}x{h}")
    if enc_name not in ("raw", "auto"):
        raise ValueError("product tools: only raw (delta → ../delta-rle-lab)")
    enc_bin = ensure_encode_rect()
    r = subprocess.run(
        [str(enc_bin), "--enc", "raw", "--w", str(w), "--h", str(h)],
        input=raw,
        capture_output=True,
        check=True,
    )
    return "raw", r.stdout


def enc_id(name: str) -> int:
    if name == "delta":
        raise ValueError("delta removed from product (see ../delta-rle-lab)")
    return ENC_RAW


def check_inline_max(msg: bytes, *, enforce: bool) -> None:
    if len(msg) > INLINE_MAX:
        msg_txt = f"envelope {len(msg)} B exceeds inline_max={INLINE_MAX}"
        if enforce:
            raise ValueError(msg_txt)
        print(f"warn: {msg_txt}", file=sys.stderr)


def peek_id_seq(payload: bytes) -> tuple[int, int]:
    if len(payload) < 12:
        return 0, 0
    id_, seq = struct.unpack_from("<HH", payload, 8)
    return int(id_), int(seq)


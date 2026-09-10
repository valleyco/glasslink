#!/usr/bin/env python3
"""
Host chart recipes (W25) — compose gauge / bar / pie from L1 batch ops.

Returns lists suitable for wd_mqtt.pack_batch / group_define.
"""

from __future__ import annotations

import math


def bar_graph(
    x: int,
    y: int,
    w: int,
    h: int,
    values: list[float],
    *,
    fg: int = 0x07E0,
    bg: int = 0x2104,
    gap: int = 2,
) -> list:
    """Horizontal row of vertical bars; values in 0..1."""
    ops: list = [{"fill": {"x": x, "y": y, "w": w, "h": h, "color": bg}}]
    n = max(1, len(values))
    bw = max(1, (w - gap * (n + 1)) // n)
    for i, v in enumerate(values):
        v = max(0.0, min(1.0, float(v)))
        bh = max(1, int(round(h * v)))
        bx = x + gap + i * (bw + gap)
        by = y + h - bh
        ops.append({"fill": {"x": bx, "y": by, "w": bw, "h": bh, "color": fg}})
    return ops


def gauge(
    cx: int,
    cy: int,
    r: int,
    value: float,
    *,
    fg: int = 0xFFE0,
    bg: int = 0x8410,
    needle: int = 0xF800,
) -> list:
    """Simple semicircle gauge (bottom flat); value 0..1. Uses poly wedges + needle line."""
    value = max(0.0, min(1.0, float(value)))
    ops: list = []
    # Background arc as polygon fan (8 segments, upper semicircle)
    segs = 8
    pts = [[cx, cy]]
    for i in range(segs + 1):
        a = math.pi + (math.pi * i / segs)  # pi .. 2pi
        pts.append([int(cx + r * math.cos(a)), int(cy + r * math.sin(a))])
    ops.append({"poly": {"color": bg, "points": pts}})
    # Value wedge
    nfill = max(1, int(round(segs * value)))
    pts_v = [[cx, cy]]
    for i in range(nfill + 1):
        a = math.pi + (math.pi * i / segs)
        pts_v.append([int(cx + r * math.cos(a)), int(cy + r * math.sin(a))])
    if len(pts_v) >= 3:
        ops.append({"poly": {"color": fg, "points": pts_v}})
    # Needle
    a = math.pi + math.pi * value
    nx = int(cx + (r - 2) * math.cos(a))
    ny = int(cy + (r - 2) * math.sin(a))
    ops.append({"move_to": {"x": cx, "y": cy}})
    ops.append({"line_to": {"x": nx, "y": ny, "color": needle}})
    return ops


def pie(
    cx: int,
    cy: int,
    r: int,
    shares: list[float],
    colors: list[int] | None = None,
) -> list:
    """Pie chart; shares normalized to sum. Each slice ≥3 pts poly."""
    ops: list = []
    total = sum(max(0.0, float(s)) for s in shares) or 1.0
    palette = colors or (0xF800, 0x07E0, 0x001F, 0xFFE0, 0x07FF, 0xF81F)
    ang = -math.pi / 2
    for i, s in enumerate(shares):
        frac = max(0.0, float(s)) / total
        if frac <= 0:
            continue
        sweep = frac * 2 * math.pi
        steps = max(3, int(round(16 * frac)))
        pts = [[cx, cy]]
        for k in range(steps + 1):
            a = ang + sweep * k / steps
            pts.append([int(cx + r * math.cos(a)), int(cy + r * math.sin(a))])
        ops.append({"poly": {"color": palette[i % len(palette)], "points": pts}})
        ang += sweep
    return ops

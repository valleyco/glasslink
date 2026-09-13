#!/usr/bin/env python3
"""Host unit smoke for glasslink pack/charts (no broker)."""

from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "sdk" / "python"))

import glasslink as gl  # noqa: E402


def main() -> int:
    n = 0
    clear = gl.pack_clear(1, 1, 0xF800)
    assert clear[:4] == gl.MAGIC and len(clear) == gl.HDR_SIZE
    n += 1
    batch = gl.pack_batch(
        1,
        2,
        [{"fill": {"x": 0, "y": 0, "w": 10, "h": 10, "color": 0x07E0}}],
    )
    assert batch[:4] == gl.MAGIC and len(batch) > gl.HDR_SIZE
    n += 1
    ops = gl.bar_graph(0, 0, 40, 20, [0.5, 1.0])
    assert any("fill" in o for o in ops)
    n += 1
    assert gl.topic_cmd("cyd1") == "wd/cyd1/cmd"
    n += 1
    print(f"== test_glasslink ==\nasserts: {n} ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

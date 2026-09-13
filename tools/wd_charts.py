#!/usr/bin/env python3
"""Shim → glasslink.charts (kept for older imports)."""

from __future__ import annotations

import sys
from pathlib import Path

_SDK = Path(__file__).resolve().parents[1] / "sdk" / "python"
if _SDK.is_dir() and str(_SDK) not in sys.path:
    sys.path.insert(0, str(_SDK))

from glasslink.charts import bar_graph, gauge, pie  # noqa: E402

__all__ = ["bar_graph", "gauge", "pie"]

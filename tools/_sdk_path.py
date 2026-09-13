"""Ensure repo sdk/python is on sys.path (when not pip-installed)."""

from __future__ import annotations

import sys
from pathlib import Path

_SDK = Path(__file__).resolve().parents[1] / "sdk" / "python"


def ensure_sdk_path() -> None:
    if _SDK.is_dir() and str(_SDK) not in sys.path:
        sys.path.insert(0, str(_SDK))

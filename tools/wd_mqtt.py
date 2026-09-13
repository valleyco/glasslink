#!/usr/bin/env python3
"""
CLI shim → glasslink SDK. Prefer `pip install -e sdk/python` + `glasslink …`.

Keeps `./tools/wd_mqtt.py` and `import wd_mqtt as wm` working for demos/Makefiles.
"""

from __future__ import annotations

import os
import sys
from pathlib import Path

_REPO = Path(__file__).resolve().parents[1]
_SDK = _REPO / "sdk" / "python"
if _SDK.is_dir() and str(_SDK) not in sys.path:
    sys.path.insert(0, str(_SDK))

_VENV_PY = Path(__file__).resolve().parent / ".venv" / "bin" / "python"
try:
    import paho.mqtt.client  # noqa: F401
except ImportError:
    if _VENV_PY.exists() and os.path.realpath(sys.executable) != os.path.realpath(_VENV_PY):
        os.execv(str(_VENV_PY), [str(_VENV_PY), str(Path(__file__).resolve()), *sys.argv[1:]])
    print(
        f"Need paho-mqtt (pip install -r tools/requirements.txt; venv={_VENV_PY}).",
        file=sys.stderr,
    )
    sys.exit(1)

import glasslink.protocol as _proto  # noqa: E402
from glasslink.cli import main  # noqa: E402

# Re-export protocol surface for `import wd_mqtt as wm`.
for _name in dir(_proto):
    if _name.startswith("_"):
        continue
    globals()[_name] = getattr(_proto, _name)
del _name, _proto

if __name__ == "__main__":
    raise SystemExit(main())

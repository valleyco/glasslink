#!/usr/bin/env python3
"""
Audit ESP-IDF linker map: const tables in .rodata (flash), mutable pools in .bss (DRAM).

Usage:
  ./tools/audit_elf_mem.py [path/to/esp32-wl-display.map]
  make audit-mem   # after build-mqtt

Exit 0 if checks pass; 1 if a required symbol is missing or in the wrong section class.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_MAP = ROOT / "build-esp32-mqtt" / "esp32-wl-display.map"

# Must stay flash-backed (.rodata*) — not copied into DRAM as .data/.bss.
EXPECT_RODATA = ("FONT5X7",)

# Must be mutable DRAM (.bss*).
EXPECT_BSS = (
    "s_body",  # HTTP static RX pool
    "s_groups",  # group.define storage
    "s_slots",  # bind slots
)

# .rodata.FONT5X7
# .bss.s_body    0x3ffb4f3c    0x10000 ...
LINE_RE = re.compile(
    r"^\s*(\.(?:rodata|bss|data)\.([\w.]+))"
    r"(?:\s+0x([0-9a-fA-F]+)\s+0x([0-9a-fA-F]+))?",
    re.M,
)


def classify(section: str) -> str:
    s = section.lower()
    if "rodata" in s:
        return "rodata"
    if "bss" in s:
        return "bss"
    if "data" in s:
        return "data"
    return "other"


def parse_map(text: str) -> dict[str, tuple[str, str, int | None]]:
    """symbol -> (section, class, size_or_None)."""
    found: dict[str, tuple[str, str, int | None]] = {}
    for m in LINE_RE.finditer(text):
        section, sym = m.group(1), m.group(2)
        if sym in ("rodata", "bss", "data"):
            continue
        # Prefer leaf name (FONT5X7, s_body); skip str1.4-style string junk as keys
        # we care about by allowing any; EXPECT_* filters.
        size = int(m.group(4), 16) if m.group(4) else None
        prev = found.get(sym)
        # Keep entry that has a size if we see a continuation-less header first
        if prev is None or (prev[2] is None and size is not None):
            found[sym] = (section, classify(section), size)
    # Second pass: sizes often on the *next* line after ".rodata.FONT5X7"
    for m in re.finditer(
        r"^\s*(\.(?:rodata|bss|data)\.([\w.]+))\s*\n\s*0x[0-9a-fA-F]+\s+0x([0-9a-fA-F]+)",
        text,
        re.M,
    ):
        section, sym, size_s = m.group(1), m.group(2), m.group(3)
        found[sym] = (section, classify(section), int(size_s, 16))
    return found


def main() -> int:
    map_path = Path(sys.argv[1]) if len(sys.argv) > 1 else DEFAULT_MAP
    if not map_path.is_file():
        print(f"missing map: {map_path}", file=sys.stderr)
        print("hint: make build-mqtt first", file=sys.stderr)
        return 1

    text = map_path.read_text(errors="replace")
    found = parse_map(text)
    rc = 0

    print(f"map: {map_path}")
    print("\n## Const / flash (.rodata)")
    for sym in EXPECT_RODATA:
        if sym not in found:
            print(f"  FAIL {sym}: not found")
            rc = 1
            continue
        sec, cls, size = found[sym]
        ok = cls == "rodata"
        sz = f", {size} B" if size is not None else ""
        print(f"  {'OK' if ok else 'FAIL'} {sym}: {sec} ({cls}{sz})")
        if not ok:
            rc = 1

    print("\n## Mutable / DRAM (.bss)")
    for sym in EXPECT_BSS:
        if sym not in found:
            print(f"  FAIL {sym}: not found")
            rc = 1
            continue
        sec, cls, size = found[sym]
        ok = cls == "bss"
        sz = f", {size} B" if size is not None else ""
        print(f"  {'OK' if ok else 'FAIL'} {sym}: {sec} ({cls}{sz})")
        if not ok:
            rc = 1

    print("\n## Policy")
    print("  static const → .rodata (flash DROM); no PROGMEM / IDF attrs required")
    print("  mutable static pools → .bss (DRAM) by design")
    print("  host-shared C stays free of ESP_PLATFORM placement macros")

    if rc == 0:
        print("\naudit-mem: PASS")
    else:
        print("\naudit-mem: FAIL", file=sys.stderr)
    return rc


if __name__ == "__main__":
    sys.exit(main())

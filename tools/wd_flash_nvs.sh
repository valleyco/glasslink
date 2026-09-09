#!/usr/bin/env bash
# Generate NVS partition image from tools/nvs.csv and flash to device.
# Requires IDF env (source export.sh) and tools/nvs.csv (see nvs.example.csv).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CSV="${WD_NVS_CSV:-$ROOT/tools/nvs.csv}"
OUT="${WD_NVS_BIN:-$ROOT/tools/nvs.bin}"
PORT="${PORT:-/dev/ttyUSB0}"
# Default single-app table: nvs @ 0x9000 size 0x6000
NVS_OFFSET="${NVS_OFFSET:-0x9000}"
NVS_SIZE="${NVS_SIZE:-0x6000}"

if [[ ! -f "$CSV" ]]; then
  echo "Missing $CSV — copy tools/nvs.example.csv and fill secrets." >&2
  exit 1
fi

# Prepend namespace row required by generator
TMP="$(mktemp)"
{
  echo "key,type,encoding,value"
  echo "wd,namespace,,"
  # Skip header if present in user csv
  if head -1 "$CSV" | grep -qi '^key,'; then
    tail -n +2 "$CSV"
  else
    cat "$CSV"
  fi
} >"$TMP"

GEN="${IDF_PATH:?IDF_PATH not set}/components/nvs_flash/nvs_partition_generator/nvs_partition_gen.py"
python3 "$GEN" generate "$TMP" "$OUT" "$NVS_SIZE"
rm -f "$TMP"
echo "Wrote $OUT — flashing @ $NVS_OFFSET on $PORT"
esptool.py -p "$PORT" write_flash "$NVS_OFFSET" "$OUT"
echo "NVS flash done."

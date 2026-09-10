# Memory placement (ESP32 / no PSRAM)

## Policy

1. **Immutable tables** use `static const` in shared C (`codec` / `contract` / `render` / …).  
   On IDF they link into **`.rodata`** (flash-mapped DROM). Do **not** add Arduino `PROGMEM` or IDF-only attributes in those components — they must stay host-buildable.
2. **Mutable pools** (`static` without `const`) live in **`.bss`** DRAM: HTTP body, group batches, bind slots, SPI strip buffers.
3. Prefer stack scratch (codec decode rows) over heap for hot paths (Step 13).

## Audit

After `make build-mqtt`:

```bash
make audit-mem
# or: ./tools/audit_elf_mem.py build-esp32-mqtt/esp32-wl-display.map
```

Expect:

| Symbol | Section |
|--------|---------|
| `FONT5X7` | `.rodata` |
| `s_body` | `.bss` (~64 KiB) |
| `s_groups` | `.bss` (~4 KiB) |
| `s_slots` | `.bss` (binds) |

`nm` type letters can be misleading on Xtensa (flash-backed symbols sometimes show as `d`); **trust the map section name**.

## Related

- Heap / fragmentation: PLAN Step 13 / W18  
- Live heap on glass: MQTT `status` `heap_free` / `heap_largest`

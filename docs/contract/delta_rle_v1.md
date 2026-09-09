# delta_rle_v1 bitstream (**FROZEN** — Step 7, 2026-09-09)

`enc = CODEC_ENC_DELTA_RLE_V1` (1). Host/device C in `components/codec`.  
See also [`docs/benches/codec-v1.md`](../benches/codec-v1.md).

## Prediction (RGB565 samples, uint16 wrap)

Input: row-major `rgb[y*w + x]`.

1. **Vertical:** for `y = 1 .. h-1`, for all `x`:  
   `p[y][x] = rgb[y][x] - rgb[y-1][x]`  
   row `y=0`: `p[0][x] = rgb[0][x]`
2. **Horizontal DPCM** on the vertical residual (use the *pre-horizontal* left sample, not an already-delta’d neighbor):  
   for each `y`, let `v[x] = p[y][x]` after step 1; then  
   `p[y][0] = v[0]`,  
   `p[y][x] = v[x] - v[x-1]` for `x = 1 .. w-1`  
   (implementation may walk right→left in-place so the left cell is still `v[x-1]`).

Decode: undo horizontal left→right per row (`row[x] += row[x-1]`), then vertical top→bottom (`row[y][x] += row[y-1][x]`).

## Entropy stage

Flatten `p` as **little-endian** `uint16` bytes (`w*h*2` bytes).  
Apply byte RLE:

| Control `c` | Meaning |
|-------------|---------|
| `0x00..0x7F` | `(c+1)` literal bytes follow |
| `0x80..0xFF` | repeat the next byte `(c-0x80+1)` times |

Max length per opcode: **128**.  
Encoder must not emit a literal longer than 128 (clamp before writing `c = lit-1`); a longer `lit` would wrap the control byte into the run range.

## Limits

- `w` in `1 .. 320` (`CODEC_MAX_WIDTH`)
- `h >= 1`

## Notes

- Encoder may use heap for scratch on host; device path can be tightened later (caller buffers).
- Streaming decode expands RLE then emits rows via callback (still needs residual buffer of `w*h*2` in current impl — improve in tune phase if needed).

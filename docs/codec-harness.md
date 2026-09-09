# Codec correctness, corpus, and tuning harness

Status: design for Steps 2 + 7 (and ongoing).  
Goals: (1) never ship a broken decoder, (2) **measure** compression on realistic content, (3) **tune** delta_rle / variants with evidence.

---

## 1. Two tracks (do not mix)

| Track | Purpose | Gate | Fail = |
|-------|---------|------|--------|
| **A. Correctness** | Bit-exact round-trip, edge cases, streaming API | `make test-codec` | CI red |
| **B. Corpus / tune** | Size, encode/decode µs, peak scratch, profile wins | `make bench-codec` | Report only (or soft budget warnings) |

Correctness fixtures are **tiny hand-made** bitstreams.  
Corpus images are **synthetic generators + optional real PNGs** converted to RGB565.

Tuning never changes the decoder “by feel” — only via **named profiles** + bench tables.

---

## 2. Wire identity vs tune knobs

### Stable on the wire (v1)

```text
enc = raw_rgb565 | delta_rle_v1 | (later plain_rle, qoi, …)
```

`delta_rle_v1` should mean **one normative bitstream** once frozen (test vectors locked).

### Experimental knobs (host bench / `delta_rle_dev`)

Until freeze, encoder may take a **profile** struct. Profiles that win can become `delta_rle_v1` or `delta_rle_v2`.

| Knob | Options (initial) | Why |
|------|-------------------|-----|
| `color_mode` | `rgb565_packed` · `split_rgb` · `yuv_approx` | goal.md planes/YUV |
| `predict` | `none` · `h` · `v` · `h_then_v` · `v_then_h` · `paeth_lite` | Stage 1 order |
| `delta_domain` | `u16_wrapping` · `per_channel5/6/5` | residual distribution |
| `rle` | `off` · `byte` · `unit16` · `ctrl_lvgl_style` | Stage 2 |
| `rle_max_run` | 16 / 64 / 128 / 255 | opcode design |
| `row_filter_reset` | every row · never · every N | error isolation / ratio |
| `endian` | host LE recorded in header flag | cross tools |

**Rule:** decoder for a given `enc` id has **no runtime knobs** (device simplicity).  
Knobs exist only when selecting which `enc` / version to emit.

---

## 3. Image / rect classes (corpus taxonomy)

Each case is a **rect** (full 320×240 or partial). Tag every asset:

```text
class / size / motion? / notes
```

### 3.1 Synthetic generators (primary — deterministic, no binary blobs in git required)

| Class ID | Content | Stresses |
|----------|---------|----------|
| `solid` | single color | max RLE |
| `h_runs` | horizontal stripes / long H runs | H-predict + RLE |
| `v_runs` | vertical stripes | V-predict |
| `grid` | checker / fine grid | worst-case RLE |
| `gradient_h` | smooth H gradient | delta small, RLE weak |
| `gradient_v` | smooth V gradient | V-delta |
| `gradient_2d` | diagonal/radial | mixed |
| `ui_panel` | title bar + body + border boxes | real dashboard-ish |
| `ui_text` | glyph-like blocks / 8×8 font stamp | high spatial freq, sparse |
| `ui_gauge` | arcs approximated by blocks | mixed edges |
| `noise_white` | full random RGB565 | compression floor (~raw) |
| `noise_1bit` | dithered B/W noise | mid |
| `photo_soft` | low-pass noise + blobs | “photo-ish” without assets |
| `sparse_dirty` | mostly solid + small widgets | **partial update** reality |
| `partial_tl` / `partial_br` | small rects at corners | clip + tiny streams |
| `adversarial_rle` | alternating pixels | RLE expansion risk |

Sizes: `8×8`, `16×16`, `32×32`, `64×48`, `128×128`, `320×240`, plus odd `319×1`, `1×240`, `3×5`.

### 3.2 Optional real images (`testdata/corpus/real/`)

- UI screenshots (PNG → RGB565 via host tool)
- One photo wallpaper  
- Git-LFS or download script if large; **not required for CI**

Manifest: `testdata/corpus/manifest.json` lists id, class, path or generator+seed, w, h.

### 3.3 Sequence corpus (later — goal.md image sequence)

| Class | Pattern |
|-------|---------|
| `seq_static` | identical frames |
| `seq_cursor` | 8×8 block moves 1px/frame |
| `seq_scroll` | vertical scroll of text rows |
| `seq_clock` | digits region only changes |

Metrics: sum of dirty-rect sizes vs full frame; keyframe interval sweep.

---

## 4. Metrics (every bench row)

| Metric | Definition |
|--------|------------|
| `raw_bytes` | `w * h * 2` |
| `comp_bytes` | bitstream length (payload only) |
| `ratio` | `raw_bytes / comp_bytes` (≥1 better) |
| `save_pct` | `100 * (1 - comp/raw)` |
| `enc_us` | host encode time |
| `dec_us` | host decode time (C decoder) |
| `scratch_peak` | max extra RAM decoder needs (lines + state) |
| `expansion` | true if `comp_bytes > raw_bytes` (flag) |
| `ok` | round-trip match |

Aggregate: **geo-mean ratio** per class; **p95 dec_us** on 320×240; **worst expansion**.

### Device budgets (planning — measure later on CYD)

| Budget | Target (v1 lean) |
|--------|------------------|
| Decoder scratch | ≤ 2 lines + ≤ 256 B state (~1.5 KiB) |
| Prefer | `comp_bytes ≤ inline_max` for typical dirty rects |
| Decode | fast enough that SPI dominates (order-of-magnitude) |

---

## 5. Harness layout

```text
components/codec/           # encode + decode (C), profile behind enc id
host/codec/
  test_*.c                  # Track A correctness
  bench_main.c              # Track B runner
  gen/                      # synthetic image generators
tools/corpus/               # optional PNG→565, manifest lint
testdata/codec/
  golden/                   # fixed bitstreams for enc=delta_rle_v1
  corpus/manifest.json      # bench cases
docs/benches/               # committed summary tables (Step 7+)
```

### Commands

```bash
make test-codec          # Track A — must stay green
make bench-codec         # Track B — prints markdown/CSV
make bench-codec PROFILE=all
make bench-codec CLASS=ui_panel,sparse_dirty
make bench-codec TUNE=sweep   # grid search over knobs → ranked table
```

### Bench output (example)

```markdown
| case | class | enc/profile | raw | comp | ratio | dec_us | expand |
| ... |
## Rank by geo-mean ratio (UI classes only)
## Rank by dec_us
## Pareto: ratio vs dec_us
```

---

## 6. Tuning process (how we pick “best”)

### Phase T0 — Correctness freeze path

1. Implement `raw_rgb565`  
2. Implement one simple `delta_rle_dev` (e.g. `v_then_h` + byte RLE)  
3. Goldens + round-trip on all synthetic classes (even if ratios poor)

### Phase T1 — Single-knob sweeps

Fix all but one knob; sweep on **core set**:  
`solid`, `h_runs`, `v_runs`, `grid`, `gradient_2d`, `ui_panel`, `ui_text`, `sparse_dirty`, `noise_white`, `320×240` + `32×32`.

Record tables in `docs/benches/`.

### Phase T2 — Pareto pick

Define **primary score** for v1 product:

```text
score = geo_mean_ratio(classes in {ui_*, sparse_*, solid, h_runs, v_runs})
subject to:
  - no mandatory expand on solid/h_runs (or allow “store raw” escape)
  - scratch ≤ budget
  - dec_us within 2× of raw memcpy baseline on host for 64×64
```

Secondary: photo/noise (informational — may stay raw or JPEG later).

### Phase T3 — Escape hatches (encoder policy, not new knobs on device)

Host encoder **policy**:

```text
if predicted_comp >= raw * 0.98 → send enc=raw_rgb565
else → enc=delta_rle_v1
```

Optional: per-class force raw (noise).

### Phase T4 — Freeze `delta_rle_v1`

- Lock bitstream doc + goldens  
- Remove unused knobs from device decode path  
- Keep sweep code under `host/codec` for v2 experiments (`delta_rle_v2`)

---

## 7. Correctness battery (Track A) — extensive but cheap

Beyond round-trip on generators:

| Test | Intent |
|------|--------|
| Empty/1×1/odd sizes | framing |
| Max run / run break | RLE borders |
| All-zero deltas | |
| Wraparound delta (0x0000 vs 0xFFFF neighbors) | |
| Truncated stream / overlong run | reject errors |
| Streaming: row callback count == h | no full FB |
| Fuzz: random RGB565 small rects N=1000 seeds | round-trip |
| Bit-exact golden for frozen profile | regression |

Fuzz is still **host CI-friendly** (seconds, not minutes).

---

## 8. Relation to MQTT / inline_max

Bench should mark cases where `comp_bytes > inline_max` (e.g. 6144):

- drives dirty-rect sizing advice  
- triggers future `B-http`  
- encoder may split rects (later policy)

---

## 9. What we explicitly defer

- On-device cycle counters in v1 harness (add when firmware exists)  
- Human SSIM (lossless path — N/A)  
- Training Huffman tables until residual histograms from T1 say so  
- Real video codecs  

---

## 10. Suggested PLAN mapping

| Step | Deliverable |
|------|-------------|
| **2a** | Track A: raw + one delta_rle_dev + tests/fuzz |
| **2b** | Generators + `bench-codec` MVP + first table |
| **7** | Full sweep, freeze v1 profile, `docs/benches/codec-v1.md` |
| ongoing | Real PNG corpus optional |

Step 2 “done” for pipeline unblock = **2a**.  
**2b** can complete before or with Step 7; don’t block contract/MQTT on full tune.

---

## 11. Decision hooks (for `/plan-decide` later)

- Freeze predict order: `v_then_h` vs `h_then_v`?  
- RLE unit: byte vs pixel?  
- Auto-raw fallback threshold?  
- Is expand-on-noise acceptable always?  

*This document is the harness contract; implement against it.*

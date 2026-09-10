# Codec v1 freeze (Step 7)

**Date:** 2026-09-09  
**Decision:** **KEEP** `raw_rgb565` + **`delta_rle_v1`** as v1 wire codecs.  
**Host policy:** `codec_encode_auto` — noisy/photo probe → raw; else delta unless `comp ≥ 0.98 × raw`, then raw.

Normative bitstream: [`docs/contract/delta_rle_v1.md`](../contract/delta_rle_v1.md) (**frozen**).  
Track A: `make test-codec` (raw + delta_rle + auto). Track B: `make bench-codec`.

---

## Primary score (harness T2)

```text
geo_mean_ratio(ui_panel, ui_text, ui_gauge, sparse_dirty, solid, h_runs, v_runs)
```

| Metric (host) | Value |
|---------------|-------|
| geo-mean ratio (primary classes) | **~37.7×** (n=11) |
| geo-mean ratio (all corpus) | **~11.1×** (n=26) |
| expand cases | 4 (noise/grid/tiny adversarial) — handled by auto-raw |
| Track A fails | 0 |

Re-run: `make bench-codec` (also refreshes `docs/benches/codec-bench-mvp.md`).

---

## Keep / kill

| Option | Verdict | Why |
|--------|---------|-----|
| `raw_rgb565` | **KEEP** (baseline) | Required; escape hatch; exact pixels |
| `delta_rle_v1` (v_then_h + byte RLE) | **KEEP / FREEZE** | Strong on UI/sparse/solid/runs; already on glass |
| Predict-order variants (`h_then_v`, …) | **defer** (`delta_rle_v2` host) | Would need new `enc` id; current profile meets score |
| Plain RLE (no delta) | **defer** | Unlikely to beat delta on UI; no device need |
| mini-QOI / JPEG | **kill for v1** | Noise/photo not primary; JPEG needs heavier deps; use raw or later B-* |
| Huffman / YUV planes | **kill for v1** | Backlog B6 |

---

## Encoder policy (host)

```text
if looks_noisy(rgb)          → enc=raw_rgb565   # sparse unique-color probe
encode delta_rle_v1
if delta_len * 100 >= raw_len * 98 → enc=raw_rgb565
else → enc=delta_rle_v1
```

Tools default to `--enc auto` (`inject rect`, `asset`, scene/demo encode).  
Large photos: prefer HTTP `uri` + `asset --enc auto` over many MQTT strips.

---

## inline_max note

Typical UI dirty rects compress far under **6144**. Full-frame 320×240 delta (~2.4 KiB solid/UI) fits; noisy full frames should stay **raw** or smaller dirty rects (else B-http).

---

## Freeze checklist

- [x] Bitstream doc locked (`delta_rle_v1.md`)
- [x] Track A green including auto policy
- [x] Primary geo-mean recorded
- [x] Auto-raw escape hatch
- [x] QOI/JPEG/plain-RLE not on v1 wire
- [ ] Optional: file goldens under `testdata/codec/golden/` (embedded asserts already cover)

---

## Sweep archive

Full knob `TUNE=sweep` / alternate predictors were **not** productized for v1. MVP benches (Step 2b) + this freeze are sufficient evidence to keep the shipping profile. Further sweeps belong under host-only `delta_rle_v2` experiments.

#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Synthetic corpus classes (docs/codec-harness.md §3.1). */
typedef enum corpus_class {
    CORPUS_SOLID = 0,
    CORPUS_H_RUNS,
    CORPUS_V_RUNS,
    CORPUS_GRID,
    CORPUS_GRADIENT_H,
    CORPUS_GRADIENT_V,
    CORPUS_GRADIENT_2D,
    CORPUS_UI_PANEL,
    CORPUS_UI_TEXT,
    CORPUS_UI_GAUGE,
    CORPUS_NOISE_WHITE,
    CORPUS_NOISE_1BIT,
    CORPUS_PHOTO_SOFT,
    CORPUS_SPARSE_DIRTY,
    CORPUS_PARTIAL_TL,
    CORPUS_PARTIAL_BR,
    CORPUS_ADVERSARIAL_RLE,
    CORPUS_CLASS_COUNT
} corpus_class_t;

const char *corpus_class_name(corpus_class_t cls);

/** Parse name; returns -1 if unknown. */
int corpus_class_from_name(const char *name);

/**
 * Fill tightly packed RGB565 buffer (w*h). Deterministic for (cls,w,h,seed).
 * @return 0 on success, -1 on bad args.
 */
int corpus_fill(corpus_class_t cls, uint16_t *rgb, int w, int h, unsigned seed);

#ifdef __cplusplus
}
#endif

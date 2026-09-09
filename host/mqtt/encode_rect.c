/*
 * Encode stdin raw RGB565 (w*h*2) to stdout codec bitstream.
 * encode_rect --enc raw|delta|auto --w W --h H < raw > out
 * With --enc auto, prints chosen enc name on stderr.
 */

#include "codec.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
    codec_enc_t enc = CODEC_ENC_RAW_RGB565;
    int use_auto = 0;
    int w = 0, h = 0;
    size_t need, got, bound, out_len = 0;
    uint16_t *rgb;
    uint8_t *out;
    int rc;
    codec_enc_t chosen = CODEC_ENC_RAW_RGB565;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--enc") == 0 && i + 1 < argc) {
            i++;
            if (strcmp(argv[i], "raw") == 0) {
                enc = CODEC_ENC_RAW_RGB565;
                use_auto = 0;
            } else if (strcmp(argv[i], "delta") == 0) {
                enc = CODEC_ENC_DELTA_RLE_V1;
                use_auto = 0;
            } else if (strcmp(argv[i], "auto") == 0) {
                use_auto = 1;
            } else {
                fprintf(stderr, "bad enc (raw|delta|auto)\n");
                return 2;
            }
        } else if (strcmp(argv[i], "--w") == 0 && i + 1 < argc) {
            w = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--h") == 0 && i + 1 < argc) {
            h = atoi(argv[++i]);
        }
    }
    if (w <= 0 || h <= 0) {
        fprintf(stderr, "need --w --h\n");
        return 2;
    }
    need = (size_t)w * (size_t)h * sizeof(uint16_t);
    rgb = (uint16_t *)malloc(need);
    if (!rgb) {
        return 2;
    }
    got = fread(rgb, 1, need, stdin);
    if (got != need) {
        fprintf(stderr, "short stdin got=%zu need=%zu\n", got, need);
        free(rgb);
        return 2;
    }
    bound = use_auto ? codec_encode_bound(CODEC_ENC_DELTA_RLE_V1, w, h)
                     : codec_encode_bound(enc, w, h);
    if (use_auto) {
        size_t raw_b = codec_encode_bound(CODEC_ENC_RAW_RGB565, w, h);
        if (raw_b > bound) {
            bound = raw_b;
        }
    }
    out = (uint8_t *)malloc(bound);
    if (!out) {
        free(rgb);
        return 2;
    }
    if (use_auto) {
        rc = codec_encode_auto(rgb, w, h, out, bound, &out_len, &chosen);
        fprintf(stderr, "enc=%s\n",
                chosen == CODEC_ENC_DELTA_RLE_V1 ? "delta" : "raw");
    } else {
        rc = codec_encode(enc, rgb, w, h, out, bound, &out_len);
    }
    free(rgb);
    if (rc != CODEC_OK) {
        fprintf(stderr, "encode rc=%d\n", rc);
        free(out);
        return 1;
    }
    if (fwrite(out, 1, out_len, stdout) != out_len) {
        free(out);
        return 2;
    }
    free(out);
    return 0;
}

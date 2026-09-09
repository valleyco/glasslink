/*
 * Encode stdin raw RGB565 (w*h*2) to stdout codec bitstream.
 * encode_rect --enc raw|delta --w W --h H < raw > out
 */

#include "codec.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
    codec_enc_t enc = CODEC_ENC_RAW_RGB565;
    int w = 0, h = 0;
    size_t need, got, bound, out_len = 0;
    uint16_t *rgb;
    uint8_t *out;
    int rc;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--enc") == 0 && i + 1 < argc) {
            i++;
            if (strcmp(argv[i], "raw") == 0) {
                enc = CODEC_ENC_RAW_RGB565;
            } else if (strcmp(argv[i], "delta") == 0) {
                enc = CODEC_ENC_DELTA_RLE_V1;
            } else {
                fprintf(stderr, "bad enc\n");
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
    bound = codec_encode_bound(enc, w, h);
    out = (uint8_t *)malloc(bound);
    if (!out) {
        free(rgb);
        return 2;
    }
    rc = codec_encode(enc, rgb, w, h, out, bound, &out_len);
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

/*
 * Pure form helpers (host-testable; no IDF).
 */

#include "wd_form.h"

#include <ctype.h>
#include <string.h>

static int hex_nibble(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

int wd_url_decode(char *dst, size_t dst_sz, const char *src, size_t src_len)
{
    size_t di = 0;
    size_t si = 0;

    if (!dst || dst_sz == 0 || (!src && src_len > 0)) {
        return -1;
    }
    while (si < src_len) {
        char c = src[si++];
        if (c == '+') {
            c = ' ';
        } else if (c == '%' && si + 1 < src_len) {
            int hi = hex_nibble(src[si]);
            int lo = hex_nibble(src[si + 1]);
            if (hi < 0 || lo < 0) {
                return -1;
            }
            c = (char)((hi << 4) | lo);
            si += 2;
        }
        if (di + 1 >= dst_sz) {
            return -1;
        }
        dst[di++] = c;
    }
    dst[di] = '\0';
    return 0;
}

int wd_form_get_field(const char *body, size_t body_len, const char *key,
                      char *out, size_t out_sz)
{
    size_t key_len;
    size_t i = 0;

    if (!body || !key || !out || out_sz == 0) {
        return -1;
    }
    key_len = strlen(key);
    if (key_len == 0) {
        return -1;
    }

    while (i < body_len) {
        size_t start = i;
        size_t eq = body_len;
        size_t amp = body_len;
        size_t j;

        for (j = i; j < body_len; j++) {
            if (body[j] == '=') {
                eq = j;
                break;
            }
            if (body[j] == '&') {
                amp = j;
                break;
            }
        }
        if (eq == body_len) {
            /* no '=' — skip token */
            if (amp == body_len) {
                break;
            }
            i = amp + 1;
            continue;
        }
        for (j = eq + 1; j < body_len; j++) {
            if (body[j] == '&') {
                amp = j;
                break;
            }
        }
        if ((eq - start) == key_len && memcmp(body + start, key, key_len) == 0) {
            size_t vlen = amp - (eq + 1);
            return wd_url_decode(out, out_sz, body + eq + 1, vlen);
        }
        if (amp >= body_len) {
            break;
        }
        i = amp + 1;
    }
    out[0] = '\0';
    return 1;
}

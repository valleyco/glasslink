#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Percent-decode src into dst (NUL-terminated). '+' → space. Returns 0 or -1. */
int wd_url_decode(char *dst, size_t dst_sz, const char *src, size_t src_len);

/**
 * Extract one application/x-www-form-urlencoded field (URL-decoded).
 * Returns 0 if found, 1 if missing, -1 on error.
 */
int wd_form_get_field(const char *body, size_t body_len, const char *key,
                      char *out, size_t out_sz);

#ifdef __cplusplus
}
#endif

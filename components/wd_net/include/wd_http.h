#pragma once

#include "contract.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Max accepted HTTP body (bytes). Advertised in MQTT status. */
size_t wd_http_max_body(void);

/**
 * contract_fetch_fn: GET http:// URL → static body pool (valid until next fetch).
 * Pair with contract_set_fetch_release(wd_http_release).
 */
int wd_http_fetch(const uint8_t *url, size_t url_len, uint8_t **body_out,
                  size_t *body_len_out, void *user);

/** No-op release for the static HTTP body pool. */
void wd_http_release(uint8_t *body, void *user);

#ifdef __cplusplus
}
#endif

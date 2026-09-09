#pragma once

#include "contract.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Max accepted HTTP body (bytes). Advertised in MQTT status. */
size_t wd_http_max_body(void);

/** contract_fetch_fn compatible: GET http:// URL → malloc'd body. */
int wd_http_fetch(const uint8_t *url, size_t url_len, uint8_t **body_out,
                  size_t *body_len_out, void *user);

#ifdef __cplusplus
}
#endif

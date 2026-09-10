/*
 * Thin HTTP GET for FLAG_URI raster.rect (Step 10-B / 13b).
 * LAN http:// only (no TLS — W10). Fixed static body buffer (no malloc churn).
 */

#include "wd_http.h"

#include "esp_http_client.h"
#include "esp_log.h"

#include <string.h>

static const char *TAG = "wd_http";

#ifndef WD_HTTP_MAX_BODY
#define WD_HTTP_MAX_BODY 65536
#endif

#ifndef WD_HTTP_TIMEOUT_MS
#define WD_HTTP_TIMEOUT_MS 8000
#endif

static uint8_t s_body[WD_HTTP_MAX_BODY];

size_t wd_http_max_body(void)
{
    return (size_t)WD_HTTP_MAX_BODY;
}

void wd_http_release(uint8_t *body, void *user)
{
    (void)body;
    (void)user;
    /* static pool — nothing to free */
}

int wd_http_fetch(const uint8_t *url, size_t url_len, uint8_t **body_out,
                  size_t *body_len_out, void *user)
{
    char urlz[CONTRACT_URI_MAX + 1];
    esp_http_client_config_t cfg;
    esp_http_client_handle_t client;
    int content_len;
    int status;
    int total = 0;
    int r;
    esp_err_t err;

    (void)user;
    if (!url || !body_out || !body_len_out) {
        return -1;
    }
    *body_out = NULL;
    *body_len_out = 0;

    if (url_len == 0 || url_len > CONTRACT_URI_MAX) {
        return -1;
    }
    /* plaintext HTTP only */
    if (url_len < 7 || memcmp(url, "http://", 7) != 0) {
        ESP_LOGW(TAG, "reject url (need http://)");
        return -1;
    }

    memcpy(urlz, url, url_len);
    urlz[url_len] = '\0';

    memset(&cfg, 0, sizeof(cfg));
    cfg.url = urlz;
    cfg.timeout_ms = WD_HTTP_TIMEOUT_MS;
    cfg.buffer_size = 1024;

    client = esp_http_client_init(&cfg);
    if (!client) {
        return -1;
    }

    err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "open failed: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return -1;
    }

    content_len = esp_http_client_fetch_headers(client);
    status = esp_http_client_get_status_code(client);
    if (status != 200) {
        ESP_LOGW(TAG, "HTTP status %d", status);
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return -1;
    }

    if (content_len > (int)WD_HTTP_MAX_BODY) {
        ESP_LOGW(TAG, "body %d > max %d", content_len, WD_HTTP_MAX_BODY);
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return -1;
    }

    {
        size_t cap = (content_len > 0) ? (size_t)content_len : (size_t)WD_HTTP_MAX_BODY;
        if (cap > (size_t)WD_HTTP_MAX_BODY) {
            cap = (size_t)WD_HTTP_MAX_BODY;
        }

        while (total < (int)cap) {
            r = esp_http_client_read(client, (char *)s_body + total,
                                     (int)cap - total);
            if (r < 0) {
                ESP_LOGW(TAG, "read err");
                esp_http_client_close(client);
                esp_http_client_cleanup(client);
                return -1;
            }
            if (r == 0) {
                break;
            }
            total += r;
            if (total > WD_HTTP_MAX_BODY) {
                esp_http_client_close(client);
                esp_http_client_cleanup(client);
                return -1;
            }
        }
    }

    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    if (total <= 0) {
        return -1;
    }

    ESP_LOGI(TAG, "fetched %d B from %s", total, urlz);
    *body_out = s_body;
    *body_len_out = (size_t)total;
    return 0;
}

/*
 * SoftAP + HTTP field provision (W26 / Step 23).
 * Open AP WD_PROVISION_AP_SSID → GET / form, POST /config → NVS + reboot.
 */

#include "wd_provision.h"

#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <stdio.h>
#include <string.h>

static const char *TAG = "wd_prov";
static wd_provision_ui_fn s_ui;
static wd_cfg_t s_cfg;
static httpd_handle_t s_httpd;

void wd_provision_set_ui(wd_provision_ui_fn fn)
{
    s_ui = fn;
}

static const char PAGE[] =
    "<!DOCTYPE html><html><head><meta name=viewport content=\"width=device-width,"
    "initial-scale=1\"><title>glasslink</title>"
    "<style>body{font-family:sans-serif;margin:1.2rem;background:#0b1220;color:#e8eef8}"
    "input{width:100%%;max-width:22rem;padding:.4rem;margin:.25rem 0 .8rem;box-sizing:border-box}"
    "label{font-size:.85rem;color:#9db0c9}button{padding:.5rem 1rem;background:#3d8bfd;"
    "border:0;color:#fff;border-radius:4px}</style></head><body>"
    "<h1>glasslink setup</h1>"
    "<form method=POST action=/config>"
    "<label>Wi-Fi SSID</label><br>"
    "<input name=wifi_ssid required value=\"%s\"><br>"
    "<label>Wi-Fi password</label><br>"
    "<input name=wifi_pass type=password value=\"%s\"><br>"
    "<label>MQTT URI</label><br>"
    "<input name=mqtt_uri required value=\"%s\"><br>"
    "<label>Device id</label><br>"
    "<input name=device_id required value=\"%s\"><br>"
    "<button type=submit>Save &amp; reboot</button>"
    "</form>"
    "<p style=\"color:#9db0c9;font-size:.8rem\">AP open · no TLS · LAN only</p>"
    "</body></html>";

static void html_escape_attr(char *dst, size_t dst_sz, const char *src)
{
    size_t di = 0;
    if (!dst || dst_sz == 0) {
        return;
    }
    if (!src) {
        dst[0] = '\0';
        return;
    }
    while (*src && di + 1 < dst_sz) {
        char c = *src++;
        if (c == '"' || c == '<' || c == '>' || c == '&') {
            dst[di++] = '_';
        } else {
            dst[di++] = c;
        }
    }
    dst[di] = '\0';
}

static esp_err_t send_form(httpd_req_t *req)
{
    char ssid[WD_CFG_SSID_MAX + 1];
    char pass[WD_CFG_PASS_MAX + 1];
    char uri[WD_CFG_URI_MAX + 1];
    char id[WD_CFG_ID_MAX + 1];
    char page[1400];
    int n;

    html_escape_attr(ssid, sizeof(ssid), s_cfg.wifi_ssid);
    html_escape_attr(pass, sizeof(pass), s_cfg.wifi_pass);
    html_escape_attr(uri, sizeof(uri), s_cfg.mqtt_uri);
    html_escape_attr(id, sizeof(id), s_cfg.device_id);

    n = snprintf(page, sizeof(page), PAGE, ssid, pass, uri, id);
    if (n < 0 || n >= (int)sizeof(page)) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "page");
        return ESP_FAIL;
    }
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, page, n);
    return ESP_OK;
}

static esp_err_t get_root(httpd_req_t *req)
{
    return send_form(req);
}

static void reboot_later(void *arg)
{
    (void)arg;
    vTaskDelay(pdMS_TO_TICKS(800));
    esp_restart();
}

static esp_err_t post_config(httpd_req_t *req)
{
    char body[384];
    int total = 0;
    int r;
    wd_cfg_t next = s_cfg;

    if (req->content_len <= 0 || req->content_len >= (int)sizeof(body)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "bad length");
        return ESP_FAIL;
    }
    while (total < req->content_len) {
        r = httpd_req_recv(req, body + total, req->content_len - total);
        if (r <= 0) {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "recv");
            return ESP_FAIL;
        }
        total += r;
    }
    body[total] = '\0';

    if (wd_cfg_apply_form(&next, body, (size_t)total) != 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
                            "need wifi_ssid + mqtt://... + device_id");
        return ESP_FAIL;
    }
    if (wd_cfg_save(&next) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "nvs");
        return ESP_FAIL;
    }
    s_cfg = next;
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_sendstr(req, "saved — rebooting\n");
    ESP_LOGI(TAG, "config saved ssid=%s device=%s — reboot", next.wifi_ssid,
             next.device_id);
    xTaskCreate(reboot_later, "prov_reboot", 2048, NULL, 5, NULL);
    return ESP_OK;
}

static void start_httpd(void)
{
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.server_port = 80;
    cfg.max_uri_handlers = 4;
    cfg.lru_purge_enable = true;

    if (httpd_start(&s_httpd, &cfg) != ESP_OK) {
        ESP_LOGE(TAG, "httpd_start failed");
        abort();
    }
    httpd_uri_t root = {.uri = "/", .method = HTTP_GET, .handler = get_root};
    httpd_uri_t conf_get = {.uri = "/config", .method = HTTP_GET, .handler = get_root};
    httpd_uri_t conf_post = {
        .uri = "/config", .method = HTTP_POST, .handler = post_config};
    httpd_register_uri_handler(s_httpd, &root);
    httpd_register_uri_handler(s_httpd, &conf_get);
    httpd_register_uri_handler(s_httpd, &conf_post);
    ESP_LOGI(TAG, "HTTP config at %s", WD_PROVISION_URL);
}

static void wifi_ap_start(void)
{
    wifi_config_t ap = {0};

    strncpy((char *)ap.ap.ssid, WD_PROVISION_AP_SSID, sizeof(ap.ap.ssid) - 1);
    ap.ap.ssid_len = (uint8_t)strlen(WD_PROVISION_AP_SSID);
    ap.ap.channel = 1;
    ap.ap.authmode = WIFI_AUTH_OPEN;
    ap.ap.max_connection = 4;

    (void)esp_wifi_stop();
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "SoftAP SSID=%s (open)", WD_PROVISION_AP_SSID);
}

void wd_provision_run(const wd_cfg_t *seed)
{
    if (seed) {
        s_cfg = *seed;
    } else {
        memset(&s_cfg, 0, sizeof(s_cfg));
        strncpy(s_cfg.mqtt_uri, "mqtt://192.168.0.1:1883", sizeof(s_cfg.mqtt_uri) - 1);
        strncpy(s_cfg.device_id, "cyd1", sizeof(s_cfg.device_id) - 1);
    }
    if (wd_cfg_needs_provision(&s_cfg)) {
        s_cfg.wifi_ssid[0] = '\0';
        s_cfg.wifi_pass[0] = '\0';
    }

    if (s_ui) {
        s_ui(WD_PROVISION_AP_SSID, WD_PROVISION_URL);
    }

    wifi_ap_start();
    start_httpd();

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

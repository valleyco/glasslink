/*
 * Thin esp-mqtt adapter: binary cmd payload → contract_dispatch.
 * No Wi-Fi here — caller brings up STA first.
 */

#include "wd_mqtt.h"

#include "bind.h"
#include "contract.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include "wd_http.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *TAG = "wd_mqtt";

static esp_mqtt_client_handle_t s_client;
static char s_device[32];
static char s_topic_cmd[64];
static char s_topic_lwt[64];
static char s_topic_status[64];
static char s_topic_ack[64];
static char s_topic_bind[72]; /* wd/{id}/bind/+/set */
static int s_inline_max = 6144;
static volatile bool s_connected;

static void publish_status(void)
{
    char body[448];
    size_t heap_free = (size_t)esp_get_free_heap_size();
    size_t heap_largest =
        heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    snprintf(body, sizeof(body),
             "{\"fw\":\"wl-display\",\"device_id\":\"%s\","
             "\"disp\":{\"w\":320,\"h\":240},"
             "\"inline_max\":%d,\"http\":true,\"http_max\":%u,"
             "\"l1\":[\"fill_rect\",\"text\",\"batch\",\"poly\",\"move_to\","
             "\"line_to\",\"cubic_to\"],\"bind_slots\":%d,"
             "\"groups\":%d,\"heap_free\":%u,\"heap_largest\":%u,"
             "\"codecs\":[\"raw_rgb565\"]}",
             s_device, s_inline_max, (unsigned)wd_http_max_body(),
             BIND_SLOT_MAX, CONTRACT_GROUP_SLOTS, (unsigned)heap_free,
             (unsigned)heap_largest);
    esp_mqtt_client_publish(s_client, s_topic_status, body, 0, 1, 1);
}

static void publish_ack(int rc, int n, unsigned id, unsigned seq)
{
    char body[96];
    snprintf(body, sizeof(body), "{\"rc\":%d,\"n\":%d,\"id\":%u,\"seq\":%u}", rc, n, id,
             seq);
    esp_mqtt_client_publish(s_client, s_topic_ack, body, 0, 0, 0);
}

static void handle_cmd(const uint8_t *data, int len)
{
    int rc;
    unsigned id = 0;
    unsigned seq = 0;
    if (!data || len <= 0) {
        return;
    }
    if (len >= 12) {
        id = (unsigned)data[8] | ((unsigned)data[9] << 8);
        seq = (unsigned)data[10] | ((unsigned)data[11] << 8);
    }
    ESP_LOGI(TAG, "cmd %d bytes id=%u seq=%u", len, id, seq);
    rc = contract_dispatch(data, (size_t)len);
    if (rc != CONTRACT_OK) {
        ESP_LOGW(TAG, "dispatch rc=%d", rc);
    }
    publish_ack(rc, len, id, seq);
}

/** Topic wd/{device}/bind/{slot}/set → update slot text. */
static int parse_bind_slot(const char *topic, int topic_len)
{
    /* expect .../bind/<digits>/set */
    const char *p;
    const char *end;
    char buf[64];
    int n;
    unsigned long slot;
    char *slash;

    if (topic_len <= 0 || topic_len >= (int)sizeof(buf)) {
        return -1;
    }
    memcpy(buf, topic, (size_t)topic_len);
    buf[topic_len] = '\0';
    p = strstr(buf, "/bind/");
    if (!p) {
        return -1;
    }
    p += 6;
    end = strstr(p, "/set");
    if (!end || end[4] != '\0') {
        return -1;
    }
    n = (int)(end - p);
    if (n <= 0 || n > 3) {
        return -1;
    }
    {
        char num[4];
        memcpy(num, p, (size_t)n);
        num[n] = '\0';
        slot = strtoul(num, &slash, 10);
        if (slash == num || *slash != '\0' || slot >= BIND_SLOT_MAX) {
            return -1;
        }
    }
    return (int)slot;
}

static void handle_bind_set(int slot, const uint8_t *data, int len)
{
    int rc;
    if (slot < 0 || !data || len <= 0) {
        return;
    }
    rc = bind_set_text((uint8_t)slot, data, (size_t)len);
    ESP_LOGI(TAG, "bind set slot=%d len=%d rc=%d", slot, len, rc);
}

static void mqtt_event(void *args, esp_event_base_t base, int32_t event_id,
                       void *event_data)
{
    esp_mqtt_event_handle_t ev = event_data;
    (void)args;
    (void)base;

    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        s_connected = true;
        ESP_LOGI(TAG, "connected; subscribe %s + %s", s_topic_cmd, s_topic_bind);
        esp_mqtt_client_publish(s_client, s_topic_lwt, "online", 0, 1, 1);
        publish_status();
        esp_mqtt_client_subscribe(s_client, s_topic_cmd, 1);
        esp_mqtt_client_subscribe(s_client, s_topic_bind, 1);
        break;
    case MQTT_EVENT_DISCONNECTED:
        s_connected = false;
        ESP_LOGW(TAG, "disconnected");
        break;
    case MQTT_EVENT_DATA:
        if (ev->topic_len > 0 && ev->topic) {
            if ((size_t)ev->topic_len == strlen(s_topic_cmd) &&
                memcmp(ev->topic, s_topic_cmd, (size_t)ev->topic_len) == 0) {
                handle_cmd((const uint8_t *)ev->data, ev->data_len);
                break;
            }
            {
                int slot = parse_bind_slot(ev->topic, ev->topic_len);
                if (slot >= 0) {
                    handle_bind_set(slot, (const uint8_t *)ev->data, ev->data_len);
                    break;
                }
            }
        }
        /* Fallback: treat as cmd if looks like WLD1 */
        if (ev->data_len >= 4 && ev->data &&
            memcmp(ev->data, "WLD1", 4) == 0) {
            handle_cmd((const uint8_t *)ev->data, ev->data_len);
        }
        break;
    default:
        break;
    }
}

void wd_mqtt_start(const wd_mqtt_cfg_t *cfg)
{
    if (!cfg || !cfg->uri || !cfg->device_id) {
        ESP_LOGE(TAG, "bad cfg");
        return;
    }
    if (s_client) {
        return;
    }

    snprintf(s_device, sizeof(s_device), "%s", cfg->device_id);
    snprintf(s_topic_cmd, sizeof(s_topic_cmd), "wd/%s/cmd", s_device);
    snprintf(s_topic_lwt, sizeof(s_topic_lwt), "wd/%s/lwt", s_device);
    snprintf(s_topic_status, sizeof(s_topic_status), "wd/%s/status", s_device);
    snprintf(s_topic_ack, sizeof(s_topic_ack), "wd/%s/ack", s_device);
    snprintf(s_topic_bind, sizeof(s_topic_bind), "wd/%s/bind/+/set", s_device);
    s_inline_max = cfg->inline_max > 0 ? cfg->inline_max : 6144;

    int buf = cfg->buffer_size > 0 ? cfg->buffer_size : 8192;
    if (buf < s_inline_max + 256) {
        buf = s_inline_max + 256;
    }

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = cfg->uri,
        .session =
            {
                .last_will =
                    {
                        .topic = s_topic_lwt,
                        .msg = "offline",
                        .msg_len = 7,
                        .qos = 1,
                        .retain = true,
                    },
            },
        .buffer.size = buf,
        .buffer.out_size = 1024,
    };

    s_client = esp_mqtt_client_init(&mqtt_cfg);
    if (!s_client) {
        ESP_LOGE(TAG, "init failed");
        return;
    }
    esp_mqtt_client_register_event(s_client, ESP_EVENT_ANY_ID, mqtt_event, NULL);
    ESP_LOGI(TAG, "start uri=%s device=%s buf=%d", cfg->uri, s_device, buf);
    esp_mqtt_client_start(s_client);
}

bool wd_mqtt_is_connected(void)
{
    return s_connected;
}

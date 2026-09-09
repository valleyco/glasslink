/*
 * Thin esp-mqtt adapter: binary cmd payload → contract_dispatch.
 * No Wi-Fi here — caller brings up STA first.
 */

#include "wd_mqtt.h"

#include "contract.h"
#include "esp_log.h"
#include "mqtt_client.h"

#include <stdio.h>
#include <string.h>

static const char *TAG = "wd_mqtt";

static esp_mqtt_client_handle_t s_client;
static char s_device[32];
static char s_topic_cmd[64];
static char s_topic_lwt[64];
static char s_topic_status[64];
static char s_topic_ack[64];
static int s_inline_max = 6144;
static volatile bool s_connected;

static void publish_status(void)
{
    char body[256];
    snprintf(body, sizeof(body),
             "{\"fw\":\"wl-display\",\"device_id\":\"%s\","
             "\"disp\":{\"w\":320,\"h\":240},"
             "\"inline_max\":%d,\"codecs\":[\"raw_rgb565\",\"delta_rle_v1\"]}",
             s_device, s_inline_max);
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

static void mqtt_event(void *args, esp_event_base_t base, int32_t event_id,
                       void *event_data)
{
    esp_mqtt_event_handle_t ev = event_data;
    (void)args;
    (void)base;

    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        s_connected = true;
        ESP_LOGI(TAG, "connected; subscribe %s", s_topic_cmd);
        esp_mqtt_client_publish(s_client, s_topic_lwt, "online", 0, 1, 1);
        publish_status();
        esp_mqtt_client_subscribe(s_client, s_topic_cmd, 1);
        break;
    case MQTT_EVENT_DISCONNECTED:
        s_connected = false;
        ESP_LOGW(TAG, "disconnected");
        break;
    case MQTT_EVENT_DATA:
        if (ev->topic_len > 0 && ev->topic &&
            (size_t)ev->topic_len == strlen(s_topic_cmd) &&
            memcmp(ev->topic, s_topic_cmd, (size_t)ev->topic_len) == 0) {
            handle_cmd((const uint8_t *)ev->data, ev->data_len);
        } else {
            /* Some builds deliver topic separately; still try dispatch if data */
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

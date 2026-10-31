// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2018 Barry Peterson

#include "mqtt_app.h"

#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "mqtt_client.h"
#include "sdkconfig.h"

static const char *TAG = "mqtt";
static const int CMD_LEN = 64;

static esp_mqtt_client_handle_t s_client;
static QueueHandle_t s_queue;
static volatile bool s_connected;
static bool s_started;

static void enqueue_command(const char *data, int data_len)
{
    if (s_queue == NULL || data == NULL || data_len <= 0) {
        return;
    }
    char item[64];
    int n = data_len;
    if (n >= CMD_LEN) {
        n = CMD_LEN - 1;
    }
    memcpy(item, data, n);
    item[n] = '\0';
    while (n > 0 && (item[n - 1] == '\r' || item[n - 1] == '\n' || item[n - 1] == ' ')) {
        item[--n] = '\0';
    }
    if (n == 0) {
        return;
    }
    if (xQueueSend(s_queue, item, 0) != pdTRUE) {
        ESP_LOGW(TAG, "command queue full, dropped");
    }
}

static void mqtt_event(void *args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    (void)args;
    (void)base;
    esp_mqtt_event_handle_t event = event_data;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        s_connected = true;
        ESP_LOGI(TAG, "connected");
        esp_mqtt_client_subscribe(event->client, "esp/msg", 0);
        break;
    case MQTT_EVENT_DISCONNECTED:
        s_connected = false;
        ESP_LOGW(TAG, "disconnected");
        break;
    case MQTT_EVENT_DATA:
        if (event->topic != NULL && event->topic_len == 7 && strncmp(event->topic, "esp/msg", 7) == 0) {
            enqueue_command(event->data, event->data_len);
        }
        break;
    case MQTT_EVENT_ERROR:
        if (event->error_handle != NULL && event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            ESP_LOGW(TAG, "transport error tls=%d sock=%d", event->error_handle->esp_tls_last_esp_err,
                     event->error_handle->esp_transport_sock_errno);
        } else {
            ESP_LOGW(TAG, "mqtt error");
        }
        break;
    default:
        break;
    }
}

esp_err_t mqtt_app_start(const char *client_id)
{
    if (s_started) {
        return ESP_OK;
    }
    if (client_id == NULL || client_id[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }
    if (CONFIG_IOTROBOT_MQTT_URI[0] == '\0') {
        ESP_LOGW(TAG, "MQTT URI is empty");
        return ESP_ERR_INVALID_STATE;
    }

    s_queue = xQueueCreate(8, CMD_LEN);
    if (s_queue == NULL) {
        return ESP_ERR_NO_MEM;
    }

    esp_mqtt_client_config_t cfg = {0};
    cfg.broker.address.uri = CONFIG_IOTROBOT_MQTT_URI;
    cfg.credentials.client_id = client_id;
    if (CONFIG_IOTROBOT_MQTT_USERNAME[0] != '\0') {
        cfg.credentials.username = CONFIG_IOTROBOT_MQTT_USERNAME;
        cfg.credentials.authentication.password = CONFIG_IOTROBOT_MQTT_PASSWORD;
    }
    cfg.network.disable_auto_reconnect = false;

    s_client = esp_mqtt_client_init(&cfg);
    if (s_client == NULL) {
        return ESP_ERR_NO_MEM;
    }
    esp_mqtt_client_register_event(s_client, ESP_EVENT_ANY_ID, mqtt_event, NULL);
    esp_err_t err = esp_mqtt_client_start(s_client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "start failed: %s", esp_err_to_name(err));
        return err;
    }
    s_started = true;
    ESP_LOGI(TAG, "client %s broker %s", client_id, CONFIG_IOTROBOT_MQTT_URI);
    return ESP_OK;
}

bool mqtt_app_connected(void)
{
    return s_connected;
}

void mqtt_app_publish(const char *topic, const char *payload)
{
    if (!s_connected || s_client == NULL || topic == NULL || payload == NULL) {
        return;
    }
    int msg_id = esp_mqtt_client_publish(s_client, topic, payload, 0, 0, 0);
    if (msg_id < 0) {
        ESP_LOGW(TAG, "publish %s failed", topic);
    }
}

bool mqtt_app_take_command(char *buf, size_t len)
{
    if (s_queue == NULL || buf == NULL || len == 0) {
        return false;
    }
    char item[64];
    if (xQueueReceive(s_queue, item, 0) != pdTRUE) {
        return false;
    }
    snprintf(buf, len, "%s", item);
    return true;
}

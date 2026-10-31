// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2018 Barry Peterson

#include "wifi_sta.h"

#include <string.h>

#include "esp_check.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "sdkconfig.h"

static const char *TAG = "wifi";
static const uint64_t RETRY_US = 5000000;

static esp_timer_handle_t s_retry_timer;
static wifi_got_ip_cb_t s_on_got_ip;

static void retry_cb(void *arg)
{
    (void)arg;
    ESP_LOGI(TAG, "reconnecting");
    esp_err_t err = esp_wifi_connect();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "esp_wifi_connect: %s", esp_err_to_name(err));
    }
}

static void schedule_retry(void)
{
    if (s_retry_timer == NULL) {
        return;
    }
    if (!esp_timer_is_active(s_retry_timer)) {
        esp_timer_start_once(s_retry_timer, RETRY_US);
    }
}

static void wifi_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t *disc = data;
        ESP_LOGW(TAG, "disconnected, reason %d", disc != NULL ? disc->reason : -1);
        schedule_retry();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = data;
        ESP_LOGI(TAG, "got ip " IPSTR, IP2STR(&event->ip_info.ip));
        if (s_on_got_ip != NULL) {
            s_on_got_ip();
        }
    }
}

esp_err_t wifi_sta_start(wifi_got_ip_cb_t on_got_ip)
{
    if (CONFIG_IOTROBOT_WIFI_SSID[0] == '\0') {
        ESP_LOGW(TAG, "Wi-Fi SSID is empty. Set CONFIG_IOTROBOT_WIFI_SSID (menuconfig or sdkconfig.defaults.private).");
        return ESP_ERR_INVALID_STATE;
    }

    s_on_got_ip = on_got_ip;

    const esp_timer_create_args_t timer_args = {
        .callback = retry_cb,
        .name = "wifi_retry",
    };
    ESP_RETURN_ON_ERROR(esp_timer_create(&timer_args, &s_retry_timer), TAG, "retry timer");

    ESP_RETURN_ON_ERROR(esp_netif_init(), TAG, "netif");
    ESP_RETURN_ON_ERROR(esp_event_loop_create_default(), TAG, "event loop");
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&init_cfg), TAG, "wifi init");
    ESP_RETURN_ON_ERROR(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event, NULL), TAG, "wifi handler");
    ESP_RETURN_ON_ERROR(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event, NULL), TAG, "ip handler");

    wifi_config_t wifi_config = {0};
    if (strlen(CONFIG_IOTROBOT_WIFI_SSID) >= sizeof(wifi_config.sta.ssid)) {
        ESP_LOGW(TAG, "SSID truncated to %u characters", (unsigned)sizeof(wifi_config.sta.ssid) - 1);
    }
    if (strlen(CONFIG_IOTROBOT_WIFI_PASSWORD) >= sizeof(wifi_config.sta.password)) {
        ESP_LOGW(TAG, "Wi-Fi password truncated to %u characters", (unsigned)sizeof(wifi_config.sta.password) - 1);
    }
    snprintf((char *)wifi_config.sta.ssid, sizeof(wifi_config.sta.ssid), "%s", CONFIG_IOTROBOT_WIFI_SSID);
    snprintf((char *)wifi_config.sta.password, sizeof(wifi_config.sta.password), "%s", CONFIG_IOTROBOT_WIFI_PASSWORD);
    if (CONFIG_IOTROBOT_WIFI_PASSWORD[0] == '\0') {
        wifi_config.sta.threshold.authmode = WIFI_AUTH_OPEN;
    } else {
        wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    }
    wifi_config.sta.pmf_cfg.capable = true;
    wifi_config.sta.pmf_cfg.required = false;

    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), TAG, "set mode");
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_STA, &wifi_config), TAG, "set config");
    ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "wifi start");
    // A robot should not doze the radio between control packets.
    ESP_RETURN_ON_ERROR(esp_wifi_set_ps(WIFI_PS_NONE), TAG, "wifi ps");

    ESP_LOGI(TAG, "joining SSID \"%s\"", CONFIG_IOTROBOT_WIFI_SSID);
    return ESP_OK;
}

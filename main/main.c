// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2018 Barry Peterson

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_log.h"
#include "esp_mac.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mqtt_app.h"
#include "motor.h"
#include "nunchuck.h"
#include "nvs_flash.h"
#include "sdkconfig.h"
#include "wifi_sta.h"

static const char *TAG = "iotrobot";

static char s_client_id[24];
static bool s_nunchuck_ready;

static int axis_to_speed(int axis)
{
    const int center = CONFIG_IOTROBOT_JOY_CENTER;
    const int dead = CONFIG_IOTROBOT_JOY_DEADZONE;
    int delta = axis - center;
    if (delta > -dead && delta < dead) {
        return 0;
    }
    int span = 127 - dead;
    if (span < 1) {
        span = 1;
    }
    int mag = (abs(delta) - dead) * 1023 / span;
    if (mag < 0) {
        mag = 0;
    }
    if (mag > 1023) {
        mag = 1023;
    }
    return delta > 0 ? mag : -mag;
}

static void apply_mqtt_command(const char *message, int *mqtt_speed, bool *mqtt_valid)
{
    if (strcmp(message, "motor-stop") == 0) {
        *mqtt_speed = 0;
        *mqtt_valid = true;
        ESP_LOGI(TAG, "mqtt motor-stop");
    } else if (strcmp(message, "motor-forward") == 0) {
        *mqtt_speed = 700;
        *mqtt_valid = true;
        ESP_LOGI(TAG, "mqtt motor-forward");
    } else if (strcmp(message, "motor-back") == 0) {
        *mqtt_speed = -700;
        *mqtt_valid = true;
        ESP_LOGI(TAG, "mqtt motor-back");
    } else if (strncmp(message, "motor-speed=", 12) == 0) {
        int percent = atoi(message + 12);
        if (percent > 100) {
            percent = 100;
        }
        if (percent < -100) {
            percent = -100;
        }
        *mqtt_speed = percent * 1023 / 100;
        *mqtt_valid = true;
        ESP_LOGI(TAG, "mqtt motor-speed %d", percent);
    } else {
        ESP_LOGI(TAG, "ignored esp/msg: %s", message);
    }
}

static void publish_axis(int x, int y, bool z, int *last_x, int *last_y, bool *last_z, bool *have)
{
    char buf[8];
    if (!*have || abs(x - *last_x) >= 2) {
        snprintf(buf, sizeof(buf), "%d", x);
        mqtt_app_publish("esp/input_x", buf);
        *last_x = x;
    }
    if (!*have || abs(y - *last_y) >= 2) {
        snprintf(buf, sizeof(buf), "%d", y);
        mqtt_app_publish("esp/input_y", buf);
        *last_y = y;
    }
    if (!*have || z != *last_z) {
        mqtt_app_publish("esp/button_z", z ? "1" : "0");
        mqtt_app_publish("esp/log", z ? "Z pressed" : "Z released");
        *last_z = z;
    }
    *have = true;
}

static void control_task(void *arg)
{
    (void)arg;
    int mqtt_speed = 0;
    bool mqtt_valid = false;
    int last_x = 0;
    int last_y = 0;
    bool last_z = false;
    bool have_sample = false;
    bool announced = false;
    int nunchuck_failures = 0;
    bool nunchuck_ok = s_nunchuck_ready;
    char cmd[64];

    while (true) {
        if (!announced && mqtt_app_connected()) {
            char line[48];
            snprintf(line, sizeof(line), "online %s", s_client_id);
            mqtt_app_publish("esp/log", line);
            announced = true;
        }
        if (!mqtt_app_connected()) {
            announced = false;
        }

        while (mqtt_app_take_command(cmd, sizeof(cmd))) {
            apply_mqtt_command(cmd, &mqtt_speed, &mqtt_valid);
        }

        if (nunchuck_ok) {
            nunchuck_reading_t reading;
            if (nunchuck_read(&reading) == ESP_OK) {
                nunchuck_failures = 0;
                publish_axis(reading.x, reading.y, reading.z_pressed, &last_x, &last_y, &last_z, &have_sample);
                if (reading.z_pressed) {
                    motor_stop();
                } else if (abs(reading.y - CONFIG_IOTROBOT_JOY_CENTER) > CONFIG_IOTROBOT_JOY_DEADZONE) {
                    motor_drive(axis_to_speed(reading.y));
                    mqtt_valid = false;
                } else if (mqtt_valid) {
                    motor_drive(mqtt_speed);
                } else {
                    motor_stop();
                }
            } else if (++nunchuck_failures >= 10) {
                ESP_LOGW(TAG, "nunchuck reads failing; MQTT commands still apply");
                nunchuck_ok = false;
                motor_stop();
            }
        } else if (mqtt_valid) {
            motor_drive(mqtt_speed);
            mqtt_valid = false;
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

static void on_got_ip(void)
{
    esp_err_t err = mqtt_app_start(s_client_id);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "MQTT not started: %s", esp_err_to_name(err));
    }
}

void app_main(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    uint8_t mac[6] = {0};
    ESP_ERROR_CHECK(esp_read_mac(mac, ESP_MAC_WIFI_STA));
    snprintf(s_client_id, sizeof(s_client_id), "robot-%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4],
             mac[5]);
    ESP_LOGI(TAG, "ESP-IDF %s client %s", esp_get_idf_version(), s_client_id);

    ESP_ERROR_CHECK(motor_init());

    err = nunchuck_init();
    s_nunchuck_ready = (err == ESP_OK);
    if (!s_nunchuck_ready) {
        ESP_LOGW(TAG, "nunchuck init: %s", esp_err_to_name(err));
    }

    err = wifi_sta_start(on_got_ip);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Wi-Fi not started (%s). Local nunchuck control still runs.", esp_err_to_name(err));
    }

    if (xTaskCreate(control_task, "control", 6144, NULL, 5, NULL) != pdPASS) {
        ESP_LOGE(TAG, "control task create failed");
    }
}

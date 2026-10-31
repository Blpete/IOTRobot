# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2018 Barry Peterson

idf_component_register(
    SRCS "main.c" "wifi_sta.c" "mqtt_app.c" "nunchuck.c" "motor.c"
    INCLUDE_DIRS "."
    REQUIRES esp_wifi esp_netif esp_event nvs_flash mqtt esp_timer driver esp_hw_support
)

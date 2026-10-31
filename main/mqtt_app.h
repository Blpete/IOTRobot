// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2018 Barry Peterson

#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "esp_err.h"

// Safe to call more than once. The client reconnects on its own after Wi-Fi drops.
esp_err_t mqtt_app_start(const char *client_id);

bool mqtt_app_connected(void);
void mqtt_app_publish(const char *topic, const char *payload);

// Copies one queued esp/msg payload. Returns false when the queue is empty.
bool mqtt_app_take_command(char *buf, size_t len);

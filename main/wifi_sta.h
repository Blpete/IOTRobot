// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2018 Barry Peterson

#pragma once

#include "esp_err.h"

typedef void (*wifi_got_ip_cb_t)(void);

// Starts the station. Reconnects from the event loop with a 5 s backoff.
// Returns ESP_ERR_INVALID_STATE when the SSID is empty.
esp_err_t wifi_sta_start(wifi_got_ip_cb_t on_got_ip);

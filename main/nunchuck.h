// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2018 Barry Peterson

#pragma once

#include <stdbool.h>

#include "esp_err.h"

typedef struct {
    int x;
    int y;
    bool z_pressed;
    bool c_pressed;
} nunchuck_reading_t;

// Probes I2C address 0x52 and sends the unencrypted-mode init.
// Returns ESP_ERR_NOT_SUPPORTED when the feature is off in menuconfig,
// or ESP_ERR_NOT_FOUND when no device ACKs.
esp_err_t nunchuck_init(void);
esp_err_t nunchuck_read(nunchuck_reading_t *out);

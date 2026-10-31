// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2018 Barry Peterson

#pragma once

#include "esp_err.h"

// One L298N channel. speed_signed is -1023..1023 (10-bit duty).
esp_err_t motor_init(void);
void motor_drive(int speed_signed);
void motor_stop(void);

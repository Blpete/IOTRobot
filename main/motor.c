// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2018 Barry Peterson

#include "motor.h"

#include <stdlib.h>

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "sdkconfig.h"

static const char *TAG = "motor";

static SemaphoreHandle_t s_lock;

static void warn_strapping(int pin, const char *name)
{
    if (pin == 12) {
        ESP_LOGW(TAG,
                 "%s GPIO%d is an ESP32 strapping pin. High at reset selects 1.8 V flash and the chip will not boot.",
                 name, pin);
    } else if (pin == 0 || pin == 2 || pin == 15) {
        ESP_LOGW(TAG, "%s GPIO%d is an ESP32 strapping pin. It can block boot if the driver holds it.", name, pin);
    }
}

static void set_duty(int duty)
{
    if (duty < 0) {
        duty = 0;
    }
    if (duty > 1023) {
        duty = 1023;
    }
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0));
}

esp_err_t motor_init(void)
{
    const int ena = CONFIG_IOTROBOT_L298N_ENA_GPIO;
    const int in1 = CONFIG_IOTROBOT_L298N_IN1_GPIO;
    const int in2 = CONFIG_IOTROBOT_L298N_IN2_GPIO;

    if (ena == in1 || ena == in2 || in1 == in2) {
        ESP_LOGE(TAG, "L298N ENA/IN1/IN2 must be three different GPIOs");
        return ESP_ERR_INVALID_ARG;
    }

    warn_strapping(ena, "ENA");
    warn_strapping(in1, "IN1");
    warn_strapping(in2, "IN2");

    gpio_set_level(in1, 0);
    gpio_set_level(in2, 0);

    gpio_config_t io = {
        .pin_bit_mask = (1ULL << in1) | (1ULL << in2),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&io), TAG, "gpio_config");
    gpio_set_level(in1, 0);
    gpio_set_level(in2, 0);

    ledc_timer_config_t timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = CONFIG_IOTROBOT_L298N_PWM_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_RETURN_ON_ERROR(ledc_timer_config(&timer), TAG, "ledc timer");

    ledc_channel_config_t channel = {
        .gpio_num = ena,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
        .hpoint = 0,
    };
    ESP_RETURN_ON_ERROR(ledc_channel_config(&channel), TAG, "ledc channel");

    s_lock = xSemaphoreCreateMutex();
    if (s_lock == NULL) {
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "L298N ENA=%d IN1=%d IN2=%d pwm=%d Hz", ena, in1, in2, CONFIG_IOTROBOT_L298N_PWM_HZ);
    return ESP_OK;
}

void motor_drive(int speed_signed)
{
    if (s_lock == NULL) {
        return;
    }
    if (speed_signed > 1023) {
        speed_signed = 1023;
    }
    if (speed_signed < -1023) {
        speed_signed = -1023;
    }

    xSemaphoreTake(s_lock, portMAX_DELAY);
    const int in1 = CONFIG_IOTROBOT_L298N_IN1_GPIO;
    const int in2 = CONFIG_IOTROBOT_L298N_IN2_GPIO;
    if (speed_signed == 0) {
        gpio_set_level(in1, 0);
        gpio_set_level(in2, 0);
        set_duty(0);
    } else if (speed_signed > 0) {
        gpio_set_level(in1, 1);
        gpio_set_level(in2, 0);
        set_duty(speed_signed);
    } else {
        gpio_set_level(in1, 0);
        gpio_set_level(in2, 1);
        set_duty(-speed_signed);
    }
    xSemaphoreGive(s_lock);
}

void motor_stop(void)
{
    motor_drive(0);
}

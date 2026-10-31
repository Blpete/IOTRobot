// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2018 Barry Peterson

#include "nunchuck.h"

#include "driver/i2c_master.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

static const char *TAG = "nunchuck";
static const uint16_t NUNCHUCK_ADDR = 0x52;

static i2c_master_bus_handle_t s_bus;
static i2c_master_dev_handle_t s_dev;
static bool s_ready;

#if !CONFIG_IOTROBOT_NUNCHUCK_ENABLE

esp_err_t nunchuck_init(void)
{
    ESP_LOGI(TAG, "nunchuck disabled in menuconfig");
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t nunchuck_read(nunchuck_reading_t *out)
{
    (void)out;
    return ESP_ERR_INVALID_STATE;
}

#else

static esp_err_t write_reg(uint8_t reg, uint8_t value)
{
    uint8_t buf[2] = {reg, value};
    return i2c_master_transmit(s_dev, buf, sizeof(buf), 100);
}

esp_err_t nunchuck_init(void)
{
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = CONFIG_IOTROBOT_NUNCHUCK_SDA_GPIO,
        .scl_io_num = CONFIG_IOTROBOT_NUNCHUCK_SCL_GPIO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .intr_priority = 0,
        .trans_queue_depth = 0,
        .flags.enable_internal_pullup = true,
    };
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&bus_cfg, &s_bus), TAG, "i2c bus");

    esp_err_t probe = i2c_master_probe(s_bus, NUNCHUCK_ADDR, 50);
    if (probe != ESP_OK) {
        ESP_LOGW(TAG, "no nunchuck at 0x52 on SDA %d SCL %d (%s). External 4.7k pull-ups are recommended.",
                 CONFIG_IOTROBOT_NUNCHUCK_SDA_GPIO, CONFIG_IOTROBOT_NUNCHUCK_SCL_GPIO, esp_err_to_name(probe));
        return probe;
    }

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = NUNCHUCK_ADDR,
        .scl_speed_hz = 100000,
    };
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(s_bus, &dev_cfg, &s_dev), TAG, "add device");

    // Standard handshake that turns off the nunchuck's encryption.
    ESP_RETURN_ON_ERROR(write_reg(0xF0, 0x55), TAG, "init 0xF0");
    vTaskDelay(pdMS_TO_TICKS(10));
    ESP_RETURN_ON_ERROR(write_reg(0xFB, 0x00), TAG, "init 0xFB");
    vTaskDelay(pdMS_TO_TICKS(10));

    s_ready = true;
    ESP_LOGI(TAG, "nunchuck ready on SDA %d SCL %d", CONFIG_IOTROBOT_NUNCHUCK_SDA_GPIO,
             CONFIG_IOTROBOT_NUNCHUCK_SCL_GPIO);
    return ESP_OK;
}

esp_err_t nunchuck_read(nunchuck_reading_t *out)
{
    if (!s_ready || out == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t reg = 0x00;
    uint8_t data[6] = {0};
    esp_err_t err = i2c_master_transmit(s_dev, &reg, 1, 100);
    if (err != ESP_OK) {
        return err;
    }
    vTaskDelay(pdMS_TO_TICKS(2));
    err = i2c_master_receive(s_dev, data, sizeof(data), 100);
    if (err != ESP_OK) {
        err = i2c_master_transmit_receive(s_dev, &reg, 1, data, sizeof(data), 100);
        if (err != ESP_OK) {
            return err;
        }
    }

    out->x = data[0];
    out->y = data[1];
    // Bits are active-low after the unencrypted init.
    out->z_pressed = (data[5] & 0x01) == 0;
    out->c_pressed = (data[5] & 0x02) == 0;
    return ESP_OK;
}

#endif

/* Copyright (C) 2026 Sean D'Epagnier <seandepagnier@gmail.com>
 *
 * This Program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 */

#include <stdint.h>
#include <stdbool.h>

#include "driver/i2c.h"

#define I2C_MASTER_NUM I2C_NUM_0
#define I2C_MASTER_TIMEOUT_MS 100

#define LIS2DW_ADDRESS0 0x18
#define LIS2DW_ADDRESS1 0x19

// Global handles needed for transactions
static uint8_t lis2dw12_addr;

#define LIS2DW12_CTRL 0x1F   // CTRL1 is CTRL+1

static esp_err_t i2c_setup()
{
    int sda = 21, scl = 22;
    int frequency = 100000;

    printf("Initializing I2C Master: sda=%d scl=%d freq=%d\n", sda, scl, frequency);

    i2c_config_t conf = {};
    conf.mode = I2C_MODE_MASTER;
    conf.scl_io_num = (gpio_num_t)scl;
    conf.sda_io_num = (gpio_num_t)sda;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = frequency;
    conf.clk_flags = 0;//I2C_SCLK_SRC_FLAG_FOR_NOMAL;  //Any one clock source that is available for the specified frequency may be chosen

    esp_err_t ret = i2c_param_config((i2c_port_t)0, &conf);
    if (ret != ESP_OK) {
        printf("i2c_param_config failed\n");
    } else {
        ret = i2c_driver_install((i2c_port_t)0, conf.mode, 0, 0, 0);
        if (ret != ESP_OK) {
            printf("i2c_driver_install failed\n");
        } else {
            //Clock Stretching Timeout: 20b:esp32, 5b:esp32-c3, 24b:esp32-s2
            //i2c_set_timeout((i2c_port_t)0, 31);
            return false;
        }
    }
    return ret;
}

static esp_err_t i2c_read(uint8_t dev_addr, uint8_t reg_addr, uint8_t *data, size_t len)
{
    return i2c_master_write_read_device(I2C_MASTER_NUM, dev_addr, &reg_addr, 1, data, len, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
}

static esp_err_t i2c_write_byte(uint8_t dev_addr, uint8_t reg_addr, uint8_t data)
{
    int ret;
    uint8_t write_buf[2] = {reg_addr, data};

    ret = i2c_master_write_to_device(I2C_MASTER_NUM, dev_addr, write_buf, sizeof(write_buf), I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);

    return ret;
}


void lis2dw12_init()
{
    i2c_setup();
    uint8_t lis2dw12_0 = LIS2DW_ADDRESS0;
    uint8_t lis2dw12_1 = LIS2DW_ADDRESS1;

    uint8_t whoami[1] = {0};
    i2c_read(lis2dw12_0, 0x0F, whoami, 1);
    if(whoami[0] == 0x44)
        lis2dw12_addr = lis2dw12_0;
    else {
        i2c_read(lis2dw12_1, 0x0F, whoami, 1);
        if(whoami[0] == 0x44)
            lis2dw12_addr = lis2dw12_1;
        else {
            lis2dw12_addr = 0;
            return;
        }
    }

    i2c_write_byte(lis2dw12_addr, LIS2DW12_CTRL + 1, 0x46);
    i2c_write_byte(lis2dw12_addr, LIS2DW12_CTRL + 6, 0x10);
    i2c_write_byte(lis2dw12_addr, LIS2DW12_CTRL + 2, 0x08 | 0x04);
}

bool lis2dw12_read(int16_t *x, int16_t *y, int16_t *z)
{
    // every 100 reads, read whoami again to make sure the device is working
    static int reads;
    if(++reads >= 100) {
        uint8_t whoami[1] = {0};
        i2c_read(lis2dw12_addr, 0x0F, whoami, 1);
        if(whoami[0] != 0x44)
            lis2dw12_init();            

        reads = 0;
    }

    if(!lis2dw12_addr) {
        *x = 0;
        *y = 0;
        *z = 0;
        return false;
    }
    
    uint8_t reg_addr = 0x28;
    uint8_t data[6];
    i2c_read(lis2dw12_addr, reg_addr, data, 6);
    
    *z = (data[1] << 8) | data[0];
    *x = (data[3] << 8) | data[2];
    *y = (data[5] << 8) | data[4];
    return true;
}

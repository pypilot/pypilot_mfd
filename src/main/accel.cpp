/* Copyright (C) 2024 Sean D'Epagnier <seandepagnier@gmail.com>
 *
 * This Program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 */

#include <Wire.h>

#include "display.h"

// support either address..  due to poor quality, both are placed on the pcb and hopefully one works
#define DEVICE_ADDRESS0 0x19  // 0x32  // LIS2DW12TR
#define DEVICE_ADDRESS1 0x18  // 0x32  // LIS2DW12TR
#define LIS2DW12_CTRL 0x1F   // CTRL1 is CTRL+1

#if 0
#define I2C_MASTER_WRITE 0
#if 1
#include "esp32-hal-periman.h"
esp_err_t i2cInitme(uint8_t i2c_num, int8_t sda, int8_t scl, uint32_t frequency) {

//    perimanSetBusDeinit(ESP32_BUS_TYPE_I2C_MASTER_SDA, i2cDetachBus);
//    perimanSetBusDeinit(ESP32_BUS_TYPE_I2C_MASTER_SCL, i2cDetachBus);

    if (!perimanClearPinBus(sda) || !perimanClearPinBus(scl)) {
        return false;
    }

    log_i("Initializing I2C Master: sda=%d scl=%d freq=%d", sda, scl, frequency);

    i2c_config_t conf = {};
    conf.mode = I2C_MODE_MASTER;
    conf.scl_io_num = (gpio_num_t)scl;
    conf.sda_io_num = (gpio_num_t)sda;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = frequency;
    conf.clk_flags = I2C_SCLK_SRC_FLAG_FOR_NOMAL;  //Any one clock source that is available for the specified frequency may be chosen

    esp_err_t ret = i2c_param_config((i2c_port_t)i2c_num, &conf);
    if (ret != ESP_OK) {
        printf("i2c_param_config failed\n");
    } else {
        ret = i2c_driver_install((i2c_port_t)i2c_num, conf.mode, 0, 0, 0);
        if (ret != ESP_OK) {
            printf("i2c_driver_install failed\n");
        } else {
            //Clock Stretching Timeout: 20b:esp32, 5b:esp32-c3, 24b:esp32-s2
            i2c_set_timeout((i2c_port_t)i2c_num, 31);
            if (!perimanSetPinBus(sda, ESP32_BUS_TYPE_I2C_MASTER_SDA, (void *)(i2c_num + 1), i2c_num, -1)
                || !perimanSetPinBus(scl, ESP32_BUS_TYPE_I2C_MASTER_SCL, (void *)(i2c_num + 1), i2c_num, -1)) {
                //i2cDetachBus((void *)(i2c_num + 1));
                return false;
            }
        }
    }
    return ret;
}

int i2cWriteme(uint8_t i2c_num, uint16_t address, const uint8_t *buff, size_t size, uint32_t timeOutMillis) {
    esp_err_t ret = ESP_FAIL;
    i2c_cmd_handle_t cmd = NULL;

    //short implementation does not support zero size writes (example when scanning) PR in IDF?
    //ret =  i2c_master_write_to_device((i2c_port_t)i2c_num, address, buff, size, timeOutMillis / portTICK_PERIOD_MS);

    ret = ESP_OK;
    uint8_t cmd_buff[I2C_LINK_RECOMMENDED_SIZE(1)] = {0};
    cmd = i2c_cmd_link_create_static(cmd_buff, I2C_LINK_RECOMMENDED_SIZE(1));
    ret = i2c_master_start(cmd);
    if (ret != ESP_OK) {
        printf("write 1%d\n", ret);
        goto end;
    }

    ret = i2c_master_write_byte(cmd, (address << 1) | I2C_MASTER_WRITE, true);
    if (ret != ESP_OK) {
        printf("write 2%d\n", ret);
        goto end;
    }
    if (size) {
        ret = i2c_master_write(cmd, buff, size, true);
        if (ret != ESP_OK) {
            printf("write 3%d\n", ret);
            goto end;
        }
    }
    ret = i2c_master_stop(cmd);
    if (ret != ESP_OK) {
        printf("write 4%d\n", ret);
        goto end;
    }

    ret = i2c_master_cmd_begin((i2c_port_t)i2c_num, cmd, timeOutMillis / portTICK_PERIOD_MS);

end:
    if (cmd != NULL)
        i2c_cmd_link_delete_static(cmd);
    return ret;
}    




int i2cReadme(uint8_t i2c_num, uint16_t address, uint8_t *buff, size_t size, uint32_t timeOutMillis) {
    esp_err_t ret = ESP_FAIL;
    i2c_cmd_handle_t cmd = NULL;

    //short implementation does not support zero size writes (example when scanning) PR in IDF?
    //ret =  i2c_master_write_to_device((i2c_port_t)i2c_num, address, buff, size, timeOutMillis / portTICK_PERIOD_MS);

    ret = ESP_OK;
    uint8_t cmd_buff[I2C_LINK_RECOMMENDED_SIZE(1)] = {0};
    cmd = i2c_cmd_link_create_static(cmd_buff, I2C_LINK_RECOMMENDED_SIZE(1));
    ret = i2c_master_start(cmd);
    if (ret != ESP_OK)
        goto end;

    ret = i2c_master_write_byte(cmd, (address << 1) | I2C_MASTER_READ, true);
    if (ret != ESP_OK)
        goto end;

#if 1
    if (size > 1) {
        ret = i2c_master_read(cmd, buff, size-1, I2C_MASTER_ACK);
        if (ret != ESP_OK)
            goto end;
    }

    if(size > 0) {
        ret = i2c_master_read_byte(cmd, buff+size-1, I2C_MASTER_NACK);
        if (ret != ESP_OK)
            goto end;
    }
#else
    ret = i2c_master_read(cmd, buff, size, I2C_MASTER_LAST_NACK);
    if (ret != ESP_OK) {
        goto end;
    }
#endif
        
    ret = i2c_master_stop(cmd);
    if (ret != ESP_OK)
        goto end;

    ret = i2c_master_cmd_begin((i2c_port_t)i2c_num, cmd, timeOutMillis / portTICK_PERIOD_MS);

end:
    i2c_cmd_link_delete_static(cmd);
    return ret;
}    


#endif

#include "esp32-hal-i2c.h"

#endif

static uint8_t device_address, who_am_i;
bool detect_address(uint8_t address)
{
    Wire.beginTransmission(address);
    int error = Wire.endTransmission();
    if(error == 0) {
        // ok detect LIS2DW12 vs LIS2DH12
        Wire.beginTransmission(address);
        Wire.write(0x0F);
        Wire.endTransmission();
        Wire.requestFrom(address, 1);
        if (Wire.available()) {
            who_am_i = Wire.read();
            if(who_am_i == 0x44)
                printf("accel found LIS2DW12 at %x\n", address);
            else if(who_am_i == 0x33)
                printf("accel found LIS2DH12 at %x\n", address);
            else
                return false;
            device_address = address;
            return true;
        }
    }
    return false;
}

void accel_setup() {
#ifdef CONFIG_IDF_TARGET_ESP32S3
    Wire.begin(40, 41, 100000);
#else
    Wire.begin();
#endif
    //printf("i2c clock1 %ld\n", Wire.getClock());
    // detect address
    if(!detect_address(DEVICE_ADDRESS0) && !detect_address(DEVICE_ADDRESS1)) {
        printf("failed to detect accelerometer at either address!\n");
        return;
    }
    printf("init sensor...............................\n");
    // init communication with accelerometer
    Wire.beginTransmission(device_address);
    Wire.write(LIS2DW12_CTRL + 1);
    if(who_am_i == 0x44)
        Wire.write(0x44);  // high performance
    else if(who_am_i == 0x33)
        Wire.write(0x47);
    Wire.endTransmission();
#if 0
    delay(10);
    Wire.beginTransmission(device_address);
    Wire.write(LIS2DW12_CTRL + 6);
    Wire.write(0x10);  // set +/- 4g FS, LP filter ODR/2
    Wire.endTransmission();
#endif
#if 1
    if(who_am_i == 0x44) {
        // enable block data update (bit 3) and auto register address increment (bit 2)
        Wire.beginTransmission(device_address);
        Wire.write(LIS2DW12_CTRL + 2);
        Wire.write(0x08 | 0x04);
        Wire.endTransmission();
    }
#endif
}

void accel_read() {
    if(!device_address)
        return;

#if 1
    Wire.beginTransmission(device_address);
    if(who_am_i == 0x44)
        Wire.write(0x28);
    else if(who_am_i == 0x33)
        Wire.write(0x80 | 0x28); // auto increment

    Wire.endTransmission();

    Wire.requestFrom(device_address, 6);
    int i = 0;
    uint8_t data[6] = { 0 };
    while (Wire.available() && i < 6)
        data[i++] = Wire.read();
    if (i != 6) {
        printf("unable to read accel\n");
        return;
    }
#else
    uint8_t data[6] = { 0 };
    for(int i=0; i<6; i++) {
        Wire.beginTransmission(device_address);
        Wire.write(0x28+i); // auto increment
        Wire.endTransmission();

        Wire.requestFrom(device_address, 1);
        if (!Wire.available())
            break;
        data[i] = Wire.read();
    }    
#endif
    int16_t x = (data[1] << 8) | data[0];
    int16_t y = (data[3] << 8) | data[2];

    int rotation;
    if (abs(x) > abs(y)) {
        rotation = x < 0 ? 0 : 2;
    } else
        rotation = y < 0 ? 3 : 1;

    display_set_mirror_rotation(rotation);
    //printf("accel %d %d %d\n", rotation, x, y);
//    printf("accel\t%x\t%x\t%x\t%x\t%x\t%x\t%d\t%d\n", data[0], data[1], data[2], data[3], data[4], data[5], x, y);
}

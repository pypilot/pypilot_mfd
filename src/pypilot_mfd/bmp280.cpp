/* Copyright (C) 2022 Sean D'Epagnier <seandepagnier@gmail.com>
 *
 * This Program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 */

#include <stdint.h>
#include <driver/i2c.h>

#include <esp32-hal-gpio.h>

#include "Arduino.h"

#include "settings.h"
#include "display.h"
#include "nmea.h"
#include "signalk.h"

uint32_t pressure, temperature;
int32_t pressure_comp, temperature_comp;
int bmp280_count;

uint8_t have_bmp280 = 0;
uint8_t bmX280_tries = 10;  // try 10 times to configure
uint16_t dig_T1, dig_P1;
int16_t dig_T2, dig_T3, dig_P2, dig_P3, dig_P4, dig_P5, dig_P6, dig_P7, dig_P8, dig_P9;

#define DEVICE_ADDRESS 0x76


void i2c_init() {
    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = GPIO_NUM_21;        // the GPIO pin connected to SDA
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_io_num = GPIO_NUM_22;        // the GPIO pin connected to SCL
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = 100000;  // desired I2C clock frequency

    i2c_param_config(I2C_NUM_0, &conf);
    i2c_driver_install(I2C_NUM_0, conf.mode, 0, 0, 0);  
}

int i2c_write_byte(uint8_t data) {
    uint8_t d[1] = {data};
    //return i2c_master_write_to_device(I2C_NUM_0, DEVICE_ADDRESS, d, 1, pdMS_TO_TICKS(1000));
}

int i2c_write(uint8_t address, uint8_t data) {
    uint8_t d[2] = {address, data};
   // i2c_master_write_to_device(I2C_NUM_0, DEVICE_ADDRESS, d, 2, pdMS_TO_TICKS(1000));
}

int i2c_read(uint8_t *data, uint8_t count) {
    //i2c_master_read_from_device(I2C_NUM_0, DEVICE_ADDRESS, data, count, pdMS_TO_TICKS(1000));
}

void bmX280_setup()
{
    Serial.println(F("bmX280 setup"));

    // NOTE:  local version of twi does not enable pullup as
    // bmX_280 device is 3.3v and arduino runs at 5v
    i2c_init();

    // incase arduino version (although pullups will put
    // too high voltage for short time anyway....
    pinMode(SDA, INPUT);
    pinMode(SCL, INPUT);
    delay(1);

    have_bmp280 = 0;
    bmX280_tries--;

    uint8_t d[24];
    if(i2c_write_byte(0xd0) == ESP_OK &&
       i2c_read(d, 1) == ESP_OK &&
       d[0] == 0x58) {
        have_bmp280 = 1;
    }
    else {
        Serial.print(F("bmp280 not found: "));
        Serial.println(d[0]);
        // attempt reset command
        return;
    }

    if(i2c_write_byte(0x88) != ESP_OK) {
        Serial.println(F("bmp280 F1"));
        have_bmp280 = 0;
    }

    if(i2c_read(d, 24) != ESP_OK) {
        Serial.println(F("bmp280 failed to read calibration"));
        have_bmp280 = 0;
    }
      
    dig_T1 = d[0] | d[1] << 8;
    dig_T2 = d[2] | d[3] << 8;
    dig_T3 = d[4] | d[5] << 8;
    dig_P1 = d[6] | d[7] << 8;
    dig_P2 = d[8] | d[9] << 8;
    dig_P3 = d[10] | d[11] << 8;
    dig_P4 = d[12] | d[13] << 8;
    dig_P5 = d[14] | d[15] << 8;
    dig_P6 = d[16] | d[17] << 8;
    dig_P7 = d[18] | d[19] << 8;
    dig_P8 = d[20] | d[21] << 8;
    dig_P9 = d[22] | d[23] << 8;

    
    // b00011111  // configure
    if(i2c_write(0xf4, 0xff) != ESP_OK) {
        Serial.println(F("bmp280 F2"));
        have_bmp280 = 0;
    }
}

int32_t t_fine;
int32_t bmp280_compensate_T_int32(int32_t adc_T)
{
    int32_t var1, var2, T;
    var1 = ((((adc_T>>3) - ((int32_t)dig_T1<<1))) * ((int32_t)dig_T2)) >> 11;
    var2 = (((((adc_T>>4) - ((int32_t)dig_T1)) * ((adc_T>>4) - ((int32_t)dig_T1))) >> 12) * ((int32_t)dig_T3)) >> 14;
    t_fine = var1 + var2;
    T = (t_fine * 5 + 128) >> 8;
    return T;
}

// Returns pressure in Pa as unsigned 32 bit integer in Q24.8 format (24 integer bits and 8 fractional bits).
// Output value of “24674867” represents 24674867/256 = 96386.2 Pa = 963.862 hPa
uint32_t bmp280_compensate_P_int64(int32_t adc_P)
{
    uint64_t var1, var2, p;
    var1 = ((int64_t)t_fine) - 128000;
    var2 = var1 * var1 * (uint64_t)dig_P6;
    var2 = var2 + ((var1*(uint64_t)dig_P5)<<17);
    var2 = var2 + (((uint64_t)dig_P4)<<35);
    var1 = ((var1 * var1 * (uint64_t)dig_P3)>>8) + ((var1 * (uint64_t)dig_P2)<<12);
    var1 = (((((int64_t)1)<<47)+var1))*((uint64_t)dig_P1)>>33;
    if (var1 == 0)
    {
        return 0; // avoid exception caused by division by zero
    }
    p = 1048576-adc_P;
    p = (((p<<31)-var2)*3125)/var1;
    var1 = (((uint64_t)dig_P9) * (p>>13) * (p>>13)) >> 25;
    var2 = (((uint64_t)dig_P8) * p) >> 19;
    p = ((p + var1 + var2) >> 8) + (((uint64_t)dig_P7)<<4);
    return (uint32_t)p;
}


void read_pressure_temperature()
{
    if(!bmX280_tries)
        return;

    uint8_t buf[6] = {0};
    buf[0] = 0xf7;
    uint8_t r = i2c_write_byte(0xf7);
    if(r && have_bmp280) {
        Serial.print(F("bmp280 twierror "));
        Serial.println(r);
    }

    //if(i2c_read(buf, 6) != ESP_OK c != 6 && have_bmp280)
      //  Serial.println(F("bmp280 failed to read 6 bytes from bmp280"));

    int32_t p, t;
    
    p = (int32_t)buf[0] << 16 | (int32_t)buf[1] << 8 | (int32_t)buf[2];
    t = (int32_t)buf[3] << 16 | (int32_t)buf[4] << 8 | (int32_t)buf[5];
    
    if(t == 0 || p == 0) {
        if(have_bmp280)
            Serial.println(F("bmp280 zero read"));
        have_bmp280 = 0;
    }

    pressure += p >> 2;
    temperature += t >> 2;
    bmp280_count++;

    if(!have_bmp280) {
        if(bmp280_count == 512) {
            bmp280_count = 0;
            /* only re-run setup when count elapse */
            bmX280_setup();
        }
        return;
    }

    if(bmp280_count == 1024) {
        bmp280_count = 0;
        pressure >>= 12;
        temperature >>= 12;
    
        temperature_comp = bmp280_compensate_T_int32(temperature);// + 10*eeprom_data.temperature_offset;
        pressure_comp = (bmp280_compensate_P_int64(pressure) >> 8);// + eeprom_data.barometer_offset;
        pressure = temperature = 0;
  
        uint32_t t = millis();
        char buf[128];
        int ap = pressure_comp/100000;
        uint32_t rp = pressure_comp - ap*100000UL;
        snprintf(buf, sizeof buf, "MDA,,,%d.%05ld,B,,,,,,,,,,,,,,,,", ap, rp);
        nmea_send(buf);

        float pressuref = pressure_comp/1.e5;
        display_data_update(BAROMETRIC_PRESSURE, pressuref, ESP_DATA);


        int32_t temperature = temperature_comp;
        int a = temperature / 100;
        int r = temperature - a*100;
        snprintf(buf, sizeof buf, "MTA,%d.%02d,C", a, abs(r));
        nmea_send(buf);
        float temperaturef = temperature / 100.0f;
        display_data_update(AIR_TEMPERATURE, temperaturef, ESP_DATA);

        if(settings.wifi_data == SIGNALK) {
            signalk_send("environment.outside.temperature", temperaturef+273.15);
            signalk_send("environment.outside.pressure", pressuref);
        }
    }
}

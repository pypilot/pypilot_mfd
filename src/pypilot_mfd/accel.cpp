/* Copyright (C) 2024 Sean D'Epagnier <seandepagnier@gmail.com>
 *
 * This Program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 */

#include <Wire.h>

#include "display.h"

#define DEVICE_ADDRESS 0x19  // 0x32  // LIS2DW12TR
#define LIS2DW12_CTRL 0x1F   // CTRL1 is CTRL+1

void accel_setup() {
    // init communication with accelerometer
    Wire.begin();
    Wire.beginTransmission(0x19);
    Wire.write(LIS2DW12_CTRL + 1);
    Wire.write(0x44);  // high performance
    Wire.endTransmission();

    Wire.beginTransmission(0x19);
    Wire.write(LIS2DW12_CTRL + 6);
    Wire.write(0x10);  // set +/- 4g FS, LP filter ODR/2
    Wire.endTransmission();

    // enable block data update (bit 3) and auto register address increment (bit 2)
    Wire.beginTransmission(0x19);
    Wire.write(LIS2DW12_CTRL + 2);
    Wire.write(0x08 | 0x04);
    Wire.endTransmission();
}

void accel_read() {
    Wire.beginTransmission(0x19);
    Wire.write(0x28);
    Wire.endTransmission();
    Wire.requestFrom(0x19, 6);
    int i = 0;
    uint8_t data[6] = { 0 };
    while (Wire.available() && i < 6)
        data[i++] = Wire.read();
    if (i != 6) {
        printf("unable to read accel\n");
        return;
    }
    int16_t x = (data[1] << 8) | data[0];
    int16_t y = (data[3] << 8) | data[2];

    int rotation;
    if (abs(x) > abs(y)) {
        rotation = x < 0 ? 0 : 2;
    } else
        rotation = y < 0 ? 3 : 1;

    display_set_mirror_rotation(rotation);
    printf("accel %d %d %d\n", rotation, x, y);
}

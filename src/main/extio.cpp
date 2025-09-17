/* Copyright (C) 2024 Sean D'Epagnier <seandepagnier@gmail.com>
 *
 * This Program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 */

#include <stdint.h>
#include <Wire.h>

#include "extio.h"

#define DEVICE_ADDRESS 0x24

static int io_expander_addr = 0;

uint8_t cur = 0;
void extio_set(uint8_t val, bool on)
{
    if(io_expander_addr) {
        uint8_t prev = cur;
        if(on)
            cur |= val;
        else
            cur &= ~val;
        if(prev == cur)
            return;
        Wire.beginTransmission(io_expander_addr);
        Wire.write(0x2); // output port 0
        Wire.write(cur);
        Wire.endTransmission();
    } else {
        // if we dont have io expander (esp32 monochrome display)
        if(val | EXTIO_LED) {
            pinMode(EXTIO_LED_num, OUTPUT);
            digitalWrite(EXTIO_LED_num, on);
        } if(val | EXTIO_ENA_NMEA) {
//            pinMode(EXTIO_NMEA_num, OUTPUT);
//            digitalWrite(EXTIO_NMEA_num, on);
        } else if(val | EXTIO_DISP) {
            // remove this!?!
        }            
    }
}

void extio_clr(uint8_t val)
{
    extio_set(val, false);
}

void extio_poll()
{
#ifdef CONFIG_IDF_TARGET_ESP32S3
    float adcv = analogRead(18) * 2.54 / 4096;
    float bl_v = adcv * 206.8 / 6.8;

    // todo read from pgood flag and report here
    if(io_expander_addr) {
        Wire.beginTransmission(io_expander_addr);
        Wire.write(0x1); // input port 1
        Wire.endTransmission();
        Wire.requestFrom(io_expander_addr, 1);
        if (!Wire.available())
            return;
        uint8_t byte = Wire.read();

        printf("power good %x %f\n", byte, bl_v);

        // could support additional keys?
    }
    
#endif
}

void extio_setup()
{
    Wire.beginTransmission(DEVICE_ADDRESS);
    int error = Wire.endTransmission();

    if(error == 0) {
        printf("found io expander at address %x\n", DEVICE_ADDRESS);
        io_expander_addr = DEVICE_ADDRESS;

        Wire.beginTransmission(io_expander_addr);
        Wire.write(0x2); // output port 0
        Wire.write(0);   // all low
        Wire.endTransmission();

        Wire.beginTransmission(io_expander_addr);
        Wire.write(0x6); // configuration port 0
        Wire.write(0); // all output
        Wire.endTransmission();
    } else
        printf("no io expander found\n");
}

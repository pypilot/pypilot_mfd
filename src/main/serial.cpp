/* Copyright (C) 2024 Sean D'Epagnier <seandepagnier@gmail.com>
 *
 * This Program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 */

#include <Arduino.h>

#include "settings.h"
#include "display.h"
#include "serial.h"
#include "nmea.h"

struct SerialLinebuffer {
    SerialLinebuffer(HardwareSerial &s, data_source_e so)
        : serial(s), source(so) {}

    std::string readline() {
        int c;
        for (;;) {
            c = serial.read();
            if (c < 0)
                return "";
            if (c == '\n' || c == '\r') {
                if (buf.empty())
                    continue;
                std::string data = buf;
                buf = "";
                return data;
            }
            buf += (char)c;
            if(buf.length() > 180)
                buf = "";
        }
    }

    void read(bool enabled = true) {
        for (;;) {
            std::string line = readline();
            if (line.empty())
                break;

            if (enabled)
                nmea_parse_line(line.c_str(), source);
        }
    }

    std::string buf;
    HardwareSerial &serial;
    data_source_e source;
};

SerialLinebuffer SerialBuffer(Serial, USB_DATA);
SerialLinebuffer Serial2Buffer(Serial2, RS422_DATA);

void serial_setup()
{
    Serial.begin(settings.usb_baud_rate == 38400 ? 38400 : 115200);
    Serial.setTimeout(0);
    Serial2.begin(settings.rs422_baud_rate == 4800 ? 4800 : 38400,
                  SERIAL_8N1, 16, 17);  //Hardware Serial of ESP32
    Serial2.setTimeout(0);
}

void serial_poll() {
    SerialBuffer.read(settings.input_usb);
    Serial2Buffer.read();
}

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

    String readline() {
        int c;
        for (;;) {
            c = serial.read();
            if (c < 0)
                return "";
            if (c == '\n' || c == '\r') {
                if (buf.isEmpty())
                    continue;
                String data = buf;
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
            String line = readline();
            if (line.isEmpty())
                break;

            if (enabled)
                nmea_parse_line(line.c_str(), source);
        }
    }

    String buf;
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
    uint32_t t0 = millis();
    SerialBuffer.read(settings.input_usb);
    uint32_t t1 = millis();
    Serial2Buffer.read();
    uint32_t t2 = millis();
}

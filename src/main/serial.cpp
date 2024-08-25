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

            if(settings.forward_nmea_serial_to_serial)
                nmea_write_serial(line.c_str(), &serial);
            if(settings.forward_nmea_serial_to_wifi)
                nmea_write_wifi(line.c_str());
        }
    }

    std::string buf;
    HardwareSerial &serial;
    data_source_e source;
};

SerialLinebuffer Serial0Buffer(Serial0, USB_DATA);
SerialLinebuffer Serial1Buffer(Serial1, RS422_DATA);
SerialLinebuffer Serial2Buffer(Serial2, RS422_DATA);

void serial_setup()
{
    Serial0.end();
    Serial0.begin(settings.usb_baud_rate);
    Serial0.setTimeout(0);


    // TODO: enable power for 422 if either serial is enabled

    /*
    Serial1.end();
    if(settings.rs422_1_baud_rate) {
        Serial1.begin(settings.rs422_1_baud_rate,
                      SERIAL_8N1, 15, 2);  //Hardware Serial of ESP32
        Serial1.setTimeout(0);
    }
    */

    Serial2.end();
    if(settings.rs422_2_baud_rate) {
        Serial2.begin(settings.rs422_2_baud_rate,
                      SERIAL_8N1, 16, 17);  //Hardware Serial of ESP32
        Serial2.setTimeout(0);
    }
}

void serial_poll() {
    Serial0Buffer.read(settings.input_usb);
    //Serial1Buffer.read(settings.rs422_1_baud_rate);
    Serial2Buffer.read(settings.rs422_2_baud_rate);
}

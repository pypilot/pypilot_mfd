/* Copyright (C) 2024 Sean D'Epagnier <seandepagnier@gmail.com>
 *
 * This Program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 */

bool nmea_parse_line(const char*, data_source_e);
void nmea_write_serial(const char *buf, HardwareSerial *source=0);
void nmea_write_wifi(const char *buf);
void nmea_send(const char *buf);
void nmea_poll();

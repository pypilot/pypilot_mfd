/* Copyright (C) 2024 Sean D'Epagnier <seandepagnier@gmail.com>
 *
 * This Program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 */

#include <SPIFFS.h>

float deg2rad(float degrees);
float rad2deg(float radians);
float nice_number(float v, int dir = 1);
float resolv(float angle, float mid=0);
void distance_bearing(float lat1, float lon1, float lat2, float lon2, float *dist, float *brg);

uint64_t mac_as_int(const uint8_t *mac_addr);
uint64_t mac_str_to_int(String mac);
String mac_int_to_str(uint64_t i);

void listDir(fs::FS &fs, const char * dirname, uint8_t levels);

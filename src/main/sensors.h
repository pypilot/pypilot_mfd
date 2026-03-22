/* Copyright (C) 2026 Sean D'Epagnier <seandepagnier@gmail.com>
 *
 * This Program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 */

#include <unordered_set>

void sensors_wind_update(uint8_t mac[6], float dir, float knots, float paccel_x, float paccel_y, float paccel_z);
void sensors_water_update(uint8_t mac[6], float speed, float depth, float temperature);
void sensors_air_update(uint8_t mac[6], float pressure, float rel_humidity, float temperature, float air_quality, float battery_voltage);
void sensors_lightning_update(uint8_t mac[6], float distance);
void sensors_info_update(uint8_t mac[6], uint32_t runtime, uint32_t packet_count);
bool sensors_have_mac(const std::string &mac);
std::unordered_set<std::string> sensors_macs();

enum sensor_position {PRIMARY, SECONDARY, PORT, STARBOARD, IGNORED};

sensor_position wireless_str_position(const std::string &p);

extern float lpwind_dir, wind_knots;
extern float accel_x, accel_y, accel_z;

extern float lightning_distance;

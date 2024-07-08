/* Copyright (C) 2024 Sean D'Epagnier <seandepagnier@gmail.com>
 *
 * This Program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 */

#include <map>

struct sensor_transmitter_t {
  sensor_transmitter_t()
    : position(SECONDARY) {}
  uint32_t t;  // last received message
  // settings
  sensor_position position;
};

struct wind_transmitter_t : sensor_transmitter_t {
  wind_transmitter_t()
    : offset(0) {}
  float dir, knots;

  float offset;  // transmitter settings
};

struct air_transmitter_t : sensor_transmitter_t {
  float pressure, temperature, rel_humidity, air_quality;
};

struct water_transmitter_t : sensor_transmitter_t {
  float speed, depth, temperature;
};

struct lightning_uv_transmitter_t : sensor_transmitter_t {
  float distance, uva, uvb, uvc;
};

void wireless_toggle_mode();
void wireless_setup();

extern String wifi_networks_html;

void wireless_scan_networks();
void wireless_scan_devices();
void wireless_poll();

String wireless_json_sensors();

extern bool wifi_ap_mode;
extern float lpwind_dir, wind_knots;

String get_wifi_data_str();

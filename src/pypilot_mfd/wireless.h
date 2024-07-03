/* Copyright (C) 2024 Sean D'Epagnier <seandepagnier@gmail.com>
 *
 * This Program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 */

enum wireless_data_e {NMEA_PYPILOT, NMEA_SIGNALK, NMEA_CLIENT, NMEA_SERVER, SIGNALK};

void wireless_toggle_mode();
void wireless_setup();

extern String wifi_networks_html;

void wireless_scan_networks();
void wireless_scan_devices();
void wireless_poll();

extern bool wifi_ap_mode;
extern std::map<uint64_t, wind_transmitter_t> wind_transmitters;
extern float lpdir, knots;

String get_wifi_data_str();

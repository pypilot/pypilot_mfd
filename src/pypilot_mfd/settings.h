/* Copyright (C) 2024 Sean D'Epagnier <seandepagnier@gmail.com>
 *
 * This Program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 */

#include <map>

#define VERSION 0.1

enum wireless_data_e {NMEA_PYPILOT, NMEA_SIGNALK, NMEA_CLIENT, NMEA_SERVER, SIGNALK};

struct settings_t {
    String magic;

    String ssid;
    String psk;

    uint16_t channel;

    bool input_usb, output_usb;
    int usb_baud_rate;
    int rs422_baud_rate;

    bool input_wifi, output_wifi;
    wireless_data_e wifi_data;
    String nmea_client_addr;
    int nmea_client_port, nmea_server_port;

    uint16_t transmitter_count;

    bool compensate_accelerometer;

    //display
    bool use_360, use_fahrenheit, use_inHg, use_depth_ft;
    String lat_lon_format;
    int contrast, backlight;
    bool show_status;
    int rotation; // 0-3 are set, 4 is auto
    int mirror; // 2 is auto

    String enabled_pages;

    // cache mdns 
    String pypilot_addr, signalk_addr;
    int signalk_port;
    String signalk_token;
};

enum wind_position {PRIMARY, SECONDARY, PORT, STARBOARD, IGNORED};
struct wind_transmitter_t {
    float dir, knots;
    uint32_t t;  // last received message

    // settings
    wind_position position;
    float offset;
};

String get_wifi_data_str();

bool settings_load(String suffix="");
bool settings_store(String suffix="");

extern std::map<uint64_t, wind_transmitter_t> wind_transmitters;

extern settings_t settings;
extern bool wifi_ap_mode;

extern float lpdir, knots;

void scan_wifi_networks();
void scan_devices();

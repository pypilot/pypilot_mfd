/* Copyright (C) 2024 Sean D'Epagnier <seandepagnier@gmail.com>
 *
 * This Program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 */

#include <map>

#define VERSION 0.1

#define OLD

#ifdef OLD
#define KEY_UP_IO 27
#else
#define KEY_UP_IO 35
#endif

enum data_source_e {ESP_DATA, USB_DATA, RS422_DATA, WIFI_DATA, DATA_SOURCE_COUNT};

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

enum sensor_position {PRIMARY, SECONDARY, PORT, STARBOARD, IGNORED};

bool settings_load();
bool settings_store();

extern settings_t settings;

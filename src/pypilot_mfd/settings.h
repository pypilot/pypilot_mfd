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
enum lat_lon_format_e {DECIMAL_DEGREES, DECIMAL_MINUTES, DECIMAL_SECONDS};

struct settings_t {
    uint32_t magic;

    bool wifi_client;
    char ssid[32];
    char psk[32];

    uint16_t channel;

    bool input_usb, output_usb;
    int usb_baud_rate;

    int rs422_baud_rate;

    bool input_wifi, output_wifi;
    wireless_data_e wifi_data;
    char nmea_client_addr[32];
    int nmea_client_port, nmea_server_port;

    uint16_t transmitter_count;

    bool compensate_accelerometer;

    //display
    bool use_fahrenheit, use_depth_ft;
    lat_lon_format_e lat_lon_format;
    int contrast;
    bool enabled_pages[20];
    bool show_status;
    bool landscape;

} __attribute__((packed));

void write_settings();


enum wind_position {PRIMARY, SECONDARY, PORT, STARBOARD, IGNORED};
struct wind_transmitter_t {
    uint32_t magic;
    uint8_t mac[6];

    float dir, knots;
    uint32_t t;  // last received message

    wind_position position;
    float offset;
} __attribute__((packed));

extern std::map<uint64_t, wind_transmitter_t> wind_transmitters;


extern settings_t settings;
extern bool wifi_ap_mode;

extern float lpdir, knots;

void scan_devices();

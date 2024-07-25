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

    bool compensate_wind_with_accelerometer;
    bool compute_true_wind_from_gps;
    bool compute_true_wind_from_water;

    //display
    bool use_360, use_fahrenheit, use_inHg, use_depth_ft;
    String lat_lon_format;

    bool invert;
    String color_scheme;
    int contrast, backlight;
    bool show_status;
    int rotation; // 0-3 are set, 4 is auto
    int mirror; // 2 is auto
    bool powerdown; // power down or just turn of screen on power button

    String enabled_pages;

    // alarms
    bool anchor_alarm;
    int anchor_alarm_distance; // meters

    bool course_alarm;
    int course_alarm_course;
    int course_alarm_error;

    bool gps_speed_alarm;
    int gps_min_speed_alarm_knots;
    int gps_max_speed_alarm_knots;

    bool wind_speed_alarm;
    int wind_min_speed_alarm_knots;
    int wind_max_speed_alarm_knots;

    bool water_speed_alarm;
    int water_min_speed_alarm_knots;
    int water_max_speed_alarm_knots;
    
    bool weather_alarm_pressure;
    int weather_alarm_min_pressure;
    bool weather_alarm_pressure_rate;
    int weather_alarm_pressure_rate_value; // in mbar/min
    bool weather_alarm_lightning;
    int weather_alarm_lightning_distance; // in NMi

    bool depth_alarm;
    int depth_alarm_min;
    bool depth_alarm_rate;
    int depth_alarm_rate_value; // in m/min

    bool ais_alarm;
    int ais_alarm_cpa;  // in NMi
    int ais_alarm_tcpa; // in minutes
    
    bool pypilot_alarm_noconnection;
    bool pypilot_alarm_fault;
    bool pypilot_alarm_no_imu;
    bool pypilot_alarm_no_motor_controller;
    bool pypilot_alarm_lost_mode;
    
    // cache mdns 
    String pypilot_addr, signalk_addr;
    int signalk_port;
    String signalk_token;
};

enum sensor_position {PRIMARY, SECONDARY, PORT, STARBOARD, IGNORED};

void settings_load();
void settings_store();

extern settings_t settings;

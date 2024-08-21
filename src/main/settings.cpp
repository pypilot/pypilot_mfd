/* Copyright (C) 2024 Sean D'Epagnier <seandepagnier@gmail.com>
 *
 * This Program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 */

#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/stringbuffer.h"

#include "utils.h"
#include "settings.h"
#include "wireless.h"

#define DEFAULT_CHANNEL 6
#define MAGIC "3A61CF00"

settings_t settings;
static std::string settings_filename = "/settings/settings.json";
bool settings_wifi_ap_mode = false;

std::string get_wifi_data_str()
{
    switch(settings.wifi_data) {
        case NMEA_PYPILOT: return "nmea_pypilot";
        case NMEA_SIGNALK: return "nmea_signalk";
        case NMEA_CLIENT:  return "nmea_client";
        case NMEA_SERVER:  return "nmea_server";
        case SIGNALK:      return "signalk";
    }
    return "";
}

static bool settings_load_suffix(std::string suffix="")
{
    settings.usb_baud_rate = 115200;
    //settings.channel = 6;

    // start with default settings
    settings.magic = MAGIC;

    std::string filename = settings_filename + suffix;
    bool ret = false;
    rapidjson::Document s;
#ifndef __linux__
    String sfilename = filename.c_str();
    File file = SPIFFS.open(sfilename, "r");

    if(file && !file.isDirectory()) {
        std::string data;
        while(file.available())
            data += file.readString().c_str();
        file.close();

        printf("READ SETTINGS %s\n", data.c_str());

        if(s.Parse(data.c_str()).HasParseError()) {
            printf("settings file '%s' invalid/corrupted, will ignore data\n", filename.c_str());
        } else
            ret = true;
    } else {
        printf("failed to open '%s' for reading\n", filename.c_str());
    }
#endif

    if(!ret || !s.IsObject() || !s.HasMember("magic") || s["magic"].GetString() != settings.magic) {
        ret = false;
        printf("settings file '%s' magic identifier failed, will ignore data\n", filename.c_str());
    }

    if(!ret) {
        if(suffix.empty())
            return false;
        s.Parse("{}").HasParseError();
    }        

#define LOAD_SETTING_B(NAME, DEFAULT)   if(s.HasMember(#NAME) && s[#NAME].IsBool()) settings.NAME = s[#NAME].GetBool(); else settings.NAME = DEFAULT;
#define LOAD_SETTING_I(NAME, DEFAULT)   if(s.HasMember(#NAME) && s[#NAME].IsInt()) settings.NAME = s[#NAME].GetInt(); else settings.NAME = DEFAULT;
#define LOAD_SETTING_F(NAME, DEFAULT)   if(s.HasMember(#NAME) && s[#NAME].IsDouble()) settings.NAME = s[#NAME].GetFloat(); else settings.NAME = DEFAULT;
#define LOAD_SETTING_S(NAME, DEFAULT)   if(s.HasMember(#NAME) && s[#NAME].IsString()) settings.NAME = s[#NAME].GetString(); else settings.NAME = DEFAULT;
#define LOAD_SETTING_E(NAME, TYPE, DEFAULT) if(s.HasMember(#NAME) && s[#NAME].IsInt()) settings.NAME = (TYPE)(int)s[#NAME].GetInt(); else settings.NAME = DEFAULT;

#define LOAD_SETTING_BOUND(NAME, DEFAULT, MIN, MAX) \
    LOAD_SETTING_I(NAME, DEFAULT) \
    if(settings.NAME < MIN) settings.NAME = MIN; \
    if(settings.NAME > MAX) settings.NAME = MAX; \

    LOAD_SETTING_S(ssid, "pypilot")
    LOAD_SETTING_S(psk, "")

    LOAD_SETTING_I(channel, DEFAULT_CHANNEL);
    if(settings.channel == 0 || settings.channel > 12)
        settings.channel = 6;

    LOAD_SETTING_B(input_usb, true)
    LOAD_SETTING_B(output_usb, true)
    LOAD_SETTING_I(usb_baud_rate, 115200)
    LOAD_SETTING_I(rs422_baud_rate, 38400)

    LOAD_SETTING_B(input_wifi, true)
    LOAD_SETTING_B(output_wifi, false)
    LOAD_SETTING_E(wifi_data, wireless_data_e, NMEA_PYPILOT)

    LOAD_SETTING_S(nmea_client_addr, "")
    LOAD_SETTING_I(nmea_client_port, 0);
    LOAD_SETTING_I(nmea_server_port, 3600);
    LOAD_SETTING_B(compensate_wind_with_accelerometer, false)
    LOAD_SETTING_B(compute_true_wind_from_gps, false)
    LOAD_SETTING_B(compute_true_wind_from_water, true)

    //display
    LOAD_SETTING_B(use_360, false)
    LOAD_SETTING_B(use_fahrenheit, false)
    LOAD_SETTING_B(use_inHg, false)
    LOAD_SETTING_B(use_depth_ft, false)
    LOAD_SETTING_S(lat_lon_format, "minutes")
    LOAD_SETTING_B(invert, false)
    LOAD_SETTING_S(color_scheme, "default")
    LOAD_SETTING_BOUND(contrast, 20, 0, 50)
    LOAD_SETTING_BOUND(backlight, 10, 0, 20)
    
    LOAD_SETTING_B(show_status, true)
    LOAD_SETTING_I(rotation, 4)
    LOAD_SETTING_I(mirror, 2)
    LOAD_SETTING_S(power_button, "powersave")

    LOAD_SETTING_S(enabled_pages, "ABCD")
    LOAD_SETTING_I(cur_page, 0)

    // alarms
    LOAD_SETTING_B(anchor_alarm, false)
    LOAD_SETTING_F(anchor_lat, 0)
    LOAD_SETTING_F(anchor_lon, 0)
    LOAD_SETTING_I(anchor_alarm_distance, 10)

    LOAD_SETTING_B(course_alarm, false)
    LOAD_SETTING_BOUND(course_alarm_course, 0, 0, 360)
    LOAD_SETTING_BOUND(course_alarm_error, 20, 5, 90)
        
    LOAD_SETTING_B(gps_speed_alarm, false)
    LOAD_SETTING_BOUND(gps_min_speed_alarm_knots, 0, 0, 10)
    LOAD_SETTING_BOUND(gps_max_speed_alarm_knots, 10, 1, 100)

    LOAD_SETTING_B(wind_speed_alarm, false)
    LOAD_SETTING_BOUND(wind_min_speed_alarm_knots, 0, 0, 100)
    LOAD_SETTING_BOUND(wind_max_speed_alarm_knots, 30, 1, 100)

    LOAD_SETTING_B(water_speed_alarm, false)
    LOAD_SETTING_BOUND(water_min_speed_alarm_knots, 0, 0, 10)
    LOAD_SETTING_BOUND(water_max_speed_alarm_knots, 10, 1, 100)

    LOAD_SETTING_B(weather_alarm_pressure, false)
    LOAD_SETTING_BOUND(weather_alarm_min_pressure, 980, 900, 1100)
    LOAD_SETTING_B(weather_alarm_pressure_rate, false)
    LOAD_SETTING_BOUND(weather_alarm_pressure_rate_value, 10, 1, 100)
    LOAD_SETTING_B(weather_alarm_lightning, false)
    LOAD_SETTING_BOUND(weather_alarm_lightning_distance, 10, 1, 50)

    LOAD_SETTING_B(depth_alarm, false)
    LOAD_SETTING_I(depth_alarm_min, 3)
    LOAD_SETTING_B(depth_alarm_rate, false)
    LOAD_SETTING_I(depth_alarm_rate_value, 5)

    LOAD_SETTING_B(ais_alarm, false)
    LOAD_SETTING_I(ais_alarm_cpa, 1)
    LOAD_SETTING_I(ais_alarm_tcpa, 10)
        
    LOAD_SETTING_B(pypilot_alarm_noconnection, false)
    LOAD_SETTING_B(pypilot_alarm_fault, false)
    LOAD_SETTING_B(pypilot_alarm_no_imu, false)
    LOAD_SETTING_B(pypilot_alarm_no_motor_controller, false)
    LOAD_SETTING_B(pypilot_alarm_lost_mode, false)

    // mdns
    LOAD_SETTING_S(pypilot_addr, "192.168.14.1")
    LOAD_SETTING_S(signalk_addr, "10.10.10.1")
    LOAD_SETTING_I(signalk_port, 3000)

    // transmitters
    if(s.HasMember("transmitters"))
        wireless_read_transmitters(s["transmitters"]);

    return ret;
}

static bool settings_store_suffix(std::string suffix="")
{
    rapidjson::StringBuffer s;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> w(s);
    w.SetMaxDecimalPlaces(6);

    w.StartObject();
    
#define STORE_SETTING(NAME, SET, VALUE) w.Key(#NAME); w.SET(VALUE);
#define STORE_SETTING_S(NAME) STORE_SETTING(NAME, String, settings.NAME.c_str())
#define STORE_SETTING_B(NAME) STORE_SETTING(NAME, Bool, settings.NAME)
#define STORE_SETTING_I(NAME) STORE_SETTING(NAME, Int, (int)settings.NAME)
#define STORE_SETTING_E(NAME) STORE_SETTING(NAME, Uint, (unsigned int)settings.NAME)
#define STORE_SETTING_F(NAME) STORE_SETTING(NAME, Double, settings.NAME)

    STORE_SETTING_S(magic)

    STORE_SETTING_S(ssid)
    STORE_SETTING_S(psk)
    STORE_SETTING_I(channel)

    STORE_SETTING_B(input_usb)
    STORE_SETTING_B(output_usb)
    STORE_SETTING_I(usb_baud_rate)
    STORE_SETTING_I(rs422_baud_rate)

    STORE_SETTING_B(input_wifi)
    STORE_SETTING_B(output_wifi)
    STORE_SETTING_E(wifi_data)

    STORE_SETTING_S(nmea_client_addr)
    STORE_SETTING_I(nmea_client_port)
    STORE_SETTING_I(nmea_server_port)

    STORE_SETTING_B(compensate_wind_with_accelerometer)
    STORE_SETTING_B(compute_true_wind_from_gps)
    STORE_SETTING_B(compute_true_wind_from_water)

    //display
    STORE_SETTING_B(use_360)
    STORE_SETTING_B(use_fahrenheit)
    STORE_SETTING_B(use_inHg)
    STORE_SETTING_B(use_depth_ft)
    STORE_SETTING_S(lat_lon_format)
    STORE_SETTING_B(invert)
    STORE_SETTING_S(color_scheme)
    STORE_SETTING_I(contrast)
    STORE_SETTING_I(backlight)
    
    STORE_SETTING_B(show_status)
    STORE_SETTING_I(rotation)
    STORE_SETTING_B(mirror)
    STORE_SETTING_S(power_button)

    STORE_SETTING_S(enabled_pages)
    STORE_SETTING_I(cur_page)

    // alarms
    STORE_SETTING_B(anchor_alarm)
    STORE_SETTING_F(anchor_lat)
    STORE_SETTING_F(anchor_lon)
    STORE_SETTING_I(anchor_alarm_distance)

    STORE_SETTING_B(course_alarm)
    STORE_SETTING_I(course_alarm_course)
    STORE_SETTING_I(course_alarm_error)
        
    STORE_SETTING_B(gps_speed_alarm)
    STORE_SETTING_I(gps_min_speed_alarm_knots)
    STORE_SETTING_I(gps_max_speed_alarm_knots)

    STORE_SETTING_B(wind_speed_alarm)
    STORE_SETTING_I(wind_min_speed_alarm_knots)
    STORE_SETTING_I(wind_max_speed_alarm_knots)

    STORE_SETTING_B(water_speed_alarm)
    STORE_SETTING_I(water_min_speed_alarm_knots)
    STORE_SETTING_I(water_max_speed_alarm_knots)

    STORE_SETTING_B(weather_alarm_pressure)
    STORE_SETTING_I(weather_alarm_min_pressure)
    STORE_SETTING_B(weather_alarm_pressure_rate)
    STORE_SETTING_I(weather_alarm_pressure_rate_value)
    STORE_SETTING_B(weather_alarm_lightning)
    STORE_SETTING_I(weather_alarm_lightning_distance)

    STORE_SETTING_B(depth_alarm)
    STORE_SETTING_I(depth_alarm_min)
    STORE_SETTING_I(depth_alarm_rate)
    STORE_SETTING_I(depth_alarm_rate_value)

    STORE_SETTING_B(pypilot_alarm_noconnection)
    STORE_SETTING_B(pypilot_alarm_fault)
    STORE_SETTING_B(pypilot_alarm_no_imu)
    STORE_SETTING_B(pypilot_alarm_no_motor_controller)
    STORE_SETTING_B(pypilot_alarm_lost_mode)
        
    // mdns
    STORE_SETTING_S(pypilot_addr)
    STORE_SETTING_S(signalk_addr)
    STORE_SETTING_I(signalk_port)

    // transmitters
    w.Key("transmitters");
    wireless_write_transmitters(w, false);

    w.EndObject();

#ifndef __linux__
    std::string filename = settings_filename + suffix;
    File file = SPIFFS.open(filename.c_str(), "w");
    if(!file || file.isDirectory()){
        printf("failed to open '%s' for writing\n", settings_filename.c_str());
        return false;
    }

    const char *cdata = s.GetString();
    int len = s.GetSize();
    file.write((uint8_t*)cdata, len);
    file.close();
#endif
    return true;
}

void settings_load()
{
#ifndef __linux__
    if (!SPIFFS.begin(true)) {
        printf("SPIFFS Mount Failed\n");
        return;
    }
#endif    
    listDir(SPIFFS, "/", 0);

    if (settings_load_suffix())
        settings_store_suffix(".bak");
    else
        settings_load_suffix(".bak");
}

void settings_store()
{
    settings_store_suffix();
}

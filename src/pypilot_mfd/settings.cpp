/* Copyright (C) 2024 Sean D'Epagnier <seandepagnier@gmail.com>
 *
 * This Program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 */

#include "Arduino.h"
#include <Arduino_JSON.h>

#include "utils.h"
#include "settings.h"

#define DEFAULT_CHANNEL 6
#define MAGIC "3A61CF00"

String settings_filename = "/settings.json";

String get_wifi_data_str()
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

static String JString(const JSONVar &j)
{
    return j;
}

static bool settings_load_suffix(String suffix="")
{
    settings.usb_baud_rate = 115200;

    if (!SPIFFS.begin(true)) {
        printf("SPIFFS Mount Failed\n");
        return false;
    }
    
    //    listDir(SPIFFS, "/", 0);


    //settings.channel = 6;

    printf("settings load\n");

    // start with default settings
    settings.magic = MAGIC;

    File file = SPIFFS.open(settings_filename + suffix, "r");
    JSONVar s;
    bool ret = true;
    if(file && !file.isDirectory()) {
        String data;
        while(file.available())
            data += file.readString();
        file.close();

        //printf("READ SETTINGS %s\n", data.c_str());

        s = JSON.parse(data);
        String magic = s["magic"];
        if(!s.hasOwnProperty("magic") || magic != MAGIC) {
            printf("settings file invalid/corrupted, will ignore data");
            s = JSONVar();
            ret = false;
        }
    } else {
        printf("failed to open '%s' for reading\n", settings_filename.c_str());
        ret = false;
    }

#define LOAD_SETTING(NAME, DEFAULT)   if(s.hasOwnProperty(#NAME)) settings.NAME = s[#NAME]; else settings.NAME = DEFAULT;
#define LOAD_SETTING_F(NAME, DEFAULT)   if(s.hasOwnProperty(#NAME)) settings.NAME = (double)s[#NAME]; else settings.NAME = DEFAULT;
#define LOAD_SETTING_S(NAME, DEFAULT) if(s.hasOwnProperty(#NAME)) settings.NAME = JString(s[#NAME]); else settings.NAME = DEFAULT;
#define LOAD_SETTING_E(NAME, TYPE, DEFAULT) if(s.hasOwnProperty(#NAME)) settings.NAME = (TYPE)(int)s[#NAME]; else settings.NAME = DEFAULT;

#define LOAD_SETTING_BOUND(NAME, MIN, MAX, DEFAULT) \
    LOAD_SETTING(NAME, DEFAULT) \
    if(settings.NAME < MIN) settings.NAME = MIN; \
    if(settings.NAME > MAX) settings.NAME = MAX; \

    LOAD_SETTING_S(ssid, "pypilot")
    LOAD_SETTING_S(psk, "")

    LOAD_SETTING(channel, DEFAULT_CHANNEL);
    if(settings.channel == 0 || settings.channel > 12)
        settings.channel = 6;

    LOAD_SETTING(input_usb, true)
    LOAD_SETTING(output_usb, true)
    LOAD_SETTING(usb_baud_rate, 115200)
    LOAD_SETTING(rs422_baud_rate, 38400)

    LOAD_SETTING(input_wifi, true)
    LOAD_SETTING(output_wifi, false)
    LOAD_SETTING_E(wifi_data, wireless_data_e, NMEA_PYPILOT)

    LOAD_SETTING_S(nmea_client_addr, "")
    LOAD_SETTING(nmea_client_port, 0);
    LOAD_SETTING(nmea_server_port, 3600);

    LOAD_SETTING(compensate_wind_with_accelerometer, false)
    LOAD_SETTING(compute_true_wind_from_gps, false)
    LOAD_SETTING(compute_true_wind_from_water, true)

    //display
    LOAD_SETTING(use_360, false)
    LOAD_SETTING(use_fahrenheit, false)
    LOAD_SETTING(use_inHg, false)
    LOAD_SETTING(use_depth_ft, false)
    LOAD_SETTING_S(lat_lon_format, "minutes")
    LOAD_SETTING(invert, false)
    LOAD_SETTING_BOUND(contrast, 0, 50, 20)
    LOAD_SETTING_BOUND(backlight, 0, 100, 50)
    
    LOAD_SETTING(show_status, true)
    LOAD_SETTING(rotation, 4)
    LOAD_SETTING(mirror, 2)
    LOAD_SETTING(powerdown, false)

    LOAD_SETTING_S(enabled_pages, "ABCD")

    // alarms
    LOAD_SETTING(anchor_alarm, false)
    LOAD_SETTING_F(anchor_lat, 0)
    LOAD_SETTING_F(anchor_lon, 0)
    LOAD_SETTING(anchor_alarm_distance, 10)

    LOAD_SETTING(course_alarm, false)
    LOAD_SETTING_BOUND(course_alarm_course, 0, 0, 360)
    LOAD_SETTING_BOUND(course_alarm_error, 20, 5, 90)
        
    LOAD_SETTING(gps_speed_alarm, false)
    LOAD_SETTING_BOUND(gps_min_speed_alarm_knots, 0, 0, 10)
    LOAD_SETTING_BOUND(gps_max_speed_alarm_knots, 10, 1, 100)

    LOAD_SETTING(wind_speed_alarm, false)
    LOAD_SETTING_BOUND(wind_min_speed_alarm_knots, 0, 0, 100)
    LOAD_SETTING_BOUND(wind_max_speed_alarm_knots, 30, 1, 100)

    LOAD_SETTING(water_speed_alarm, false)
    LOAD_SETTING_BOUND(water_min_speed_alarm_knots, 0, 0, 10)
    LOAD_SETTING_BOUND(water_max_speed_alarm_knots, 10, 1, 100)

    LOAD_SETTING(weather_alarm_pressure, false)
    LOAD_SETTING_BOUND(weather_alarm_min_pressure, 980, 900, 1100)
    LOAD_SETTING(weather_alarm_pressure_rate, false)
    LOAD_SETTING_BOUND(weather_alarm_pressure_rate_value, 10, 1, 100)
    LOAD_SETTING(weather_alarm_lightning, false)
    LOAD_SETTING_BOUND(weather_alarm_lightning_distance, 10, 1, 50)

    LOAD_SETTING(depth_alarm, false)
    LOAD_SETTING(depth_alarm_min, 3)
    LOAD_SETTING(depth_alarm_rate, false)
    LOAD_SETTING(depth_alarm_rate_value, 5)

    LOAD_SETTING(ais_alarm, false)
    LOAD_SETTING(ais_alarm_cpa, 1)
    LOAD_SETTING(ais_alarm_tcpa, 10)
        
    LOAD_SETTING(pypilot_alarm_noconnection, false)
    LOAD_SETTING(pypilot_alarm_fault, false)
    LOAD_SETTING(pypilot_alarm_no_imu, false)
    LOAD_SETTING(pypilot_alarm_no_motor_controller, false)
    LOAD_SETTING(pypilot_alarm_lost_mode, false)

    // mdns
    LOAD_SETTING_S(pypilot_addr, "192.168.14.1")
    LOAD_SETTING_S(signalk_addr, "10.10.10.1")
    LOAD_SETTING(signalk_port, 3000)

    // transmitters
    if(s.hasOwnProperty("transmitters")) {
        JSONVar t = s["transmitters"];
        JSONVar k = t.keys();

        for(int i=0; i<k.length(); i++) {
            String mac = k[i];
            JSONVar u = t[mac];
#if 0
            wind_transmitter_t tr = {0};
            tr.position = (wind_position)(int)u["position"];
            tr.offset = (double)u["offset"];

            String str = JSON.stringify(u);//["position"];
            uint64_t maci = mac_str_to_int(mac);
            wind_transmitters[maci] = tr;
#endif
        }
    }

    return ret;
}

static bool settings_store_suffix(String suffix="")
{
    JSONVar s;
#define STORE_SETTING(NAME)    s[#NAME] = settings.NAME;
    STORE_SETTING(magic)

    STORE_SETTING(ssid)
    STORE_SETTING(psk)
    STORE_SETTING(channel)

    STORE_SETTING(input_usb)
    STORE_SETTING(output_usb)
    STORE_SETTING(usb_baud_rate)
    STORE_SETTING(rs422_baud_rate)

    STORE_SETTING(input_wifi)
    STORE_SETTING(output_wifi)
    STORE_SETTING(wifi_data)

    STORE_SETTING(nmea_client_addr)
    STORE_SETTING(nmea_client_port)
    STORE_SETTING(nmea_server_port)

    STORE_SETTING(compensate_wind_with_accelerometer)
    STORE_SETTING(compute_true_wind_from_gps)
    STORE_SETTING(compute_true_wind_from_water)

    //display
    STORE_SETTING(use_360)
    STORE_SETTING(use_fahrenheit)
    STORE_SETTING(use_inHg)
    STORE_SETTING(use_depth_ft)
    STORE_SETTING(lat_lon_format)
    STORE_SETTING(invert)
    STORE_SETTING(contrast)
    STORE_SETTING(backlight)
    
    STORE_SETTING(show_status)
    STORE_SETTING(rotation)
    STORE_SETTING(mirror)
    STORE_SETTING(powerdown)

    STORE_SETTING(enabled_pages)

    // alarms
    STORE_SETTING(anchor_alarm)
    STORE_SETTING(anchor_lat)
    STORE_SETTING(anchor_lon)
    STORE_SETTING(anchor_alarm_distance)

    STORE_SETTING(course_alarm)
    STORE_SETTING(course_alarm_course)
    STORE_SETTING(course_alarm_error)
        
    STORE_SETTING(gps_speed_alarm)
    STORE_SETTING(gps_min_speed_alarm_knots)
    STORE_SETTING(gps_max_speed_alarm_knots)

    STORE_SETTING(wind_speed_alarm)
    STORE_SETTING(wind_min_speed_alarm_knots)
    STORE_SETTING(wind_max_speed_alarm_knots)

    STORE_SETTING(water_speed_alarm)
    STORE_SETTING(water_min_speed_alarm_knots)
    STORE_SETTING(water_max_speed_alarm_knots)

    STORE_SETTING(weather_alarm_pressure)
    STORE_SETTING(weather_alarm_min_pressure)
    STORE_SETTING(weather_alarm_pressure_rate)
    STORE_SETTING(weather_alarm_pressure_rate_value)
    STORE_SETTING(weather_alarm_lightning)
    STORE_SETTING(weather_alarm_lightning_distance)

    STORE_SETTING(depth_alarm)
    STORE_SETTING(depth_alarm_min)
    STORE_SETTING(depth_alarm_rate)
    STORE_SETTING(depth_alarm_rate_value)

    STORE_SETTING(pypilot_alarm_noconnection)
    STORE_SETTING(pypilot_alarm_fault)
    STORE_SETTING(pypilot_alarm_no_imu)
    STORE_SETTING(pypilot_alarm_no_motor_controller)
    STORE_SETTING(pypilot_alarm_lost_mode)
        
    // mdns
    STORE_SETTING(pypilot_addr)
    STORE_SETTING(signalk_addr)
    STORE_SETTING(signalk_port)

    // transmitters
    JSONVar transmitters;
    #if 0
    for(std::map<uint64_t, wind_transmitter_t>::iterator it = wind_transmitters.begin();
        it != wind_transmitters.end(); it++) {
        JSONVar t;
        wind_transmitter_t &tr = it->second;
        t["position"] = tr.position;
        t["offset"] = tr.offset;
        transmitters[mac_int_to_str(it->first)] = t;
    }
    #endif
    s["transmitters"] = transmitters;

    File file = SPIFFS.open(settings_filename + suffix, "w");
    if(!file || file.isDirectory()){
        printf("failed to open '%s' for writing\n", settings_filename.c_str());
        return false;
    }

    String data = JSON.stringify(s);
    printf("store data %s\n", data.c_str());
    uint8_t *cdata = (uint8_t*)data.c_str();
    int len = data.length();
    file.write(cdata, len);
    file.close();
    return true;
}

void settings_load()
{
    if (settings_load_suffix())
        settings_store_suffix(".bak");
    else
        settings_load_suffix(".bak");
}

void settings_store()
{
    settings_store_suffix();
}

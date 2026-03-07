/* Copyright (C) 2024 Sean D'Epagnier <seandepagnier@gmail.com>
 *
 * This Program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 */

#include <vector>

#include <esp_log.h>
#include <esp_spiffs.h>
#include <esp_wifi_types_generic.h>

#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>

#include "utils.h"
#include "settings.h"

void sensors_write_transmitters(rapidjson::Writer<rapidjson::StringBuffer> &writer, bool info);
void sensors_read_transmitters(const rapidjson::Value &t);

#define DEFAULT_CHANNEL 1
#define MAGIC "3A61CF00"

#define TAG "settings"

settings_t settings;
static std::string settings_filename = "/spiffs/settings.json";
bool force_wifi_ap_mode = false;
uint8_t hw_version;

std::string get_wifi_mode_str()
{
    switch(settings.wifi_mode) {
        case WIFI_MODE_AP: return "ap";
        case WIFI_MODE_STA: return "client";
        default:  break;
    }
    return "none";    
}

void settings_reset() {
    std::string filename = settings_filename;
    if(remove(filename.c_str()) != 0)
        ESP_LOGW(TAG, "failed to remove: %s", filename.c_str());
    filename += ".bak";
    if(remove(filename.c_str()) != 0)
        ESP_LOGW(TAG, "failed to remove: %s", filename.c_str());
}    

static bool settings_parse(rapidjson::Document &s,
                           std::string suffix="")
{
    // start with default settings
    std::string filename = settings_filename + suffix;
    bool ret = false;
    FILE *f = fopen(filename.c_str(), "rb");
    if(f) {
        fseek(f, 0, SEEK_END);
        long n = ftell(f);
        rewind(f);

        std::vector<char> data(n+1);
    
        size_t got = fread(data.data(), n, 1, f);
        fclose(f);
        data[n] = 0;
        
        if (got != 1) return false;
        
        //        printf("READ SETTINGS %ld %d %s\n", n, got, data.data());
        if(s.Parse(data.data()).HasParseError())
            printf("settings file '%s' invalid/corrupted, will ignore data\n", filename.c_str());
        else
            ret = true;

        if(!s.IsObject() || !s.HasMember("magic") || !s["magic"].IsString() || s["magic"].GetString() != std::string(MAGIC)) {
            ret = false;
            printf("settings file '%s' magic identifier failed, will ignore data\n", filename.c_str());
        }
    } else
        printf("failed to open '%s' for reading\n", filename.c_str());

    if(!ret) {
        if(suffix.empty())
            return false;
        s.Parse("{}").HasParseError();
    }

    return ret;
}

static bool settings_write(const char *cdata, int len, std::string suffix="")
{
    printf("SETTINGS WRITE %s\n", cdata);
    
    std::string filename = settings_filename + suffix;
    FILE *f = fopen(filename.c_str(), "wb");
    if(!f){
        printf("failed to open '%s' for writing\n", settings_filename.c_str());
        return false;
    }

    fwrite((uint8_t*)cdata, len, 1, f);
    fclose(f);
    return true;
}

std::unordered_set<std::string> settings_keys() {
    std::unordered_set<std::string> keys;

    rapidjson::Document s;
    if(!settings_parse(s))
        return keys;

    for (auto& m : s.GetObject()) {
        const char* key = m.name.GetString();
        keys.insert(key);
    }
    return keys;
}

static std::string to_string(const rapidjson::Value& v)
{
    if (v.IsString()) return v.GetString();
    if (v.IsInt()) return std::to_string(v.GetInt());
    if (v.IsDouble()) return std::to_string(v.GetFloat());
    if (v.IsBool()) return v.GetBool() ? "true" : "false";
    return "<unsupported>";
}

std::string settings_get_value(const std::string &key) {
    rapidjson::Document s;
    if(!settings_parse(s))
        return "failed to parse";

    if(!s.HasMember(key.c_str()))
        return "not found";

    return to_string(s[key.c_str()]);
}

static bool set_bool(rapidjson::Value& v, const std::string& s) {
    if (s == "true" || s == "1")
        v.SetBool(true);
    else
    if (s == "false" || s == "0")
        v.SetBool(false);
    else
        return false;
    return true;
}

static bool set_int(rapidjson::Value& v, const std::string& s, bool unsign) {
    char* end = nullptr;
    errno = 0;
    int32_t n = std::strtol(s.c_str(), &end, 0); // base 0 handles 0x.. too
    if(n < 0 || unsign)
        return false;
    if (errno != 0 || end == s.c_str() || *end != '\0') return false;
    v.SetInt((int)n);
    return true;
}

static bool set_double(rapidjson::Value& v, const std::string& s) {
    char* end = nullptr;
    errno = 0;
    double d = std::strtod(s.c_str(), &end);
    if (errno != 0 || end == s.c_str() || *end != '\0') return false;
    v.SetDouble(d);
    return true;
}

// s is an object (e.g. doc["settings"])
// key exists already and you want to set it using the existing type
bool set_by_existing_type(rapidjson::Value& v,
                          const std::string& value,
                          rapidjson::Document::AllocatorType& alloc)
{
    if (v.IsBool()) return set_bool(v, value);
    if (v.IsUint()) return set_int(v, value, true);
    if (v.IsInt()) return set_int(v, value, false);
    if (v.IsNumber()) return set_double(v, value);
    if (v.IsString()) {
        v.SetString(value.c_str(), (rapidjson::SizeType)value.size(), alloc);
        return true;
    }

    return false;
}

static bool settings_load_suffix(std::string suffix="")
{
    rapidjson::Document s;
    if(!settings_parse(s, suffix))
        return false;
    
#define LOAD_SETTING_B(NAME, DEFAULT)   if(s.HasMember(#NAME) && s[#NAME].IsBool()) settings.NAME = s[#NAME].GetBool(); else settings.NAME = DEFAULT;
#define LOAD_SETTING_I(NAME, DEFAULT)   if(s.HasMember(#NAME) && s[#NAME].IsInt()) settings.NAME = s[#NAME].GetInt(); else settings.NAME = DEFAULT;
#define LOAD_SETTING_F(NAME, DEFAULT)   if(s.HasMember(#NAME) && s[#NAME].IsDouble()) settings.NAME = s[#NAME].GetFloat(); else settings.NAME = DEFAULT;
#define LOAD_SETTING_S(NAME, DEFAULT)   if(s.HasMember(#NAME) && s[#NAME].IsString()) settings.NAME = s[#NAME].GetString(); else settings.NAME = DEFAULT;
#define LOAD_SETTING_E(NAME, TYPE, DEFAULT) if(s.HasMember(#NAME) && s[#NAME].IsInt()) settings.NAME = (TYPE)(int)s[#NAME].GetInt(); else settings.NAME = DEFAULT;

#define LOAD_SETTING_BOUND(NAME, DEFAULT, MIN, MAX) \
    LOAD_SETTING_I(NAME, DEFAULT) \
    if(settings.NAME < MIN) settings.NAME = MIN; \
    if(settings.NAME > MAX) settings.NAME = MAX; \

    LOAD_SETTING_I(wifi_mode, WIFI_MODE_AP)
#ifdef CONFIG_IDF_TARGET_ESP32S3
    LOAD_SETTING_S(ap_ssid, "pypilot_mfd")
#else
    LOAD_SETTING_S(ap_ssid, "wind_receiver")
#endif
        
    LOAD_SETTING_S(ap_psk, "")

    LOAD_SETTING_S(client_ssid, "pypilot")
    LOAD_SETTING_S(client_psk, "")

    LOAD_SETTING_I(wifi_channel, DEFAULT_CHANNEL);
    if(settings.wifi_channel == 0 || settings.wifi_channel > 12)
        settings.wifi_channel = DEFAULT_CHANNEL;

    LOAD_SETTING_B(output_usb, true)
    LOAD_SETTING_I(usb_baud_rate, 115200)

#ifdef CONFIG_IDF_TARGET_ESP32S3
    LOAD_SETTING_B(input_usb, true)
    LOAD_SETTING_I(rs422_1_baud_rate, 0)
    LOAD_SETTING_I(rs422_2_baud_rate, 38400)

    LOAD_SETTING_B(input_nmea_pypilot, true)
    LOAD_SETTING_B(input_nmea_signalk, false)
    LOAD_SETTING_B(input_nmea_client, false)
    LOAD_SETTING_B(input_nmea_server, false)
    LOAD_SETTING_B(input_signalk, false)
#endif

    LOAD_SETTING_B(output_nmea_pypilot, false)
    LOAD_SETTING_B(output_nmea_signalk, false)
    LOAD_SETTING_B(output_nmea_client, false)
    LOAD_SETTING_B(output_nmea_server, false)
    LOAD_SETTING_B(output_signalk, false)

    LOAD_SETTING_S(nmea_client_addr, "")
    LOAD_SETTING_I(nmea_client_port, 0);
    LOAD_SETTING_I(nmea_server_port, 3600);

    // forwarding data
#ifdef CONFIG_IDF_TARGET_ESP32S3
    LOAD_SETTING_B(forward_nmea_serial_to_serial, false)
    LOAD_SETTING_B(forward_nmea_serial_to_wifi, true)
#endif
        
    // computations
    LOAD_SETTING_B(compensate_wind_with_accelerometer, false)
#ifdef CONFIG_IDF_TARGET_ESP32S3
    LOAD_SETTING_B(compute_true_wind_from_gps, true)
    LOAD_SETTING_B(compute_true_wind_from_water, true)
#endif
        
    //display
#ifdef CONFIG_IDF_TARGET_ESP32S3
    LOAD_SETTING_B(use_360, false)
    LOAD_SETTING_B(use_fahrenheit, false)
    LOAD_SETTING_B(use_inHg, false)
    LOAD_SETTING_B(use_depth_ft, false)
    LOAD_SETTING_S(lat_lon_format, "minutes")
    LOAD_SETTING_B(invert, false)
    LOAD_SETTING_S(color_scheme, "default")
    LOAD_SETTING_BOUND(contrast, 20, 0, 50)
    LOAD_SETTING_BOUND(backlight, 10, 0, 20)
    LOAD_SETTING_BOUND(buzzer_volume, 5, 0, 10)
    
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
#endif

    // mdns
    LOAD_SETTING_S(pypilot_addr, "192.168.14.1")
    LOAD_SETTING_S(signalk_addr, "10.10.10.1")
    LOAD_SETTING_I(signalk_port, 3000)

    // transmitters
    if(s.HasMember("transmitters"))
        sensors_read_transmitters(s["transmitters"]);

    return true;
}

bool settings_set_value(const std::string &key, const std::string &value) {
    rapidjson::Document doc;
    if(!settings_parse(doc))
        return false;

    if (!doc.IsObject() || !doc.HasMember(key.c_str()))
        return false;

    if(!set_by_existing_type(doc[key.c_str()], value, doc.GetAllocator()))
        return false;

    rapidjson::StringBuffer s;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> w(s);
    doc.Accept(w);
 
    settings_write(s.GetString(), s.GetSize());

    settings_load_suffix();
    return true;
}

static bool settings_store_suffix(std::string suffix="")
{
    printf("SETTINGS STORE %s\n", suffix.c_str());
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

    w.Key("magic");
    w.String(MAGIC);

    STORE_SETTING_I(wifi_mode)
    STORE_SETTING_S(ap_ssid)
    STORE_SETTING_S(ap_psk)
    STORE_SETTING_S(client_ssid)
    STORE_SETTING_S(client_psk)
    STORE_SETTING_I(wifi_channel)

    STORE_SETTING_B(output_usb)
    STORE_SETTING_I(usb_baud_rate)

#ifdef CONFIG_IDF_TARGET_ESP32S3
    STORE_SETTING_B(input_usb)
    STORE_SETTING_I(rs422_1_baud_rate)
    STORE_SETTING_I(rs422_2_baud_rate)

    STORE_SETTING_B(input_nmea_pypilot)
    STORE_SETTING_B(input_nmea_signalk)
    STORE_SETTING_B(input_nmea_client)
    STORE_SETTING_B(input_nmea_server)
    STORE_SETTING_B(input_signalk)
#endif
        
    STORE_SETTING_B(output_nmea_pypilot)
    STORE_SETTING_B(output_nmea_signalk)
    STORE_SETTING_B(output_nmea_client)
    STORE_SETTING_B(output_nmea_server)
    STORE_SETTING_B(output_signalk)

    STORE_SETTING_S(nmea_client_addr)
    STORE_SETTING_I(nmea_client_port)
    STORE_SETTING_I(nmea_server_port)

    // forwarding data
#ifdef CONFIG_IDF_TARGET_ESP32S3
    STORE_SETTING_B(forward_nmea_serial_to_serial)
    STORE_SETTING_B(forward_nmea_serial_to_wifi)
#endif

    STORE_SETTING_B(compensate_wind_with_accelerometer)
#ifdef CONFIG_IDF_TARGET_ESP32S3
    STORE_SETTING_B(compute_true_wind_from_gps)
    STORE_SETTING_B(compute_true_wind_from_water)
#endif
        
    //display
#ifdef CONFIG_IDF_TARGET_ESP32S3
    STORE_SETTING_B(use_360)
    STORE_SETTING_B(use_fahrenheit)
    STORE_SETTING_B(use_inHg)
    STORE_SETTING_B(use_depth_ft)
    STORE_SETTING_S(lat_lon_format)
    STORE_SETTING_B(invert)
    STORE_SETTING_S(color_scheme)
    STORE_SETTING_I(contrast)
    STORE_SETTING_I(backlight)
    STORE_SETTING_I(buzzer_volume)
    
    STORE_SETTING_B(show_status)
    STORE_SETTING_I(rotation)
    STORE_SETTING_I(mirror)
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
#endif

    // mdns
    STORE_SETTING_S(pypilot_addr)
    STORE_SETTING_S(signalk_addr)
    STORE_SETTING_I(signalk_port)

    // transmitters
    w.Key("transmitters");
    void wireless_write_transmitters(rapidjson::Writer<rapidjson::StringBuffer> &writer, bool info);
    sensors_write_transmitters(w, false);

    w.EndObject();

    return settings_write(s.GetString(), s.GetSize(), suffix);
}

static esp_err_t spiffs_mount_format_on_fail(void) {
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = true
    };

    esp_err_t err = esp_vfs_spiffs_register(&conf);
    ESP_LOGW(TAG, "register returned: %s", esp_err_to_name(err));
    return err;
}

void settings_load()
{
#ifndef __linux__
    ESP_ERROR_CHECK(spiffs_mount_format_on_fail());
#endif    

    if (settings_load_suffix())
        settings_store_suffix(".bak");
    else {
        settings_load_suffix(".bak");
        settings_store();
    }

#if 0
    nvs_handle_t version_handle;
    if(nvs_open("version", NVS_READONLY, &version_handle) == ESP_OK) {
        nvs_get_u8(version_handle, "hw_version", &hw_version);
        nvs_close(version_handle);
    }

    printf("Hardware Version: %d\n", hw_version);
    if(hw_version != 1) { // power
        gpio_hold_dis((gpio_num_t)14);
        pinMode(14, OUTPUT);  // 422 power
        digitalWrite(14, 1);  // enable 422 for now?  disable if settings disable it
    }
#endif
}

void settings_store()
{
    settings_store_suffix();
}

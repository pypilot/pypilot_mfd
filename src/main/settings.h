/* Copyright (C) 2024 Sean D'Epagnier <seandepagnier@gmail.com>
 *
 * This Program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 */

#include <string>
#include <vector>
#include <algorithm>
#include <unordered_set>

#define VERSION 0.20
enum data_source_e {ESP_DATA, USB_DATA, RS422_DATA, COMPUTED_DATA, WIFI_DATA, DATA_SOURCE_COUNT};

struct SettingsChoice {
    SettingsChoice(std::vector<std::string> c, const char *s)
        : choices(std::move(c)) {
        set(s);
    }

    void set(const char *s) {
        auto f = std::find(choices.begin(), choices.end(), s);
        if(f != choices.end())
            choice = f-choices.begin();
        else {
            printf("invalid choice %s, valid choices include: ", s);
            for(auto i = choices.begin(); i!=choices.end(); i++)
                printf("%s ", i->c_str());
            printf("\n");
        }
    }        
    
    operator const std::string&() const { return choices[choice]; }
    bool operator==(const char* s) const { return choices[choice] == s; }
    bool operator==(const std::string& s) const { return choices[choice] == s; }
    
    std::vector<std::string> choices;
    int choice;
};

struct ChoiceWifiMode : SettingsChoice {
    ChoiceWifiMode(const char *s) : SettingsChoice({"ap", "client", "none"}, s) {} };
struct ChoiceLatLonFormat : SettingsChoice {
    ChoiceLatLonFormat(const char *s) : SettingsChoice({"degrees", "minutes", "seconds"}, s) {} };
struct ChoiceColorScheme : SettingsChoice { ChoiceColorScheme(const char *s) :
    SettingsChoice({"none", "light", "sky", "mars"}, s) {} };
struct ChoicePowerButton : SettingsChoice { ChoicePowerButton(const char *s) :
    SettingsChoice({"screenoff", "powersave", "powerdown"}, s) {} };
struct ChoiceLogLevel : SettingsChoice { ChoiceLogLevel(const char *s) :
    SettingsChoice({"none", "error", "warn", "info", "debug"}, s) {} };

#ifdef CONFIG_IDF_TARGET_ESP32S3
#define DEF_SSID "pypilot_mfd"
#else
#define DEF_SSID "wind_receiver"
#endif
#define DEF_WIFI_CHANNEL 1

#define SETTINGS_FIELDS_BASE(X)                 \
    X(ChoiceWifiMode, wifi_mode, "ap")          \
    X(std::string, ap_ssid, DEF_SSID)           \
    X(std::string, ap_psk, "")                  \
    X(std::string, client_ssid, "pypilot")      \
    X(std::string, client_psk, "")              \
    X(uint8_t, wifi_channel, DEF_WIFI_CHANNEL)  \
    X(bool, input_usb, false)                   \
    X(bool, output_usb, true)                   \
    X(int, usb_baud_rate, 115200)               \
    \
    X(bool, input_nmea_pypilot, false)                  \
    X(bool, output_nmea_pypilot, false)                 \
    X(bool, input_nmea_signalk, false)                  \
    X(bool, output_nmea_signalk, false)                 \
    X(bool, input_nmea_tcp_client, false)               \
    X(bool, output_nmea_tcp_client, false)              \
    X(bool, input_nmea_tcp_server, false)               \
    X(bool, output_nmea_tcp_server, false)              \
    X(std::string, nmea_tcp_client_addr, "")            \
    X(int, nmea_tcp_client_port, 3000)                  \
    X(int, nmea_tcp_server_port, 7114)                  \
    X(bool, output_signalk, false)                      \
    X(bool, input_signalk, false)                       \
    \
    X(bool, forward_nmea_serial_to_wifi, false)         \
    \
    X(bool, compensate_wind_with_accelerometer, false)  \
    \
    X(ChoiceLogLevel, loglevel, "warn")              \
    \
    X(std::string, pypilot_addr, "192.168.14.1")    \
    X(std::string, signalk_addr, "10.10.10.1")      \
    X(int, signalk_port, 3000)                      \
    X(std::string, signalk_token, "")               \

#ifdef CONFIG_IDF_TARGET_ESP32S3
#define SETTINGS_FIELDS_MFD(X)               \
    X(bool, input_usb_host, false)           \
    X(bool, output_usb_host, false)          \
    X(int, usb_host_baud_rate, 38400)        \
    X(int rs422_1_baud_rate, 38400)          \
    X(int rs422_2_baud_rate, 38400)          \
    \
    X(bool, forward_nmea_serial_to_serial)      \
    X(bool, compute_true_wind_from_gps)         \
    X(bool, compute_true_wind_from_water)       \
    \
    X(bool, use_360, false)                                     \
    X(bool, use_fahrenheit, false)                              \
    X(bool, use_inHg, false)                                    \
    X(bool, use_depth_ft, false)                                \
    X(ChoiceLatLonFormat, lat_lon_format, "minutes")      \
    \
    X(bool, invert, false)                      \
    X(ChoiceColorScheme, color_scheme, "none")  \
    X(int, contrast, 20, 0, 50)                 \
    X(int, backlight, 10, 0, 20)                \
    X(int, buzzer_volume, 5, 0, 10)             \
    \
    X(bool, show_status, true)                       \
    X(int, rotation, 0)                              \
    X(int, mirror, 2)                               \
     // power down or just turn of screen on power button
    X(ChoicePowerButton, power_button, "screeoff")      \
    /
    X(std::string, enabled_pages, "ABCD")                \
    X(int, cur_page, 0)                                  \
    /
    // alarms
    X(bool, anchor_alarm, false)                        \
    X(float, anchor_lat, 0)                             \
    X(float, anchor_lon, 0)                             \
    X(int, anchor_alarm_distance, 5)                    \
    /
    X(bool, course_alarm, false)                      \
    X(int, course_alarm_course, 0, 0, 360)            \
    X(int, course_alarm_error 20, 5, 90)              \
    
    X(bool, gps_speed_alarm, false)                      \
    X(int, gps_min_speed_alarm_knots, 0, 0, 30)          \
    X(int, gps_max_speed_alarm_knots, 10, 1, 100)        \
    \
    X(bool, wind_speed_alarm, false)                          \
    X(int, wind_min_speed_alarm_knots, 0, 0, 100)             \
    X(int, wind_max_speed_alarm_knots, 30, 1, 100)            \
    \
    X(bool, water_speed_alarm, false)                         \
    X(int, water_min_speed_alarm_knots, 0, 0, 10)             \
    X(int, water_max_speed_alarm_knots, 10, 1, 100)           \
    \
    X(bool, weather_alarm_pressure, false)                              \
    X(int, weather_alarm_min_pressure, 980, 900, 1100)                  \
    X(bool, weather_alarm_pressure_rate, false)                         \
    X(int, weather_alarm_pressure_rate_value, 10, 1, 100)               \ // in mbar/min
    X(bool, weather_alarm_lightning, false)                             \
    X(int, weather_alarm_lightning_distance, 10, 1, 50) \ // in NMi
    \
    X(bool, depth_alarm, false)                                         \
    X(int, depth_alarm_min, 5, 1, 100)                                  \
    X(bool, depth_alarm_rate, false)                                    \
    X(int, depth_alarm_rate_value, 1, 1, 100)              \ // in m/min
    \
    X(bool, ais_alarm, false)                                           \
    X(int, ais_alarm_cpa, 5, 1, 100)                       \ // in NMi
    X(int, ais_alarm_tcpa, 10, 1, 100)                     \ // in minutes
    \
    X(bool, pypilot_alarm_noconnection, false)                          \
    X(bool, pypilot_alarm_fault, false)                                 \
    X(bool, pypilot_alarm_no_imu, false)                                \
    X(bool, pypilot_alarm_no_motor_controller, false)                   \
    X(bool, pypilot_alarm_lost_mode, false)                             \

#else
#define SETTINGS_FIELDS_MFD(X)
#endif

#define SETTINGS_FIELDS(X)                      \
    SETTINGS_FIELDS_BASE(X)                     \
    SETTINGS_FIELDS_MFD(X)                      \

struct settings_t {
#define DECL_FIELD(type, name, def, ...) type name = {def};
    SETTINGS_FIELDS(DECL_FIELD)
#undef DECL_FIELD

    void defaults();
};

void settings_reset();
void settings_store();
void settings_load();

std::unordered_set<std::string> settings_keys();
void settings_list();
bool settings_get_string(const std::string &key, std::string &result);

bool settings_set_value(const std::string &key, const std::string &value);
bool settings_get_transmitter(const std::string &mac, const std::string &key, std::string &output, std::string *type=0);

extern settings_t settings;
extern bool force_wifi_ap_mode;
extern uint8_t hw_version;

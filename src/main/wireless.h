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
      : position(SECONDARY), t(0) {}
    virtual void json(rapidjson::Writer<rapidjson::StringBuffer> &writer, bool info) = 0;
    virtual void setting(const rapidjson::Value &t) {}
    // settings
    sensor_position position;
    uint32_t t;  // last received message
};

struct wind_transmitter_t : sensor_transmitter_t {
  wind_transmitter_t()
      : dir(NAN), knots(NAN), offset(0) {}
    virtual void json(rapidjson::Writer<rapidjson::StringBuffer> &writer, bool info);
    virtual void setting(const rapidjson::Value &t);

    float dir, knots;
    float offset;  // transmitter settings
};

struct air_transmitter_t : sensor_transmitter_t {
    air_transmitter_t()
        : pressure(NAN), temperature(NAN), rel_humidity(NAN), air_quality(NAN), battery_voltage(NAN) {}
    virtual void json(rapidjson::Writer<rapidjson::StringBuffer> &writer, bool info);

    float pressure, temperature, rel_humidity, air_quality, battery_voltage;
};

struct water_transmitter_t : sensor_transmitter_t {
    water_transmitter_t() : speed(NAN), depth(NAN), temperature(NAN) {}
    virtual void json(rapidjson::Writer<rapidjson::StringBuffer> &writer, bool info);

    float speed, depth, temperature;
};

struct lightning_uv_transmitter_t : sensor_transmitter_t {
    lightning_uv_transmitter_t() : distance(NAN), uva(NAN), uvb(NAN), uvc(NAN), visible(NAN) {}
    virtual void json(rapidjson::Writer<rapidjson::StringBuffer> &writer, bool info);

    float distance, uva, uvb, uvc, visible;
};

sensor_position wireless_str_position(const std::string &p);
void wireless_write_transmitters(rapidjson::Writer<rapidjson::StringBuffer> &writer, bool info);
void wireless_read_transmitters(const rapidjson::Value &t);
void wireless_setting(const rapidjson::Document &d);

extern std::string wifi_networks_html;

void wireless_scan_networks();
void wireless_scan_devices();

std::string wireless_json_sensors();

extern float lpwind_dir, wind_knots;

/* Copyright (C) 2026 Sean D'Epagnier <seandepagnier@gmail.com>
 *
 * This Program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 */

#include <math.h>
#include <map>

#include <esp_timer.h>

#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>

#include "settings.h"
#include "sensors.h"
#include "utils.h"
#include "nmea.h"
#include "signalk.h"
#include "display.h"

float lpwind_dir = NAN, wind_knots = 0;
float accel_x = 0, accel_y = 0, accel_z;

float lightning_distance;

using PrettyWriter = rapidjson::PrettyWriter<rapidjson::StringBuffer>;
bool read_field(float &x, const rapidjson::Value& v);

static float lowpass(float a, float b) {
    const float lp = .2;
    return (1-lp)*a + lp*b;
}

static float lowpass_direction(float lp_dir, float dir) {
    if (isnan(dir)) 
        return dir;

    float ldir = dir;
    if (ldir > 360)
        ldir -= 360;
    // lowpass wind direction
    if (lp_dir - ldir > 180)
        ldir += 360;
    else if (ldir - lpwind_dir > 180)
        ldir -= 360;
    //  printf("%x %f %f\n", packet->id, ldir, lpwind_dir);

    const float lp = .2;
    if (!isnan(lp_dir) && fabs(lp_dir - ldir) < 180)  // another test which should never fail
        dir = lp * ldir + (1 - lp) * lp_dir;

    return resolv(dir, 180);
}

struct sensor_transmitter_t {
  sensor_transmitter_t()
      : position(SECONDARY), t(0), info_t(0),
        rate(0), packet_loss(NAN), last_info_packet_count(0), packet_count(0),
        runtime(0) {}
    virtual void json(PrettyWriter &writer, bool info) = 0;
    virtual void settings_read(const rapidjson::Value &t) {}
    // settings
    sensor_position position;
    uint64_t t;  // last received message
    uint64_t info_t;  // last info message

    float rate, packet_loss;
    uint32_t last_info_packet_count, packet_count; // count of packets received
    uint32_t runtime;
};

struct wind_transmitter_t : sensor_transmitter_t {
  wind_transmitter_t()
      : dir(NAN), knots(NAN), offset(0) {}
    virtual void json(PrettyWriter &writer, bool info);
    virtual void settings_read(const rapidjson::Value &t);

    float dir, knots;
    float offset;  // transmitter settings
};

struct air_transmitter_t : sensor_transmitter_t {
    air_transmitter_t()
        : pressure(NAN), temperature(NAN), rel_humidity(NAN), air_quality(NAN), battery_voltage(NAN) {}
    virtual void json(PrettyWriter &writer, bool info);

    float pressure, temperature, rel_humidity, air_quality, battery_voltage;
};

struct water_transmitter_t : sensor_transmitter_t {
    water_transmitter_t() : speed(NAN), depth(NAN), temperature(NAN) {}
    virtual void json(PrettyWriter &writer, bool info);

    float speed, depth, temperature;
};

struct lightning_uv_transmitter_t : sensor_transmitter_t {
    lightning_uv_transmitter_t() : distance(NAN), uva(NAN), uvb(NAN), uvc(NAN), visible(NAN) {}
    virtual void json(PrettyWriter &writer, bool info);

    float distance, uva, uvb, uvc, visible;
};

static std::string position_str(sensor_position p)
{
    switch(p) {
    case PRIMARY:   return "Primary";
    case SECONDARY: return "Secondary";
    case PORT:      return "Port";
    case STARBOARD: return "Starboard";
    default:   break;
    }
    return "Ignored";
}

static void wireless_str_position(const std::string &p, sensor_position &pos)
{
    const char *q = p.c_str();
    if(strcasecmp(q, "Primary") == 0)   pos = PRIMARY;
    if(strcasecmp(q, "Secondary") == 0) pos = SECONDARY;
    if(strcasecmp(q, "Port") == 0)      pos = PORT;
    if(strcasecmp(q, "Starboard") == 0) pos = STARBOARD;
    if(strcasecmp(q, "Ignored") == 0)   pos = IGNORED;
}

template<class T>
struct transmitters {
    std::map<uint64_t, T> macs;
    uint64_t cur_primary;
    uint32_t cur_primary_time;

    transmitters() : cur_primary(0), cur_primary_time(0) {}

    T &update(uint8_t mac[6], bool &primary, float dir = 0) {
        uint64_t mac_int = mac_as_int(mac);
        uint64_t t = esp_timer_get_time();
        T& wt = macs[mac_int];
        if (macs.size() == 1)
            // if there are no known transmitters assign it as primary
            wt.position = PRIMARY;
        if (!isnan(dir)) {
            if ((wt.position == PRIMARY) || (wt.position == SECONDARY && !cur_primary) || (wt.position == PORT && (dir > 200 && dir < 340)) || (wt.position == STARBOARD && (dir > 20 && dir < 160)))
                cur_primary = mac_int;
        }

        primary = cur_primary == mac_int;
        if (primary)
            cur_primary_time = t;
        wt.t = t;
        wt.packet_count++;
        if (t - cur_primary_time > 1000)
            cur_primary = 0;
        return wt;
    }

    void info(uint8_t mac[6], uint32_t runtime, uint32_t packet_count) {
        uint64_t mac_int = mac_as_int(mac);
        if(macs.find(mac_int) == macs.end())
            return;
        
        T& wt = macs[mac_int];
        wt.runtime = runtime;

        uint64_t t = esp_timer_get_time(), dt = t-wt.info_t;
        if(t > 0)
            wt.rate = (float)wt.packet_count * 1e6 / dt;
        wt.info_t = t;

        uint32_t diff = packet_count - wt.last_info_packet_count;
        wt.last_info_packet_count = packet_count;
        if(diff > 0)
            wt.packet_loss = (1.0f - (float)wt.packet_count / diff) * 100.0f;

        wt.packet_count = 0;
    }

    void json(PrettyWriter &writer, bool info=false) {
        writer.StartObject();
        for(auto i = macs.begin(); i != macs.end(); i++) {
            std::string x = mac_int_to_str(i->first);
            writer.Key(x.c_str());
            T &wt = i->second;
            writer.StartObject();
            writer.Key("position");
            writer.String(position_str(wt.position).c_str());
            wt.json(writer, info);
            if(info) {
                writer.Key("dt");
                uint64_t t = esp_timer_get_time();
                writer.Int((int)(t - wt.t)/1000);

                writer.Key("rate");
                writer.Double(wt.rate);
                writer.Key("packet_loss");
                if(std::isnan(wt.packet_loss))
                    writer.Null();
                else
                    writer.Double(wt.packet_loss);
                writer.Key("runtime");
                writer.Uint(wt.runtime);
            }
            writer.EndObject();
        }
        writer.EndObject();
    }

    void settings_read(std::string &mac, const rapidjson::Value &s) {
        uint64_t maci = mac_str_to_int(mac);

        T &t = macs[maci];
        if(s.HasMember("position"))
            wireless_str_position(s["position"].GetString(), t.position);

        t.settings_read(s);
    }

    void settings_read(const rapidjson::Value &t) {
        for (rapidjson::Value::ConstMemberIterator itr = t.MemberBegin(); itr != t.MemberEnd(); ++itr) {
            std::string mac = itr->name.GetString();
            const rapidjson::Value &u = itr->value;
            settings_read(mac, u);
        }
    }
};

static transmitters<wind_transmitter_t> wind_transmitters;
static transmitters<air_transmitter_t> air_transmitters;
static transmitters<water_transmitter_t> water_transmitters;
static transmitters<lightning_uv_transmitter_t> lightning_uv_transmitters;

void wind_transmitter_t::json(PrettyWriter &writer, bool info)
{
    writer.Key("offset");
    writer.Double(offset);
    if(!info)
        return;

    writer.Key("direction");
    if(std::isnan(dir) || std::isinf(dir))
        writer.Null();
    else
        writer.Double(dir);
    writer.Key("knots");
    writer.Double(knots);
}

void wind_transmitter_t::settings_read(const rapidjson::Value &s)
{
    if(s.HasMember("offset")) {
        float off;
        if(read_field(off, s["offset"]))
            offset = fminf(fmaxf(off, -180), 180);
    }
}

void air_transmitter_t::json(PrettyWriter &writer, bool info)
{
    if(!info)
        return;

    writer.Key("pressure");
    writer.SetMaxDecimalPlaces(5);
    writer.Double(pressure);
    writer.Key("temperature");
    writer.SetMaxDecimalPlaces(2);
    writer.Double(temperature);
    writer.Key("rel_humidity");
    writer.Double(rel_humidity);
    writer.Key("air_quality");
    writer.Double(air_quality);
    writer.Key("battery_voltage");
    writer.Double(battery_voltage);
}

void water_transmitter_t::json(PrettyWriter &writer, bool info)
{
    if(!info)
        return;

    writer.Key("speed");
    writer.Double(speed);
    writer.Key("depth");
    writer.Double(depth);
    writer.Key("temperature");
    writer.Double(temperature);
}

void lightning_uv_transmitter_t::json(PrettyWriter &writer, bool info)
{
    if(!info)
        return;

    writer.Key("distance");
    writer.Double(distance);
    writer.Key("uva");
    writer.Double(uva);
    writer.Key("uvb");
    writer.Double(uvb);
    writer.Key("uvc");
    writer.Double(uvc);
    writer.Key("visible");
    writer.Double(visible);
}

void sensors_write_transmitters(PrettyWriter &writer, bool info)
{
    writer.StartObject();
    if(wind_transmitters.macs.size()) {
        writer.Key("wind");
        wind_transmitters.json(writer, info);
    }

    if(air_transmitters.macs.size()) {
        writer.Key("air");
        air_transmitters.json(writer, info);
    }

    if(water_transmitters.macs.size()) {
        writer.Key("water");
        water_transmitters.json(writer, info);
    }

    if(lightning_uv_transmitters.macs.size()) {
        writer.Key("lightning_uv");
        lightning_uv_transmitters.json(writer, info);
    }
    writer.EndObject();
}

void sensors_read_transmitters(const rapidjson::Value &t)
{
    for (rapidjson::Value::ConstMemberIterator itr = t.MemberBegin(); itr != t.MemberEnd(); ++itr) {
        std::string name = itr->name.GetString();
        if(name == "wind")
            wind_transmitters.settings_read(itr->value);
        else if(name == "air")
            air_transmitters.settings_read(itr->value);
        else if(name == "water")
            water_transmitters.settings_read(itr->value);
        else if(name == "lightning_uv")
            lightning_uv_transmitters.settings_read(itr->value);
    }
}

void sensors_wind_update(uint8_t mac[6], float dir, float knots, float paccel_x, float paccel_y, float paccel_z) {
    bool primary;
    wind_transmitter_t &wt = wind_transmitters.update(mac, primary, dir);
    wt.dir = resolv(dir + wt.offset);
    wt.knots = knots;

    if(!primary) // if not primary done
        return;

    lpwind_dir = lowpass_direction(lpwind_dir, wt.dir);

    accel_x = lowpass(accel_x, paccel_x);
    accel_y = lowpass(accel_y, paccel_y);
    accel_z = lowpass(accel_z, paccel_z);

    char buf[128];
    if (!isnan(lpwind_dir))
        snprintf(buf, sizeof buf, "MWV,%d.%02d,R,%d.%02d,N,A", (int)lpwind_dir, (uint16_t)(lpwind_dir * 100.0f) % 100U, (int)wind_knots, (int)(wind_knots * 100) % 100);
    else  // invalid wind direction (no magnet?)
        snprintf(buf, sizeof buf, "MWV,,R,%d.%02d,N,A", (int)wind_knots, (int)(wind_knots * 100) % 100);

    nmea_send(buf);

//    printf("lpwind_dir %f\n", lpwind_dir);
//    uint32_t t3 = millis();

    if (settings.output_signalk) {
        if (!isnan(lpwind_dir))
            signalk_send("environment.wind.angleApparent", M_PI / 180.0f * lpwind_dir);
        signalk_send("environment.wind.speedApparent", wind_knots * .514444f);
    }

    //printf("WIND DATA %f %f\n", wind_knots, lpwind_dir);

    display_data_update(WIND_SPEED, wind_knots, ESP_DATA);
    if (!isnan(lpwind_dir))
        display_data_update(WIND_ANGLE, lpwind_dir, ESP_DATA);
    //    uint32_t t4 = millis();

//    printf("recv wind %ld %ld %ld %ld\n", t1-t0, t2-t1, t3-t2, t4-t3);
}

void sensors_water_update(uint8_t mac[6], float speed, float depth, float temperature) {
    bool primary;
    water_transmitter_t &wt = water_transmitters.update(mac, primary);
    wt.speed = speed;
    wt.depth = depth;
    wt.temperature = temperature;

    if (!primary)
        return;

    char buf[128];
    snprintf(buf, sizeof buf, "VHW,,,,,%.2f,N,,", wt.speed);
    nmea_send(buf);

    snprintf(buf, sizeof buf, "DBT,,,%.5f,M,,", wt.depth);
    nmea_send(buf);

    snprintf(buf, sizeof buf, "MTW,%.2f,C", wt.temperature);
    nmea_send(buf);

    if (settings.output_signalk) {
        signalk_send("navigation.speedThroughWater", wt.speed);
        signalk_send("environment.depth.belowSurface", wt.depth);
        signalk_send("environment.water.temperature", wt.temperature);
    }

    display_data_update(WATER_SPEED, wt.speed, ESP_DATA);
    display_data_update(WATER_TEMPERATURE, wt.temperature, ESP_DATA);
    display_data_update(DEPTH, wt.depth, ESP_DATA);
}

void sensors_air_update(uint8_t mac[6], float pressure, float rel_humidity, float temperature, float air_quality, float battery_voltage) {
    bool primary;
    air_transmitter_t &wt = air_transmitters.update(mac, primary);
    wt.pressure = pressure;
    wt.rel_humidity = rel_humidity;
    wt.temperature = temperature;
    wt.air_quality = air_quality;
    wt.battery_voltage = battery_voltage;

    if(!primary)
        return;

    char buf[128];
    snprintf(buf, sizeof buf, "MDA,,,%.5f,B,,,,,%.2f,,,,,,,,,,,", wt.pressure, wt.rel_humidity);
    nmea_send(buf);

    snprintf(buf, sizeof buf, "MTA,%.2f,C", wt.temperature);
    nmea_send(buf);

    if (settings.output_signalk) {
        signalk_send("environment.outside.pressure", wt.pressure * 100000);
        signalk_send("environment.outside.relativeHumidity", wt.rel_humidity / 100);
        signalk_send("environment.outside.temperature", wt.temperature);
        signalk_send("electrical.batteries.house.voltage", wt.battery_voltage);
    }

    display_data_update(BAROMETRIC_PRESSURE, wt.pressure, ESP_DATA);
    display_data_update(RELATIVE_HUMIDITY, wt.rel_humidity, ESP_DATA);
    display_data_update(AIR_TEMPERATURE, wt.temperature, ESP_DATA);
    display_data_update(AIR_QUALITY, wt.air_quality, ESP_DATA);
    display_data_update(BATTERY_VOLTAGE, wt.battery_voltage, ESP_DATA);
}

void sensors_lightning_update(uint8_t mac[6], float distance) {
    bool primary;
    lightning_uv_transmitter_t &wt = lightning_uv_transmitters.update(mac, primary);
    wt.distance = distance;
}

void sensors_info_update(uint8_t mac[6], uint32_t runtime, uint32_t packet_count) {
    wind_transmitters.info(mac, runtime, packet_count);
    air_transmitters.info(mac, runtime, packet_count);
    water_transmitters.info(mac, runtime, packet_count);
    lightning_uv_transmitters.info(mac, runtime, packet_count);
}

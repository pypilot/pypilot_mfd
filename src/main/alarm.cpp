/* Copyright (C) 2024 Sean D'Epagnier <seandepagnier@gmail.com>
 *
 * This Program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 */

#include "Arduino.h"

#include "alarm.h"
#include "display.h"
#include "ais.h"
#include "pypilot_client.h"
#include "utils.h"
#include "settings.h"
#include "menu.h"
#include "buzzer.h"

std::string alarm_name(alarm_e alarm)
{
    switch(alarm) {
    case ANCHOR_ALARM: return "Anchor Alarm";
    case COURSE_ALARM: return "Course Alarm";
    case GPS_SPEED_ALARM: return "GPS Speed Alarm";
    case WIND_SPEED_ALARM: return "Wind Speed Alarm";
    case WATER_SPEED_ALARM: return "Water Speed Alarm";
    case WEATHER_ALARM: return "Weather Alarm";
    case DEPTH_ALARM: return "Depth Alarm";
    case PYPILOT_ALARM: return "pypilot Alarm";
    }
}

static uint32_t last_alarm_trigger[ALARM_COUNT];
static std::string alarm_reason[ALARM_COUNT];
static void trigger(alarm_e alarm, std::string reason="")
{
    //printf("alarm trigger %d\n", alarm);
    uint32_t t = millis();
    if(t - last_alarm_trigger[alarm] < 5000)
        return;

    buzzer_buzz(reason.length() ? 4000 : 2000, 1000, alarm % 4);
    last_alarm_trigger[alarm] = t;
    alarm_reason[alarm] = reason.length() ? reason : "no data";

    menu_switch_alarm(alarm);
    display_toggle(true);
}

uint32_t alarm_last(alarm_e alarm, std::string &reason)
{
    uint32_t lt = last_alarm_trigger[alarm];
    if(!lt)
        return 0;
    reason = alarm_reason[alarm];
    return millis() - lt;
}

float alarm_anchor_dist;
float lightning_distance;
    
void alarm_anchor_reset() {
    std::string source;
    uint32_t time;
    if(!display_data_get(LATITUDE, settings.anchor_lat, source, time))
        printf("Failed to get anchor");
    if(!display_data_get(LONGITUDE, settings.anchor_lon, source, time))
        printf("Failed to get anchor");
}

static void alarm_poll_anchor()
{
    if(!settings.anchor_alarm)
        return;

    float lat, lon;
    if(!display_data_get(LATITUDE, lat)||!display_data_get(LONGITUDE, lon)) {
        printf("Failed to get lat/lon when anchor alarm set");
        trigger(ANCHOR_ALARM);
    }

    distance_bearing(lat, lon, settings.anchor_lat, settings.anchor_lon, &alarm_anchor_dist, 0);
    alarm_anchor_dist *= 1852; // convert NMi to meters

    if(alarm_anchor_dist < settings.anchor_alarm_distance)
        trigger(ANCHOR_ALARM, "radius");
}    

static void alarm_poll_course()
{
    if(!settings.course_alarm)
        return;

    float heading;
    if(!display_data_get(GPS_HEADING, heading))
        trigger(COURSE_ALARM);

    if(resolv(heading, settings.course_alarm_course) > settings.course_alarm_error)
        trigger(COURSE_ALARM, "course");
}    

static void alarm_poll_speed(bool enabled, int min_speed_limit, int max_speed_limit, display_item_e item, alarm_e alarm)
{
    if(!enabled)
        return;

    float speed;
    if(!display_data_get(item, speed))
        trigger(alarm);

   // printf("alarm poll speed %f %d %d\n", speed, min_speed_limit, max_speed_limit);

    if(speed < min_speed_limit)
        trigger(alarm, "min speed");
    else if(speed > max_speed_limit)
        trigger(alarm, "max speed");
}

static void alarm_poll_weather()
{
    if(settings.weather_alarm_pressure) {
        float pressure;
        if(!display_data_get(BAROMETRIC_PRESSURE, pressure))
            trigger(WEATHER_ALARM);
        if(pressure < settings.weather_alarm_min_pressure)
            trigger(WEATHER_ALARM, "absolute pressure");
    }

    if(settings.weather_alarm_pressure_rate) {
        float pressure;
        if(!display_data_get(BAROMETRIC_PRESSURE, pressure))
            trigger(WEATHER_ALARM);

        static float pressure_rate, pressure_prev;
        static uint32_t prevt;
        uint32_t t = millis();
        float dt = (t - prevt)/1000.f;
        prevt = t;

        if(dt > 50000)
            pressure_rate = 0;
        else {
            float rate = (pressure_prev-pressure)/dt*60;
            pressure_rate = .1*(rate) + .9*pressure_rate;
        }
        pressure_prev = pressure;
        if(pressure_rate > settings.weather_alarm_pressure_rate_value)
            trigger(WEATHER_ALARM, "pressure rate");
    }

    if(settings.weather_alarm_lightning) {
        if(lightning_distance && lightning_distance < settings.weather_alarm_lightning_distance) {
            trigger(WEATHER_ALARM, "lightning");
            lightning_distance = 0;
        }
    }

}

static void alarm_poll_depth()
{
    if(settings.depth_alarm) {
        float depth;
        if(!display_data_get(DEPTH, depth))
            trigger(DEPTH_ALARM);
        if(depth < settings.depth_alarm_min)
            trigger(DEPTH_ALARM, "min depth");
    }

    if(settings.depth_alarm_rate) {
        float depth;
        if(!display_data_get(DEPTH, depth))
            trigger(DEPTH_ALARM);

        static float depth_rate, depth_prev;
        static uint32_t prevt;
        uint32_t t = millis();
        float dt = (t - prevt)/1000.f;
        prevt = t;

        if(dt > 10000)
            depth_rate = 0;
        else {
            float rate = (depth_prev-depth)/dt*60;
            depth_rate = .1*(rate) + .9*depth_rate;
        }
        depth_prev = depth;
        if(depth_rate > settings.depth_alarm_rate_value)
            trigger(DEPTH_ALARM, "depth rate");
    }
}

float alarm_ship_tcpa;
static void alarm_poll_ais()
{
    if(!settings.ais_alarm)
        return;

    uint32_t t = millis();
    alarm_ship_tcpa = INFINITY;
    for(std::map<int, ship>::iterator it = ships.begin(); it != ships.end(); it++) {
        ship &s = it->second;

        if(t-s.timestamp > 5*60) // out of date
            return;

        if(s.tcpa < alarm_ship_tcpa)
            alarm_ship_tcpa = s.tcpa;
        if(s.cpa < settings.ais_alarm_cpa && s.tcpa < settings.ais_alarm_tcpa*60)
            trigger(AIS_ALARM, "SHIPS!");
    }
}

static void alarm_poll_pypilot()
{
    float pypilot;
    if(settings.pypilot_alarm_noconnection) {
        if(!display_data_get(PYPILOT, pypilot))
            trigger(PYPILOT_ALARM);
        pypilot_client_strobe();
    }

    if(settings.pypilot_alarm_fault) {
        std::string x = pypilot_client_value("servo.flags");
        if(endsWith(x, "FAULT"))
            trigger(PYPILOT_ALARM, x);
        pypilot_client_strobe();
    }

    if(settings.pypilot_alarm_no_imu) {
        if(pypilot_client_value("imu.frequency", 0) == "0")
            trigger(PYPILOT_ALARM, "no imu");
        pypilot_client_strobe();
    }

    if(settings.pypilot_alarm_no_motor_controller) {
        if(pypilot_client_value("servo.controller") == "none")
            trigger(PYPILOT_ALARM, "no motor controller");
        pypilot_client_strobe();
    }

    if(settings.pypilot_alarm_lost_mode) {
        if(pypilot_client_value("ap.lostmode") == "true")
            trigger(PYPILOT_ALARM, "lost mode"); 
        pypilot_client_strobe();
    }
}

void alarm_poll()
{
    alarm_poll_anchor();
    alarm_poll_course();
    alarm_poll_speed(settings.gps_speed_alarm, settings.gps_min_speed_alarm_knots,
                     settings.gps_max_speed_alarm_knots, GPS_SPEED, GPS_SPEED_ALARM);
    alarm_poll_speed(settings.wind_speed_alarm, settings.wind_min_speed_alarm_knots,
                     settings.wind_max_speed_alarm_knots, WIND_SPEED, WIND_SPEED_ALARM);
    alarm_poll_speed(settings.water_speed_alarm, settings.water_min_speed_alarm_knots,
                     settings.water_max_speed_alarm_knots, WATER_SPEED, WATER_SPEED_ALARM);
    alarm_poll_weather();
    alarm_poll_depth();
    alarm_poll_ais();
    alarm_poll_pypilot();
}

void alarm_setup()
{
    // configure pypilot watches for alarms
    pypilot_watch("servo.flags"); // for faults
    pypilot_watch("imu.frequency"); // for lost imu
    pypilot_watch("servo.controller"); // for lost controller
    pypilot_watch("ap.lostmode"); // for lost mode
}

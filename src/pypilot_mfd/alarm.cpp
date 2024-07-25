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

String alarm_name(alarm_e alarm)
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
static String alarm_reason[ALARM_COUNT];
static void trigger(alarm_e alarm, String reason="")
{
    printf("alarm trigger %d\n", alarm);
    uint32_t t = millis();
    if(t - last_alarm_trigger[alarm] < 5000)
        return;

    buzzer_buzz(reason.length() ? 4000 : 2000, 1000, alarm % 4);
    last_alarm_trigger[alarm] = t;
    alarm_reason[alarm] = reason.length() ? reason : "no data";

    menu_switch_alarm(alarm);
}

uint32_t alarm_last(alarm_e alarm, String &reason)
{
    uint32_t lt = last_alarm_trigger[alarm];
    if(!lt)
        return 0;
    reason = alarm_reason[alarm];
    return millis() - lt;
}

static float anchor_lat, anchor_lon;
float anchor_dist;
    
void alarm_anchor_reset() {
    String source;
    uint32_t time;
    if(!display_data_get(LATITUDE, anchor_lat, source, time))
        printf("Failed to get anchor");
    if(!display_data_get(LONGITUDE, anchor_lon, source, time))
        printf("Failed to get anchor");
}

float course;
void alarm_course_reset() {
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

    distance_bearing(lat, lon, anchor_lat, anchor_lon, &anchor_dist, 0);
    anchor_dist *= 1852; // convert NMi to meters

    if(anchor_dist < settings.anchor_alarm_distance)
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

    printf("alarm poll speed %f %d %d\n", speed, min_speed_limit, max_speed_limit);

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

    // todo  baro pressure rate
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
        float depth_rate;
/* TODO :  depth rate of change
        if(!display_data_get(DEPTH, depth))
            trigger(DEPTH_ALARM, true);
        if(rate < settings.depth_alarm_rate_value)
            trigger(DEPTH_ALARM);
            */
    }
}

static void alarm_poll_ais()
{
    if(!settings.ais_alarm)
        return;

    uint32_t t = millis();
    for(std::map<int, ship>::iterator it = ships.begin(); it != ships.end(); it++) {
        ship &s = it->second;

        if(t-s.timestamp > 5*60) // out of date
            return;

        if(s.cpa < settings.ais_alarm_cpa && s.tcpa < settings.ais_alarm_tcpa*60)
            trigger(AIS_ALARM, "SHIPS!");
    }
}

static void alarm_poll_pypilot()
{
    float pypilot;
    if(settings.pypilot_alarm_noconnection)
        if(!display_data_get(PYPILOT, pypilot))
            trigger(PYPILOT_ALARM);

    if(settings.pypilot_alarm_fault) {
        String x = pypilot_client_value("servo.flags");
        if(x.endsWith("FAULT"))
            trigger(PYPILOT_ALARM, x);
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

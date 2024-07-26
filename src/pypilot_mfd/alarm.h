/* Copyright (C) 2024 Sean D'Epagnier <seandepagnier@gmail.com>
 *
 * This Program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 */

enum alarm_e {ANCHOR_ALARM, COURSE_ALARM, GPS_SPEED_ALARM, WIND_SPEED_ALARM,
              WATER_SPEED_ALARM, WEATHER_ALARM, DEPTH_ALARM, AIS_ALARM, PYPILOT_ALARM, ALARM_COUNT};

void alarm_anchor_reset();
String alarm_name(alarm_e alarm);
uint32_t alarm_last(alarm_e alarm, String &reason);
void alarm_poll();
void alarm_setup();

void menu_switch_alarm(enum alarm_e alarm);

extern float alarm_anchor_dist;
extern float lightning_distance;
extern float alarm_ship_tcpa;

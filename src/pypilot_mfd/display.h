/* Copyright (C) 2024 Sean D'Epagnier <seandepagnier@gmail.com>
 *
 * This Program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 */

#include <vector>

enum display_item_e { WIND_SPEED,
                      WIND_DIRECTION,
                      BAROMETRIC_PRESSURE,
                      AIR_TEMPERATURE,
                      WATER_TEMPERATURE,
                      GPS_SPEED,
                      GPS_HEADING,
                      LATITUDE,
                      LONGITUDE,
                      WATER_SPEED,
                      COMPASS_HEADING,
                      PITCH,
                      HEEL,
                      DEPTH,
                      RATE_OF_TURN,
                      RUDDER_ANGLE,
                      TIME,
                      DISPLAY_COUNT };

enum data_source_e {LOCAL_DATA, USB_DATA, RS422_DATA, WIFI_DATA, DATA_SOURCE_COUNT};

void display_setup();
void display_change_page(int dir);
void display_change_scale();
void display_render();
void display_data_update(display_item_e item, float value, data_source_e);

struct page_info {
    page_info(String n, String &d) : name(n), description(d), enabled(true) {}
    String name;
    String &description;
    bool enabled;
};

struct route_info_t {
    String from_wpt, to_wpt;
    float target_bearing;
    float wpt_lat, wpt_lon;
    float xte, brg;
};

extern route_info_t route_info;

extern std::vector<page_info> display_pages;
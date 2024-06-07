/* Copyright (C) 2024 Sean D'Epagnier <seandepagnier@gmail.com>
 *
 * This Program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 */

#include <list>
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
                      ROUTE_INFO,
                      AIS_DATA,
                      PYPILOT,
                      DISPLAY_COUNT };

enum data_source_e {ESP_DATA, USB_DATA, RS422_DATA, WIFI_DATA, DATA_SOURCE_COUNT};

void display_setup();
void display_set_mirror_rotation(int);
void display_change_page(int dir);
void display_change_scale();
void display_render();
void display_data_update(display_item_e item, float value, data_source_e);
bool display_data_get(display_item_e item, float &value, String &source, uint32_t &time);
void display_auto();
void display_items(std::list<display_item_e> &items);

String getItemLabel(display_item_e item);

struct page_info {
    page_info(char n, String &d) : name(n), description(d), enabled(false) {}
    char name;
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

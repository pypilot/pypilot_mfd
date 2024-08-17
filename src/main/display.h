/* Copyright (C) 2024 Sean D'Epagnier <seandepagnier@gmail.com>
 *
 * This Program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 */

#include <string>
#include <list>
#include <vector>

enum display_item_e { WIND_SPEED,
                      WIND_DIRECTION,
                      TRUE_WIND_SPEED,
                      TRUE_WIND_DIRECTION,
                      GPS_SPEED,
                      GPS_HEADING,
                      LATITUDE,
                      LONGITUDE,
                      BAROMETRIC_PRESSURE,
                      AIR_TEMPERATURE,
                      RELATIVE_HUMIDITY,
                      AIR_QUALITY,
                      BATTERY_VOLTAGE,
                      WATER_SPEED,
                      WATER_TEMPERATURE,
                      DEPTH,
                      COMPASS_HEADING,
                      PITCH,
                      HEEL,
                      RATE_OF_TURN,
                      RUDDER_ANGLE,
                      TIME,
                      ROUTE_INFO,
                      AIS_DATA,
                      PYPILOT,
                      DISPLAY_COUNT };

enum data_source_e {ESP_DATA, USB_DATA, RS422_DATA, WIFI_DATA, COMPUTED_DATA, DATA_SOURCE_COUNT};

void display_setup();
bool display_toggle(bool on=false);
void display_pwr_led(bool on);
void display_set_mirror_rotation(int);
void display_change_page(int dir);
void display_enter_exit_menu();
void display_menu_scale();
void display_poll();
void display_data_update(display_item_e item, float value, data_source_e);
bool display_data_get(display_item_e item, float &value);
bool display_data_get(display_item_e item, float &value, std::string &source, uint32_t &time);
void display_auto();
void display_items(std::list<display_item_e> &items);

std::string display_get_item_label(display_item_e item);

struct page_info {
    page_info(char n, std::string &d) : name(n), description(d), enabled(false) {}
    char name;
    std::string &description;
    bool enabled;
};

struct route_info_t {
    std::string from_wpt, to_wpt;
    float target_bearing;
    float wpt_lat, wpt_lon;
    float xte, brg;
};

struct display {
    display()
        : x(0), y(0), w(0), h(0), expanding(true) {}
    display(int _x, int _y, int _w, int _h)
        : x(_x), y(_y), w(_w), h(_h) {}
    virtual void render() = 0;
    virtual void fit() {}
    virtual void getAllItems(std::list<display_item_e> &items) {}

    int x, y, w, h;
    bool expanding;
};

struct grid_display : public display {
    grid_display(grid_display *parent=0, int _cols = 1, int _rows = -1)
         : cols(_cols), rows(_rows) { if(parent) parent->add(this); }
    void fit();
    void render();
    void add(display *item);
    void getAllItems(std::list<display_item_e> &items_);

    int cols, rows;
    std::list<display*> items;
};

struct page : public grid_display {
    page(std::string _description);
    virtual ~page() {}

    std::string description;
};

extern route_info_t route_info;

extern std::vector<page_info> display_pages;
extern bool landscape;
extern int page_width, page_height;

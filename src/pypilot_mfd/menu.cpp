/* Copyright (C) 2024 Sean D'Epagnier <seandepagnier@gmail.com>
 *
 * This Program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 */

#include <Arduino.h>

#include "alarm.h"
#include "draw.h"
#include "settings.h"
#include "display.h"
#include "utils.h"
#include "menu.h"

bool in_menu;


void selectFont(int &wt, int &ht, String str) {
    // based on width and height determine best font
    int height = ht;
    for(;;) {
        if(!draw_set_font(ht))
            break;
        
        int width = draw_text_width(str.c_str());
        if(width < wt && ht < height) {
            wt = width;
            break;
        }
        ht--;
    }
}

struct menu_label : public display {
    menu_label(String _str) : label(_str) { /* expanding = false */ }
    void render() {
        // try to fit text along side label
        int wt = w, ht = h;
        selectFont(wt, ht, label.c_str());

        // center text
        int yp = y - ht/2;
        int xp = x - wt/2;
        draw_text(xp, yp, label.c_str());
    }

    const uint8_t *label_font;
    int wt, ht;
    int label_w, label_h;
    bool centered; // currently means centered over x (and no label)  maybe should change this
    String label;
};

struct menu_item : public menu_label
{
    menu_item(String n) : menu_label(n) {}
    virtual void select() = 0;
};

struct menu;
static menu *main_menu, *curmenu = 0;

struct menu_item_menu : public menu_item
{
    menu_item_menu(String _name, menu *_menu) : menu_item(_name), item_menu(_menu) {}

    void select() { curmenu = item_menu; }
    menu *item_menu;
};

struct menu : public page
{
    menu(String n) : page(n) {
        parent = 0;
        position = 0;
        cols = 1; // 1 column for menu
        have_back = false;
        add(new menu_label(n));
        menu_items = new grid_display(this, landscape ? 1 : 2);
    }

    void add_item(menu_item *item) {
        menu_items->add(item);
    }

    void add_menu(menu *m) {
        menu_items->add(new menu_item_menu(m->description, m));
        m->parent = this;
    }

    void render() {
        if(!have_back) {
            add(new menu_item_menu("Back", parent));
            have_back = true;
        }
        page::render();
        std::list<display*>::iterator it = menu_items->items.begin();
        std::advance(it, position);
        display *selected = *it;
        // todo: invert or draw box?
        draw_line(selected->x, selected->y, selected->x+selected->w, selected->y+selected->h);
    }

    void select() {
        if(position < menu_items->items.size()) {
            std::list<display*>::iterator it = menu_items->items.begin();
            std::advance(it, position);
            menu_item *i = (menu_item*)*it;
            i->select();
        } else
            printf("invalid menu position!?\n");
    }

    bool have_back;
    uint8_t position;
    menu *parent;
    grid_display *menu_items;
};

struct menu_item_bool : public menu_item
{
    menu_item_bool(String n, bool &s) : menu_item(n), setting(s) {}
    bool &setting;
    void render() {
        menu_label::render();
        if(setting)
            draw_circle(x, y, 12, 2);
    }

    void select() { setting = !setting; }
};

struct menu_item_int : public menu_item
{
    menu_item_int(String n, int &s, int _min, int _max, int _step) :
        menu_item(n), setting(s), min(_min), max(_max), step(_step) {}
    int &setting;
    void render() {
        menu_item::render();
        String s(setting);
        draw_text(x, y, s.c_str());
    }

    void select() { setting += step; if(setting > max) setting = min; }
    int min, max, step;
};
    
static std::map <alarm_e, menu*> alarm_menus;

struct alarm_menu : public menu
{
    alarm_menu(enum alarm_e _alarm) : menu(alarm_name(_alarm)), alarm(_alarm)
    {
        alarm_menus[_alarm] = this;
    }
    alarm_e alarm;
};

void menu_switch_alarm(enum alarm_e alarm)
{
    curmenu = alarm_menus[alarm];
    in_menu = true;
}
            
struct anchor_alarm_display : public display
{
    void render() {
        int thickness = w/80;
        draw_circle(x+w/2, y+h/2, w/2, thickness);

        // render boat and history
        int ht = 12;
        draw_set_font(ht);
        String s = "Dist " + String(anchor_dist);
        draw_text(x, y, s.c_str());
    }
};

struct anchor_alarm_menu : public alarm_menu
{
    anchor_alarm_menu() : alarm_menu(ANCHOR_ALARM) {
        add(new anchor_alarm_display);
        grid_display *d = new grid_display(this);
        d->expanding = false;
        add(d);
        add_item(new menu_item_bool("Enable", settings.anchor_alarm));
        add_item(new menu_item_int("Range", settings.anchor_alarm_distance, 10, 100, 10));
    }
};

struct course_alarm_display : public display
{
    void render() {
        float course_alarm = settings.course_alarm_course;
        int xp = tanf(deg2rad(course_alarm))*h;
        draw_line(x+w/2, y+h, x+w/2-xp, y);
        draw_line(x+w/2, y+h, x+w/2+xp, y);

        float course;
        if(display_data_get(GPS_HEADING, course)) {
            float course_error = course - course_alarm;
            draw_line(x+w/2, y+h, x+w/2+xp, y);
        }
        // render course and course alarm text
        String cs = String(course, 1);
        draw_text(x, y, cs.c_str());
//        draw_text(x, y, course_alarm);
    }
};


// hook to set the course
struct menu_item_course_alarm_enable : public menu_item_bool
{
    menu_item_course_alarm_enable() : menu_item_bool("Enable", settings.course_alarm) {}

    void select() {
        menu_item_bool::select();
        float course;
        if(display_data_get(GPS_HEADING, course))
            settings.course_alarm_course = course;
    }
};
    
struct course_alarm_menu : public alarm_menu
{
    course_alarm_menu() : alarm_menu(COURSE_ALARM) {
        
        add(new course_alarm_display);
        add_item(new menu_item_course_alarm_enable);
        add_item(new menu_item_int("Course Error", settings.course_alarm_course, 10, 40, 5));
    }
};

struct speed_alarm_display : public display
{
    speed_alarm_display(alarm_e _alarm) : alarm(_alarm) {}

    void render() {
        switch(alarm) {
        case GPS_SPEED_ALARM:
            break;
        case WIND_SPEED_ALARM:
            break;
        case WATER_SPEED_ALARM:
            break;
        }
    }
    alarm_e alarm;
};

struct speed_alarm_menu : public alarm_menu
{
    speed_alarm_menu(alarm_e alarm, bool &speed_alarm, int &speed) : alarm_menu(alarm) {
        add(new speed_alarm_display(alarm));
        add_item(new menu_item_bool("Enable", speed_alarm));
        add_item(new menu_item_int("Max Speed", speed, 10, 40, 5));
    }
};

struct weather_alarm_menu : public alarm_menu
{
    weather_alarm_menu() : alarm_menu(WEATHER_ALARM)    {
        add_item(new menu_item_bool("Enable Min", settings.weather_alarm_pressure));
        add_item(new menu_item_int("Pressure", settings.weather_alarm_min_pressure, 900, 1020, 5));
        add_item(new menu_item_bool("Enable Falling", settings.weather_alarm_pressure_rate));
        add_item(new menu_item_int("Falling Rate (mbar/min)", settings.weather_alarm_pressure_rate_value, 1, 10, 1));
        add_item(new menu_item_bool("Enable Lightning", settings.weather_alarm_lightning));
        add_item(new menu_item_int("Lightning Distance NMi", settings.weather_alarm_lightning_distance, 1, 20, 1));        
    }
};
    
struct depth_alarm_menu : public alarm_menu
{
    depth_alarm_menu() : alarm_menu(DEPTH_ALARM) {
        add_item(new menu_item_bool("Enable Min", settings.depth_alarm));
        add_item(new menu_item_int("Depth (m)", settings.depth_alarm_min, 1, 30, 1));
        add_item(new menu_item_bool("Depth Rate", settings.depth_alarm_rate));
        add_item(new menu_item_int("Depth in (M/min)", settings.depth_alarm_rate_value, 1, 0, 1));
    }
};

struct pypilot_alarm_menu : public alarm_menu
{
    pypilot_alarm_menu() : alarm_menu(PYPILOT_ALARM)    {
        add_item(new menu_item_bool("No Connection", settings.pypilot_alarm_noconnection));
        add_item(new menu_item_bool("Over Temperature", settings.pypilot_alarm_fault));
        add_item(new menu_item_bool("No IMU", settings.pypilot_alarm_noIMU));
        add_item(new menu_item_bool("No Motor Controller", settings.pypilot_alarm_no_motor_controller));
        add_item(new menu_item_bool("Lost Mode", settings.pypilot_alarm_lost_mode));
/*
        m_bNoRudderFeedback(false), m_bNoMotorTemperature(false),
        m_bEndOfTravel(false), m_bLostMode(false),
        m_bCourseError(false), m_dCourseError(20)
*/
    }
};

struct menu_choice_item : public menu_label
{
    menu_choice_item(String _label, String &_setting)
        : menu_label(_label), setting(_setting) {}

    void render() {
        menu_label::render();
        if(setting == label) // indicate selection
            draw_circle(x+h/2, y+h/2, h/3);
    }
    
    void select() {
        setting = label;
    }

    String &setting;
};

struct menu_choice : public menu
{
    menu_choice(String label, std::vector<String> choices, String &setting)
        : menu(label) {
        for(int i=0; i<choices.size(); i++) {
            add(new menu_choice_item(choices[i], setting));
            if(choices[i] == setting)
                position = i;
        }
    }
};

// switch to the menu that triggerd the alarm
void menu_alarm(alarm_e alarm)
{
    curmenu = alarm_menus[alarm];
    in_menu = true;
}
    
void menu_select()
{
    if(!in_menu) {
        in_menu = true;
        return;
    }
    curmenu->select();
    in_menu = !!curmenu;
}

void menu_render()
{
    if(!curmenu)
        curmenu = main_menu;

    curmenu->render();
}

void menu_setup()
{
    menu *alarms = new menu("Alarms");
    alarms->add_menu(new anchor_alarm_menu);
    alarms->add_menu(new course_alarm_menu);
    alarms->add_menu(new speed_alarm_menu(GPS_SPEED_ALARM, settings.gps_speed_alarm, settings.gps_speed_alarm_knots));
    alarms->add_menu(new speed_alarm_menu(WIND_SPEED_ALARM, settings.wind_speed_alarm, settings.wind_speed_alarm_knots));
    alarms->add_menu(new speed_alarm_menu(WATER_SPEED_ALARM, settings.water_speed_alarm, settings.water_speed_alarm_knots));
    alarms->add_menu(new weather_alarm_menu);
    alarms->add_menu(new depth_alarm_menu);
    alarms->add_menu(new pypilot_alarm_menu);

    menu *settings_menu = new menu("Settings");
    settings_menu->add_item(new menu_item_bool("use 360 degrees", settings.use_360));
    settings_menu->add_item(new menu_item_bool("use fahrenheit", settings.use_fahrenheit));
    settings_menu->add_item(new menu_item_bool("use in Hg", settings.use_inHg));
    settings_menu->add_item(new menu_item_bool("use depth_ft", settings.use_depth_ft));
    std::vector<String> fmts{"degrees", "minutes", "seconds"};
    settings_menu->add_menu(new menu_choice("lat/lon format", fmts, settings.lat_lon_format));

    menu *display = new menu("Display");
    //display->add(backlight);
    //display->add(contrast);
#ifdef USE_U8G2
    display->add_item(new menu_item_bool("Invert", settings.invert));
#else
    std::vector<String> fmts{"Normal", "Dark", "Alt1"};
    display->add_menu(new menu_choice("Color Scheme", schemes, settings.color_scheme));
#endif
    //display->add_item(new menu_item_bool("Mirror", settings.mirror));
    display->add_item(new menu_item_bool("Show Status", settings.show_status));
    

    main_menu = new menu("Main Menu");
    main_menu->add_menu(alarms);
    main_menu->add_menu(display);
    main_menu->add_menu(settings_menu);
}

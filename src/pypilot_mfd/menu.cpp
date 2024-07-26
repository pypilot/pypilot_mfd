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
#include "buzzer.h"

bool in_menu;

static bool selectFont(int &wt, int &ht, String str) {
    // based on width and height determine best font
    if(ht > 40) ht = 40;

    int height = ht;
    for(;;) {
        if(!draw_set_font(ht))
            break;
        
        int width = draw_text_width(str.c_str());
        if(width < wt && ht < height) {
            wt = width;
            return true;
        }
        ht--;
    }
    return false;
}

struct menu_label : public display {
    menu_label(String _str, int _h=0) : label(_str) {
        expanding = false;
        h=_h;
    }

    void render() {
        ht = h;  // it will never get bigger
        int wt = w;
        if(!selectFont(wt, ht, label.c_str()))
            return;

        // center text
        int yp = y + h/2 - ht/2;
        int xp = x + w/2 - wt/2;
        draw_text(xp, yp, label.c_str());
    }

    int ht;
    String label;
};

struct menu_item : public menu_label
{
    menu_item(String n) : menu_label(n) {}
    virtual void select() = 0;
};


struct menu_page : public page
{
    menu_page(String n) : page(n) {}
    virtual void select() = 0;
    virtual void arrows(int dir) = 0;
    virtual void reset() {}
};
static menu_page *curmenu = 0;

struct menu_item_menu : public menu_item
{
    menu_item_menu(String _name, menu_page *_menu) : menu_item(_name), item_menu(_menu) {}

    void select() { curmenu->reset(); curmenu = item_menu; }
    menu_page *item_menu;
};

struct menu : public menu_page
{
    menu(String n) : menu_page(n) {
        parent = 0;
        position = 0;
        cols = 1; // 1 column for menu
        have_back = false;
        add(new menu_label(n, h/4));
        menu_items = new grid_display(this, 1);
    }

    // put stuff in the menu between the label and the list of menu items
    void insert(display *item)
    {
        auto it = items.end(); // Points to one past the last element
        --it; // Now points to the last element
        items.insert(it, item);
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
            add_item(new menu_item_menu("Back", parent));
            have_back = true;
            fit();
        }
        menu_page::render();
        std::list<display*>::iterator it = menu_items->items.begin();
        std::advance(it, position);
        display *selected = *it;
        draw_invert(1);
        // todo: invert or draw box?
        draw_box(selected->x, selected->y, selected->w, selected->h);
        draw_invert(0);
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

    void arrows(int dir) {
        if(landscape)
            dir = -dir;
        buzzer_buzz(800, 50, 0);
        int count = menu_items->items.size();
        int pos = position - dir;
        if(pos < 0)
            pos = count - 1;
        else if(pos >= count)
            pos = 0;
        position = pos;
    }

    void reset() { position = 0; }

    bool have_back;
    uint8_t position;
    menu *parent;
    grid_display *menu_items;
};

struct menu_item_setting : public menu_item
{
    menu_item_setting(String n) : menu_item(n) { }
    void render() {
        // try to fit text along side label
        int wt = w - h, ht = h;
        if(!selectFont(wt, ht, label.c_str()))
            return;

        // center text
        int yp = y + h/2 - ht/2;
        int xp = x + h + (w-h)/2 - wt/2;
        draw_text(xp, yp, label.c_str());

        printf("circle %d\n", h);
        if(isset())
            draw_circle(x+h/2+2, y+h/2, h/3-2, 2);
    }

    virtual bool isset() = 0;
};

struct menu_item_bool : public menu_item_setting
{
    menu_item_bool(String n, bool &s) : menu_item_setting(n), setting(s) { }
    bool &setting;
    bool isset() { return setting; }
    void select() { setting = !setting; }
};

struct setting_int : public menu_page
{
    setting_int(menu_page *_back, String n, int &s, int _min, int _max, int _step) :
        menu_page(n), back(_back), setting(s), min(_min), max(_max), step(_step) {
        cols = 1; // 1 column for menu
        add(new menu_label(n, h/4));
        label = new menu_label(n, h/4);
        add(label);
        fit();
    }

    void render() {
        label->label = String(setting);
         menu_page::render();
    }

    void arrows(int dir) {
        if(dir > 0) {
            setting += step;
            if(setting > max) {
                setting = max;
                buzzer_buzz(1800, 50, 0);
                return;
            }
        } else if(dir < 0) {
            setting -= step;
            if(setting < min) {
                setting = min;
                buzzer_buzz(1800, 50, 0);
                return;
            }
        }
        buzzer_buzz(800, 50, 0);
    }

    void select() { curmenu = back; }

    menu_page *back;
    int &setting;
    int min, max, step;
    menu_label *label;
};

struct menu_item_int : public menu_item
{
    menu_item_int(menu_page *back, String n, int &s, int _min, int _max, int _step) :
        menu_item(n),
        setting(back, n, s, _min, _max, _step) {}

    void render() {
        String s = String(setting.setting);
        String l = s + " " + label;

        int wt = w - h, ht = h;
        if(!selectFont(wt, ht, l.c_str()))
            return;

        // align text
        int yp = y + h/2 - ht/2;
        draw_text(x, yp, s.c_str());
        int width = draw_text_width(label.c_str());
        draw_text(x+w-width, yp, label.c_str());
    }

    void select() { curmenu = &setting; }
    setting_int setting;
};
    
static std::map <alarm_e, menu*> alarm_menus;

struct alarm_display : public grid_display
{
    alarm_display(alarm_e alarm_) : alarm(alarm_),
    current_label("Current:"), last_label("Last alarm:"), reason_label("Reason:"),
    current(" N/A "), last(" never ever "), reason("  ") {
        cols=2;
        if(alarm != PYPILOT_ALARM) {
            add(&current_label);
            add(&current);
        }
        add(&last_label);
        add(&last);
        add(&reason_label);
        add(&reason);
    }

    void render() {
        float c;
        display_item_e item = DISPLAY_COUNT;
        switch(alarm) {
        case ANCHOR_ALARM: c = alarm_anchor_dist;   break;
        case COURSE_ALARM: item = GPS_HEADING;      break;
        case GPS_SPEED_ALARM: item = GPS_SPEED;     break;
        case WIND_SPEED_ALARM: item = WIND_SPEED;   break;
        case WATER_SPEED_ALARM: item = WATER_SPEED; break;
        case WEATHER_ALARM: item = BAROMETRIC_PRESSURE; break;
        case DEPTH_ALARM: item = DEPTH;             break;
        case AIS_ALARM:   c = alarm_ship_tcpa;      break;
        case PYPILOT_ALARM:    c = NAN;             break;
        }

        if(c != DISPLAY_COUNT) {
            if(display_data_get(item, c))
                current.label = String(c, 2);
            else
                current.label = " N/A ";
        }

        String s_reason;
        float dt = alarm_last(alarm, s_reason);
        if(!dt) {
            last.label = "never";
            reason.label = "";
        } else {
            last.label = millis_to_str(dt);
            reason.label = s_reason;
        }

        grid_display::render();
    }

    alarm_e alarm;
    menu_label current_label, last_label, reason_label;
    menu_label current, last, reason;
};

struct alarm_menu : public menu
{
    alarm_menu(enum alarm_e _alarm) : menu(alarm_name(_alarm)), alarm(_alarm)
    {
        insert(new alarm_display(alarm));
        alarm_menus[_alarm] = this;
    }
    alarm_e alarm;
};

void menu_switch_alarm(enum alarm_e alarm)
{
    curmenu = alarm_menus[alarm];
    curmenu->reset(); // reset menu position
    in_menu = true;
}
            
struct anchor_alarm_display : public display
{
    void render() {
        int thickness = w/120;
        int r;
        if(h < w)
            r = h;
        else
            r = w;
        r -= thickness;
        draw_circle(x+w/2, y+h/2, r/2, thickness);

        // now draw the boat in the circle
        float lat, lon;
        if(!display_data_get(LATITUDE, lat) || !display_data_get(LONGITUDE, lon)) {
            printf("Failed to get lat/lon when anchor alarm set");
            return;
        }

        double slat = settings.anchor_lat, slon = settings.anchor_lon;
        double x = cosf(deg2rad(lat))*resolv(lon-slon) * 60;
        double y = (lat-slat)*60;

        // x and y are in miles, normalize to anchor range
        x *= settings.anchor_alarm_distance / 1852;
        y *= settings.anchor_alarm_distance / 1852;

        x *= r, y *= r; // convert to pixels;
        draw_circle(x, y, thickness, thickness);
    }
};

struct menu_item_anchor_alarm_enable : public menu_item_bool
{
    menu_item_anchor_alarm_enable() : menu_item_bool("Enable", settings.anchor_alarm) {}

    void select() {
        menu_item_bool::select();
        alarm_anchor_reset();
    }
};

struct anchor_alarm_menu : public alarm_menu
{
    anchor_alarm_menu() : alarm_menu(ANCHOR_ALARM) {
        insert(new anchor_alarm_display);
        add_item(new menu_item_anchor_alarm_enable);
        add_item(new menu_item_int(this, "Range", settings.anchor_alarm_distance, 10, 100, 10));
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
            xp = tanf(deg2rad(course_error))*h;
            draw_line(x+w/2, y+h, x+w/2+xp, y);
        }
        // render course and course alarm text
        String cs = String(course, 1);
        draw_text(x, y, cs.c_str());
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
        insert(new course_alarm_display);
        add_item(new menu_item_course_alarm_enable);
        add_item(new menu_item_int(this, "Course Error", settings.course_alarm_course, 5, 90, 5));
    }
};

struct speed_alarm_menu : public alarm_menu
{
    speed_alarm_menu(alarm_e alarm, bool &speed_alarm, int &min_speed, int &max_speed) : alarm_menu(alarm) {
        add_item(new menu_item_bool("Enable", speed_alarm));
        add_item(new menu_item_int(this, "Min Speed", min_speed, 0, 10, 1));
        add_item(new menu_item_int(this, "Max Speed", max_speed, 1, 100, 1));
    }
};

struct weather_alarm_menu : public alarm_menu
{
    weather_alarm_menu() : alarm_menu(WEATHER_ALARM)    {
        add_item(new menu_item_bool("Enable Min Pressure", settings.weather_alarm_pressure));
        add_item(new menu_item_int(this, "Pressure", settings.weather_alarm_min_pressure, 900, 1100, 5));
        add_item(new menu_item_bool("Enable Falling Pressure", settings.weather_alarm_pressure_rate));
        add_item(new menu_item_int(this, "Falling Pressure Rate (mbar/min)", settings.weather_alarm_pressure_rate_value, 1, 10, 1));
        add_item(new menu_item_bool("Enable Lightning", settings.weather_alarm_lightning));
        add_item(new menu_item_int(this, "Lightning Distance NMi", settings.weather_alarm_lightning_distance, 1, 50, 1));        
    }
};
    
struct depth_alarm_menu : public alarm_menu
{
    depth_alarm_menu() : alarm_menu(DEPTH_ALARM) {
        add_item(new menu_item_bool("Enable Min", settings.depth_alarm));
        add_item(new menu_item_int(this, "Depth (m)", settings.depth_alarm_min, 1, 30, 1));
        add_item(new menu_item_bool("Depth Rate", settings.depth_alarm_rate));
        add_item(new menu_item_int(this, "Depth in (M/min)", settings.depth_alarm_rate_value, 1, 10, 1));
    }
};

struct pypilot_alarm_menu : public alarm_menu
{
    pypilot_alarm_menu() : alarm_menu(PYPILOT_ALARM)    {
        add_item(new menu_item_bool("No Connection", settings.pypilot_alarm_noconnection));
        add_item(new menu_item_bool("Over Temperature", settings.pypilot_alarm_fault));
        add_item(new menu_item_bool("No IMU", settings.pypilot_alarm_no_imu));
        add_item(new menu_item_bool("No Motor Controller", settings.pypilot_alarm_no_motor_controller));
        add_item(new menu_item_bool("Lost Mode", settings.pypilot_alarm_lost_mode));
/*
        m_bNoRudderFeedback(false), m_bNoMotorTemperature(false),
        m_bEndOfTravel(false), m_bLostMode(false),
        m_bCourseError(false), m_dCourseError(20)
*/
    }
};

struct menu_choice_item : public menu_item_setting
{
    menu_choice_item(String _label, String &_setting)
        : menu_item_setting(_label), setting(_setting) {}
    
    bool isset() { return setting==label; }

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
            add_item(new menu_choice_item(choices[i], setting));
            if(choices[i] == setting)
                position = i;
        }
    }
};

void menu_arrows(int dir)
{
    curmenu->arrows(dir);
}

void menu_select()
{
    curmenu->select();
    in_menu = !!curmenu;
    if(!in_menu)
        settings_store();
    buzzer_buzz(1000, 50, 0);
}

static menu *main_menu;

void menu_render()
{
    if(!curmenu)
        curmenu = main_menu;

    curmenu->render();
}

void menu_reset()
{
    curmenu = NULL;
    in_menu = false;
}

void menu_setup()
{
    menu *alarms = new menu("Alarms");
    alarms->menu_items->cols = landscape ? 2 : 1;
    alarms->add_menu(new anchor_alarm_menu);
    alarms->add_menu(new course_alarm_menu);
    alarms->add_menu(new speed_alarm_menu(GPS_SPEED_ALARM, settings.gps_speed_alarm, settings.gps_min_speed_alarm_knots, settings.gps_max_speed_alarm_knots));
    alarms->add_menu(new speed_alarm_menu(WIND_SPEED_ALARM, settings.wind_speed_alarm, settings.wind_min_speed_alarm_knots, settings.wind_max_speed_alarm_knots));
    alarms->add_menu(new speed_alarm_menu(WATER_SPEED_ALARM, settings.water_speed_alarm, settings.water_min_speed_alarm_knots, settings.water_max_speed_alarm_knots));
    alarms->add_menu(new weather_alarm_menu);
    alarms->add_menu(new depth_alarm_menu);
    alarms->add_menu(new pypilot_alarm_menu);

    menu *settings_menu = new menu("Settings");
    settings_menu->add_item(new menu_item_bool("use 360 degrees", settings.use_360));
    settings_menu->add_item(new menu_item_bool("use fahrenheit", settings.use_fahrenheit));
    settings_menu->add_item(new menu_item_bool("use in Hg", settings.use_inHg));
    settings_menu->add_item(new menu_item_bool("use depth ft", settings.use_depth_ft));
    std::vector<String> fmts{"degrees", "minutes", "seconds"};
    settings_menu->add_menu(new menu_choice("lat/lon format", fmts, settings.lat_lon_format));

    menu *display = new menu("Display");
    display->add_item(new menu_item_int(display, "Backlight", settings.backlight, 0, 100, 1));
    display->add_item(new menu_item_int(display, "Contrast", settings.contrast, 0, 50, 1));
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

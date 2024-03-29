/* Copyright (C) 2024 Sean D'Epagnier <seandepagnier@gmail.com>
 *
 * This Program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 */

#include <math.h>
#include <WiFi.h>

#include <list>

#include "Arduino.h"

#include "bmp280.h"
#include "settings.h"   
#include "display.h"
#include "ais.h"
#include "pypilot_client.h"
#include "utils.h"

#if 1

// autocompensate contrast based on light and temperature
#define BACKLIGHT_PIN 14
#define NTC_PIN       35
#define PHOTO_RESISTOR_PIN 34

#include <U8g2lib.h>

U8G2_ST75256_JLX256160_F_4W_SW_SPI u8g2(U8G2_R1, /* clock=*/15, /* data=*/13, /* cs=*/12, /* dc=*/5, /* reset=*/14);

String getItemLabel(display_item_e item) {
    switch (item) {
    case WIND_SPEED: return "Wind Speed";
    case WIND_DIRECTION: return "Wind Dir";
    case BAROMETRIC_PRESSURE: return "Baro Pressure";
    case AIR_TEMPERATURE: return "Air Temp";
    case WATER_TEMPERATURE: return "Water Temp";
    case GPS_SPEED: return "GPS Speed";
    case GPS_HEADING: return "GPS Heading";
    case LATITUDE: return "Lat";
    case LONGITUDE: return "Lon";
    case WATER_SPEED: return "Water Spd";
    case COMPASS_HEADING: return "Compass Hdg";
    case PITCH:        return "Pitch";
    case HEEL:         return "Heel";
    case DEPTH:        return "Depth";
    case RATE_OF_TURN: return "Rate of Turn";
    case RUDDER_ANGLE: return "Rudder Angle";
    case TIME:         return "Time";
    }
    return "";
}

// 5m, 1 hr, 1d
enum history_range_e {MINUTE, HOUR, DAY, RANGE_COUNT};
uint32_t history_range_time[] = {5000, 60, 20, 24};

int history_display_range;

String getHistoryLabel(history_range_e range)
{
    switch(range) {
        case MINUTE: return "5m";
        case HOUR:   return "1h";
        case DAY:    return "1d";
    }
    return "";
}

display_item_e history_items[] = { WIND_SPEED, BAROMETRIC_PRESSURE, DEPTH, GPS_SPEED, WATER_SPEED };

struct history_element {
    float value;
    uint32_t time;
    history_element(float _value, uint32_t _time) : value(_value), time(_time) {}
};

struct history
{
    std::list<history_element> data[RANGE_COUNT];

    void put(float value, uint32_t time)
    {
        int range_time = history_range_time[0];
        for(int range = 0; range < RANGE_COUNT; range++) {
            history_element &front = data[range].front();
            uint32_t dt = time - front.time;
            if(dt > range_time) {
                if(range > 0) {
                    // average previous range
                    value = 0;
                    int count = 0;
                    for(std::list<history_element>::iterator it = data[range-1].begin(); it != data[range-1].end(); it++) {
                        value += it->value;
                        count++;
                    }
                    value /= count;
                }
                data[range].push_front(history_element(value, time));
            }
            range_time *= history_range_time[range+1];

            while(!data[range].empty()) {
                history_element &back = data[range].back();
                if(time - back.time < range_time)
                    break;
                data[range].pop_back();
            }
        }
    }
};

#define HISTORY_COUNT  (sizeof history_items) / (sizeof *history_items)

history histories[HISTORY_COUNT];


struct display_data_t {
    display_data_t() : time(-10000) {}

    float value;
    uint32_t time;
    data_source_e source;
};

display_data_t display_data[DISPLAY_COUNT];

uint32_t data_source_time[DATA_SOURCE_COUNT];


void display_data_update(display_item_e item, float value, data_source_e source) {
    uint32_t time = millis();
    //Serial.printf("data_update %d %s %f\n", item, getItemLabel(item).c_str(), value);

    if(source > display_data[item].source) {
        // ignore if higher priority data source updated in last 5 seconds
        if(time - display_data[item].time < 5000)
            return;
    }

    for(int i=0; i<(sizeof history_items)/(sizeof *history_items); i++)
        if(history_items[i] == item) {
            histories[i].put(value, time);
            break;
        }

    display_data[item].value = value;
    display_data[item].time = time;
    display_data[item].source = source;
    data_source_time[source] = time;
}

struct display {
  display()
    : x(0), y(0), w(0), h(0), expanding(true) {}
  display(int _x, int _y, int _w, int _h)
    : x(_x), y(_y), w(_w), h(_h) {}
  virtual void render() {}
  virtual void fit() {}

  int x, y, w, h;
  bool expanding;
};

struct display_item {
    display_item(display_item_e _item)
    : item(_item) {}

    uint32_t age() { return millis() - display_data[item].time; }

    enum display_item_e item;
};

struct text_display : public display {
    text_display() {
        expanding = false;
    }

    void fit() {
        ht = h;

        String str = getLabel();
        label_font = u8g2_font_helvB08_tf;
        u8g2.setFont(label_font);
        label_w = u8g2.getStrWidth(str.c_str());
        label_h = 10;

        if(label_w > w/2 || label_h > h/2) {
            label_font = u8g2_font_5x7_tf;
            u8g2.setFont(label_font);
            label_w = u8g2.getStrWidth(str.c_str());
            label_h = 7;

        }
    }

    const uint8_t *getFont() {
            if(ht < 7) return 0;
            if(ht < 11) return ht = 7, u8g2_font_5x7_tf;
            if (ht < 13) return ht = 11, u8g2_font_helvB08_tf;
            if (ht < 15) return ht = 13, u8g2_font_helvB10_tf;
            if (ht < 18) return ht = 15, u8g2_font_helvB12_tf;
            if (ht < 24) return ht = 18, u8g2_font_helvB14_tf;
            if (ht < 28) return ht = 24, u8g2_font_helvB18_tf;
            if (ht < 30) return ht = 28, u8g2_font_helvB24_tf;
            if (ht < 35) return ht = 30, u8g2_font_inb24_mf;
            if (ht < 40) return ht = 35, u8g2_font_inb27_mf;
            if (ht < 44) return ht = 40, u8g2_font_inb30_mf;
            if (ht < 48) return ht = 44, u8g2_font_inb33_mf;
            if (ht < 52) return ht = 48, u8g2_font_inb38_mf;
            if (ht < 56) return ht = 52, u8g2_font_inb42_mf;
            return ht = 56, u8g2_font_inb46_mf;
    }

    void selectFont(int &wt, String str) {
        // based on width and height determine best font
        for(;;) {
            const uint8_t *font = getFont();
            if(!font)
                break;
            u8g2.setFont(font);

            int width = u8g2.getUTF8Width(str.c_str());
            if(width < wt) {
                wt = width;
                break;
            }
            ht--;
        }
    }

    void render() {
        // determine font size to use from text needed
        String str = getText();

        // try to fit text along side label
        int wt = w;
        selectFont(wt, str);

        bool label = true;
        if(ht + label_h > h && wt + label_w > w)
            label = false;

        int yp = y;
        if(label_h) yp += label_h/2;
        yp += (h-ht)/2;
        int xp = x + w-wt;

        //Serial.printf("draw text %s %d %d %d %d %d %d %d %d\n", getLabel(item).c_str(), x, y, w, h, wt, ht, label_w, label_h);
        u8g2.drawUTF8(xp, yp, str.c_str());
        if(label) {
            u8g2.setFont(label_font);
            u8g2.drawUTF8(x, y, getLabel().c_str());
        }
    }

    virtual String getLabel() = 0;
    virtual String getText() = 0;
    const uint8_t *label_font;
    int ht;
    int label_w, label_h;
};

struct text_display_item : public text_display, public display_item {
    text_display_item(display_item_e _i) : display_item(_i) {}

    String getText() {
        if(age() > 5000)
            return "N/A";
        return getTextItem();
    }

    virtual String getTextItem() = 0;

    String getLabel() {
        return getItemLabel(item);
    }
};

struct speed_text_display : public text_display_item  {
    speed_text_display(display_item_e _i) : text_display_item(_i) {}

    String getTextItem() {
        if(w < 20)
            return String(display_data[item].value, 0);
        String s = String(display_data[item].value, 1);
        if (w < 30 || w/h < 3)
            return s;
        return s + " kt";
    }
};

struct heading_text_display : public text_display_item {
    heading_text_display(display_item_e _i) : text_display_item(_i) {}

    String getTextItem() {
        String s = String(display_data[item].value, 0);
        if(w < 30)
            return s;
        return s + "°";
    }
};

struct temperature_text_display : public text_display_item {
     temperature_text_display(display_item_e _i) : text_display_item(_i) {}

    String getTextItem() {
        String s, u;
        if (settings.use_fahrenheit) {
            s = String(display_data[item].value * 9 / 5 + 32, 1);
            u = " °F";
        } else {
            s = String(display_data[item].value, 1);
            u = " °C";
        }
        if(w < 30)
            return s;
        return s + u;
    }
};

struct depth_text_display : public text_display_item {
    depth_text_display() : text_display_item(DEPTH) {}

    String getTextItem() {
        String s, u;
        if (settings.use_depth_ft) {
            s = String(display_data[item].value * 3.28, 1);
            u = "ft";
        } else {
            s = String(display_data[item].value, 1);
            u = "m";
        }
        if(w < 30)
            return s;
        return s + u;
    }
};

struct pressure_text_display : public text_display_item {
    pressure_text_display() : text_display_item(BAROMETRIC_PRESSURE) {}

    String getTextItem() {
        float cur = display_data[item].value;
        String s;
        if(w > 80)
            s = String(cur, 2);
        else if(w > 50)
            s = String(cur, 1);
        else
            s = String(cur, 0);
        if(w > 60 && prev.size()>3) {
            if(cur < prev.front() - 1)
                s += "↓";
            else if(cur > prev.front() + 1)
                s += "↑";
            else
                s += "~";
        }

        if(millis() - prev_time > 60000) {
            prev.push_back(cur);
            if(prev.size() > 5)
                prev.pop_front();
            prev_time = millis();
        }

        if(w > 100)
            s += " mbar";

        return s;
    }

    std::list<float> prev;
    uint32_t prev_time;
};

struct position_text_display : public text_display_item {
    position_text_display(display_item_e _i) : text_display_item(_i) {}

    String getTextItem() {
        float v = display_data[item].value;

        char s;
        if (item == LATITUDE)
            s = (v < 0) ? 'S' : 'N';
        else
            s = (v < 0) ? 'W' : 'E';
        v = fabsf(v);
        String str;
        if (settings.lat_lon_format == DECIMAL_DEGREES)
            str = String(v, 6);
        else {
            float i;
            float f = modff(v, &i) * 60;
            str = String(i) + ", ";
            if (settings.lat_lon_format == DECIMAL_MINUTES)
                str += String(f, 4);
            else {
                f = modff(f, &i) * 60;
                str += String(i) + ", " + String(f, 2);
            }
        }

        return str + ' ' + "°" + s;
    }
};

struct time_text_display : public text_display_item {
    time_text_display() : text_display_item(TIME) {}

    String getTextItem() {
        float t = display_data[item].value;
        float hours, minutes;
        float seconds = modff(t/60, &minutes)*60;
        minutes = modff(minutes/60, &hours)*60;

        char buf[24];
        snprintf(buf, sizeof buf, "%02d:%02d:%02d", (int)hours, (int)minutes, (int)seconds);
        return String(buf);
    }
};

struct label_text_display : public text_display  {
    label_text_display(String label_)
        : label(label_) {}

    String getLabel() {
        return label;
    }
    
    String label;
};

struct float_text_display : public label_text_display  {
    float_text_display(String label_, float &value_, int digits_)
        : label_text_display(label_), value(value_), digits(digits_) { }

    String getText() {
        return String(value, digits);
    }

    float &value;
    int digits;
};

struct string_text_display : public label_text_display  {
    string_text_display(String label_, String &value_)
        : label_text_display(label_), value(value_) { }

    String getText() {
        return value;
    }

    String &value;
};

struct pypilot_text_display : public label_text_display  {
    pypilot_text_display(String label_, String key_, bool expanding_=false)
        : label_text_display(label_), key(key_) { expanding = expanding_; pypilot_watch(key); }

    String getText() {
        return pypilot_client_value(key);
    }

    String key;
};

struct gauge : public display, public display_item {
    gauge(display_item_e _i, int _min_v, int _max_v, int _min_ang, int _max_ang, float _ang_step)
        : display_item(_i),
        min_v(_min_v), max_v(_max_v),
        min_ang(_min_ang), max_ang(_max_ang), ang_step(_ang_step) {

        nxp = 0, nyp = 0;
        dialfmt = "%03.0f";
    }

    void fit() {
        if(w > h)
            w = h;
        else
            h = w;
    }

    void render_ring() {
        xc = x + w / 2;
        yc = y + h / 2;
        r = min(w / 2, h / 2);

        int thick = r / 36;
        r -= thick*2;
        for (int i = -thick; i <= thick; i++)
            for (int j = -thick; j <= thick; j++)
                u8g2.drawCircle(xc + i, yc + j, r);
    }

    void render_tick(float angle, int u, int &x0, int &y0) {
        float rad = deg2rad(angle);
        float s = sin(rad), c = cos(rad);

        x0 = xc + (r - u) * s;
        y0 = yc - (r - u) * c;
        int v = 3;
        int x1 = xc + r * s;
        int xp = v * c;
        int y1 = yc - r * c;
        int yp = v * s;
        u8g2.drawTriangle(x1 - xp, y1 - yp, x0, y0, x1 + xp, y1 + yp);
    }

    void render_ticks(bool text = false) {
        char buf[16];

        if (w > 64)
            u8g2.setFont(u8g2_font_helvB12_tf);
        else
            u8g2.setFont(u8g2_font_helvB08_tf);

        for (float angle = min_ang; angle <= max_ang; angle += ang_step) {
            int x0, y0;
            render_tick(angle, w/12, x0, y0);

            if (text) {
                float val = angle * (max_v - min_v) / (max_ang - min_ang) + min_v;
                snprintf(buf, sizeof buf, "%03d", (int)val);
                u8g2.drawStr(x0, y0, buf);
            }

            if (text) {  // intermediate ticks
                float iangle = angle + ang_step / 2.0f;
                render_tick(iangle, w/20, x0, y0);
            }
        }
    }

    void render_dial() {
        // draw actual arrow toward wind direction
        float val = display_data[item].value;
        if (age() > 5000)
            return;

        // map over range from 0 - 1
        float v = (val-min_v)/ (max_v-min_v);

        // now convert to gauge angle
        v = min_ang + v * (max_ang - min_ang);

        float rad = deg2rad(v);
        float s = sin(rad), c = cos(rad);
        int x0 = r * s;
        int y0 = -r * c;
        int u = 1+w/30;
        int xp = u * c;
        int yp = u * s;

        u8g2.drawTriangle(xc - xp, yc - yp, xc + x0, yc + y0, xc + xp, yc + yp);

        xp = -r / 3 * s, yp = r / 4 * c;

        nxp = (xp + 15 * nxp) / 16;
        nyp = (yp + 15 * nyp) / 16;
        xp = xc + nxp - 31, yp = yc + nyp - 20;

        if (w > 100)
            u8g2.setFont(u8g2_font_helvB24_tf);
        else
            u8g2.setFont(u8g2_font_helvB14_tf);

        u8g2.drawStr(xp, yp, dialText().c_str());

        u8g2.setFont(u8g2_font_5x7_tf);
        String label = getItemLabel(item);
        int lw = u8g2.getStrWidth(label.c_str());
        if(lw < w/2)
            u8g2.drawStr(x, y, label.c_str());
    }

    String dialText()
    {
        char buf[16];
        snprintf(buf, sizeof buf, dialfmt.c_str(), display_data[item].value);
        return buf;
    }

    virtual void render() {
        render_ring();
        render_ticks();
        render_dial();
    }

    int xc, yc, r;
    int min_v, max_v;
    bool i_ticks;
    int min_ang, max_ang;
    float ang_step;

    float nxp, nyp;
    String dialfmt;
};

struct wind_direction_gauge : public gauge {
    wind_direction_gauge() : gauge(WIND_DIRECTION, -180, 180, -180, 180, 45) {}

    void render() {
        gauge::render();
        // now compute true wind from apparent
        // render true wind indicator near ring
        uint32_t t = millis() - 5000;
        if( display_data[WIND_DIRECTION].time > t &&
            display_data[WIND_SPEED].time > t &&
            display_data[GPS_SPEED].time > t &&
            display_data[GPS_HEADING].time > t) {
            float wind = display_data[WIND_SPEED].value;
            float windd = display_data[WIND_DIRECTION].value;
            float rad = deg2rad(windd);
            float kts = display_data[GPS_SPEED].value;
            float ax = cos(rad)*kts;
            float ay = sin(rad)*kts;
            float track = display_data[GPS_HEADING].value;
            rad = deg2rad(track);
            float bx = cos(rad)*kts;
            float by = sin(rad)*kts;

            float tx = ax + bx;
            float ty = ay + by;

            float tws = hypot(tx, ty);
            float twd = atan2(ty, tx);
            float s = sin(twd), c = cos(twd);

            int x0 = s*r, y0 = c*r;
            int x1 = x0*8/10, y1 = y0*8/10;
            int xp = c*3, yp = s*3;
            u8g2.drawTriangle(xc + x0 - xp, yc + y0 - yp,
                              xc + x1, yc + y1,
                              xc + x0 + xp, yc + y0 + yp);
        }
    }
};

float boat_coords[] = {0, -.6, .15, -.4, .2, -.25, .15, .5, -.15, .5, -.2, -.25, -.15, -.4, 0, -.6};

void drawThickLine(int x1, int y1, int x2, int y2, int w)
{
    int dx = x2-x1, dy = y2-y1;
    int d = sqrt(dx*dx + dy*dy);
    dx = dx*w/d;
    dy = dy*w/d;

    int ax = x1+dy, ay = y1-dx;
    int bx = x2+dy, by = y2-dx;
    u8g2.drawTriangle(x1, y1, x2, y2, ax, ay);
    u8g2.drawTriangle(x2, y2, bx, by, ax, ay);
}

struct heading_gauge : public gauge {
    heading_gauge(display_item_e _i) : gauge(_i, -180, 180, -180, 180, 45) {}

    void render() {
        render_ring();
        render_ticks();
        if (age() > 5000)
            return;
        float v = display_data[item].value;

        int lx, ly;
        float rad = deg2rad(v);
        float s = sin(rad), c = cos(rad);
        int w = r/25;
        for(int i=0; i<(sizeof boat_coords)/(sizeof *boat_coords); i+=2) {
            float x = boat_coords[i], y = boat_coords[i+1];
            int x1 = (c*x - s*y)*r + xc;
            int y1 = (c*y + s*x)*r + yc;
            if(i>0)
                drawThickLine(x1, y1, lx, ly, w);

            lx = x1;
            ly = y1;
        }
    }
};

struct speed_gauge : public gauge {
    speed_gauge(display_item_e _i) : gauge(_i, 0, 5, -135, 135, 22.5), niceminmax(0) { dialfmt = "%2.1f"; }

    void render() {
        float v = display_data[item].value;
        int nicemax = nice_number(v);

        // auto range to nice number
        if(nicemax > max_v) {
            max_v = nicemax;
            niceminmax = nice_number(max_v, -1);
        }

        // reset tmeout
        if(v > niceminmax)
            minmaxt = millis();

        // if we dont exceed the next lower nice number for a minute drop the scale back down
        if(millis() - minmaxt > 1000*60) {
            max_v = niceminmax;
            niceminmax = nice_number(max_v, -1);
        }

        if(max_v < 5) // keep at least 5
            max_v = 5;

        //Serial.printf("maxv %f %d %d %d\n", v, nicemax, max_v, niceminmax);
        gauge::render();
    }

    int niceminmax;
    uint32_t minmaxt;
};

struct rudder_angle_gauge : public gauge {
    rudder_angle_gauge() : gauge(RUDDER_ANGLE, -60, 60, -60, 60, 10) {}
};

struct rate_of_turn_gauge : public gauge {
    rate_of_turn_gauge() : gauge(RATE_OF_TURN, -20, 20, -90, 90, 30) { dialfmt = "%2.1f"; }
};

struct history_display : public display, public display_item {
    history_display(display_item_e _i, text_display_item* _label, bool _min_zero=true, bool _inverted=false)
        : display_item(_i), label(*_label), min_zero(_min_zero), inverted(_inverted)
    {
        for(int i=0; i<(sizeof history_items)/(sizeof *history_items); i++)
            if(history_items[i] == item) {
                history_item = i;
                return;
            }
        history_item = 0; // error
    }

    void fit() {
        label.x = x+w/4;
        label.y = y;
        label.w = w/2;
        label.h = 14;
        label.fit();
    }

    void render()
    {
        int r = history_display_range;
        uint32_t totalmillis = 1;
        for(int i=0; i<=r+1; i++)
            totalmillis *= history_range_time[i];

        uint32_t time = millis();
        std::list<history_element> &data = histories[history_item].data[r];

        // compute range
        float minv = INFINITY, maxv = -INFINITY;
        for(std::list<history_element>::iterator it = data.begin(); it!=data.end(); it++) {
            if(it->value < minv)
                minv = it->value;
            else if(it->value > maxv)
                maxv = it->value;
        }

        // todo: match range to nice values, render ticks, and label ticks etc

        // draw history data
        maxv = nice_number(maxv);
        float range;
        if(min_zero)
            minv = 0;
        else
            minv = nice_number(minv, -1);
        range = maxv - minv;

        int lxp = -1, lyp;
        for(std::list<history_element>::iterator it = data.begin(); it!=data.end(); it++) {
            int xp = w - (time - it->time) * w / totalmillis;
            int yp = h-it->value * h / range;

            if(lxp >= 0)
                u8g2.drawLine(x+lxp, y+lyp, x+xp, y+yp);
            lxp = xp, lyp = yp;
        }

        u8g2.setFont(u8g2_font_helvB08_tf);
        u8g2.drawStr(x, y, String(maxv, 0).c_str());
        u8g2.drawStr(x, y+h/2-4, String(minv, 1).c_str());
        u8g2.drawStr(x, y+h-8, String(minv, 0).c_str());

        String history_label = getHistoryLabel((history_range_e)r);
        int sw = u8g2.getStrWidth(history_label.c_str());
        u8g2.drawStr(x+w-sw, y, history_label.c_str());
        label.render();
    }

    int history_item;
    text_display_item &label;
    bool min_zero, inverted;
};

struct route_display : public display {
    void render() {
        float scog = display_data[GPS_HEADING].value;
        float course_error = resolv(route_info.target_bearing - scog);
        int p = course_error/(w/20);
        u8g2.drawLine(x+w/2+p-w/8, y, x+w/2-w/4, y+h);
        u8g2.drawLine(x+w/2+p+w/8, y, x+w/2+w/4, y+h);
        u8g2.drawLine(x+w/2+p,     y, x+w/2,     y+h);
    }
};

static int ships_range = 2;
static float ships_range_table[] = {0.25, .5, 1, 2, 5, 10, 20};
struct ais_ships_display : public display {
    void fit() {
        int cols;
        if(w > h) {
            xc = x + w / 2;
            yc = xc;
            r = h / 2;
            tx = h;
            ty = 0;
            tw = w-tx;
            th = h;
            cols = 1;
        } else {
            yc = y + h / 2;
            xc = yc;
            r = w / 2;
            tx = 0;
            ty = w;
            tw = w;
            th = h-ty;
            cols = 2;
        }
    }

    void render_ring(float r) {

        int thick = r / 60;
        r -= thick*2;
        for (int i = -thick; i <= thick; i++)
            for (int j = -thick; j <= thick; j++)
                u8g2.drawCircle(xc + i, yc + j, r);
    }

    void drawRightText(String text) {
        int textw = u8g2.getStrWidth(text.c_str());
        u8g2.drawStr(tx+tw-textw, ty+ty0, text.c_str());
    }

    void drawItem(const char *label, String text) {
        if(ty0 + tdy >= ty + th)
            return;
        u8g2.drawStr(tx, ty0, label);
        ty0 += tdyl;
        drawRightText(text);
        ty0 += tdy;
    }

    void render_text(ship *closest) {
        u8g2.setFont(u8g2_font_helvB10_tf);
        // draw range in upper left corner
        String str = String(ships_range_table[ships_range], 2) + "NMi";
        u8g2.drawStr(x, y, str.c_str());
        
        ty0 = 0;
        tdy = 12;
        tdyl = tw > th ? 0 : tdy;

        if(!closest) {
            u8g2.drawStr(tx, ty, "no target");
            return;
        }

        if(closest->name)
            str = closest->name;
        else
            str = closest->mmsi;

        float slat = display_data[LATITUDE].value;
        float slon = display_data[LONGITUDE].value;
        float ssog = display_data[GPS_SPEED].value;
        float scog = display_data[GPS_HEADING].value;
        uint32_t gps_time = display_data[GPS_HEADING].time;

        drawItem("Closest Target", str);
        closest->compute(slat, slon, ssog, scog, gps_time);

        drawItem("CPA", String(closest->cpa, 2));

        float tcpa = closest->tcpa, itcpa;
        tcpa = 60*modff(tcpa/60, &itcpa);
        drawItem("TCPA", String(itcpa) + ":" + String(tcpa, 2));

        drawItem("SOG", String(closest->sog));
        drawItem("COG", String(closest->sog));
        drawItem("Distance", String(closest->dist));
    }
    
    void render() {
        render_ring(1);
        render_ring(0.5);

        float sr = 1+w/60, rp = w/20;

        ship *closest = NULL;
        float closest_dist = INFINITY;
        float slat = display_data[LATITUDE].value;
        float slon = display_data[LONGITUDE].value;
        float rng = ships_range_table[ships_range];
        for(std::map<int, ship>::iterator it = ships.begin(); it != ships.end(); it++) {
            ship &ship = it->second;

            float x = ship.simple_x(slon);
            float y = ship.simple_y(slat);
            
            float dist = hypot(x, y);
            if(dist > rng)
                continue;

            if(dist < closest_dist) {
                closest = &ship;
                closest_dist = dist;
            }

            x *= r / rng;
            y *= r / rng;

            int x0 = xc + x, y0 = yc + y;
            u8g2.drawCircle(x0, y0, sr);
            
            float rad = deg2rad(ship.cog);
            float s = sin(rad), c = cos(rad);
            int x1 = x0 + c*rp, y1 = y0 + s*rp;
            x0 += c*sr, y0 += s*sr;
            u8g2.drawLine(x0, y0, x1, y1);
        }

        render_text(closest);
    }

    int xc, yc, r;
    int tx, ty, tw, th;
    int ty0, tdyl, tdy;

    float range;
};

struct grid_display : public display {
    grid_display(grid_display *parent=0, int _cols = 1, int _rows = -1)
         : cols(_cols), rows(_rows) { if(parent) parent->add(this); }

    void fit() {
        if (cols == -1) {
            rows = 1;
            cols = items.size();
        } else if (rows == -1) {
            rows = items.size()/cols;
            if(items.size()%cols)
                rows++;
        }

        if(rows == 0)
            h = 0;
        if(cols == 0)
            w = 0;
        if(!w || !h) {
            printf("warning:  empty grid display");
            return;
        }

        // adjust items to fit in grid
        int min_row_height = 10;
        int min_col_width = 10;

        int row_height[rows] = {0};
        int col_width[cols] = {0};
        bool col_expanding[cols] = {0};
        int remaining_height = h, remaining_rows = 0;
        std::list<display*>::iterator it;
        
        it = items.begin();
        for (int i = 0; i < rows && it!=items.end(); i++) {
            row_height[i] = 0;
            bool row_expanding = false;
            for (int j = 0; j < cols; j++) {
                if((*it)->h > row_height[i])
                    row_height[i] = (*it)->h;
                if((*it)->w > col_width[j])
                    col_width[j] = (*it)->w;
                if((*it)->expanding) {
                     row_expanding = true;
                     col_expanding[j] = true;
                }
                if (++it == items.end())
                    break;
            }

            if(!row_height[i]) {
                if (!row_expanding)
                    row_height[i] = min_row_height;
                else
                    remaining_rows++;
            }
            remaining_height -= row_height[i];
        }

        // distribute remaining height to rows with non-text displays
        if(remaining_rows) {
            remaining_height /= remaining_rows;
            for (int i = 0; i < rows; i++) {
                if (!row_height[i])
                    row_height[i] = remaining_height;
            }
        }

        // distribute remaining column width
        int remaining_width = w, remaining_cols = 0;
        for (int j = 0; j < cols; j++) {
            if(!col_width[j]) {
                if(!col_expanding[j])
                    col_width[j] = min_col_width;
                else
                    remaining_cols++;
            }
            remaining_width -= col_width[j];
        }

        if(remaining_cols) {
            remaining_width /= remaining_cols;
            for (int j = 0; j < cols; j++)
                if(!col_width[j])
                    col_width[j] = remaining_width;
        }

        // now update expanding items with the row height and width
        it = items.begin();
        for (int i = 0; i < rows && it != items.end(); i++) {
            for (int j = 0; j < cols; j++) {
                if((*it)->expanding) {
                    if(!(*it)->w)
                        (*it)->w = col_width[j];
                    if(!(*it)->h)
                        (*it)->h = row_height[i];
                    (*it)->fit(); // recursively fit
                }
                it++;
                if (it == items.end())
                    break;
            }
        }

        // now redistribute any free space back to non-expanding items

        // first find the row/col widths required from fit expanding items
        for (int i = 0; i < rows; i++)
            row_height[i] = 0;
        for (int j = 0; j < cols; j++)
            col_width[j] = 0;
        it = items.begin();
        for (int i = 0; i < rows && it != items.end(); i++) {
            for (int j = 0; j < cols; j++) {
                if((*it)->h > row_height[i])
                    row_height[i] = (*it)->h;
                if((*it)->w > col_width[j])
                    col_width[j] = (*it)->w;
                it++;
                if (it == items.end())
                    break;
            }
        }

        // now distribute remaining space into empty cols
        remaining_width = w, remaining_cols = cols;
        for (int j = 0; j < cols; j++)
            if(col_width[j]) {
                remaining_width -= col_width[j];
                remaining_cols--;
            }

        if(remaining_cols) {
            remaining_width /= remaining_cols;
            for (int j = 0; j < cols; j++)
                if(!col_width[j])
                    col_width[j] = remaining_width;
        } else
            w -= remaining_width;

        // now distribute remaining space into empty rows
        remaining_height = h, remaining_rows = rows;
        for (int i = 0; i < rows; i++)
            if(row_height[i]) {
                remaining_height -= row_height[i];
                remaining_rows--;
            }

        if(remaining_rows) {
            remaining_height /= remaining_rows;
            for (int i = 0; i < rows; i++)
                if(!row_height[i])
                    row_height[i] = remaining_height;
        } else
            h -= remaining_height;

        // now expand any items remaining to fit space and update coordinates
        int item_y = y;
        it = items.begin();
        for (int i = 0; i < rows && it != items.end(); i++) {
            int item_x = x;
            for (int j = 0; j < cols; j++) {
                (*it)->x = item_x;
                (*it)->y = item_y;
                if(!(*it)->w)
                    (*it)->w = col_width[j];
                if(!(*it)->h)
                    (*it)->h = row_height[i];
                (*it)->fit(); // recursively fit
                it++;
                if (it == items.end())
                    break;
                item_x += col_width[j];
            }
            item_y += row_height[i];
        }
        /*
        printf("FIT cols ");
        for (int i = 0; i <  cols; i++)
            printf("%d ", col_width[i]);
        printf("\n");

        printf("FIT rows ");
        for (int i = 0; i < rows; i++)
            printf("%d ", row_height[i]);
        printf("\n");
        */
    }

    void render() {
        for (std::list<display *>::iterator it = items.begin(); it != items.end(); it++)
            (*it)->render();
    }

    void add(display *item) {
        items.push_back(item);
    }

    int cols, rows;
    std::list<display *> items;
};

int page_width = 160;
int page_height = 240;

struct page : public grid_display {
    page(String _description)
    : description(_description) { w = page_width; h = page_height; cols = settings.landscape ? 2 : 1; }

    String description;
};

// mnemonics for all possible displays
#define WIND_DIR_G     new wind_direction_gauge()
#define WIND_DIR_T     new heading_text_display(WIND_DIRECTION)
#define WIND_SPEED_T   new speed_text_display(WIND_SPEED)
#define WIND_SPEED_G   new speed_gauge(WIND_SPEED)
#define WIND_SPEED_H   new history_display(WIND_SPEED, WIND_SPEED_T)

#define PRESSURE_T     new pressure_text_display()
#define PRESSURE_H     new history_display(BAROMETRIC_PRESSURE, PRESSURE_T)
#define AIR_TEMP_T     new temperature_text_display(AIR_TEMPERATURE)

#define COMPASS_T      new heading_text_display(COMPASS_HEADING)
#define COMPASS_G      new heading_gauge(COMPASS_HEADING)
#define PITCH_T        new heading_text_display(PITCH)
#define HEEL_T         new heading_text_display(HEEL)
#define RATE_OF_TURN_T new heading_text_display(RATE_OF_TURN)
#define RATE_OF_TURN_G new rate_of_turn_gauge()

#define GPS_HEADING_T  new heading_text_display(GPS_HEADING)
#define GPS_HEADING_G  new heading_gauge(GPS_HEADING)
#define GPS_SPEED_T    new speed_text_display(GPS_SPEED)
#define GPS_SPEED_G    new speed_gauge(GPS_SPEED)
#define GPS_SPEED_H    new history_display(GPS_SPEED, GPS_SPEED_T)
#define LATITUDE_T     new position_text_display(LATITUDE)
#define LONGITUDE_T    new position_text_display(LONGITUDE)
#define TIME_T         new time_text_display()

#define DEPTH_T        new depth_text_display()
#define DEPTH_H        new history_display(DEPTH, DEPTH_T)

#define RUDDER_ANGLE_T new heading_text_display(RUDDER_ANGLE)
#define RUDDER_ANGLE_G new heading_gauge(RUDDER_ANGLE)

#define WATER_SPEED_T  new speed_text_display(WATER_SPEED)
#define WATER_SPEED_G  new speed_gauge(WATER_SPEED)
#define WATER_SPEED_H  new history_display(WATER_SPEED, WATER_SPEED_T)
#define WATER_TEMP_T   new temperature_text_display(WATER_TEMPERATURE)

#define AIS_SHIPS_DISPLAY new ais_ships_display()

struct pageA : public page {
  pageA() : page("Wind gauges with pressure") {
    add(WIND_DIR_G);
    grid_display *d = new grid_display(this, settings.landscape ? 1 : 2);
    d->expanding = false;
    d->add(WIND_SPEED_G);
    grid_display *e = new grid_display(d);
    e->add(PRESSURE_T);
    e->add(AIR_TEMP_T);
  }
};

struct pageB : public page {
  pageB() : page("Wind Speed Gauge") {
    add(WIND_SPEED_G);
    grid_display *d = new grid_display(this);
    d->expanding = false;
    d->add(WIND_DIR_T);
    d->add(PRESSURE_T);
    d->add(AIR_TEMP_T);
  }
};

struct pageC : public page {
  pageC() : page("Wind Speed and Barometric history") {
    add(WIND_SPEED_H);
    add(PRESSURE_H);
  }
};

struct pageD : public page {
  pageD() : page("Wind gauges with IMU text") {
    cols = 1;

    grid_display *d = new grid_display(this, settings.landscape ? 2 : 1);

    d->add(WIND_DIR_G);
    d->add(WIND_SPEED_G);


    grid_display *t = new grid_display(this, settings.landscape ? 3 : 1);
    t->expanding = false;
    t->add(PRESSURE_T);
    t->add(AIR_TEMP_T);
    t->add(COMPASS_T);
    if(settings.landscape)
        t->add(RATE_OF_TURN_T);
    t->add(PITCH_T);
    t->add(HEEL_T); 
  }
};

struct pageE : public page {
  pageE() : page("Wind Direction and IMU text") {
    add(WIND_DIR_G);
    grid_display *d = new grid_display(this, settings.landscape ? 1 : 2);
    d->expanding = false;

    d->add(WIND_SPEED_T);
    d->add(PRESSURE_T);
    d->add(COMPASS_T);
    d->add(AIR_TEMP_T);
    d->add(PITCH_T);
    d->add(HEEL_T);
  }
};

struct pageF : public page {
  pageF() : page("Compass Gauge with inertial information") {
    add(COMPASS_G);
    grid_display *d = new grid_display(this, settings.landscape ? 1 : 2);
    d->expanding = false;
    d->add(RATE_OF_TURN_G);

    grid_display *e = new grid_display(d);
    e->add(PITCH_T);
    e->add(HEEL_T);
  }
};

struct pageG : public page {
  pageG() : page("GPS Gauges") {
    add(GPS_HEADING_G);
    add(GPS_SPEED_G);
    if(settings.landscape) {
        add(LATITUDE_T);
        add(LONGITUDE_T);
    }
  }
};

struct pageH : public page {
  pageH() : page("GPS speed gauges with position/time text") {
    add(GPS_SPEED_G);
    grid_display *d = new grid_display(this);
    d->expanding = false;
    d->add(GPS_HEADING_T);
    d->add(LATITUDE_T);
    d->add(LONGITUDE_T);
    d->add(TIME_T);
  }
};

struct pageI : public page {
  pageI() : page("Wind and GPS speed gauge") {
    add(WIND_DIR_G);
    grid_display *d = new grid_display(this, settings.landscape ? 1 : 2);
    d->expanding = false;

    d->add(WIND_SPEED_G);
    if(settings.landscape) {
        d->add(WIND_DIR_T);
        d->add(GPS_SPEED_T);
        d->add(GPS_HEADING_T);
    } else
        d->add(GPS_SPEED_G);
  }
};


struct pageJ : public page {
  pageJ() : page("Wind Speed and GPS speed") {
    add(WIND_SPEED_G);
    add(GPS_SPEED_G);
  }
};

struct pageK : public page {
  pageK() : page("GPS speed gauge and history") {
    add(GPS_SPEED_G);
    display *gps_speed_history = GPS_SPEED_H;
    gps_speed_history->expanding = false;
    
    add(gps_speed_history);
  }
};

struct pageL : public page {
  pageL() : page("GPS heading/speed text large") {
    grid_display *c = new grid_display(this);
    c->add(GPS_HEADING_T);
    c->add(GPS_SPEED_T);
    grid_display *d = new grid_display(this);
    d->expanding = false;
    d->add(LATITUDE_T);
    d->add(LONGITUDE_T);
    d->add(TIME_T);
  }
};

struct pageM : public page {
  pageM() : page("Wind gauges, GPS and inertial text ") {
    cols = settings.landscape ? 3 : 2;
    add(WIND_DIR_G);
    add(WIND_SPEED_G);
    add(PRESSURE_T);
    add(AIR_TEMP_T);
    add(COMPASS_T);
    add(RATE_OF_TURN_T);
    add(PITCH_T);
    add(HEEL_T);
    add(GPS_HEADING_T);
    add(GPS_SPEED_T);
    add(LATITUDE_T);
    add(LONGITUDE_T);
  }
};

struct pageN : public page {
  pageN() : page("Depth text and history") {
    display *depth_t = DEPTH_T;
    if(!settings.landscape) 
        depth_t->h = h/3; // expand to 1/3rd of height of area;
    add(depth_t);
    add(DEPTH_H);
  }
};

struct pageO : public page {
  pageO() : page("Rudder angle and compass gauges") {
    add(RUDDER_ANGLE_G);
    add(COMPASS_G);
  }
};

struct pageP : public page {
  pageP() : page("wind, rudder, compass gauges") {
    if(settings.landscape) {
        cols = 3;
        grid_display *d = new grid_display(this);
        d->add(WIND_DIR_G);
        d->add(WIND_SPEED_G);

        grid_display *e = new grid_display(this);
        e->add(PRESSURE_T);
        e->add(AIR_TEMP_T);
        e->add(PITCH_T);
        e->add(HEEL_T);

        grid_display *f = new grid_display(this);
        f->add(RUDDER_ANGLE_G);
        f->add(COMPASS_G);
    } else {
        cols = 2;
        add(WIND_DIR_G);
        add(WIND_SPEED_G);
        add(RUDDER_ANGLE_G);
        add(COMPASS_G);
    
        add(PRESSURE_T);
        add(AIR_TEMP_T);
        add(PITCH_T);
        add(HEEL_T);
    }
  }
};

struct pageQ : public page {
  pageQ() : page("water speed and history") {
    add(WATER_SPEED_G);
    grid_display *d = new grid_display(this);
    d->add(WATER_SPEED_H);
    d->add(WATER_TEMP_T);
  }
};

struct pageR : public page {
  pageR() : page("water and gps speed gauge") {
    add(WATER_SPEED_G);
    add(GPS_SPEED_G);
  }
};

struct pageS : public page {
  pageS() : page("lots of gauges") {
    cols = settings.landscape ? 3 : 2;
    add(WIND_DIR_G);
    add(WIND_SPEED_G);
    add(COMPASS_G);
    add(RUDDER_ANGLE_G);
    add(GPS_HEADING_G);
    add(GPS_SPEED_G);
  }
};


struct pageT : public page {
  pageT() : page("most text fields") {
    cols = settings.landscape ? 3 : 2;
    add(WIND_DIR_T);
    add(WIND_SPEED_T);
    add(PRESSURE_T);
    add(AIR_TEMP_T);
    add(PITCH_T);
    add(HEEL_T);
    add(COMPASS_T);
    add(RATE_OF_TURN_T);
    add(GPS_HEADING_T);
    add(GPS_SPEED_T);
    add(LATITUDE_T);
    add(LONGITUDE_T);
    add(DEPTH_T);
    add(RUDDER_ANGLE_T);
    add(WATER_SPEED_T);
    if(!settings.landscape)
        add(WATER_TEMP_T);
  }
};

struct pageU : public page {
    pageU() : page("route display"), vmg(NAN) {
        add(new route_display());
        add(new float_text_display("XTE", route_info.xte, 1));
        add(new float_text_display("BRG", route_info.brg, 0));
        add(new float_text_display("VMG", vmg, 0));
        add(new string_text_display("RNG", srng));
        add(new string_text_display("TTG", sttg));
    }

    void render() {
        // update vmg
        float tbrg = route_info.target_bearing;
        float sog = display_data[GPS_SPEED].value;
        float cog = display_data[GPS_HEADING].value;
        vmg = sog * cos( deg2rad( tbrg - cog ) );

        // update rng
        float lat = display_data[LATITUDE].value;
        float lon = display_data[LONGITUDE].value;
        float wpt_lat = route_info.wpt_lat;
        float wpt_lon = route_info.wpt_lon;
        float rbrg, rng;
        distance_bearing(lat, lon, wpt_lat, wpt_lon, &rng, &rbrg);
        float nrng = rng * cosf(deg2rad(rbrg - cog));

        srng = String(rng, 2) + "/" + String(nrng, 2);

        // update ttg
        if( vmg > 0. ) {
            float ttg_hr;
            float ttg_min = 60 * modff(rng / vmg, &ttg_hr);
            float ttg_sec = 60 * modff(ttg_min, &ttg_min);
            sttg = String(ttg_hr, 0) + ":" + String(ttg_min, 0) + ":" + String(ttg_sec, 0);
        } else
            sttg = "---";

        page::render();
    }

    String srng, sttg;
    float vmg;
};

struct pageV : public page {
    pageV() : page("ais display") {
        add(AIS_SHIPS_DISPLAY);
    }
};

struct pageW : public page {
    pageW() : page("pypilot statistics") {
        add(new pypilot_text_display("heading", "ap.heading", true));
        add(new pypilot_text_display("command", "ap.heading_command", true));
        add(new pypilot_text_display("mode", "ap.mode"));
        add(new pypilot_text_display("profile", "profile"));
        add(new pypilot_text_display("uptime", "imu.uptime"));
        add(new pypilot_text_display("runtime", "imu.uptime"));
        add(new pypilot_text_display("watts", "servo.watts"));
        add(new pypilot_text_display("amp hours", "servo.amp_hours"));
        add(new pypilot_text_display("rudder", "rudder.angle"));
        add(new pypilot_text_display("voltage", "servo.voltage"));
        add(new pypilot_text_display("motor T", "servo.motor_temp"));
        add(new pypilot_text_display("controller T", "servo.controller_temp"));
    }

    void render() {
        page::render();
        pypilot_client_strobe(); // indicate we need pypilot data
    }            
};

static std::vector<page*> pages;
static int cur_page = 0;
route_info_t route_info;
std::vector<page_info> display_pages;

String ps = "page";
char page_chr = 'A';
void add(page* p) {
    p->fit();
    pages.push_back(p);
    String n = ps+page_chr;
    display_pages.push_back(page_info(n, p->description));
    page_chr++;
}

static void setup_analog_pins()
{
//    adc1_config_width(ADC_WIDTH_BIT);
 //   adc1_config_channel_atten(ADC1_CHANNEL_0, ADC_ATTEN_DB_0);

    adcAttachPin(PHOTO_RESISTOR_PIN);

  // ledcSetup(0, 1000, 10);
  //ledcAttachPin(BACKLIGHT_PIN, 0);


}

static void read_analog_pins()
{
    uint32_t t = millis();
    int val = analogRead(PHOTO_RESISTOR_PIN);
    //    printf("val %d %d\n", val, millis() - t);
    // set backlight level to 50%
    //ledcWrite(0, 512);
}

void display_setup()
{
    u8g2.begin();
    u8g2.enableUTF8Print();
    u8g2.setFontPosTop();

    cur_page='F'-'A';

    if(settings.landscape) {
        page_width = 256;
        page_height = settings.show_status ? 148 : 160;
        u8g2.setDisplayRotation(U8G2_R0);
    } else {
        page_width = 160;
        page_height = settings.show_status ? 244 : 256;
        u8g2.setDisplayRotation(U8G2_R1);
    }

    add(new pageA);
    add(new pageB);
    add(new pageC);
    add(new pageD);
    add(new pageE);
    add(new pageF);
    add(new pageG);
    add(new pageH);
    add(new pageI);
    add(new pageJ);
    add(new pageK);
    add(new pageL);
    add(new pageM);
    add(new pageN);
    add(new pageO);
    add(new pageP);
    add(new pageQ);
    add(new pageR);
    add(new pageS);
    add(new pageT);
    add(new pageU);
    add(new pageV);
    add(new pageW);

    setup_analog_pins();
}

void display_change_page(int dir)
{
    if(dir == 0) {
        if(display_pages[cur_page].enabled)
            return;
        dir = 1;
    }

    int looped = 0;
    while(looped < 2) {
        cur_page += dir;
        if(cur_page >= display_pages.size())
            looped++, cur_page = 0;
        else if(cur_page < 0)
            looped++, cur_page = display_pages.size()-1;
        if(display_pages[cur_page].enabled) {
            printf("changed to page %c\n", 'A'+cur_page);
            return;
        }
    }

    // all pages disabled??
    display_pages[0].enabled = true;
    cur_page = 0;
    printf("warning, all pages disabled, enabling first page"); 
}

void display_change_scale()
{
    if(++history_display_range == RANGE_COUNT)
        history_display_range = 0;


    if(++ships_range >= (sizeof ships_range_table) / (sizeof *ships_range_table))
        ships_range = 0;
}

static void render_status()
{
    u8g2.setFont(u8g2_font_helvB08_tf);
    int y = page_height;
    if (WiFi.status() == WL_CONNECTED)
        u8g2.drawStr(0, y, "WIFI");

    // show data source
    const char *source[] = {"E", "U", "R", "W"};
    uint32_t t0 = millis();
    for(int i = 0; i<DATA_SOURCE_COUNT; i++) {
        uint32_t dt = t0 - data_source_time[i];
        if(dt < 5000)
            u8g2.drawStr(60+i*15, y, source[i]);
    }

    // show page letter
    char letter[] = "A";
    letter[0] += cur_page;
    u8g2.drawStr(page_width - 20, y, letter);
}


void display_render()
{
    read_analog_pins();

    u8g2.setContrast(settings.contrast + 160);
    uint32_t t0 = millis();
    u8g2.clearBuffer();
    u8g2.setDrawColor(1);
    uint32_t t1 = millis();

    u8g2.setFont(u8g2_font_helvB14_tf);

    if (wifi_ap_mode) {
        u8g2.drawStr(0, 0, "WIFI AP");

        u8g2.drawStr(0, 20, "windAP");
        u8g2.drawStr(0, 60, "http://wind.local");

        char buf[16];
        snprintf(buf, sizeof buf, "Version %.2f", VERSION);
        u8g2.drawStr(0, 100, buf);
        
        u8g2.sendBuffer();
        return;
    }

    if(settings.show_status)
        render_status();

    uint32_t t2 = millis();

    pages[cur_page]->render();

    uint32_t t3 = millis();

    u8g2.sendBuffer();

    uint32_t t4 = millis();
    //Serial.printf("render took %d %d %d %d\n", t1-t0, t2-t1, t3-t2, t4-t3);
}

#else

#include <TFT_eSPI.h>  // Hardware-specific library
#include <SPI.h>


TFT_eSPI tft = TFT_eSPI();

#define WHITE 0xffff
#define BLACK 0

void display_setup() {
  tft.init();

  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);

  tft.setTextSize(1);
}

void display_render() {
  static bool last_wifi_ap_mode;
  if (wifi_ap_mode != last_wifi_ap_mode) {
    tft.fillScreen(TFT_BLACK);


    last_wifi_ap_mode = wifi_ap_mode;
  }
  if (wifi_ap_mode) {
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.drawString("WIFI AP", 0, 0, 4);

    tft.drawString("windAP", 0, 20, 4);
    tft.drawString("http://wind.local", 0, 60, 2);
    return;
  }

  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  char buf[16];
  int coords[3][2] = { 0 };
  tft.fillTriangle(coords[0][0], coords[0][1], coords[1][0], coords[1][1], coords[2][0], coords[2][1], BLACK);

  tft.fillRect(xp, yp, 41, 25, BLACK);  // clear heading text area

  tft.drawEllipse(67, 67, 67, 67, WHITE);

  if (lpdir >= 0) {
    const uint8_t xc = 67, yc = 67, r = 64;
    float wind_rad = deg2rad(lpdir);
    int x = r * sin(wind_rad);
    int y = -r * cos(wind_rad);
    int s = 4;
    int xp = s * cos(wind_rad);
    int yp = s * sin(wind_rad);

    coords[0][0] = xc - xp;
    coords[0][1] = yc - yp;
    coords[1][0] = xc + x;
    coords[1][1] = yc + y;
    coords[2][0] = xc + xp;
    coords[2][1] = yc + yp;

    xp = -31.0 * sin(wind_rad), yp = 20.0 * cos(wind_rad);
    static float nxp = 0, nyp = 0;
    nxp = (xp + 15 * nxp) / 16;
    nyp = (yp + 15 * nyp) / 16;
    xp = xc + nxp - 31, yp = yc + nyp - 20;

    snprintf(buf, sizeof buf, "%03d", (int)lpdir);

    tft.fillTriangle(coords[0][0], coords[0][1], coords[1][0], coords[1][1], coords[2][0], coords[2][1], WHITE);

    tft.drawString(buf, xp, yp, 4);
  }

  float iknots;
  float iknotf = 10 * modff(knots, &iknots);

  snprintf(buf, sizeof buf, "%02d.%d", (int)iknots, (int)iknotf);
  tft.drawString(buf, 0, 150, 6);
}
#endif
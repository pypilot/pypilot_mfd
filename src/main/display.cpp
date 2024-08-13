/* Copyright (C) 2024 Sean D'Epagnier <seandepagnier@gmail.com>
 *
 * This Program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 */

#include <math.h>
#include <WiFi.h>

#include "Arduino.h"

#include "settings.h"
#include "draw.h"
#include "display.h"
#include "ais.h"
#include "pypilot_client.h"
#include "utils.h"
#include "menu.h"
#include "history.h"
#include "buzzer.h"

// autocompensate contrast based on light and temperature
#ifdef CONFIG_IDF_TARGET_ESP32S3

#define NTC_PIN -1 // s3 has internal temp sensor
#define PHOTO_RESISTOR_PIN 1
#define PWR_LED 18

#else

#define BACKLIGHT_PIN 14
#define NTC_PIN 39
#define PHOTO_RESISTOR_PIN 36
#define PWR_LED 26

#endif

static bool display_on = true;
bool landscape = false;

std::string display_get_item_label(display_item_e item) {
    switch (item) {
    case WIND_SPEED: return "Wind Speed";
    case WIND_DIRECTION: return "Wind Angle";
    case BAROMETRIC_PRESSURE: return "Baro Pressure";
    case AIR_TEMPERATURE: return "Air Temp";
    case WATER_TEMPERATURE: return "Water Temp";
    case GPS_SPEED: return "GPS Speed";
    case GPS_HEADING: return "GPS Heading";
    case LATITUDE: return "Lat";
    case LONGITUDE: return "Lon";
    case WATER_SPEED: return "Water Speed";
    case COMPASS_HEADING: return "Compass Heading";
    case PITCH: return "Pitch";
    case HEEL: return "Heel";
    case DEPTH: return "Depth";
    case RATE_OF_TURN: return "Rate of Turn";
    case RUDDER_ANGLE: return "Rudder Angle";
    case TIME: return "Time";
    case ROUTE_INFO: return "Route Info";
    case PYPILOT: return "pypilot";
    default: break;
    }
    return "";
}

/* ESP is esp-now
   USB is via usb
   RS422 is via isolated second serial port
   W is wifi either nmea0183 or signalk
   C is computed from other data, eg: true wind
*/
const char *source_name[] = { "ESP", "USB", "RS422", "W", "C" };

struct display_data_t {
    display_data_t()
        : time(-10000) {}

    float value;
    uint32_t time;
    data_source_e source;
};

display_data_t display_data[DISPLAY_COUNT];

uint32_t data_source_time[DATA_SOURCE_COUNT];

static void compute_true_wind(float wind_dir) {
    if (display_data[TRUE_WIND_DIRECTION].source != COMPUTED_DATA && !isnan(display_data[TRUE_WIND_DIRECTION].value))
        return;  // already have true wind from a better source

    // first try to compute from water speed
    float speed = NAN;
    if (settings.compute_true_wind_from_water)
        speed = display_data[WATER_SPEED].value;
    if (settings.compute_true_wind_from_gps && isnan(speed))
        speed = display_data[GPS_SPEED].value;

    if (isnan(speed))
        return;

    float wind_speed = display_data[WIND_SPEED].value;
    if (isnan(wind_speed))
        return;

    float rad = deg2rad(wind_dir);  // apparent wind in radians
    float windvx = wind_speed * sinf(rad), windvy = wind_speed * cosf(rad) - speed;
    float true_wind_speed = hypotf(windvx, windvy);
    float true_wind_dir = rad2deg(atan2f(windvx, windvy));
    display_data_update(TRUE_WIND_SPEED, true_wind_speed, COMPUTED_DATA);
    display_data_update(TRUE_WIND_DIRECTION, true_wind_dir, COMPUTED_DATA);
}

void display_data_update(display_item_e item, float value, data_source_e source) {
    uint32_t time = millis();
    if (isnan(value))
        printf("invalid display data update %d %d\n", item, source);
    //printf("data_update %d %s %f\n", item, display_get_item_label(item).c_str(), value);

    if (source > display_data[item].source) {
        // ignore if higher priority data source updated in last 5 seconds
        if (time - display_data[item].time < 5000)
            return;
    }

    history_put(item, value);

    display_data[item].value = value;
    display_data[item].time = time;
    display_data[item].source = source;
    data_source_time[source] = time;

    if (item == WIND_DIRECTION)  // possibly compute true wind
        compute_true_wind(value);
}

bool display_data_get(display_item_e item, float &value) {
    if (isnan(display_data[item].value))
        return false;
    value = display_data[item].value;
    return true;
}

bool display_data_get(display_item_e item, float &value, std::string &source, uint32_t &time) {
    if (isnan(display_data[item].value))
        return false;
    value = display_data[item].value;
    source = source_name[display_data[item].source];
    time = display_data[item].time;
    return true;
}

struct display_item : public display {
    display_item(display_item_e _item)
        : item(_item) {}
    void getAllItems(std::list<display_item_e> &items) {
        items.push_back(item);
    }

    enum display_item_e item;
};

struct text_display : public display_item {
    text_display(display_item_e _i, std::string _units = "")
        : display_item(_i), units(_units) {
        x0 = 0;
        expanding = false;
        centered = false;
    }

    void fit() {
        ht = h;

        if (centered) {  // if centered do not draw label either for now
            label_w = label_h = 0;
        } else {
            std::string str = getLabel();
            label_font = 8;
            draw_set_font(label_font);
            label_w = draw_text_width(str);
            label_h = 8;

            if (label_w > w / 2 || label_h > h / 2) {
                label_font = 7;
                draw_set_font(label_font);
                label_w = draw_text_width(str);
                label_h = 7;
            }
        }
    }

    void selectFont(std::string str) {
        // based on width and height determine best font
        for (;;) {
            if (!draw_set_font(ht))
                return;

            int width = draw_text_width(str);
            if ((width + label_w < wt || ht + label_h < h) && width < wt && ht < h) {
                wt = width;
                break;
            }
            ht--;
        }
    }

    void render() {
        // determine font size to use from text needed
        std::string str = getText();

        // try to fit text along side label
        wt = w;
        selectFont(str);

        //if(ht + label_h > h && wt + label_w > w)
        //    label = false;

        int yp = y;
        int xp;
        if (centered) {
            yp = y - ht / 2;
            xp = x - wt / 2;  // center text over x position
            x0 = 0;
        } else {
            if (ht + label_h < h) {
                xp = x + (w - wt) / 2;
                x0 = 0;
                yp += (label_h + h - ht)/2;
            // center bounds
            } else {  // right justify
                xp = x + w - wt;
                x0 = label_w;
                yp += (h - ht) / 2;
            }
        }
        //printf("draw text %s %d %d %d %d %d %d %d %d\n", str.c_str(), x, y, w, h, wt, ht, xp, yp);
        draw_text(xp, yp, str);
        if (label_h && label_w < w && label_h < h) {
            std::string label = getLabel();
            if (!label.empty()) {
                int lx = x, ly = y;
                if (ht + label_h < h)
                    lx += w / 2 - label_w / 2;
                else if (wt + label_w < w)
                    ly += h / 2 - label_h / 2;
                draw_set_font(label_font);
                draw_text(lx, ly, label);
            }
        }
    }
    std::string getText() {
        float v = display_data[item].value;
        if (isnan(v))
            return "---";
        std::string s = getTextItem();
        if (w < 30 || w / h < 3 || units.empty())
            return s;
        return s + units;
    }

    virtual std::string getTextItem() = 0;

    virtual std::string getLabel() {
        return display_get_item_label(item);
    }

    bool use_units;
    std::string units;
    int label_font;
    int wt, ht, x0;
    int label_w, label_h;
    bool centered;  // currently means centered over x (and no label)  maybe should change this
};

struct generic_text_display : public text_display {
    generic_text_display(display_item_e _i, const char *units, int _digits)
        : text_display(_i, units) , digits(_digits) {}

    std::string getTextItem() {
        return float_to_str(display_data[item].value, digits);
    }
    int digits;
};

struct speed_text_display : public text_display {
    speed_text_display(display_item_e _i)
        : text_display(_i, "kt") {}

    std::string getTextItem() {
        return float_to_str(display_data[item].value, display_data[item].value < 10 ? 1 : 0);
    }
};

struct angle_text_display : public generic_text_display {
    angle_text_display(display_item_e _i, int _digits = 0)
        : generic_text_display(_i, "°", _digits) {}
};

struct dir_angle_text_display : public angle_text_display {
    dir_angle_text_display(display_item_e _i, bool _has360 = false)
        : angle_text_display(_i, _has360 ? 0 : 1), has360(_has360) {}

    std::string getTextItem() {
        float v = display_data[item].value;
        if (isnan(v))
            return "---";
        if (settings.use_360 && has360)
            return float_to_str(resolv(v, 180));

        return float_to_str(fabsf(v), digits);
    }

    void fit() {
        text_display::fit();
        ht = h*4/5;
    }

    void render() {
        text_display::render();
        
        if ((settings.use_360 && has360) || centered)
            return;
        float v = display_data[item].value;
        if (isnan(v))
            return;
        float xa, xb;
        if (v > 0)
            xa = .95, xb = .85f;
        else if (v < 0)
            xa = .05, xb = .15f;
        else
            return;
        int w0 = w-x0;
        int x1 = x + x0 + w0 * xa, x2 = x + x0 + w0 * xb, x3 = x + x0 + w0 * (xa * 2 + xb) / 3;
        int y1 = y + h / 2, y2 = y + h * .35f, y3 = y + h * .65f;
        int t = h / 30+1;

        draw_thick_line(x1, y1, x2, y1, t);
        draw_thick_line(x1, y1, x3, y2, t);
        draw_thick_line(x3, y3, x1, y1, t);
    }
    bool has360;
};

struct temperature_text_display : public text_display {
    temperature_text_display(display_item_e _i)
        : text_display(_i) {}

    std::string getTextItem() {
        std::string s, u;
        if (settings.use_fahrenheit) {
            s = float_to_str(display_data[item].value * 9 / 5 + 32, 1);
            units = "°F";
        } else {
            s = float_to_str(display_data[item].value, 1);
            units = "°C";
        }
        return s;
    }
};

struct depth_text_display : public text_display {
    depth_text_display()
        : text_display(DEPTH) {}

    std::string getTextItem() {
        std::string s, u;
        if (settings.use_depth_ft) {
            s = float_to_str(display_data[item].value * 3.28, 1);
            units = "ft";
        } else {
            s = float_to_str(display_data[item].value, 1);
            units = "m";
        }
        return s;
    }
};

struct pressure_text_display : public text_display {
    pressure_text_display()
        : text_display(BAROMETRIC_PRESSURE) {}

    std::string getTextItem() {
        float cur = display_data[item].value;
        if (settings.use_inHg)
            cur /= 33.8639;
        std::string s;
        if (w > 100)
            s = float_to_str(cur, 5);
        else if (w > 50)
            s = float_to_str(cur, 4);
        else
            s = float_to_str(cur, 3);

        if (w > 50) {
            if (prev.size() > 3) {
                if (cur < prev.front() - .005f)
                    units = "↓";
                else if (cur > prev.front() + .005f)
                    units = "↑";
                else
                    units = "~";
            } else
                units = " ";
        } else
            units = "";

        if (millis() - prev_time > 60000) {
            prev.push_back(cur);
            if (prev.size() > 5)
                prev.pop_front();
            prev_time = millis();
        }

        if (w > 100) {
            if (settings.use_inHg)
                units += " inHg";
            else
                units += " mbar";
        }
        //printf("prev %s %d %d\n", units.c_str(), prev.size(), w);

        return s;
    }

    static std::list<float> prev;
    uint32_t prev_time;
};

std::list<float> pressure_text_display::prev;  // static member for pressure trend

struct position_text_display : public text_display {
    position_text_display(display_item_e _i)
        : text_display(_i) {}

    std::string getTextItem() {
        float v = display_data[item].value;

        char s;
        if (item == LATITUDE)
            s = (v < 0) ? 'S' : 'N';
        else
            s = (v < 0) ? 'W' : 'E';
        v = fabsf(v);
        std::string str;
        if (settings.lat_lon_format == "degrees")
            str = float_to_str(v, 6);
        else {
            float i;
            float f = modff(v, &i) * 60;
            str = float_to_str(i) + ", ";
            if (settings.lat_lon_format == "seconds") {
                f = modff(f, &i) * 60;
                str += float_to_str(i) + ", " + float_to_str(f, 2);
            } else
                str += float_to_str(f, 4);
        }
        units = "°" + s;
        return str;
    }
};

struct time_text_display : public text_display {
    time_text_display()
        : text_display(TIME) {}

    std::string getTextItem() {
        float t = display_data[item].value;
        float hours, minutes;
        float seconds = modff(t / 60, &minutes) * 60;
        minutes = modff(minutes / 60, &hours) * 60;

        char buf[24];
        snprintf(buf, sizeof buf, "%02d:%02d:%02d", (int)hours, (int)minutes, (int)seconds);
        return std::string(buf);
    }
};

struct label_text_display : public text_display {
    label_text_display(display_item_e _i, std::string label_)
        : text_display(_i), label(label_) {}

    std::string getLabel() {
        return label;
    }

    std::string label;
};

struct float_text_display : public label_text_display {
    float_text_display(display_item_e _i, std::string label_, float &value_, int digits_)
        : label_text_display(_i, label_), value(value_), digits(digits_) {}

    std::string getTextItem() {
        return float_to_str(value, digits);
    }

    float &value;
    int digits;
};

struct string_text_display : public label_text_display {
    string_text_display(display_item_e _i, std::string label_, std::string &value_)
        : label_text_display(_i, label_), value(value_) {}

    std::string getTextItem() {
        return value;
    }

    std::string &value;
};

struct pypilot_text_display : public label_text_display {
    pypilot_text_display(std::string label_, std::string key_, bool expanding_ = false, int digits_ = 1)
        : label_text_display(PYPILOT, label_), key(key_), digits(digits_) {
        expanding = expanding_;
        pypilot_watch(key);
    }

    std::string getTextItem() {
        return pypilot_client_value(key, digits);
    }

    std::string key;
    int digits;
};

struct pypilot_command_display : public pypilot_text_display {
    pypilot_command_display()
        : pypilot_text_display("command", "ap.heading_command", true, 0) {
        pypilot_watch("ap.enabled");
    }

    std::string getTextItem() {
        std::string x = pypilot_client_value("ap.enabled");
        if (x == "false")
            return "standby";
        return pypilot_text_display::getTextItem();
    }
};

struct stat_display : public display_item {
    stat_display(display_item_e _i)
        : display_item(_i) { expanding=false; }

    void selectFont(std::string str) {
        // based on width and height determine best font
        int ht = h;
        for (;;) {
            if (!draw_set_font(ht))
                return;

            int wt = draw_text_width(str);
            if (wt < w && ht < h/4)
                break;
            ht--;
        }
    }
    
    virtual void render() {
        //printf("stat display %d %d %d %d\n", x, y, w, h);
        int ht = h/4;
        std::string label = display_get_item_label(item) + " Min/Max ";
        selectFont(label);
        int sw = draw_text_width(label);
        draw_color(YELLOW);
        draw_text(x + (w - sw) / 2, y, label);

        int totalseconds;
        float high, low;
        std::list<history_element> *data = history_find(item, history_display_range,
                                                        totalseconds, high, low);
        std::string slow, shigh;
        int digits = 1;
        if (!data)
            slow = " --- ";
        else {
            slow = "low: " + float_to_str(low, digits);
            shigh = "high: " + float_to_str(high, digits);
        }

        draw_color(MAGENTA);
        selectFont(slow);
        sw = draw_text_width(slow);
        draw_text(x + (w - sw) / 2, y + ht, slow);
        sw = draw_text_width(shigh);
        draw_text(x + (w - sw) / 2, y + 2*ht, shigh);

        std::string history_label = history_get_label((history_range_e)history_display_range);
        selectFont(history_label);
        sw = draw_text_width(history_label);
        draw_color(RED);
        draw_text(x + (w - sw) / 2, y + h - ht, history_label);            
    }
};

struct gauge : public display_item {
    gauge(text_display *_text, int _min_v, int _max_v, int _min_ang, int _max_ang, float _ang_step)
        : display_item(_text->item), text(*_text),
          min_v(_min_v), max_v(_max_v),
          min_ang(_min_ang), max_ang(_max_ang), ang_step(_ang_step) {

        nxp = 0, nyp = 0;
        text.centered = true;
    }

    void fit() {
        if (w > h)
            w = h;
        else
            h = w;
        r = min(w / 2, h / 2);

        // set text are proportional to the gauge area
        text.w = w / 1.8f;
        text.h = h / 3;
        text.fit();
    }

    void render_ring() {
        xc = x + w / 2;
        yc = y + h / 2;

        int thick = r / 36;
        //int r2 = r - thick - 1;
        draw_circle(xc, yc, r, thick);
    }

    void render_tick(float angle, int u, int &x0, int &y0) {
        float rad = deg2rad(angle);
        float s = sinf(rad), c = cosf(rad);

        x0 = xc + (r - u) * s;
        y0 = yc - (r - u) * c;
        int v = 3;
        int x1 = xc + r * s;
        int xp = v * c;
        int y1 = yc - r * c;
        int yp = v * s;
        draw_triangle(x1 - xp, y1 - yp, x0, y0, x1 + xp, y1 + yp);
    }

    void render_ticks(bool text = false) {
        int th;
#ifdef USE_RGB_LCD
        th = 21;
#else
        if (w > 90)
            th = 8;
        else
            th = 7;
#endif
        draw_set_font(th);

        for (float angle = min_ang; angle <= max_ang; angle += ang_step) {
            int x0, y0;
            render_tick(angle, w / 12, x0, y0);
        }

        float step = (int)(((float)max_v - min_v) / 8 + .5);
        for (float val = min_v; val <= max_v; val += step) {
            if (max_ang >= 160 && val > 160)
                break;  // avoid rendering overlapping 180

            float ival = (int)val;
            // map over range from 0 - 1
            float v = (ival - min_v) / (max_v - min_v);
            // now convert to gauge angle
            v = min_ang + v * (max_ang - min_ang);

            float rad = deg2rad(v);
            float s = sinf(rad), c = cosf(rad);
            float r1 = r - w / 10;
            int x0 = xc + r1 * s;
            int y0 = yc - r1 * c;
            //printf("gauge %d %d %f %d %f\n", x0, y0, ival, step, max_v, min_v);

            if (text) {
                std::string buf = int_to_str(abs((int)ival));
                int tsw = draw_text_width(buf);
                if (x0 > xc + w / 4)
                    x0 -= tsw;
                else if (x0 > xc - w / 4)
                    x0 -= tsw / 2;
                if (y0 > yc + h / 4)
                    y0 -= th;
                else if (y0 > yc - h / 4)
                    y0 -= th / 2;
                draw_text(x0, y0, buf);
            }

            if (0 && text) {  // intermediate ticks
                //float iangle = angle + ang_step / 2.0f;
                //render_tick(iangle, w/20, x0, y0);
            }
        }
    }

    virtual void render_dial() {
        // draw actual arrow toward wind direction
        float val = display_data[item].value;
        if (isnan(val)) {
            //printf("ISNAN%f\n", val);
            return;
        }

        // map over range from 0 - 1
        float v = (val - min_v) / (max_v - min_v);

        // now convert to gauge angle
        v = min_ang + v * (max_ang - min_ang);

        float rad = deg2rad(v);
        float s = sin(rad), c = cos(rad);
        int x0 = r * s;
        int y0 = -r * c;
        int u = 1 + w / 30;
        int xp = u * c;
        int yp = u * s;
        int txp = -r / 2 * s, typ = r / 3 * c;

        nxp = (txp + 15 * nxp) / 16;
        nyp = (typ + 15 * nyp) / 16;
        text.x = xc + nxp;
        text.y = yc + nyp;
        draw_color(YELLOW);
        text.render();

        draw_color(RED);
        draw_triangle(xc - xp, yc - yp, xc + x0, yc + y0, xc + xp, yc + yp);
    }

    void render_label() {
        std::string label = display_get_item_label(item);
        int space1 = label.find(' '), space2 = label.rfind(' ');
        std::string label1 = space1 > 0 ? label.substr(0, space1) : label;
        std::string label2 = space2 > 0 ? label.substr(space2) : "";

#ifdef USE_RGB_LCD
        const uint8_t fonts[] = { 30, 24, 21, 0 };
#else
        const uint8_t fonts[] = { 8, 7, 0 };
#endif
        int fth[] = { 10, 8, 0 };
        int lw1, lw2=0;
        for (int i = 0; i < (sizeof fonts) / (sizeof *fonts); i++) {
            if (!fonts[i])
                return;
            int ht = fonts[i];
            draw_set_font(ht);
            lw1 = draw_text_width(label1);
            lw2 = draw_text_width(label2);

            int x1 = -w / 2 + lw1, y12 = -h / 2 + fth[i];
            int x2 = w / 2 - lw2;
            int r1_2 = x1 * x1 + y12 * y12, r2_2 = x2 * x2 + y12 * y12;
            int mr = r * r * 9 / 10;
            //printf("%s   %d %d %d   %d %d %d\n", label1.c_str(), x1, x2, y12, r1_2, r2_2, mr);
            if (r1_2 < mr && r2_2 < mr)
                break;
        }
        draw_color(MAGENTA);
        draw_text(x, y, label1);
        draw_text(x + w - lw2-1, y, label2);
    }

    virtual void render() {
        render_label();
        render_dial();
        draw_color(GREY);
        render_ticks(w > 60);
        draw_color(GREEN);
        render_ring();
    }

    text_display &text;

    int xc, yc, r;
    int min_v, max_v;
    bool i_ticks;
    int min_ang, max_ang;
    float ang_step;

    float nxp, nyp;
};

struct wind_direction_gauge : public gauge {
    wind_direction_gauge(text_display *_text)
        : gauge(_text, -180, 180, -180, 180, 45) {}

    void render() {
        gauge::render();
        // now compute true wind from apparent
        // render true wind indicator near ring
        float twd = display_data[TRUE_WIND_DIRECTION].value;
        if (!isnan(twd)) {
            float s = sin(twd), c = cos(twd);

            int x0 = s * r, y0 = c * r;
            int x1 = x0 * 8 / 10, y1 = y0 * 8 / 10;
            int xp = c * 3, yp = s * 3;
            draw_color(GREEN);
            draw_triangle(xc + x0 - xp, yc + y0 - yp,
                          xc + x1, yc + y1,
                          xc + x0 + xp, yc + y0 + yp);
        }
    }
};

float boat_coords[] = { 0, -.5, .15, -.35, .2, -.25, .15, .45, -.15, .45, -.2, -.25, -.15, -.35, 0, -.5 };

struct heading_gauge : public gauge {
    heading_gauge(text_display *_text)
        : gauge(_text, -180, 180, -180, 180, 45) {}

    void render_dial() {
        float v = display_data[item].value;
        if (isnan(v))
            return;

        text.x = xc;
        text.y = yc + h * .16;
        text.render();

        int lx, ly;
        float rad = deg2rad(v);
        float s = sin(rad), c = cos(rad);
        int lw = r / 25;
        for (int i = 0; i < (sizeof boat_coords) / (sizeof *boat_coords); i += 2) {
            float x = boat_coords[i], y = boat_coords[i + 1];
            int x1 = (c * x - s * y) * r + xc;
            int y1 = (c * y + s * x) * r + yc;
            if (i > 0)
                draw_thick_line(x1, y1, lx, ly, lw);

            lx = x1;
            ly = y1;
        }
    }
};

struct speed_gauge : public gauge {
    speed_gauge(text_display *_text)
        : gauge(_text, 0, 5, -135, 135, 22.5), niceminmax(0) {}

    void render() {
        float v = display_data[item].value;
        int nicemax = nice_number(v);

        // auto range to nice number
        if (nicemax > max_v) {
            max_v = nicemax;
            niceminmax = nice_number(max_v, -1);
        }

        // reset tmeout
        if (v > niceminmax)
            minmaxt = millis();

        // if we dont exceed the next lower nice number for a minute drop the scale back down
        if (millis() - minmaxt > 1000 * 60) {
            max_v = niceminmax;
            niceminmax = nice_number(max_v, -1);
        }

        if (max_v < 5)  // keep at least 5
            max_v = 5;

        //printf("maxv %f %d %d %d\n", v, nicemax, max_v, niceminmax);
        gauge::render();
    }

    int niceminmax;
    uint32_t minmaxt;
};

struct rudder_angle_gauge : public gauge {
    rudder_angle_gauge(text_display *_text)
        : gauge(_text, -60, 60, -60, 60, 10) {}
};

struct rate_of_turn_gauge : public gauge {
    rate_of_turn_gauge(text_display *_text)
        : gauge(_text, -10, 10, -90, 90, 30) {}
};

uint32_t start_time;
struct history_display : public display_item {
    history_display(text_display *_text, bool _min_zero = true, bool _inverted = false)
        : display_item(_text->item), text(*_text), min_zero(_min_zero), inverted(_inverted) {
    }

    void fit() {
        text.x = x + w / 4;
        text.y = y+3;
        text.w = w * 3/5;
        text.h = h/10;
        text.fit();
    }

    void render() {
        // draw label
        draw_color(GREEN);
#ifdef USE_RGB_LCD
        int ht = h/12;
#else
        int ht = 8;
#endif
        draw_set_font(ht);

        std::string history_label = history_get_label((history_range_e)history_display_range);
        int sw = draw_text_width(history_label);
        draw_text(x + w - sw, y, history_label);
        text.render();

        int totalseconds;
        float high, low;
        std::list<history_element> *data = history_find(item, history_display_range,
                                                        totalseconds, high, low);
        if (!data)
            return;

        // draw history data
        float range, minv, maxv;
        int digits = 5;
        if (min_zero) {
            minv = 0;
            maxv = nice_number(high);
            digits = 0;
        } else {
            // extend range slightly
            float rng = high - low;
            maxv = high + rng / 8;
            minv = low - rng / 8;
        }

        range = maxv - minv;

        int lxp = -1, lyp=0;
        uint32_t ltime = 0;
        uint64_t total_ms = totalseconds * 1000;
        uint64_t st = (uint64_t)start_time*1000 + esp_timer_get_time()/1000L;

        draw_color(ORANGE);
        for (std::list<history_element>::iterator it = data->begin(); it != data->end(); it++) {
            int xp = w;
            xp -= (st - 1000*it->time) * w / total_ms;

            if (xp < 0)
                break;

            if(isnan(it->value)) 
                lxp = -1;
            else {
                int yp = h - 1 - (it->value - minv) * (h - 1) / range;

                // if the time is sufficiently large,  insert a "gap"
                int range_timeout = totalseconds / 60;
                int dt = it->time - ltime;
                if(dt > range_timeout)
                    ; // nothing
                else
                if (lxp >= 0 && yp >= 0 && yp < h && lyp >= 0 && lyp < h) {
                    //if (xp - lxp < 3)
                        draw_line(x + lxp, y + lyp, x + xp, y + yp);
                    //              if(abs(lxp - xp) > 100)
                    //printf("BADLINE ! %d %d %d %d %d %d %\n", cnt++, it->time, lastt, xp, lxp, yp, lyp);
                }
                lxp = xp, lyp = yp;
                ltime = it->time;
            }
        }

        //render scale
        draw_color(WHITE);
        draw_text(x, y, float_to_str(maxv, digits));
        draw_text(x, y + h / 2 - 4, float_to_str((minv + maxv) / 2, digits+1));
        draw_text(x, y + h - 8, float_to_str(minv, digits));

        if (h > 100) {
            draw_color(CYAN);
            std::string minmax = "low: " + float_to_str(low, digits) + "  high: " + float_to_str(high, digits);
            int sw = draw_text_width(minmax);
            draw_text(x + (w - sw) / 2, y + h-ht*2, minmax);
        }
    }

    text_display &text;
    bool min_zero, inverted;
};

struct route_display : public display_item {
    route_display()
        : display_item(ROUTE_INFO) {}
    void render() {
        float scog = display_data[GPS_HEADING].value;
        float course_error = resolv(route_info.target_bearing - scog);
        int p = course_error / (w / 20);
        draw_line(x + w / 2 + p - w / 8, y, x + w / 2 - w / 4, y + h);
        draw_line(x + w / 2 + p + w / 8, y, x + w / 2 + w / 4, y + h);
        draw_line(x + w / 2 + p, y, x + w / 2, y + h);
    }
};

static int ships_range = 2;
static float ships_range_table[] = { 0.25, .5, 1, 2, 5, 10, 20 };
struct ais_ships_display : public display_item {
    ais_ships_display()
        : display_item(AIS_DATA) {}
    void fit() {
        if (w < h) {
            xc = x + w / 2;
            yc = y + xc;
            r = w / 2 - 1;
            tx = 0;
            ty = w;
            tw = w;
            th = h - w;
        } else {
            yc = y + h / 2;
            xc = x + yc;
            r = h / 2;
            tx = w;
            ty = 0;
            tw = w - h;
            th = h;
        }
    }

    void render_ring(float rm) {
        int thick = r / 100;
        int ri = r * rm - thick * 2;
        draw_circle(xc, yc, ri, thick);
    }

    void drawItem(const char *label, std::string text) {
        if (ty0 + tdy >= ty + th)
            return;
        int ly = ty + ty0;
        int lw = tdyl ? 0 : draw_text_width(label);
        ty0 += tdyl;

        int textw = draw_text_width(text);
        int scrolldist = textw - (tw - lw);
        if (scrolldist < 0)
            scrolldist = textw;
        else {
            uint32_t mso = millis() / 32;
            scrolldist = mso % scrolldist + tw - lw;
        }
        draw_text(tx + tw - scrolldist, ty + ty0, text);
        ty0 += tdy;

        draw_text(tx, ly, label);
    }

    void render_text(ship *closest) {
        int ht = 8;
        draw_set_font(ht);
        // draw range in upper left corner
        std::string str = float_to_str(ships_range_table[ships_range], 1) + "NMi";
        int textw = draw_text_width(str);
        draw_text(x + w - textw, y, str);

        ht = 10;
        draw_set_font(ht);
        draw_text(x, y, "AIS");

        ty0 = 0;
        tdy = 12;
        tdyl = tw > th ? 0 : tdy;

        if (!closest) {
            draw_text(tx, ty, "no target");
            return;
        }

        if (closest->name.empty())
            str = closest->mmsi;
        else
            str = closest->name;

        float slat = display_data[LATITUDE].value;
        float slon = display_data[LONGITUDE].value;
        float ssog = display_data[GPS_SPEED].value;
        float scog = display_data[GPS_HEADING].value;
        uint32_t gps_time = display_data[GPS_HEADING].time;

        drawItem("Closest Target", str);
        closest->compute(slat, slon, ssog, scog, gps_time);

        drawItem("CPA", float_to_str(closest->cpa, 2));

        float tcpa = closest->tcpa, itcpa;
        tcpa = 60 * modff(tcpa / 60, &itcpa);
        drawItem("TCPA", float_to_str(itcpa) + ":" + float_to_str(tcpa, 2));

        drawItem("SOG", float_to_str(closest->sog));
        drawItem("COG", float_to_str(closest->sog));
        drawItem("Distance", float_to_str(closest->dist));
    }

    void render() {
        render_ring(1);
        render_ring(0.5);

        float sr = 1 + w / 60, rp = w / 20;

        ship *closest = NULL;
        float closest_dist = INFINITY;
        float slat = display_data[LATITUDE].value;
        float slon = display_data[LONGITUDE].value;
        float rng = ships_range_table[ships_range];
        for (std::map<int, ship>::iterator it = ships.begin(); it != ships.end(); it++) {
            ship &ship = it->second;

            float x = ship.simple_x(slon);
            float y = ship.simple_y(slat);

            float dist = hypot(x, y);
            if (dist > rng)
                continue;

            if (ship.sog > 0 && dist < closest_dist) {
                closest = &ship;
                closest_dist = dist;
            }

            x *= r / rng;
            y *= r / rng;

            int x0 = xc + x, y0 = yc - y;
            draw_circle(x0, y0, sr);

            float rad = deg2rad(ship.cog);
            float s = sin(rad), c = cos(rad);
            int x1 = x0 + s * rp, y1 = y0 - c * rp;
            x0 += s * sr, y0 -= c * sr;
            draw_line(x0, y0, x1, y1);
        }

        render_text(closest);
    }

    int xc, yc, r;
    int tx, ty, tw, th;
    int ty0, tdyl, tdy;

    float range;
};

void grid_display::fit() {
    if (cols == -1) {
        rows = 1;
        cols = items.size();
    } else if (rows == -1) {
        rows = items.size() / cols;
        if (items.size() % cols)
            rows++;
    }

    if (rows == 0)
        h = 0;
    if (cols == 0)
        w = 0;
    if (!w || !h) {
        printf("warning:  empty grid display");
        return;
    }

    // adjust items to fit in grid
    int min_row_height = 10;
    int min_col_width = 10;

    int row_height[rows] = { 0 };
    int col_width[cols] = { 0 };
    bool col_expanding[cols] = { 0 };
    int remaining_height = h, remaining_rows = 0;
    std::list<display *>::iterator it;

    it = items.begin();
    for (int i = 0; i < rows && it != items.end(); i++) {
        row_height[i] = 0;
        bool row_expanding = false;
        for (int j = 0; j < cols; j++) {
            if ((*it)->h > row_height[i])
                row_height[i] = (*it)->h;
            if ((*it)->w > col_width[j])
                col_width[j] = (*it)->w;
            if ((*it)->expanding) {
                row_expanding = true;
                col_expanding[j] = true;
            }
            if (++it == items.end())
                break;
        }

        if (!row_height[i]) {
            if (!row_expanding)
                row_height[i] = min_row_height;
            else
                remaining_rows++;
        }
        remaining_height -= row_height[i];
    }

    // distribute remaining height to rows with non-text displays
    if (remaining_rows) {
        remaining_height /= remaining_rows;
        for (int i = 0; i < rows; i++) {
            if (!row_height[i])
                row_height[i] = remaining_height;
        }
    }

    // distribute remaining column width
    int remaining_width = w, remaining_cols = 0;
    for (int j = 0; j < cols; j++) {
        if (!col_width[j]) {
            if (!col_expanding[j])
                col_width[j] = min_col_width;
            else
                remaining_cols++;
        }
        remaining_width -= col_width[j];
    }

    if (remaining_cols) {
        remaining_width /= remaining_cols;
        for (int j = 0; j < cols; j++)
            if (!col_width[j])
                col_width[j] = remaining_width;
    }

    // now update expanding items with the row height and width
    it = items.begin();
    for (int i = 0; i < rows && it != items.end(); i++) {
        for (int j = 0; j < cols; j++) {
            if ((*it)->expanding) {
                if (!(*it)->w)
                    (*it)->w = col_width[j];
                if (!(*it)->h)
                    (*it)->h = row_height[i];
                (*it)->fit();  // recursively fit
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
            if ((*it)->h > row_height[i])
                row_height[i] = (*it)->h;
            if ((*it)->w > col_width[j])
                col_width[j] = (*it)->w;
            it++;
            if (it == items.end())
                break;
        }
    }

    // now distribute remaining space into empty cols
    remaining_width = w, remaining_cols = cols;
    for (int j = 0; j < cols; j++)
        if (col_width[j]) {
            remaining_width -= col_width[j];
            remaining_cols--;
        }

    if (remaining_cols) {
        remaining_width /= remaining_cols;
        for (int j = 0; j < cols; j++)
            if (!col_width[j])
                col_width[j] = remaining_width;
    } else
        w -= remaining_width;

    // now distribute remaining space into empty rows
    remaining_height = h, remaining_rows = rows;
    for (int i = 0; i < rows; i++)
        if (row_height[i]) {
            remaining_height -= row_height[i];
            remaining_rows--;
        }

    if (remaining_rows) {
        remaining_height /= remaining_rows;
        for (int i = 0; i < rows; i++)
            if (!row_height[i])
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
            if (!(*it)->w)
                (*it)->w = col_width[j];
            if (!(*it)->h)
                (*it)->h = row_height[i];
            (*it)->fit();  // recursively fit
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

void grid_display::render() {
    for (std::list<display *>::iterator it = items.begin(); it != items.end(); it++)
        (*it)->render();
}

void grid_display::add(display *item) {
    items.push_back(item);
}

void grid_display::getAllItems(std::list<display_item_e> &items_) {
    for (std::list<display *>::iterator it = items.begin(); it != items.end(); it++)
        (*it)->getAllItems(items_);
}

int page_width = 160;
int page_height = 240;

page::page(std::string _description)
    : description(_description) {
    w = page_width;
    h = page_height;
    cols = landscape ? 2 : 1;
}

// mnemonics for all possible displays
#define WIND_DIR_T new dir_angle_text_display(WIND_DIRECTION, true)
#define WIND_DIR_G new wind_direction_gauge(WIND_DIR_T)
#define WIND_SPEED_T new speed_text_display(WIND_SPEED)
#define WIND_SPEED_G new speed_gauge(WIND_SPEED_T)
#define WIND_SPEED_H new history_display(WIND_SPEED_T)
#define WIND_SPEED_S new stat_display(WIND_SPEED)

#define PRESSURE_T new pressure_text_display()
#define PRESSURE_H new history_display(PRESSURE_T, false)
//#define PRESSURE_S     new stat_display(BAROMETRIC_PRESSURE)
#define AIR_TEMP_T new temperature_text_display(AIR_TEMPERATURE)
#define RELATIVE_HUMIDITY_T new generic_text_display(RELATIVE_HUMIDITY, "%", 2)
#define BATTERY_VOLTAGE_T new generic_text_display(BATTERY_VOLTAGE, "V", 2)
#define AIR_QUALITY_T new generic_text_display(AIR_QUALITY, "idx", 0)

#define COMPASS_T new angle_text_display(COMPASS_HEADING)
#define COMPASS_G new heading_gauge(COMPASS_T)
#define PITCH_T new angle_text_display(PITCH, 1)
#define HEEL_T new dir_angle_text_display(HEEL)
#define RATE_OF_TURN_T new dir_angle_text_display(RATE_OF_TURN)
#define RATE_OF_TURN_G new rate_of_turn_gauge(RATE_OF_TURN_T)

#define GPS_HEADING_T new angle_text_display(GPS_HEADING)
#define GPS_HEADING_G new heading_gauge(GPS_HEADING_T)
#define GPS_SPEED_T new speed_text_display(GPS_SPEED)
#define GPS_SPEED_G new speed_gauge(GPS_SPEED_T)
#define GPS_SPEED_H new history_display(GPS_SPEED_T)
#define GPS_SPEED_S new stat_display(GPS_SPEED)
#define LATITUDE_T new position_text_display(LATITUDE)
#define LONGITUDE_T new position_text_display(LONGITUDE)
#define TIME_T new time_text_display()

#define DEPTH_T new depth_text_display()
#define DEPTH_H new history_display(DEPTH_T)
//#define DEPTH_S        new stat_display(DEPTH)

#define RUDDER_ANGLE_T new dir_angle_text_display(RUDDER_ANGLE)
#define RUDDER_ANGLE_G new rudder_angle_gauge(RUDDER_ANGLE_T)

#define WATER_SPEED_T new speed_text_display(WATER_SPEED)
#define WATER_SPEED_G new speed_gauge(WATER_SPEED_T)
#define WATER_SPEED_H new history_display(WATER_SPEED_T)
#define WATER_SPEED_S new stat_display(WATER_SPEED)
#define WATER_TEMP_T new temperature_text_display(WATER_TEMPERATURE)

#define AIS_SHIPS_DISPLAY new ais_ships_display()

struct pageA : public page {
    pageA()
        : page("Wind gauges with stats") {
        add(WIND_DIR_G);
        grid_display *d = new grid_display(this, landscape ? 1 : 2);
        d->expanding = false;
        d->add(WIND_SPEED_G);
        d->add(WIND_SPEED_S);
    }
};

struct pageB : public page {
    pageB()
        : page("Wind Speed Gauge") {
        add(WIND_SPEED_G);
        grid_display *d = new grid_display(this);
        d->expanding = false;
        d->add(WIND_DIR_T);
        d->add(WIND_SPEED_S);
    }
};

struct pageC : public page {
    pageC()
        : page("Wind Speed history") {
        add(WIND_SPEED_H);
        cols = 1;
    }
};

struct pageD : public page {
    pageD()
        : page("Wind gauges with weather text") {
        cols = landscape ? 1 : 2;

        grid_display *d = new grid_display(this, landscape ? 2 : 1);

        d->add(WIND_DIR_G);
        d->add(WIND_SPEED_G);

        grid_display *t = new grid_display(this, landscape ? 3 : 1);
        t->expanding = false;
        t->add(PRESSURE_T);
        t->add(AIR_TEMP_T);
        t->add(RELATIVE_HUMIDITY_T);
        t->add(BATTERY_VOLTAGE_T);
        t->add(AIR_QUALITY_T);
    }
};

struct pageE : public page {
    pageE()
        : page("Wind and IMU text") {
        add(WIND_DIR_T);
        add(WIND_SPEED_T);
        grid_display *d = new grid_display(this, landscape ? 1 : 2);
        d->expanding = false;
        d->add(COMPASS_T);
        d->add(RATE_OF_TURN_T);
        d->add(PITCH_T);
        d->add(HEEL_T);
    }
};

struct pageF : public page {
    pageF()
        : page("Compass Gauge with inertial information") {
        add(COMPASS_G);
        grid_display *d = new grid_display(this, landscape ? 1 : 2);
        d->expanding = false;
        d->add(RATE_OF_TURN_G);

        grid_display *e = new grid_display(d);
        e->add(PITCH_T);
        e->add(HEEL_T);
    }
};

struct pageG : public page {
    pageG()
        : page("GPS Gauges") {
        add(GPS_HEADING_G);
        add(GPS_SPEED_G);
        if (landscape) {
            add(LATITUDE_T);
            add(LONGITUDE_T);
        }
    }
};

struct pageH : public page {
    pageH()
        : page("GPS speed gauges with position/time text") {
        add(GPS_SPEED_G);
        grid_display *d = new grid_display(this);
        d->expanding = false;
        d->add(GPS_HEADING_T);
        d->add(GPS_SPEED_S);
        d->add(TIME_T);
    }
};

struct pageI : public page {
    pageI()
        : page("Wind and GPS speed gauge") {
        add(WIND_DIR_G);
        grid_display *d = new grid_display(this, landscape ? 1 : 2);
        d->expanding = false;

        d->add(WIND_SPEED_G);
        if (landscape) {
            d->add(WIND_DIR_T);
            d->add(GPS_SPEED_T);
            d->add(GPS_HEADING_T);
        } else
            d->add(GPS_SPEED_G);
    }
};


struct pageJ : public page {
    pageJ()
        : page("Wind Speed and GPS speed") {
        add(WIND_SPEED_G);
        add(GPS_SPEED_G);
    }
};

struct pageK : public page {
    pageK()
        : page("GPS speed gauge and history") {
        add(GPS_SPEED_G);
        display *gps_speed_history = GPS_SPEED_H;
        gps_speed_history->expanding = false;

        add(gps_speed_history);
    }
};

struct pageL : public page {
    pageL()
        : page("GPS heading/speed text large") {
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
    pageM()
        : page("Wind gauges, GPS and inertial text ") {
        cols = landscape ? 3 : 2;
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
    pageN()
        : page("Depth text and history") {
        display *depth_t = DEPTH_T;
        if (!landscape)
            depth_t->h = h / 3;  // expand to 1/3rd of height of area;
        add(depth_t);
        add(DEPTH_H);
    }
};

struct pageO : public page {
    pageO()
        : page("Rudder angle and compass gauges") {
        add(RUDDER_ANGLE_G);
        add(COMPASS_G);
    }
};

struct pageP : public page {
    pageP()
        : page("wind, rudder, compass gauges") {
        if (landscape) {
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
    pageQ()
        : page("water speed and history") {
        add(WATER_SPEED_G);
        grid_display *d = new grid_display(this);
        d->add(WATER_SPEED_H);
        d->add(WATER_TEMP_T);
    }
};

struct pageR : public page {
    pageR()
        : page("water and gps speed gauge") {
        add(WATER_SPEED_G);
        add(GPS_SPEED_G);
    }
};

struct pageS : public page {
    pageS()
        : page("water and gps speed text") {
        add(WATER_SPEED_T);
        add(WATER_SPEED_S);
        add(GPS_SPEED_T);
        add(GPS_SPEED_S);
    }
};

struct pageT : public page {
    pageT()
        : page("baro history") {
        cols = landscape ? 3 : 2; 
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
        if (!landscape)
            add(WATER_TEMP_T);
    }
};

struct pageU : public page {
    pageU()
        : page("route display"), vmg(NAN) {
        add(new route_display());
        add(new float_text_display(ROUTE_INFO, "XTE", route_info.xte, 1));
        add(new float_text_display(ROUTE_INFO, "BRG", route_info.brg, 0));
        add(new float_text_display(ROUTE_INFO, "VMG", vmg, 0));
        add(new string_text_display(ROUTE_INFO, "RNG", srng));
        add(new string_text_display(ROUTE_INFO, "TTG", sttg));
    }

    void render() {
        // update vmg
        float tbrg = route_info.target_bearing;
        float sog = display_data[GPS_SPEED].value;
        float cog = display_data[GPS_HEADING].value;
        vmg = sog * cos(deg2rad(tbrg - cog));

        // update rng
        float lat = display_data[LATITUDE].value;
        float lon = display_data[LONGITUDE].value;
        float wpt_lat = route_info.wpt_lat;
        float wpt_lon = route_info.wpt_lon;
        float rbrg, rng;
        distance_bearing(lat, lon, wpt_lat, wpt_lon, &rng, &rbrg);
        float nrng = rng * cosf(deg2rad(rbrg - cog));

        srng = std::string(rng, 2) + "/" + std::string(nrng, 2);

        // update ttg
        if (vmg > 0.) {
            float ttg_hr;
            float ttg_min = 60 * modff(rng / vmg, &ttg_hr);
            float ttg_sec = 60 * modff(ttg_min, &ttg_min);
            sttg = std::string(ttg_hr, 0) + ":" + std::string(ttg_min, 0) + ":" + std::string(ttg_sec, 0);
        } else
            sttg = "---";

        page::render();
    }

    std::string srng, sttg;
    float vmg;
};

struct pageV : public page {
    pageV()
        : page("AIS display") {
        add(AIS_SHIPS_DISPLAY);
    }
};

struct pageW : public page {
    pageW()
        : page("pypilot statistics") {
        add(new pypilot_text_display("heading", "ap.heading", true, 0));
        add(new pypilot_command_display());
        //add(new pypilot_text_display("command", "ap.heading_command", true));
        add(new pypilot_text_display("mode", "ap.mode"));
        add(new pypilot_text_display("profile", "profile"));
        //add(new pypilot_text_display("uptime", "imu.uptime"));
        add(new pypilot_text_display("runtime", "ap.runtime"));
        add(new pypilot_text_display("watts", "servo.watts"));
        add(new pypilot_text_display("amp hours", "servo.amp_hours"));
        add(new pypilot_text_display("rudder", "rudder.angle"));
        add(new pypilot_text_display("voltage", "servo.voltage"));
        add(new pypilot_text_display("motor T", "servo.motor_temp"));
        add(new pypilot_text_display("controller T", "servo.controller_temp"));
    }

    void render() {
        page::render();
        pypilot_client_strobe();  // indicate we need pypilot data
    }
};

struct pageX : public page {
    pageX()
        : page("Barometric Pressure history") {
        add(PRESSURE_H);
        add(AIR_TEMP_T);
        cols = 1;
    }
};


static std::vector<page *> pages;
static int cur_page = 0;
route_info_t route_info;
std::vector<page_info> display_pages;

std::string ps = "page";
char page_chr = 'A';
static void add(page *p) {
    p->fit();
    pages.push_back(p);
    display_pages.push_back(page_info(page_chr, p->description));
    page_chr++;
}

void display_auto() {
    for (int i = 0; i < pages.size(); i++) {
        std::list<display_item_e> items;
        display_pages[i].enabled = true;
        pages[i]->getAllItems(items);
        for (std::list<display_item_e>::iterator jt = items.begin(); jt != items.end(); jt++)
            if (isnan(display_data[*jt].value)) {  // do not have needed data, disable page
                display_pages[i].enabled = true;
                break;
            }
    }
}

// return a list of all possible display items for enabled pages
void display_items(std::list<display_item_e> &items) {
    std::map<display_item_e, bool> map_items;
    for (int i = 0; i < pages.size(); i++) {
        std::list<display_item_e> page_items;
        if (!display_pages[i].enabled)
            continue;
        pages[i]->getAllItems(items);
        for (std::list<display_item_e>::iterator jt = page_items.begin(); jt != page_items.end(); jt++)
            map_items[*jt] = true;
    }

    for (std::map<display_item_e, bool>::iterator it = map_items.begin(); it != map_items.end(); it++)
        items.push_back(it->first);
}

static void setup_analog_pins() {
    //    adc1_config_width(ADC_WIDTH_BIT);
    //   adc1_config_channel_atten(ADC1_CHANNEL_0, ADC_ATTEN_DB_0);

    analogReadResolution(12);

#ifndef CONFIG_IDF_TARGET_ESP32S3
    ledcAttachChannel(BACKLIGHT_PIN, 500, 10, 3);
    ledcWriteChannel(3, 0);
#endif
}

// read from photo resistor and adjust the backlight
static bool over_temperature = false;
static void read_analog_pins() {
    int val = analogRead(PHOTO_RESISTOR_PIN);
#ifndef CONFIG_IDF_TARGET_ESP32S3
    //int32_t t0 = esp_timer_get_time();
    int ntc = analogRead(NTC_PIN);

    //float B = 3950;
    //float V = 3.3*ntc/4095;
    //float R25 = 10e3;
    // V = 3.3*R/(R+10e3)
    // R*4096 = ntc*(R+10e3)
    // R*(4096 - ntc) = ntc*10e3
    //float R = ntc*10e3/(4095 - ntc);
    //temperature = 1 / (log(R/R25)/B + 1/298.15) - 273.15;
    //printf("val %d %f %f %f %lld\n", ntc, V, R, temperature, esp_timer_get_time()-t0);
    // 75 = 1 / (log(R/R25)/B + 1/298.15) - 273.15;
    // (273.15+75)*log(R/R25)/B = -.1677
    // -1.9 = log(R/R25)
    // .14957 = R/R25
    // R = 1495  at 75C
    // ntc/4095 = R/(R+10e3)
    // ntc/4095 = 1495/(1495+10e3)
    // min raw value for ntc with 12 bit adc is 532 for 75C
    over_temperature = ntc < 532;
#endif

    // set backlight level
    static bool backlight_on;
    if (!display_on)
        backlight_on = false;
    else if (backlight_on && val > 3900)
        backlight_on = false;
    else if (!backlight_on && val < 3400)
        backlight_on = true;

#ifndef CONFIG_IDF_TARGET_ESP32S3
    if (backlight_on)
        ledcWriteChannel(3, 1 + settings.backlight * settings.backlight * 5 / 2);
    else
        ledcWriteChannel(3, 0);
#endif
}

static int rotation;
void display_set_mirror_rotation(int r) {
    rotation = settings.rotation;
    // if 4, get from accelerometers
    if (rotation == 4)
        rotation = r;
}

void display_toggle(bool on) {
    display_on = on ? true : !display_on;
    digitalWrite(PWR_LED, !display_on);
}

static int display_data_timeout[DISPLAY_COUNT];

void display_setup() {
    for(int i=0; i<DISPLAY_COUNT; i++) {
        display_data_timeout[i] = 5000;
        display_data[i].value = NAN; // no data
    }

    display_data_timeout[BAROMETRIC_PRESSURE] = 30000;
    display_data_timeout[AIR_TEMPERATURE] = 30000;
    display_data_timeout[RELATIVE_HUMIDITY] = 30000;
    display_data_timeout[AIR_QUALITY] = 30000;
    display_data_timeout[BATTERY_VOLTAGE] = 30000;

    display_data_timeout[ROUTE_INFO] = 10000;
    display_data_timeout[PYPILOT] = 10000;
    
    pinMode(2, INPUT_PULLUP);  // strap for display

    printf("display_setup %d\n", rotation);

    start_time = time(0);
    
#ifdef USE_U8G2
    switch (rotation) {
    case 0:
    case 2:
        page_width = 256;
        page_height = settings.show_status ? 148 : 160;
        landscape = true;
        break;
    case 1:
    case 3:
        page_width = 160;
        page_height = settings.show_status ? 244 : 256;
        landscape = false;
        break;
    }
#else
    switch (rotation) {
    case 0:
    case 2:
        page_width = 800;
        page_height = settings.show_status ? 450 : 480;
        landscape = true;
        break;
    case 1:
    case 3:
        page_width = 480;
        page_height = settings.show_status ? 800 : 770;
        landscape = false;
        break;
    }
#endif
    pinMode(PWR_LED, OUTPUT);
    digitalWrite(PWR_LED, LOW);

    draw_setup(rotation);
    cur_page = 'C' - 'A';

    for (int i = 0; i < pages.size(); i++)
        delete pages[i];
    pages.clear();
    display_pages.clear();

    // rebuild display pages
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
    add(new pageX);

    for (int i = 0; i < settings.enabled_pages.length(); i++)
        for (int j = 0; j < (int)display_pages.size(); j++)
            if (settings.enabled_pages[i] == display_pages[j].name)
                display_pages[j].enabled = true;

    setup_analog_pins();
}

void display_change_page(int dir) {
    if (rotation == 2 || rotation == 3)
        dir = -dir;

    if (in_menu) {
        menu_arrows(dir);
        return;
    }

    if (dir == 0) {
        if (display_pages[cur_page].enabled)
            return;
        dir = 1;
    } else
        buzzer_buzz(500, 20, 0);

    int looped = 0;
    while (looped < 2) {
        cur_page += dir;
        if (cur_page >= (int)display_pages.size()) {
            looped++;
            cur_page = 0;
        } else if (cur_page < 0) {
            looped++;
            cur_page = display_pages.size() - 1;
        }
        if (display_pages[cur_page].enabled) {
            printf("changed to page %c\n", 'A' + cur_page);
            return;
        }
    }

    // all pages disabled??
    display_pages[0].enabled = true;
    cur_page = 0;
    printf("warning, all pages disabled, enabling first page");
}

void display_menu_scale() {
    if (in_menu)
        menu_select();
    else {
        buzzer_buzz(800, 20, 0);
        
        if (++history_display_range >= 3) // show 5m 1h 24h
            history_display_range = 0;

        if (++ships_range >= (sizeof ships_range_table) / (sizeof *ships_range_table))
            ships_range = 0;
    }
}

static void render_status() {
#ifdef USE_RGB_LCD
    int ht = 21;
#else
    int ht = 11;
#endif
    draw_set_font(ht);

    int y = page_height;
    char wifi[] = "WIFI";
    if (WiFi.status() == WL_CONNECTED)
        draw_text(0, y, wifi);

    int x = draw_text_width(wifi) + 15;
    // show data source
    uint32_t t0 = millis();

    for (int i = 0; i < DATA_SOURCE_COUNT; i++) {
        uint32_t dt = t0 - data_source_time[i];
        if (dt < 5000) {
            draw_text(x, y, source_name[i]);
            x += draw_text_width(source_name[i]) + 5;
            if (i == WIFI_DATA)
                draw_text(x, y, get_wifi_data_str());
        }
    }

    // show page letter
    char letter[] = "A";
    letter[0] += cur_page;
    int w = draw_text_width(letter);

    draw_text(page_width - w, y, letter);
}

void data_timeout() {
    uint32_t t = millis();
    for (int i = 0; i < DISPLAY_COUNT; i++)
        if (t + 100 - display_data[i].time > display_data_timeout[i]) {
            //if(!isnan(display_data[i].value))
            //  printf("timeout %ld %ld %d\n", t0, display_data[i].time, i);
            if(!isnan(display_data[i].value)) {
                history_put((display_item_e)i, NAN); // ensure history shows lack of data
                display_data[i].value = NAN;  // timeout if no data
            }
        }
}

#if defined(USE_U8G2) || defined(USE_RGB_LCD)

void display_poll() {
    uint32_t t0 = millis();
#ifdef USE_U8G2    
    static uint32_t last_render;
    if (t0 - last_render < 200)
        return;
    last_render = t0;
#endif
    read_analog_pins();

    //printf("draw clear %d\n", display_on);
    draw_clear(display_on);
    uint32_t t1 = millis();
    if (!display_on) {
        draw_send_buffer();
        return;
    }

    draw_color(WHITE);

    static uint32_t safetemp_time;
    if(over_temperature) { // above 75C is an issue...
        int ht = 14;
        draw_set_font(ht);
        draw_text(0, 0, "OVER TEMPERATURE");
        draw_send_buffer();
        buzzer_buzz(700, 200, 3);

        if(t0 - safetemp_time > 30000) { // after 30 seconds sleep
            // hopefully safer in deep sleep
            ledcDetach(14);
            pinMode(14, OUTPUT);  // strap for display
            digitalWrite(14, 0);
            esp_deep_sleep_start();
        }
        return;
    }
    safetemp_time = t0;
    
    if (settings_wifi_ap_mode) {
        int ht = 14;
        draw_set_font(ht);
        draw_text(0, 0, "WIFI AP");

        draw_text(0, 20, "pypilot_mfd");
        draw_text(0, 60, "http://pypilot_mfd.local");

        char buf[16];
        snprintf(buf, sizeof buf, "Version %.2f", VERSION);
        draw_text(0, 100, buf);

        draw_send_buffer();
        return;
    }

    data_timeout();
    uint32_t t2 = millis();

    if (in_menu)
        menu_render();
    else
        pages[cur_page]->render();

    if (settings.show_status)
        render_status();

    uint32_t t3 = millis();

    draw_send_buffer();
    uint32_t t4 = millis();
//    printf("render took %ld %ld %ld %ld\n", t1-t0, t2-t1, t3-t2, t4-t3);
}
#endif

#ifdef USE_TFT_ESPI
//#include <TFT_eSPI.h>  // Hardware-specific library
//#include <SPI.h>

TFT_eSPI tft = TFT_eSPI();

#define WHITE 0xffff
#define BLACK 0

void display_setup() {
    tft.init();

    tft.setRotation(0);
    tft.fillScreen(TFT_BLACK);

    tft.setTextSize(1);
}

void display_poll() {
    static bool last_settings_wifi_ap_mode;
    if (settings_wifi_ap_mode != last_settings_wifi_ap_mode) {
        tft.fillScreen(TFT_BLACK);
        last_settings_wifi_ap_mode = settings_wifi_ap_mode;
    }
    if (settings_wifi_ap_mode) {
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

    if (lpwind_dir >= 0) {
        const uint8_t xc = 67, yc = 67, r = 64;
        float wind_rad = deg2rad(lpwind_dir);
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

        snprintf(buf, sizeof buf, "%03d", (int)lpwind_dir);

        tft.fillTriangle(coords[0][0], coords[0][1], coords[1][0], coords[1][1], coords[2][0], coords[2][1], WHITE);

        tft.drawString(buf, xp, yp, 4);
    }

    float iknots;
    float iknotf = 10 * modff(wind_knots, &iknots);

    snprintf(buf, sizeof buf, "%02d.%d", (int)iknots, (int)iknotf);
    tft.drawString(buf, 0, 150, 6);
}
#endif

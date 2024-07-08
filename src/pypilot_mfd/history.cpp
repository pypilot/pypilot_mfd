/* Copyright (C) 2024 Sean D'Epagnier <seandepagnier@gmail.com>
 *
 * This Program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 */

#include <Arduino_JSON.h>

// 5m, 1 hr, 1d
#include <Wire.h>

#include "display.h"
#include "history.h"

static uint32_t history_range_time[] = {5*60, 60*60, 24*60*60};

int history_display_range;

String history_get_label(history_range_e range)
{
    switch(range) {
        case MINUTE_5: return "5m";
        case HOUR:   return "1h";
        case DAY:    return "1d";
    }
    return "";
}

display_item_e history_items[] = { WIND_SPEED, BAROMETRIC_PRESSURE, DEPTH, GPS_SPEED, WATER_SPEED };

struct history
{
    std::list<history_element> data[HISTORY_RANGE_COUNT];

    // store only high and lows
    std::list<history_element> data_high[HISTORY_RANGE_COUNT];
    std::list<history_element> data_low[HISTORY_RANGE_COUNT];
    float llvalue, lvalue;
    uint32_t ltime;

    void put(float value, uint32_t time)
    {
        for(int range = 0; range < HISTORY_RANGE_COUNT; range++) {
            uint32_t range_time = history_range_time[range];
            int range_timeout = range_time * 1000 / 80;

            uint32_t fronttime = data[range].empty() ? 0 : data[range].front().time;
            uint32_t dt = time - fronttime;
            if(dt > range_timeout) {
                if(range > 0) {
                    // average previous range data
                    value = 0;
                    int count = 0;
                    for(std::list<history_element>::iterator it = data[range-1].begin(); it != data[range-1].end(); it++) {
                        value += it->value;
                        count++;
                        if(it->time < time - range_timeout)
                            break;
                    }
                    value /= count;
                }
                data[range].push_front(history_element(value, time));
            }

            // log high/low
            if(llvalue < lvalue) {
                if(lvalue > value)
                    // new peak at lvalue
                    data_high[range].push_front(history_element(lvalue, ltime));
            } else {
                if(lvalue < value)
                    data_low[range].push_front(history_element(lvalue, ltime));
                    // new trough at lvalue
            }
            llvalue = lvalue;
            lvalue = value;
            ltime = time;

            // remove elements that expired
            expire(data[range], time, range_time);
            expire(data_high[range], time, range_time);
            expire(data_low[range], time, range_time);
        }
    }

    void expire(std::list<history_element> &data, uint32_t time, int range_time)
    {
        while(!data.empty()) {
            history_element &back = data.back();
            if(time - back.time < range_time * 1000)
                break;
            data.pop_back();
        }
    }
};

#define HISTORY_COUNT  (sizeof history_items) / (sizeof *history_items)

history histories[HISTORY_COUNT];

void history_put(display_item_e item, float value, uint32_t time)
{
    for(int i=0; i<HISTORY_COUNT; i++)
        if(history_items[i] == item) {
            histories[i].put(value, time);
            break;
        }
}

std::list<history_element> *history_find(display_item_e item, int r, uint32_t &totalmillis, float &high, float &low)
{
    for(int i=0; i<(sizeof history_items)/(sizeof *history_items); i++)
        if(history_items[i] == item) {
            totalmillis = history_range_time[r] * 1000;
            std::list<history_element> &data_high = histories[i].data_high[r];
            std::list<history_element> &data_low = histories[i].data_low[r];
            // compute range
            high = -INFINITY;
            for(std::list<history_element>::iterator it = data_high.begin(); it!=data_high.end(); it++)
                if(it->value > high)
                    high = it->value;

            low = INFINITY;
            for(std::list<history_element>::iterator it = data_low.begin(); it!=data_low.end(); it++)
                if(it->value < low)
                    low = it->value;
            return &histories[i].data[r];
        }

         // error
    printf("history item not found!! %d\n", item);
    return 0;
}

JSONVar history_get_data(display_item_e item, history_range_e range)
{
    uint32_t totalmillis;
    float high, low;
    std::list<history_element> *data = history_find(item, range, totalmillis, high, low);
    JSONVar jsondata;
    if(! data)
        return jsondata;
    int i = 0;
    for(std::list<history_element>::iterator it = data->begin(); it != data->end(); it++) {
        JSONVar value;
        value["value"] = it->value;
        value["time"] = it->time / 1000.0f;
        jsondata[i++] = value;
    }
    JSONVar output;
    output["data"] = jsondata;
    output["high"] = high;
    output["low"] = low;
    output["total_time"] = totalmillis / 1000.0f;

    return output;
}

#define I2C_EEPROM_ADDRESS 0x51
bool has_i2c_eeprom;
void history_setup()
{
    Wire.beginTransmission(I2C_EEPROM_ADDRESS);
    has_i2c_eeprom = Wire.endTransmission() == 0;
    if(has_i2c_eeprom)
        printf("Detected I2C EEPROM for storing history data with power cycles\n");

    // load history data,  
}

// write current history to eeprom
void history_store()
{
    /*
    for(int i=0; i<HISTORY_COUNT; i++)
        for(int range = 0; range < HISTORY_RANGE_COUNT; range++) {
            for(std::list<history_element>::iterator it = data[range-1].begin(); it != data[range-1].end(); it++) {
                // store history
            }    
            */
}

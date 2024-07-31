/* Copyright (C) 2024 Sean D'Epagnier <seandepagnier@gmail.com>
 *
 * This Program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 */

#include "rapidjson/document.h"

// 5m, 1 hr, 1d
#include <Wire.h>

#include "display.h"
#include "history.h"


#define DEVICE_ADDRESS 0x51
#define EEPROM_END  8192  // 8*8k
#define EEPROM_PARTITION  1024 // 1/4th

int eeprom_write_pos = -1; // position to write next history packet
int eeprom_read_pos = -1; // position to read next packet

static bool have_eeprom = false;
static uint32_t epoch; // number of seconds since Jan 1 2020

enum type {HISTORY_VALUE, HISTORY_LOW, HISTORY_HIGH}; // for storing history packets on eeprom
display_item_e history_items[] = { WIND_SPEED, BAROMETRIC_PRESSURE, DEPTH, GPS_SPEED, WATER_SPEED };
#define HISTORY_ITEM_COUNT  (sizeof history_items) / (sizeof *history_items)

// 5 min, hour, day, month, year
static uint32_t history_range_time[] = {5*60, 60*60, 24*60*60, 30*24*60*60, 365*24*60*60};
int history_display_range;

static int32_t cold_time()
{
    return time(0); // time in seconds since cold boot (accounting for realtime in deepsleep)
}

std::string history_get_label(history_range_e range)
{
    switch(range) {
        case MINUTE_5: return "5m";
        case HOUR:   return "1h";
        case DAY:    return "1d";
        case MONTH:  return "1m";
        case YEAR:   return "1y";
    }
    return "";
}

static void store_packet(int range, uint8_t index, uint8_t type, int32_t time, float value);

struct history
{
    std::list<history_element> data[HISTORY_RANGE_COUNT];

    // store only high and lows
    std::list<history_element> data_high[HISTORY_RANGE_COUNT];
    std::list<history_element> data_low[HISTORY_RANGE_COUNT];

    float min_llvalue[HISTORY_RANGE_COUNT], min_lvalue[HISTORY_RANGE_COUNT];
    float max_llvalue[HISTORY_RANGE_COUNT], max_lvalue[HISTORY_RANGE_COUNT];
    uint32_t last_time[HISTORY_RANGE_COUNT];

    history() {
        // put NAN entry in each value to split history and new data
        int32_t t = cold_time();
        for(int range = 0; range < HISTORY_RANGE_COUNT; range++) {
            data[range].push_back(history_element(NAN, t));
        }
    }

    void put(uint8_t index, float value)
    {
        int32_t time = cold_time(); // 32 bits in seconds
        //printf("put %f\n", value);
        for(int range = 0; range < HISTORY_RANGE_COUNT; range++) {
            float &lvalue_min = min_lvalue[range];
            float &lvalue_max = max_lvalue[range];
            float &llvalue_min = min_llvalue[range];
            float &llvalue_max = max_llvalue[range];

            //printf("valus %d %d %d %d %f %f %f\n", range, data[range].size(), data_high[range].size(), data_low[range].size(), value, lvalue, llvalue);

            uint32_t range_time = history_range_time[range];
            int range_timeout = range_time * 1000 / 80;

            uint32_t fronttime = data[range].empty() ? 0 : data[range].front().time;
            uint32_t dt = time - fronttime;
            if(dt < range_timeout)
                break;
            float minv, maxv;
            if(range > 0) {
                // average previous range data
                value = 0;
                minv = INFINITY;
                maxv = -INFINITY;
                int count = 0;
                for(std::list<history_element>::iterator it = data[range-1].begin(); it != data[range-1].end(); it++) 
                {
                    if(!isnan(value))
                        value += it->value;
                    if(it->value < minv)
                        minv = it->value;
                    if(it->value > maxv)
                        maxv = it->value;
                    count++;
                    if(it->time < time - range_timeout)
                        break;
                }
                if(count == 0)
                    value = NAN;
                else
                    value /= count;
                store_packet(range, index, HISTORY_VALUE, time, value);
            } else
                minv = value, maxv = value;
            data[range].push_front(history_element(value, time));

            // log high/low
            if(data_high[range].empty())
                data_high[range].push_front(history_element(maxv, time));
            else if(llvalue_max < lvalue_max && lvalue_max >= maxv) {
                data_high[range].push_front(history_element(lvalue_max, time));
                if(range > 0)
                    store_packet(range, index, HISTORY_HIGH, time, value);
            }
            if(data_low[range].empty())
                data_low[range].push_front(history_element(minv, time));
            else if(llvalue_min > lvalue_min && lvalue_min <= minv) {
                data_low[range].push_front(history_element(lvalue_min, time));
                if(range > 0)
                    store_packet(range, index, HISTORY_LOW, time, value);
            }

            llvalue_min = lvalue_min;
            lvalue_min = minv;

            llvalue_max = lvalue_max;
            lvalue_max = maxv;

            // remove elements that expired
            expire(data[range], time, range_time);
            expire(data_high[range], time, range_time);
            expire(data_low[range], time, range_time);
        }
    }

    void put_back(int32_t time, float value, int range, type t) {
        std::list<history_element> *d;
        switch(t) {
        case HISTORY_VALUE: d = &data[range]; break;
        case HISTORY_LOW:   d = &data_low[range]; break;
        case HISTORY_HIGH:  d = &data_high[range]; break;
        default: printf("invalid type for put back\n"); return;
        }

        // sanity check
        if(d->front().time < time)
            d->push_back(history_element(value, time));
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

history histories[HISTORY_ITEM_COUNT];
    
void history_put(display_item_e item, float value)
{
    for(int i=0; i<HISTORY_ITEM_COUNT; i++)
        if(history_items[i] == item) {
            histories[i].put(i, value);
            break;
        }
}

std::list<history_element> *history_find(display_item_e item, int r, uint32_t &totalseconds, float &high, float &low)
{
    for(int i=0; i<(sizeof history_items)/(sizeof *history_items); i++)
        if(history_items[i] == item) {
            totalseconds = history_range_time[r];
            std::list<history_element> &data_high = histories[i].data_high[r];
            std::list<history_element> &data_low = histories[i].data_low[r];
            // compute range
            if(histories[i].data[r].empty())
                return 0;
            float v = histories[i].data[r].front().value;
            high = v;
            for(std::list<history_element>::iterator it = data_high.begin(); it!=data_high.end(); it++)
                if(it->value > high)
                    high = it->value;

            low = v;
            for(std::list<history_element>::iterator it = data_low.begin(); it!=data_low.end(); it++)
                if(it->value < low)
                    low = it->value;
            return &histories[i].data[r];
        }

         // error
    printf("history item not found!! %d\n", item);
    return 0;
}

rapidjson::Value history_get_data(display_item_e item, history_range_e range)
{
    uint32_t totalmillis;
    float high, low;
    std::list<history_element> *data = history_find(item, range, totalmillis, high, low);
    rapidjson::Value jsondata;
    if(! data)
        return jsondata;
    int i = 0;
    for(std::list<history_element>::iterator it = data->begin(); it != data->end(); it++) {
        rapidjson::Value value;
        value["value"] = it->value;
        value["time"] = it->time / 1000.0f;
        jsondata[i++] = value;
    }
    rapidjson::Value output;
    output["data"] = jsondata;
    output["high"] = high;
    output["low"] = low;
    output["total_time"] = totalmillis / 1000.0f;

    return output;
}


/* history is stored in i2c eeprom to allow retrieval in the event of power loss
   64kb of eeprom is divided into 4 regions relative to the data range (hour, day, month, year)
   each region is divided again into relative and absolute time data.

   relative time data can restore history after deep sleep, but not power loss

   absolute time data can restore history given that the real time and date is known
   both when the data was recorded as well as when it is restored
   the true date and time can be obtained from gps
 */


static uint8_t crc8(const uint8_t *data, size_t len) {
    uint8_t crc = 0xff;
    while (len--) {
        crc ^= *data++;
        for (unsigned k = 0; k < 8; k++)
            crc = crc & 0x80 ? (crc << 1) ^ 0x31 : crc << 1;
    }
    return crc;
}

/*
  packets are 8 bytes
4 bytes specify the time in seconds since jan 2020
2 bytes specify the data value

1 byte indicator
   4 bits specify type of packet
   2 bits specify range,  1 hour, 24hours, 1 month, year
   2 bits specify type  0 - data, 1 - low, 2 - high, 3 - unspecified

1 byte crc8
 */


static int32_t read_packet_time(int offset)
{
    offset *= 8;
    Wire.beginTransmission(DEVICE_ADDRESS);
    Wire.write((offset>>8)&0xff);
    Wire.write(offset & 0xff);
    Wire.endTransmission();
    // first pass find

    Wire.requestFrom(DEVICE_ADDRESS, 8);
    uint8_t data[8] = { 0 }, i=0;
    while (Wire.available() && i < 8)
        data[i++] = Wire.read();
    if(i == 0 && data[7] == crc8(data, 7))
        return *(uint32_t*)data;

    return -1;
}

static bool read_packet(bool absolute, uint8_t range, int offset)
{
    offset *= 8;
    Wire.beginTransmission(DEVICE_ADDRESS);
    Wire.write((offset>>8)&0xff);
    Wire.write(offset & 0xff);
    Wire.endTransmission();

    Wire.requestFrom(DEVICE_ADDRESS, 8);
    uint8_t data[8] = { 0 }, i=0;
    while (Wire.available() && i < 8)
        data[i++] = Wire.read();
    if(i != 8 || data[7] != crc8(data, 7))
        return false;

    // time on eeprom is absolute since epoch
    int32_t time = *(uint32_t*)data;
    if(absolute)
        time -= epoch; // relative time of historical absoute data usually will be negative since it is from before we booted

    int32_t curtime = cold_time();

    // sanity check, if history data is after current time, it is invalid, maybe our epoch is wrong?
    if(time > curtime)
        return false;

    uint16_t value = *(uint16_t*)(data+4);    
    type itype = (type)((data[6]&0x3)>>4);
    uint8_t item = data[6]&0xf;

    if(item >= HISTORY_ITEM_COUNT)
        return false; // invalid, corrupted data?
    
    // apply fixed to float conversion of 2 byte data
    float v;
    switch(history_items[item]) {
    case DEPTH: GPS_SPEED: WATER_SPEED:
    case WIND_SPEED: v = value / 100.0f; break;
    case BAROMETRIC_PRESSURE: v = value / 100.0f + 1000; break;
    }

    uint32_t age = curtime - time;
    
    uint32_t max_age = history_range_time[range+1];
    if(age > max_age)
        return range == 4; // at higher range if our time is out of range, we are done

    // add to back of history
    histories[item].put_back(time, value, range+1, itype);
    return true;
}

static void eeprom_store_packet(int position, uint8_t data[8]) {
    int offset = position * 8;
    Wire.beginTransmission(DEVICE_ADDRESS);
    Wire.write((offset>>8)&0xff);
    Wire.write(offset & 0xff);
    for(int i=0; i<8; i++)
        Wire.write(data[i]);
    Wire.endTransmission();
}

static void eeprom_store(int position, uint8_t index, uint8_t type, uint32_t time, float value)
{
    uint8_t packet[8];

    int64_t t = cold_time();
    *(uint32_t*)(packet) = t + epoch;

    uint16_t v = 0;
    switch(history_items[index]) {
    case DEPTH: GPS_SPEED: WATER_SPEED:
    case WIND_SPEED: v = value *100; break;
    case BAROMETRIC_PRESSURE: v = (value - 1000)*100; break;
    }
        
    *(uint16_t*)(packet+4) = v;
    packet[6] = ((type&0x3)<<2) | (index & 0xf);
    packet[7] = crc8(packet, 7);
        
    eeprom_store_packet(position, packet);
}


RTC_DATA_ATTR int eeprom_rel_write[4], eeprom_rel_write_wrapped[4];

struct history_eeprom_range_t {
    int rel_start, abs_start;

    int rel_read;
    int abs_read, abs_write;
    
    uint8_t range;
    // history of each eeprom section (by history range) state progresses
    enum { INIT,            // nothing initialized yet
           RELATIVE_READ,        // in relative mode, can read and write relative section
           RELATIVE_READY,  // done reading, continue writing
           ABSOLUTE_SEARCH, // scanning absolute region for start
           COPY,            // copying data from relative to absolute section
           ABSOLUTE_READ,        // absolute mode can read and write absolute section
           ABSOLUTE_READY   // done reading, continue writing
    } state;
    
    int abs_pos_search, abs_pos_search_len;
    int abs_pos_search_time;
    int32_t last_abs_time;

    history_eeprom_range_t(uint8_t _range) : range(_range), state(INIT)
    {
        // start at unknown epoch range
        rel_start = EEPROM_PARTITION * 2*range;
        abs_start = rel_start + EEPROM_PARTITION;
    }

    void store_packet(uint8_t index, uint8_t type, int32_t time, float value)
    {
        int off, roff;
        switch(state) {
        case RELATIVE_READ:
        case RELATIVE_READY:
        case ABSOLUTE_SEARCH:
         case COPY: // store packet to relative time region
            off = eeprom_rel_write[range];
            eeprom_store(rel_start + off, index, type, time, value);
            off++;
            if(off == EEPROM_PARTITION) {
                off = 0;
                eeprom_rel_write_wrapped[range] = 1;
            }
            eeprom_rel_write[range] = off;
            break;
        case ABSOLUTE_READ: // store packet to absolute time region
        case ABSOLUTE_READY:
            time += epoch; // put in absolute time
            if(time < last_abs_time) {
                printf("clock dilation, not storing\n");
                break;
            }
            last_abs_time = time;
            
            eeprom_store(abs_start + abs_write, index, type, time, value);
            abs_write++;
            if(abs_write == EEPROM_PARTITION)
                abs_write = 0;
            break;
        }
    }
    
    void poll() {
        switch(state) {
        case INIT:
            // if this is a cold boot reset these
            if(eeprom_rel_write[range] < 0) {
                // invalid, just reset to 0
                printf("invalid eeprom relative ranges %d %d %d\n", eeprom_rel_write_wrapped[range], eeprom_rel_write[range]);
                eeprom_rel_write_wrapped[range] = 0;
                eeprom_rel_write[range] = 0;
            }
            rel_read = eeprom_rel_write[range];
            state = RELATIVE_READ;
            // fall through
        case RELATIVE_READ:
            rel_read--;
            if(rel_read < 0) {
                if(eeprom_rel_write_wrapped[range])
                    rel_read = EEPROM_PARTITION-1;
                else {
                    state = RELATIVE_READY;
                    break;
                }
            }
            if(rel_read == eeprom_rel_write[range]) {
                state = RELATIVE_READY;
                break;
            }
            
            // read one packet
            if(!read_packet(false, range, rel_start + rel_read)) {
                state = RELATIVE_READY;
                break;
            }
        break;
        case RELATIVE_READY:
            // check if we have epoch, if so enter search
            if(epoch) {
                // we dont know where the most recent data entry is yet
                // initially read from position 0
                abs_pos_search = 0;
                abs_pos_search_time = read_packet_time(abs_start);
                if(abs_pos_search_time < 0) { // first entree invalid
                    abs_pos_search_len = 0;
                    last_abs_time = epoch;
                } else {
                    abs_pos_search_len = EEPROM_PARTITION/2;
                    last_abs_time = abs_pos_search_time;
                }
                state = ABSOLUTE_SEARCH;
            } else // fallthrough
                break;

        case ABSOLUTE_SEARCH:
        {
            int search_time = read_packet_time(abs_pos_search + abs_pos_search_len);
            if(search_time > abs_pos_search_time) {
                // new position is more recent, start searching this half of remaining data
                abs_pos_search += abs_pos_search_len;
                abs_pos_search_time = search_time;
                last_abs_time = search_time;
            }

            abs_pos_search_len /= 2;
            if(abs_pos_search_len > 0) // will continue the search on next poll
                break;


            printf("eeprom found most recent data offset: %d %d\n", range, abs_pos_search);
            // binary search is complete, we found most recent data
            abs_read = abs_pos_search;

            // write one position after this
            abs_write = abs_read + 1;
            if(abs_write >= EEPROM_PARTITION)
                abs_write = 0;

            rel_read = eeprom_rel_write[range];
            state = COPY;
        }

        case COPY:
            // TODO: read data from relative and write to absolute (once the other routines are working)
            state = ABSOLUTE_READ;
            break;

        case ABSOLUTE_READ:
            abs_read--;
            if(abs_read < 0)
                abs_read = EEPROM_PARTITION-1;
            if(abs_read == abs_write) {
                state = ABSOLUTE_READY;
                printf("eeprom finished reading absolute history1 %d\n", range);
                break;
            }
            
            // read one packet
            if(!read_packet(true, range, abs_start + abs_read)) {
                state = ABSOLUTE_READY;
                printf("eeprom finished reading absolute history2 %d\n", range);
                break;
            }
            break;

        case ABSOLUTE_READY:
            // do nothing
            break;
        }
    }
};

history_eeprom_range_t history_eeprom_range[4] = {{0}, {1}, {2}, {3}};

static void store_packet(int range, uint8_t index, uint8_t type, int32_t time, float value)
{
    history_eeprom_range[range].store_packet(index, type, time, value);
}

static int day_of_year(int year, uint8_t month, uint8_t day)
{
    uint8_t days_in_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    bool leap_year = (year%4==0 && year%100) || (year%400 == 0);
    if(leap_year)
        days_in_month[1] = 29;
    if(month > 12 || day > days_in_month[month-1])
        return -1;

    int days = 0;
    for(int i=0; i<month-1; i++)
        days += days_in_month[i];
    return days + day;
}

void history_set_time(uint32_t date, int hour, int minute, float second)
{
    int year = date % 100;
    date /= 100;
    int month = date % 100;
    date /= 100;
    int day = date % 100;

    int julian_day = day_of_year(year, month, day) - 1;
    if(year < 0 || julian_day < 0)
        return; // invalid

    // subtract current time to record into epoch the startup time in number of seconds since 2020
    int64_t t = cold_time();
    float time = ((((year-20) * 365.24 + julian_day) * 24 + hour) * 60 + minute)*60 + second;

    epoch = time - t;
}

uint8_t cur_range;
void history_poll()
{
    if(!have_eeprom)
        return;

    history_eeprom_range[cur_range++].poll();
    if(cur_range == 4)
        cur_range = 0;
}

void history_setup()
{
    Wire.begin();
    Wire.beginTransmission(DEVICE_ADDRESS);
    int error = Wire.endTransmission();

    if(error == 0) {
        printf("found eeprom at address %x\n", DEVICE_ADDRESS);
        have_eeprom = true;
    } else
        printf("no eeprom on address %x\n", DEVICE_ADDRESS);
}

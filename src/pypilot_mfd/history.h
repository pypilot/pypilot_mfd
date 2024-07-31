/* Copyright (C) 2024 Sean D'Epagnier <seandepagnier@gmail.com>
 *
 * This Program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 */

#include <list>

struct history_element {
    float value;
    int32_t time; // time in seconds since boot (negative for history before boot)
    history_element(float _value, uint32_t _time) : value(_value), time(_time) {}
};

enum history_range_e {MINUTE_5, HOUR, DAY, MONTH, YEAR, HISTORY_RANGE_COUNT};

void history_setup();
void history_poll();
void history_put(display_item_e item, float value);
void history_set_time(uint32_t date, int hour, int minute, float second);

std::list<history_element> *history_find(display_item_e item, int r, uint32_t &totalseconds, float &high, float &low);
std::string history_get_label(history_range_e range);

extern int history_display_range;

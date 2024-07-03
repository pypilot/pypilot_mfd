/* Copyright (C) 2024 Sean D'Epagnier <seandepagnier@gmail.com>
 *
 * This Program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 */

void history_put(float value, uint32_t time);
std::list<history_element> *history_find(display_item_e item, int r, uint32_t &totalmillis, float &high, float &low);
JSONVar history_get_data(display_item_e item, history_range_e range);
String history_get_label(history_range_e range);

enum history_range_e {MINUTE_5, HOUR, DAY, HISTORY_RANGE_COUNT};

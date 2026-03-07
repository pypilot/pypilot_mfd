/* Copyright (C) 2026 Sean D'Epagnier <seandepagnier@gmail.com>
 *
 * This Program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 */

enum display_item_e { WIND_SPEED,
                      WIND_ANGLE,
                      TRUE_WIND_SPEED,
                      TRUE_WIND_ANGLE,
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

std::string display_get_item_label(display_item_e item);
void display_data_update(display_item_e item, float value, data_source_e source);
bool display_data_get(display_item_e item, float &value, std::string &source, uint64_t &time);
void display_setup();
void display_poll();

/* Copyright (C) 2024 Sean D'Epagnier <seandepagnier@gmail.com>
 *
 * This Program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 */

extern int wireless_message_count;
extern bool wifi_connected;

sensor_position wireless_str_position(const std::string &p);

void wireless_scan_networks();
void wireless_program_channel();
void wireless_unlock_channel(const std::string &mac);

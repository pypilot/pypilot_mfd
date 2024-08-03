/* Copyright (C) 2024 Sean D'Epagnier <seandepagnier@gmail.com>
 *
 * This Program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 */

void pypilot_client_strobe();
void pypilot_client_poll();
void pypilot_watch(std::string key);
std::string pypilot_client_value(std::string key, int digits=1);

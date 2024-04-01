/* Copyright (C) 2024 Sean D'Epagnier <seandepagnier@gmail.com>
 *
 * This Program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 */

#include <mdns.h>

void mdns_analyze_results(mdns_result_t * results);
void find_mdns_service(const char * service_name, const char * proto);
void mdns_setup();

extern int signalk_discovered;
extern int pypilot_discovered;

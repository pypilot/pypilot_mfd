/* Copyright (C) 2024 Sean D'Epagnier <seandepagnier@gmail.com>
 *
 * This Program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 */
#include <map>

struct ship
{
    int mmsi;
    uint32_t timestamp;

    String status;
    
    float lat, lon, sog, cog, hdg, rot;
    String name, callsign, shiptype;
    int to_bow, to_stern, to_port, to_starboard;

    int draught;

    String destination;

    float cpa, tcpa, dist;

    float simple_x(float slon);
    float simple_y(float slat);
    void compute(float slat, float slon, float ssog, float scog, uint32_t t0);
};

extern std::map<int, ship> ships;
bool ais_parse_line(const char *line, data_source_e source);

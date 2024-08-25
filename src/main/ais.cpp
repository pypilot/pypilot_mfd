/* Copyright (C) 2024 Sean D'Epagnier <seandepagnier@gmail.com>
 *
 * This Program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 */

#include "display.h"
#include "ais.h"
#include "utils.h"

// decode nmea AIS packets as well as compute cpa and tcpa
std::map<int, ship> ships;

float ship::simple_x(float slon)
{
    return cosf(deg2rad(lat))*resolv(lon-slon) * 60;
}

float ship::simple_y(float slat)
{
    return (lat-slat)*60;
}

void ship::compute(float slat, float slon, float ssog, float scog, uint32_t t0)
{
    float dt = (t0 - timestamp) / 1000.0f;
    
    // now compute cpa and tcpa
    //if 'rot' in ais_data:
    //acog += ais_data['rot']/60*5
    float x = simple_x(slon);
    float y = simple_y(slat);
    dist = hypot(x, y);

    // velocity vectors in x, y
    float rcog = deg2rad(cog), rscog = deg2rad(scog);
    float bvx = ssog*sinf(rscog), bvy = ssog*cosf(rscog);
    float avx = sog*sinf(rcog), avy = sog*cosf(rcog);

    // update from now to time difference since last ais fix
    x += avx*dt;
    y += avy*dt;

    // relative velocity of ais target
    float vx = avx - bvx, vy = avy - bvy;

    // the formula for time of closest approach is when the
    // derivative of the distance with respect to time is zero
    // dd/dt = 2*t*vx^2 + 2*vx*x + 2*t*vy^2 + 2*vy*y
    float v2 = (vx*vx + vy*vy), t;
    if(v2 < 1e-4) // tracks are nearly parallel courses
        t = 0;
    else
        t = (vx*x + vy*y)/v2;

    // closest point of approach distance in nautical miles
    cpa = hypotf(t*vx - x, t*vy - y);
    
    // time till closest point of approach is in hours, convert to seconds
    tcpa = t * 3600;
}

static const char *skip(const char *line, int count)
{
    while(count) {
        if(!*line)
            return 0;
        if(*line == ',')
            count--;
        line++;
    }
    return line;
}

static std::vector<bool> ais_data[2];

static int ais_n(std::vector<bool> &data, int start, int len, bool sign=false)
{
    //printf("ais_n %d %d %d\n", data.size(), start, len);
    if(data.size() < start+len) {
        printf("warning, empty ais data\n");
        return 0;
    }
    bool negative = false;
    if (sign) {
        if (data[start])
            negative = true;
        start++;
        len--;
    }
            
    int result = 0;
    for(int i=0; i<len; i++)
        if(data[start+i] != negative)
            result |= (1<<(len-i-1));

    if(negative) // 2's compliment
        result = -(result + 1);

    return result;
}

std::string ais_t(std::vector<bool> &data, int start, int len) {
    std::string result = "";
    for(int i=0; i<len; i+=6) {
        char d = ais_n(data, start+i, 6);
        if( d == 0 )
            break;
        if( d <= 31)
            d += 64;
        result += d;
    }
    return result;
}

std::string ais_status(int index) {
    switch(index) {
    case 0: return "Under way using engine";
    case 1: return "At anchor";
    case 2: return "Not under command";
    case 3: return "Restricted manoeuverability";
    case 4: return "Constrained by her draught";
    case 5: return "Moored";
    case 6: return "Aground";
    case 7: return "Engaged in Fishing";
    case 8: return "Under way sailing";
    case 9: return "Reserved for future amendment of Navigational Status for HSC";
    case 10: return "Reserved for future amendment of Navigational Status for WIG";
    case 11: return "Power-driven vessel towing astern (regional use)";
    case 12: return "Power-driven vessel pushing ahead or towing alongside (regional use).";
    case 13: return "Reserved for future use";
    case 14: return "AIS-SART is active";
    }
    
    return "Undefined";
}

float sign(float v) {
    if(v>0)
        return 1;
    else if(v < 0)
        return -1;
    return 0;
}

float ais_rot(int rot) {
    if(rot == -128)
        return NAN;
    float d = rot / 4.733;
    return sign(rot) * d * d;
}

float ais_sog(int sog) {
    if(sog == 1023)
        return NAN;
    return sog/10.0;
}

float ais_cog(int cog) {
    if(cog == 3600)
        return NAN;
    return cog / 10.0;
}

float ais_hdg(int hdg) {
    if(hdg == 511)
        return NAN;
    return hdg;
}

float ais_ll(int n) {
    return n / 600000.0;
}

std::string ais_e(int type) {
    if(type == 0)                       return "N/A";
    if(type >= 20 && type <= 29)        return "WIG";
    switch(type) {
    case 30: return "Fishing";
    case 31: return "Towing";
    case 32: return "Towing: length exceeds 200m or breadth exceeds 25m";
    case 33: return "Dredging or underwater ops";
    case 34: return "Diving ops";
    case 35: return "Military ops";
    case 36: return "Sailing";
    case 37: return "Pleasure Craft";
    }
    if(type >= 40 && type <= 49)        return "High speed craft";
    switch(type) {
    case 30: return "Fishing";
    case 31: return "Towing";
    case 32: return "Towing: length exceeds 200m or breadth exceeds 25m";
    case 33: return "Dredging or underwater ops";
    case 34: return "Diving ops";
    case 35: return "Military ops";
    case 36: return "Sailing";
    case 37: return "Pleasure Craft";
    }
    if(type >= 60 && type <= 69)        return "Passenger";
    if(type >= 70 && type <= 79)        return "Cargo";
    if(type >= 80 && type <= 89)        return "Tanker";
    if(type >= 90 && type <= 99)        return "Other";

    return "Unknown";
}
    
static bool decode_ais_data(std::vector<bool> &data)
{
    int message_type = ais_n(data, 0, 6);
    int mmsi = ais_n(data, 8, 30);

    //printf("decode ais_data %d %d\n", message_type, mmsi);

    if(ships.find(mmsi) == ships.end())
        ships[mmsi] = ship();

    ship &s = ships[mmsi];
    s.mmsi = mmsi;
    if(message_type >=1 && message_type <=3) {
        s.status = ais_status(ais_n(data, 38, 4));
        s.rot = ais_rot(ais_n(data, 42, 8, true));
        s.sog = ais_sog(ais_n(data, 50, 10));
        //'pos_acc': ais_n(data[60:61)),
        s.lon = ais_ll(ais_n(data, 61, 28, true));
        s.lat = ais_ll(ais_n(data, 89, 27, true));
        s.cog = ais_cog(ais_n(data, 116, 12));
        //'hdg' = ais_hdg(data[128:137)),
        s.timestamp = millis();
        //printf("ais 1 %s %f %f %f %f %f\n", s.status.c_str(), s.rot, s.sog, s.lon, s.lat, s.cog);
    } else if(message_type == 5) {
        s.callsign = ais_t(data, 70, 42);
        s.name = ais_t(data, 112, 120);
        s.shiptype = ais_e(ais_n(data, 232, 8));
        s.to_bow = ais_n(data, 240, 9);
        s.to_stern = ais_n(data, 249, 9);
        s.to_port = ais_n(data, 258, 6);
        s.to_starboard = ais_n(data, 264, 6);
        s.draught = ais_n(data, 294, 8);
        s.destination = ais_t(data, 302, 120);
    } else if(message_type == 18) {
        s.sog = ais_sog(ais_n(data, 46, 10));
        s.lon = ais_ll(ais_n(data, 57, 28, true));
        s.lat = ais_ll(ais_n(data, 85, 27, true));
        s.cog = ais_cog(ais_n(data, 112, 12));
        s.hdg = ais_hdg(ais_n(data, 124, 9));
        s.timestamp = millis();
    } else if(message_type == 19) {
        s.sog = ais_sog(ais_n(data, 46, 10));
        s.lon = ais_ll(ais_n(data, 57, 28, true));
        s.lat = ais_ll(ais_n(data, 85, 27, true));
        s.cog = ais_cog(ais_n(data, 112, 12));
        s.hdg = ais_hdg(ais_n(data, 124, 9));
        s.name = ais_t(data, 143, 120);
        s.shiptype = ais_e(ais_n(data, 263, 8));
        s.to_bow = ais_n(data, 271, 9);
        s.to_stern = ais_n(data, 80, 9);
        s.to_port = ais_n(data, 289, 6);
        s.to_starboard = ais_n(data, 295, 6);
        s.timestamp = millis();
    } else if(message_type == 24) {
        int part_num = ais_n(data, 38, 2);
        if(part_num == 0)
            s.name = ais_t(data, 40, 120);
        else if(part_num == 1) {
            s.shiptype = ais_e(ais_n(data, 40, 8));
            s.callsign = ais_t(data, 90, 42);
            s.to_bow = ais_n(data, 132, 9);
            s.to_stern = ais_n(data, 141, 9);
            s.to_port = ais_n(data, 150, 6);
            s.to_starboard = ais_n(data, 156, 6);
        }
    } else
        return false;

    return true;
}

// decode nmea ais messages and store ship information for display
bool ais_parse_line(const char *line, data_source_e source)
{
    //printf("ais_parse_line %s\n", line);
    int len = strlen(line);
    if(len < 10 || line[3] != 'V' || line[4] != 'D' || line[5] != 'M')
        return false;

    //printf("parse ais %s\n", line);
    int fragcount, fragindex;
    if(sscanf(line+6, ",%d,%d,", &fragcount, &fragindex) != 2)
        return false;

    const char *l = skip(line+6, 4);
    if(!l)
        return false;
    char channel = 'A';
    if(*l != ',')
        channel = *l;
    l = skip(l, 1);
    if(!l)
        return false;
    const char *e = skip(l, 1);
    if(!e)
        return false;

    len = e-l-1;
    if(len > 77)
        return false;

    std::vector<bool> &data = ais_data[channel == 'B'];
    for(int i=0; i < len; i++) {
        int d = l[i] - 48;
        if(d > 40)
            d -= 8;
        for(int b=5; b>=0; b--)
            if((1<<b) & d)
                data.push_back(1);
            else
                data.push_back(0);
    }

    if(fragindex == fragcount) {
        bool result = decode_ais_data(data);
        //printf("decode ais data %d %d\n", result, source);
        data.clear();
        if(result)
            display_data_update(AIS_DATA, 0, source);
        return result;
    }
    return false;
}

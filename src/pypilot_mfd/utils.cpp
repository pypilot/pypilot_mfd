/* Copyright (C) 2024 Sean D'Epagnier <seandepagnier@gmail.com>
 *
 * This Program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 */

 #include <math.h>

float deg2rad(float degrees) {
    return degrees / 180.0 * M_PI;
}

float rad2deg(float radians) {
    return radians * 180.0 / M_PI;
}

float nice_number(float v, int dir = 1)
{
    int i=0;
    while(v > 10) {
        v/= 10;
        i++;
    }
    if(dir > 0) {
        if(v < 1) v = 1;
        else if(v < 2) v = 2;
        else if(v < 3) v = 3;
        else if(v < 5) v = 5;
        else           v = 10;
    } else {
        if(v > 5) v = 5;
        else if(v > 3) v = 3;
        else if(v > 2) v = 2;
        else if(v > 1) v = 1;
        else           v = 0;

    }
    while(i) {
        v *= 10;
        i--;
    }
    return v;
}

float resolv(float angle)
{
    while(angle < -180)
        angle += 360;
    while(angle > 180)
        angle -= 360;
    return angle;
}

void distance_bearing(float lat1, float lon1, float lat2, float lon2, float *dist, float *brg)
{
    float latd = lat2-lat1;
    float lond = resolv(lon2-lon1);

    float x = cosf(deg2rad(lat1)) * lond * 60;
    float y = latd * 60;

    if(dist)
        *dist = hypotf(x, y);

    if(brg)
        *brg = rad2deg(atan2(y, x));
}
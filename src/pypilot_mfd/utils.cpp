/* Copyright (C) 2024 Sean D'Epagnier <seandepagnier@gmail.com>
 *
 * This Program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 */

#include <math.h>

#include "utils.h"

float deg2rad(float degrees) {
    return degrees / 180.0 * M_PI;
}

float rad2deg(float radians) {
    return radians * 180.0 / M_PI;
}

float nice_number(float v, int dir)
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

uint64_t mac_str_to_int(String mac)
{
    uint8_t data[8] = {0};
    if(sscanf(mac.c_str(), "%02x:%02x:%02x:%02x:%02x:%02x", data+0, data+1, data+2, data+3, data+4, data+5) == 6)
        return *(uint64_t*)data;
    return 0;
}

String mac_int_to_str(uint64_t i)
{
    uint8_t mac_addr[8] = {0};
    memcpy(mac_addr, &i, 6);
    char macStrt[18];
    snprintf(macStrt, sizeof(macStrt), "%02x:%02x:%02x:%02x:%02x:%02x",
             mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
    return macStrt;
}


void listDir(fs::FS &fs, const char * dirname, uint8_t levels){
    Serial.printf("Listing directory: %s\r\n", dirname);

    File root = fs.open(dirname);
    if(!root){
        Serial.println("- failed to open directory");
        return;
    }
    if(!root.isDirectory()){
        Serial.println(" - not a directory");
        return;
    }

    File file = root.openNextFile();
    while(file){
        if(file.isDirectory()){
            Serial.print("  DIR : ");
            Serial.println(file.name());
            if(levels){
                listDir(fs, file.name(), levels -1);
            }
        } else {
            Serial.print("  FILE: ");
            Serial.print(file.name());
            Serial.print("\tSIZE: ");
            Serial.println(file.size());
        }
        file = root.openNextFile();
    }
}
void readFile(fs::FS &fs, const char * path){
    Serial.printf("Reading file: %s\r\n", path);

    File file = fs.open(path);
    if(!file || file.isDirectory()){
        Serial.println("- failed to open file for reading");
        return;
    }

    Serial.println("- read from file:");
    while(file.available()){
        Serial.write(file.read());
    }
    file.close();
}

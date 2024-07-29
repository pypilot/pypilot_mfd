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

float resolv(float angle, float mid)
{
    while(angle < mid-180)
        angle += 360;
    while(angle > mid+180)
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

uint64_t mac_as_int(const uint8_t *mac_addr)
{
    uint64_t r = 0;
    for(int i=0; i<6; i++) {
        r<<=8;
        r |= mac_addr[i];
    }
    return r;
}

uint64_t mac_str_to_int(std::string mac)
{
    int data[6] = {0};
    if(sscanf(mac.c_str(), "%02x:%02x:%02x:%02x:%02x:%02x", data+0, data+1, data+2, data+3, data+4, data+5) != 6)
        return 0;

    uint8_t mac_addr[6];
    for(int i=0; i<6; i++)
        mac_addr[i] = data[i];

    return mac_as_int(mac_addr);
}

std::string mac_int_to_str(uint64_t r)
{
    uint8_t mac_addr[6] = {0};
    for(int i=0; i<6; i++) {
        mac_addr[5-i] = r & 0xff;
        r>>=8;
    }
    char macStrt[18];
    snprintf(macStrt, sizeof(macStrt), "%02x:%02x:%02x:%02x:%02x:%02x",
             mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
    return macStrt;
}

void listDir(fs::FS &fs, const char * dirname, uint8_t levels){
    printf("Listing directory: %s\r\n", dirname);

    File root = fs.open(dirname);
    if(!root){
        printf("- failed to open directory\n");
        return;
    }
    if(!root.isDirectory()){
        printf(" - not a directory\n");
        return;
    }

    File file = root.openNextFile();
    while(file){
        if(file.isDirectory()){
            printf("  DIR : %s\n", file.name());
            if(levels){
                listDir(fs, file.name(), levels -1);
            }
        } else {
            printf("  FILE: %s\tSIZE: %d\n", file.name(), file.size());
        }
        file = root.openNextFile();
    }
}

void readFile(fs::FS &fs, const char * path){
    printf("Reading file: %s\r\n", path);

    File file = fs.open(path);
    if(!file || file.isDirectory()){
        printf("- failed to open file for reading\n");
        return;
    }

    printf("- read from file:\n");
    while(file.available()){
        Serial.write(file.read());
    }
    file.close();
}

std::string millis_to_str(uint32_t dt)
{
    float t = dt/1000.0f;
    int parts[][2] = {{'d', 24}, {'h', 60}, {'m', 60}};
    std::string l;
    
    int count = (sizeof parts)/(sizeof *parts);
    for(int i=0; i<count; i++) {
        int d = parts[i][1];
        for(int j=i+1; j<count; j++)
            d *= parts[j][1];
        if(t < d)
            continue;
        int v = t / d;
        t -= d*v;
        l += float_to_str(v) + (char)parts[i][0] + ' ';       
    }

    return l + float_to_str(t, 1) + "s";
}

// Custom printf_P function for ESP32
void printf_P(const __FlashStringHelper* flashString, ...) {
    // Determine the length of the flash string
    size_t len = strlen_P((PGM_P)flashString);
    
    // Define buffer size
    const size_t bufferSize = 128; // Adjust the size as needed
    char buffer[bufferSize];

    // Check if the buffer is large enough
    if (len >= bufferSize) {
        Serial.println(F("Warning: Buffer size too small for flash string!"));
        return;
    }

    // Copy the flash string to the buffer
    strcpy_P(buffer, (PGM_P)flashString);

    // Initialize the variable argument list
    va_list args;
    va_start(args, flashString);

    // Use vprintf to print the formatted string
    vprintf(buffer, args);

    // Clean up the variable argument list
    va_end(args);
}

std::string float_to_str(float v, int digits)
{
    char buffer[32];
    if(digits == -1)
        snprintf(buffer, sizeof buffer, "%f", v);
    else
        snprintf(buffer, sizeof buffer, "%.*f", digits, v);
    return std::string(buffer);
}

std::string int_to_str(int v)
{
    char buffer[32];
        snprintf(buffer, sizeof buffer, "%d", v);
    return std::string(buffer);
}

int str_to_int(const std::string &s)
{
    int x;
    sscanf(s.c_str(), "%d", x);
    return x;
}

bool endsWith(const std::string& str, const std::string& suffix) {
    if (suffix.size() > str.size()) {
        return false;
    }
    return std::equal(suffix.rbegin(), suffix.rend(), str.rbegin());
}

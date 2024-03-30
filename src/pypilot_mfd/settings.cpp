/* Copyright (C) 2024 Sean D'Epagnier <seandepagnier@gmail.com>
 *
 * This Program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 */

#include "Arduino.h"
#include <Arduino_JSON.h>

//#include <EEPROM.h>
#include "utils.h"
#include "settings.h"

#define DEFAULT_CHANNEL 6
#define MAGIC "3A61CF00"

String settings_filename = "settings.json";



bool settings_load(String suffix)
{
    printf("settings load\n");
    if(!SPIFFS.begin(true)){
        Serial.println("SPIFFS Mount Failed");
        return false;
    }
    listDir(SPIFFS, "/", 0);

    // start with default settings
    settings.magic = MAGIC;
    settings.ssid = "pypilot";
    settings.psk = "";

    settings.channel = DEFAULT_CHANNEL;

    settings.input_usb = true;
    settings.output_usb = true;

    settings.usb_baud_rate = 38400;
    settings.rs422_baud_rate = 38400;

    settings.input_wifi = false;
    settings.output_wifi = false;
    settings.wifi_data = NMEA_PYPILOT;

    settings.nmea_client_port = 0;
    settings.nmea_client_addr = "";
    settings.nmea_server_port = 3600;

    settings.transmitter_count = 0;
    wind_transmitters.clear();

    settings.compensate_accelerometer = false;
    settings.use_fahrenheit = false;
    settings.use_depth_ft = false;

    settings.lat_lon_format = "minutes";
    settings.contrast = 20;
        
    settings.enabled_pages = "ABCD";

    settings.show_status = true;
    settings.landscape = false;

return true;

    /*
    EEPROM.begin(512);
    EEPROM.get(0, settings);

    int offset = sizeof settings;
    uint16_t count = settings.transmitter_count;
    if(count > 3)  // do not allow more than 3 wind sensors for now
        count = 3;
    for(int i=0; i<count; i++) {
      wind_transmitter_t transmitter;
        EEPROM.get(offset, transmitter);
        if(transmitter.magic == MAGIC) {
            transmitter.t = 0;
            uint64_t maci = mac_as_int(transmitter.mac);
            wind_transmitters[maci] = transmitter;
        }
        offset += sizeof(wind_transmitter_t);
    }
    EEPROM.end();
    */
    File file = SPIFFS.open(settings_filename + suffix);
    if(!file || file.isDirectory()){
        printf("failed to open '%s' for reading\n", settings_filename.c_str());
        return false;
    }

    String data;
    while(file.available())
        data += file.read();
    file.close();

    JSONVar s = JSON.parse(data);
    if(!s.hasOwnProperty("magic") || s["magic"] != MAGIC) {
        printf("settings file invalid/corrupted, will ignore data");
        return false;
    }

#define LOAD_SETTING_S(NAME)    if(s.hasOwnProperty(#NAME)) settings.NAME = JSON.stringify(s[#NAME]);
#define LOAD_SETTING(NAME)    if(s.hasOwnProperty(#NAME)) settings.NAME = s[#NAME];

#define LOAD_SETTING_BOUND(NAME, MIN, MAX) \
    LOAD_SETTING(NAME) \
    if(settings.NAME < MIN) settings.NAME = MIN; \
    if(settings.NAME < MAX) settings.NAME = MAX; \

    LOAD_SETTING_S(ssid)
    LOAD_SETTING_S(psk)
    LOAD_SETTING(input_usb)
    LOAD_SETTING(output_usb)
    LOAD_SETTING(usb_baud_rate)
    LOAD_SETTING(rs422_baud_rate)
    LOAD_SETTING(input_wifi)
    LOAD_SETTING(output_wifi)
    LOAD_SETTING_S(nmea_client_addr)
    LOAD_SETTING(nmea_client_port);
    LOAD_SETTING(nmea_server_port);

    LOAD_SETTING(compensate_accelerometer)

    //display
    LOAD_SETTING(use_fahrenheit)
    LOAD_SETTING(use_depth_ft)
    LOAD_SETTING_S(lat_lon_format)
    LOAD_SETTING_BOUND(contrast, 50, 100)
    LOAD_SETTING_BOUND(backlight, 30, 100)
    
    LOAD_SETTING(show_status)
    LOAD_SETTING(landscape)

    LOAD_SETTING_S(enabled_pages)

    // transmitters
    if(s.hasOwnProperty("transmitters")) {
        JSONVar t = s["transmitters"];
        JSONVar k = t.keys();
        for(int i=0; i<k.length(); i++) {
            String mac = k[i];
            wind_transmitter_t tr = {0};
            tr.position = (wind_position)(int)t["position"];
            tr.offset = (double)t["offset"];
            uint64_t maci = mac_str_to_int(mac);
            wind_transmitters[maci] = tr;
        }
    }
    return true;
}

bool settings_store(String suffix)
{
    /*
    EEPROM.begin(512);
    EEPROM.put(0, settings);
    EEPROM.commit();
    */
    
    JSONVar s;

#define STORE_SETTING(NAME)    s[#NAME] = settings.NAME;

    STORE_SETTING(ssid)
    STORE_SETTING(psk)
    STORE_SETTING(input_usb)
    STORE_SETTING(output_usb)
    STORE_SETTING(usb_baud_rate)
    STORE_SETTING(rs422_baud_rate)
    STORE_SETTING(input_wifi)
    STORE_SETTING(output_wifi)
    STORE_SETTING(nmea_client_addr)
        STORE_SETTING(nmea_client_port);
    STORE_SETTING(nmea_server_port);

    STORE_SETTING(compensate_accelerometer)

    //display
    STORE_SETTING(use_fahrenheit)
    STORE_SETTING(use_depth_ft)
    STORE_SETTING(lat_lon_format)
    STORE_SETTING(contrast)
    STORE_SETTING(backlight)
    
    STORE_SETTING(show_status)
    STORE_SETTING(landscape)

    STORE_SETTING(enabled_pages)

    // transmitters
    JSONVar transmitters;
    for(std::map<uint64_t, wind_transmitter_t>::iterator it = wind_transmitters.begin();
        it != wind_transmitters.end(); it++) {
        JSONVar t;
        wind_transmitter_t &tr = it->second;
        t["position"] = tr.position;
        t["offset"] = tr.offset;
        transmitters[mac_int_to_str(it->first)] = t;
    }
    s["transmitters"] = transmitters;

    File file = SPIFFS.open(settings_filename + suffix, "w");
    if(!file || file.isDirectory()){
        printf("failed to open '%s' for writing\n", settings_filename.c_str());
        return false;
    }

    String data = JSON.stringify(s);
    uint8_t *cdata = (uint8_t*)data.c_str();
    int len = data.length();
    file.write(cdata, len);
    file.close();
}

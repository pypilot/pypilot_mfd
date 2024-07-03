/* Copyright (C) 2024 Sean D'Epagnier <seandepagnier@gmail.com>
 *
 * This Program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 */

#include <esp_now.h>
#include <esp_wifi.h>
#include <WiFi.h>
#include <lwip/sockets.h>

#include <Wire.h>

#include "bmp280.h"
#include "zeroconf.h"
#include "web.h"
#include "display.h"
#include "nmea.h"
#include "signalk.h"
#include "pypilot_client.h"
#include "utils.h"

#include "settings.h"

void setup() {
    serial_setup();
    Serial.println("pypilot_mfd");

    // bmX280_setup();
    pinMode(2, INPUT_PULLUP);  // strap for display

    for (int i = 0; i < KEY_COUNT; i++)
        pinMode(key_pin[i], INPUT_PULLUP);

    wireless_setup();

    mdns_setup();
    web_setup();
    accel_setup();
    printf("web setup complete\n");
    //delay(200);  ///  remove this??

    //bmX280_setup();

    int ss = CONFIG_ARDUINO_LOOP_STACK_SIZE;
    if (ss < 16384)
        printf("WARNING STACK TOO SMALL\n");
    printf("Stack Size %d\n", ss);

    Serial.println("setup complete");

    accel_read();
    display_setup();
    printf("display setup complete\n");

    //esp_log_level_set("*", ESP_LOG_NONE);
}

void loop() {
    wireless_poll();
    read_keys();
    serial_read();
    //read_pressure_temperature();
    nmea_poll();
    signalk_poll();
    pypilot_client_poll();

    web_poll();

    static uint32_t last_render;
    if(t0-last_render > 200) {
        display_render();
        last_render = t0;
    }

    // sleep remainder of second
    int dt = millis() - t0;
    const int period = 50;
    if (dt < period) {
        //printf("delay %d\n", dt);
        delay(period - dt);
    }
}

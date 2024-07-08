/* Copyright (C) 2024 Sean D'Epagnier <seandepagnier@gmail.com>
 *
 * This Program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 */

#include "zeroconf.h"
#include "web.h"
#include "display.h"
#include "nmea.h"
#include "signalk.h"
#include "pypilot_client.h"
#include "serial.h"
#include "settings.h"
#include "wireless.h"
#include "accel.h"
#include "buzzer.h"
#include "utils.h"
#include "keys.h"
#include "alarm.h"


void setup() {
    serial_setup();
    Serial.println("pypilot_mfd");

    keys_setup();
    wireless_setup();

    mdns_setup();
    web_setup();
    accel_setup();
    printf("web setup complete\n");
    //delay(200);  ///  remove this??

    int ss = CONFIG_ARDUINO_LOOP_STACK_SIZE;
    if (ss < 16384)
        printf("WARNING STACK TOO SMALL\n");
    printf("Stack Size %d\n", ss);

    Serial.println("setup complete");

    alarm_setup();
    accel_read();
    display_setup();
    printf("display setup complete\n");

    //esp_log_level_set("*", ESP_LOG_NONE);
}

void loop()
{
    uint32_t t0 = millis();
    wireless_poll();
    keys_poll();
    serial_poll();
    nmea_poll();
    signalk_poll();
    pypilot_client_poll();
    alarm_poll();
    buzzer_poll();

    web_poll();

    display_poll();

    // sleep remainder of second
    int dt = millis() - t0;
    const int period = 50;
    if (dt < period) {
        //printf("delay %d\n", dt);
        delay(period - dt);
    }
}

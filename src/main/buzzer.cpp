/* Copyright (C) 2024 Sean D'Epagnier <seandepagnier@gmail.com>
 *
 * This Program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 */

#include "Arduino.h"

#include "settings.h"

static uint32_t buzzer_timeout;
static int buzzer_pattern;

void buzzer_buzz(int freq, int duration, int pattern)
{
    if(buzzer_timeout) {
        if(hw_version == 1)
            ledcDetach(4);
    }

    // buzzer (backlight uses channel 9)
    // buzzer is io pin 4
    if(hw_version == 1)
        ledcAttachChannel(4, freq, 10, 0); // channel 0, freq, 10bit

    buzzer_timeout = millis() + duration;
    buzzer_pattern = pattern;
}

void buzzer_poll()
{
    if(!buzzer_timeout)
        return;
    if(buzzer_timeout < millis()) {
        buzzer_timeout = 0;
        if(hw_version == 1)
            ledcDetach(4);
        return;
    }

    int d = 512; // always use 50% of 10 bit duty cycle
    if(hw_version == 1) {
        switch(buzzer_pattern) {
        case 0: ledcWriteChannel(0, d);                            break;  // always on
        case 1: ledcWriteChannel(0, millis() % 400 < 200 ? d : 0); break;  // on / off at 200ms
        case 2: ledcWriteChannel(0, millis() % 600 < 200 ? d : 0); break;  // on / off at 200ms
        case 3: ledcWriteChannel(0, millis() % 300 < 100 ? d : 0); break;  // on / off at 100ms
        }
    }
}

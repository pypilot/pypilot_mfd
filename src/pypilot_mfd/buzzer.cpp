/* Copyright (C) 2024 Sean D'Epagnier <seandepagnier@gmail.com>
 *
 * This Program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 */

#include <Arduino.h>

static uint32_t buzzer_timeout;
static int buzzer_pattern;

void buzzer_buzz(int freq, int duration, int pattern)
{
    // buzzer (backlight uses channel 0)
    ledcSetup(1, freq, 10); // channel 1, freq, 10bit
    ledcAttachPin(4, 1);

    buzzer_timeout = millis() + duration;
    buzzer_pattern = pattern;
}

void buzzer_poll()
{
    if(buzzer_timeout < millis()) {
        buzzer_timeout = 0;
        ledcDetachPin(4);
    }

    if(!buzzer_timeout)
        return;

    int d = 512; // always use 50% of 10 bit duty cycle
    switch(buzzer_pattern) {
    case 0: ledcWrite(1, 512);                            break;  // always on
    case 1: ledcWrite(1, millis() % 400 < 200 ? 512 : 0); break;  // on / off at 200ms
    case 2: ledcWrite(1, millis() % 600 < 200 ? 512 : 0); break;  // on / off at 200ms
    case 3: ledcWrite(1, millis() % 300 < 100 ? 512 : 0); break;  // on / off at 100ms
    }
}

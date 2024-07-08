/* Copyright (C) 2024 Sean D'Epagnier <seandepagnier@gmail.com>
 *
 * This Program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 */

#include "Arduino.h"

#include "settings.h"
#include "wireless.h"
#include "display.h"
#include "menu.h"
#include "keys.h"

enum keys { KEY_PAGE_UP,
            KEY_MENU,
            KEY_PAGE_DOWN,
            KEY_PWR,
            KEY_COUNT };
int key_pin[KEY_COUNT] = { KEY_UP_IO, 32, 33, 0 };

void keys_setup()
{
    for (int i = 0; i < KEY_COUNT; i++)
        pinMode(key_pin[i], INPUT_PULLUP);
}

void keys_poll() {
    static uint32_t key_time, timeout = 500;
    static int key_pressed;
    
    uint32_t t0 = millis();
    if (key_pressed) {
        if (t0 - key_time < timeout)  // allow new presses to process
            return;                     // ignore keys down until timeout
        key_pressed = 0;
        timeout = 200;  // faster repeat timeout
    }

    if (!digitalRead(key_pin[KEY_PAGE_UP])) {
        printf("KEY UP\n");
        if (!digitalRead(key_pin[KEY_PAGE_DOWN]))
          wireless_toggle_mode();
        else
          display_change_page(1);
    } else
    if (!digitalRead(key_pin[KEY_PAGE_DOWN])) {
        printf("KEY DOWN\n");
        display_change_page(-1);
    } else if (!digitalRead(key_pin[KEY_MENU])) {
        printf("KEY SCALE\n");
        if(key_pressed)
            menu_select();
        else
            display_menu_scale();
    } else if (!digitalRead(0)) {
        display_toggle();
        if(settings.powerdown) {
            while(!digitalRead(0)); // wait for button to release
            esp_sleep_enable_ext0_wakeup(GPIO_NUM_0, LOW);
            printf("going to sleep");
            esp_deep_sleep_start();
        }
    } else {
        timeout = 500;
        return;
    }

    key_pressed++;
    key_time = t0;
}

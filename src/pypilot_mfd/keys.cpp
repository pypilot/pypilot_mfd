/* Copyright (C) 2024 Sean D'Epagnier <seandepagnier@gmail.com>
 *
 * This Program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 */
 
enum keys { KEY_PAGE_UP,
            KEY_SCALE,
            KEY_PAGE_DOWN,
            KEY_PWR,
            KEY_COUNT };
int key_pin[KEY_COUNT] = { KEY_UP_IO, 32, 33, 0 };


static void read_keys() {
    static uint32_t key_time, timeout = 500;
    static bool key_pressed;
    uint32_t t0 = millis();
    if (key_pressed) {
        if (t0 - key_time < timeout)  // allow new presses to process
            return;                     // ignore keys down until timeout
        key_pressed = false;
        timeout = 300;  // faster repeat timeout
    }

    if (!digitalRead(key_pin[KEY_PAGE_UP])) {
        printf("KEY UP\n");
        
          if (!digitalRead(key_pin[KEY_PAGE_DOWN]))
          toggle_wifi_mode();
          else
          display_change_page(1);
        
    } else
    if (!digitalRead(key_pin[KEY_PAGE_DOWN])) {
        printf("KEY DOWN\n");
        display_change_page(-1);
    } else if (!digitalRead(key_pin[KEY_SCALE])) {
        printf("KEY SCALE\n");
        display_change_scale();
    } else if (!digitalRead(0)) {
        display_toggle();
    } else {
        timeout = 500;
        return;
    }

    key_pressed = true;
    key_time = t0;
}
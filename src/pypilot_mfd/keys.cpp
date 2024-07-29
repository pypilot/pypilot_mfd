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
#include "buzzer.h"

enum keys { KEY_PAGE_UP,
            KEY_MENU,
            KEY_PAGE_DOWN,
            KEY_PWR,
            KEY_COUNT };
int key_pin[KEY_COUNT] = { KEY_UP_IO, 32, 33, 0 };

static uint32_t key_times[KEY_COUNT];
uint32_t timeout = 500;
bool repeated;

static void IRAM_ATTR isr(void* arg) {
    int i = (int)arg;
    key_times[i] = millis();
}

void keys_setup()
{
    for (int i = 0; i < KEY_COUNT; i++) {
        pinMode(key_pin[i], INPUT_PULLUP);
        attachInterruptArg(key_pin[i], isr, (void*)i, FALLING);
    }
}

static bool keys[KEY_COUNT];
static void readkeys()
{
    for (int i = 0; i < KEY_COUNT; i++)
        keys[i] = !digitalRead(key_pin[i]);
}

static bool pressed(int key, bool repeat=true)
{
    if(keys[key]) {
        if(repeat && key_times[key] && millis() - key_times[key] > timeout) {
            repeated = true;
            key_times[key] += timeout;
            timeout = 300; // faster repeat
            return true;
        }
    } else {
        if(key_times[key]) {
            key_times[key] = 0;
            if(!repeated)
                return true;
            repeated = false;
            timeout = 500;
        }
    }
    return false;
}

static bool held(int key)
{
    if(!repeated && keys[key] && key_times[key] && millis() - key_times[key] > 500) {
        repeated = true;
        return true;
    }
    return false;
}

void keys_poll()
{
    readkeys();

    if(keys[KEY_PAGE_UP] && keys[KEY_PAGE_DOWN])
        wireless_toggle_mode();
    else if (pressed(KEY_PAGE_UP)) {
        printf("KEY UP\n");
        display_change_page(1);
    } else if (pressed(KEY_PAGE_DOWN)) {
        printf("KEY DOWN\n");
        display_change_page(-1);
    } else if (pressed(KEY_MENU, false)) {
        printf("KEY SCALE\n");
        display_menu_scale();
    } else if (held(KEY_MENU)) {
        printf("KEY MENU\n");
        if(in_menu)
            settings_store();
        buzzer_buzz(in_menu ? 1000 : 600, 50, 0);
        in_menu = !in_menu;
    } else if (pressed(KEY_PWR, false)) {
        display_toggle();
        menu_reset();
        if(settings.powerdown) {
            while(!digitalRead(0)); // wait for button to release
            esp_sleep_enable_ext0_wakeup(GPIO_NUM_0, LOW);
            printf("going to sleep");
            esp_deep_sleep_start();
        }
    } else if (held(KEY_PWR)) {
        ESP.restart();
    }
}

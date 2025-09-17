/* Copyright (C) 2024 Sean D'Epagnier <seandepagnier@gmail.com>
 *
 * This Program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 */

#include "Arduino.h"

#include "settings.h"
#include "display.h"
#include "draw.h"
#include "menu.h"
#include "keys.h"
#include "buzzer.h"
#include "extio.h"

enum keys { KEY_PAGE_UP,
            KEY_MENU,
            KEY_PAGE_DOWN,
            KEY_PWR,
            KEY_COUNT };

#ifdef CONFIG_IDF_TARGET_ESP32S3
int key_pin[KEY_COUNT] = { 5, 6, 7, 0 };
#else
int key_pin[KEY_COUNT] = { 33, 32, 35, 0 };
#endif

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

bool keys_pwr_pressed()
{
    bool ret = key_times[KEY_PWR];
    key_times[KEY_PWR] = 0;
    return ret;
}

static bool keys[KEY_COUNT];
static void readkeys()
{
    for (int i = 0; i < KEY_COUNT; i++)
        keys[i] = !digitalRead(key_pin[i]);
//    printf("read keys %d %d %d %d\n", keys[0], keys[1] ,keys[2], keys[3]);
//    printf("read key times %ld %ld %ld %ld\n", key_times[0], key_times[1] ,key_times[2], key_times[3]);
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
        //printf("KEY UP\n");
        display_change_page(1);
    } else if (pressed(KEY_PAGE_DOWN)) {
        //printf("KEY DOWN\n");
        display_change_page(-1);
    } else if (pressed(KEY_MENU, false)) {
        //printf("KEY SCALE\n");
        display_menu_scale();
    } else if (held(KEY_MENU)) {
        //printf("KEY MENU\n");
        if(in_menu)
            settings_store();
        buzzer_buzz(in_menu ? 1000 : 600, 50, 0);
        in_menu = !in_menu;
    } else if (pressed(KEY_PWR, false)) {
        printf("KEY PWR\n");
        bool on = display_toggle();
        menu_reset();
        if(!on) {
            //printf("Settings power button is %s\n", settings.power_button.c_str());
            draw_clear(false);
            if(settings.power_button != "screenoff") {
                while(!digitalRead(0)); // wait for button to release
                esp_sleep_enable_ext0_wakeup(GPIO_NUM_0, LOW);
                // wake up in 60 seconds                
                if(settings.power_button == "powersave")
                    esp_sleep_enable_timer_wakeup(60 * 1000 * 1000);
                printf("going to sleep\n");

                // led pin
                //            gpio_hold_en((gpio_num_t)EXTIO_LED_num);
                /*
                gpio_hold_dis((gpio_num_t)26);
                pinMode(26, OUTPUT);
                digitalWrite(26, 1);*/

                if(hw_version != 1) {
                    // disable nmea power
                    extio_clr(EXTIO_ENA_NMEA);
#if EXTIO_NMEA_num >= 0
                    gpio_hold_en((gpio_num_t)EXTIO_NMEA_num);
#endif
                    // disable power led
                    extio_clr(EXTIO_LED);
#if EXTIO_LED_num >= 0
                    gpio_hold_en((gpio_num_t)EXTIO_LED_num);
#endif
                }
                
                gpio_deep_sleep_hold_en();
                esp_deep_sleep_start();
            }
        }
    } else if (held(KEY_PWR))
        ESP.restart();
}

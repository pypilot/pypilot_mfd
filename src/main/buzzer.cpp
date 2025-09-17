/* Copyright (C) 2024 Sean D'Epagnier <seandepagnier@gmail.com>
 *
 * This Program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 */

#include "Arduino.h"
#include "driver/ledc.h"

#include "settings.h"

static uint32_t buzzer_timeout;
static int buzzer_pattern;

#define BUZZERA 4
#define BUZZERB 25


void buzzer_buzz(int freq, int duration, int pattern)
{
#if 0
    if(buzzer_timeout) {
        if(hw_version <= 2) {
            ledcDetach(BUZZERA);
            ledcDetach(BUZZERB);
        }
    }

    // buzzer (backlight uses channel 9)
    // buzzer is io pin BUZZERB
    if(hw_version <= 2) {
        ledcAttachChannel(BUZZERB, freq, 10, 0); // channel 0, freq, 10bit
    }
#else
    if(hw_version <= 2) {
        if(buzzer_timeout) {
            ESP_ERROR_CHECK(ledc_stop(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0));
            ESP_ERROR_CHECK(ledc_stop(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, 0));
        }
            
        ledc_timer_config_t ledc_timer = {
            .speed_mode       = LEDC_LOW_SPEED_MODE,
            .duty_resolution  = LEDC_TIMER_10_BIT,
            .timer_num        = LEDC_TIMER_0,
            .freq_hz          = (uint32_t)freq,
            .clk_cfg          = LEDC_AUTO_CLK
        };

        ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

        // Prepare and then apply the LEDC PWM channel configuration
        ledc_channel_config_t ledc_channel_a = {
            .gpio_num       = BUZZERA,
            .speed_mode     = LEDC_LOW_SPEED_MODE,
            .channel        = LEDC_CHANNEL_0,
            .intr_type      = LEDC_INTR_DISABLE,
            .timer_sel      = LEDC_TIMER_0,
            .duty           = 0, // Set duty to 0%
            .hpoint         = 0,
            .flags = 0,
        };
        ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel_a));
        ledc_channel_config_t ledc_channel_b = {
            .gpio_num       = BUZZERB,
            .speed_mode     = LEDC_LOW_SPEED_MODE,
            .channel        = LEDC_CHANNEL_1,
            .intr_type      = LEDC_INTR_DISABLE,
            .timer_sel      = LEDC_TIMER_0,
            .duty           = 0, // Set duty to 0%
            .hpoint         = 0,
            .flags = 0,
        };
        ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel_b));
    }
#endif

    buzzer_timeout = millis() + duration;
    buzzer_pattern = pattern;
}

void buzzer_poll()
{
    int d = settings.buzzer_volume;

    if(!buzzer_timeout || !d)
        return;
    if(buzzer_timeout < millis()) {
        buzzer_timeout = 0;
        if(hw_version <= 2) {
#if 0
            ledcDetach(BUZZERA);
            ledcDetach(BUZZERB);
#else
            ESP_ERROR_CHECK(ledc_stop(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0));
            ESP_ERROR_CHECK(ledc_stop(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, 0));
#endif
            
        }
        return;
    }

    d = 10+3*d*sqrtf(d);

    switch(buzzer_pattern) {
    case 0:                          break;  // always on
    case 1: if(millis() % 400 > 200) d = 0; break;
    case 2: if(millis() % 600 > 200) d = 0; break;
    case 3: if(millis() % 300 > 100) d = 0; break;
    }

    if(hw_version <= 2) {
#if 0        
#else
        ESP_ERROR_CHECK(ledc_set_duty_with_hpoint(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, d, 0));
        // Update duty to apply the new value
        ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0));

        if(d > 50) {
            ESP_ERROR_CHECK(ledc_set_duty_with_hpoint(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, d, d));
        // Update duty to apply the new value
            ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1));
        }
#endif
    }
}

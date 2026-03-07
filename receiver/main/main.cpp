/* Copyright (C) 2026 Sean D'Epagnier <seandepagnier@proton.me>
 *
 * This Program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 */

#include <stdio.h>
#include <inttypes.h>
#include <math.h>
#include <string>

#include <nvs_flash.h>
#include <sdkconfig.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/gpio.h>
#include <esp_chip_info.h>
#include <esp_flash.h>
#include <esp_system.h>
#include <esp_timer.h>

#include "Arduino.h"

#include "settings.h"
#include "wireless.h"
#include "display.h"
#include "serial.h"
#include "nmea.h"
#include "zeroconf.h"
#include "web.h"

extern "C" void vApplicationStackOverflowHook(TaskHandle_t, char *name) {
    printf("STACK OVERFLOW in %s\n", name ? name : "(unknown)");
    fflush(stdout);
    abort();
}

extern "C" void app_main(void)
{    
    ESP_ERROR_CHECK(nvs_flash_init());
    initArduino();

    uint64_t t0 = esp_timer_get_time();
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    printf("This is %s chip with %d CPU core(s), %s%s%s%s, ",
           CONFIG_IDF_TARGET,
           chip_info.cores,
           (chip_info.features & CHIP_FEATURE_WIFI_BGN) ? "WiFi/" : "",
           (chip_info.features & CHIP_FEATURE_BT) ? "BT" : "",
           (chip_info.features & CHIP_FEATURE_BLE) ? "BLE" : "",
           (chip_info.features & CHIP_FEATURE_IEEE802154) ? ", 802.15.4 (Zigbee/Thread)" : "");
    unsigned major_rev = chip_info.revision / 100;
    unsigned minor_rev = chip_info.revision % 100;
    printf("silicon revision v%d.%d, ", major_rev, minor_rev);
    uint32_t flash_size;
    if(esp_flash_get_size(NULL, &flash_size) != ESP_OK) {
        printf("Get flash size failed");
        return;
    }
    printf("%" PRIu32 "MB %s flash\n", flash_size / (uint32_t)(1024 * 1024),
           (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

    settings_load();
    t0 = esp_timer_get_time();
    printf("settings_load_done, %lld\n", t0);

    serial_setup();
    t0 = esp_timer_get_time();
    printf("serial_setup_done, %lld\n", t0);
    
    wireless_setup();
    t0 = esp_timer_get_time();
    printf("wireless_setup_done, %lld\n", t0);

    mdns_setup();
    t0 = esp_timer_get_time();
    printf("mdns_setup_done, %lld\n", t0);

    display_setup();
    t0 = esp_timer_get_time();
    printf("display_setup  %lld\n", t0);

    web_setup();
    t0 = esp_timer_get_time();
    printf("web_setup  %lld\n", t0);

    int dt=1;
    for(;;) {
        t0 = esp_timer_get_time();
        wireless_poll();
        //nmea_poll(); //needed??
        serial_poll();
        web_poll();
        display_poll();
        
        // sleep remainder
        dt = (esp_timer_get_time() - t0)/1000;
        const int period = 200; // for now 10hz loop (should this change?)
        if (dt < period) {
            //printf("delay %d\n", dt);
            vTaskDelay((period-dt) / portTICK_PERIOD_MS);
        }
    }
}

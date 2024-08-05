/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include <inttypes.h>
#include <string>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_system.h"

#include "Arduino.h"

#include "zeroconf.h"
#include "web.h"
#include "display.h"
#include "draw.h"
#include "nmea.h"
#include "signalk.h"
#include "pypilot_client.h"
#include "serial.h"
#include "settings.h"
#include "wireless.h"
#include "accel.h"
#include "menu.h"
#include "buzzer.h"
#include "utils.h"
#include "keys.h"
#include "alarm.h"
#include "history.h"

#define DEMO

extern "C" void app_main(void)
{
    initArduino();

    // Arduino-like setup()
//    delay(1500);
//    serial_setup();
    printf("pypilot_mfd\n");

    /* Print chip information */
    esp_chip_info_t chip_info;
    uint32_t flash_size;
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
    if(esp_flash_get_size(NULL, &flash_size) != ESP_OK) {
        printf("Get flash size failed");
        return;
    }

    printf("%" PRIu32 "MB %s flash\n", flash_size / (uint32_t)(1024 * 1024),
           (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

    printf("Minimum free heap size: %" PRIu32 " bytes\n", esp_get_minimum_free_heap_size());

#ifdef DEMO
    draw_setup(2);
#else
    accel_setup();
    keys_setup();
    settings_load();
    wireless_setup();
    history_setup();
    mdns_setup();
    web_setup();

    int ss = CONFIG_ARDUINO_LOOP_STACK_SIZE;
    if (ss < 16384)
        printf("WARNING STACK TOO SMALL\n");
    printf("Stack Size %d\n", ss);

    alarm_setup();
    accel_read();
    display_setup();
    menu_setup();
    printf("display setup complete in %ld, %ld\n", t0, millis()-t0);
#endif

    int r = 60, rd = 1;
    int dt=1;
    for(;;) {
        uint32_t t0 = millis();
#ifdef DEMO
        draw_clear(true);

        draw_color(WHITE);
        draw_box(50, 0,700, 470);
#if 0
        draw_color(RED);
        draw_box(100, 400, 349, 60);

        draw_color(CYAN);
        int ht=80;
        draw_set_font(ht);
        draw_text(0, 360, "Hello world!");


        ht = 100;
        draw_color(BLUE);
        draw_set_font(ht);
        char b[128];
        sprintf(b, "%d", (int)(1000/dt));
        draw_text(200, 0, b);

        
        draw_color(GREEN);
    draw_thick_line(100, 100, 700, 380, 20);

        draw_color(GREY);
        draw_thick_line(0, 480, 800, 0, 10);
        

        draw_color(ORANGE);
        draw_circle(300, 300, r, 10);

        if(r > 100 || r < 20)
            rd=-rd;
        r += rd;
        draw_color(MAGENTA);
        draw_triangle(576+r, 100, 480, 300+r, 780, 250);
#endif
        draw_send_buffer();

#else
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
        history_poll();
#endif        
        // sleep remainder of second
        dt = millis() - t0;

        const int period = 50;
        if (dt < period) {
            //printf("delay %d\n", dt);
            //delay(period - dt);
            vTaskDelay((period-dt) / portTICK_PERIOD_MS);
        }
    }
}

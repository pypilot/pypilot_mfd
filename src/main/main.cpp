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
#include <esp_ota_ops.h>

///
#include "driver/i2c.h"
///

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
#include "accel.h"
#include "menu.h"
#include "buzzer.h"
#include "utils.h"
#include "keys.h"
#include "alarm.h"
#include "history.h"
#include "extio.h"


extern "C" void app_main(void)
{    
#ifndef CONFIG_IDF_TARGET_ESP32S3
                // gpio4 is strap by default
                gpio_set_pull_mode(GPIO_NUM_4, GPIO_FLOATING);
#endif

    initArduino();
 
    // Arduino-like setup()
    extio_setup();
    extio_set(EXTIO_LED, true);

    uint32_t t0 = millis();

    printf("pypilot_mfd, %ld\n", millis());

    const esp_partition_t *partition = esp_ota_get_running_partition();
    printf("Currently running partition: %s\r\n", partition->label);
    
    /* Print chip information */
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

    accel_setup();
    printf("accel_setup_done, %ld\n", millis());
    
    settings_load();
    printf("settings_load_done, %ld\n", millis());
    
    serial_setup();

    wireless_setup();
    printf("wireless_setup_done, %ld\n", millis());

    history_setup();
    printf("history_setup_done, %ld\n", millis());

    alarm_setup();
    printf("alarm_setup %ld\n", millis());

    keys_setup();
    printf("keys_setup_done, %ld\n", millis());

    // if we woke up to log data (power save) just do that for 5 seconds
    // then go back to sleep
    if(esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER &&
       settings.power_button == "powersave") {
        printf("woke into power save loop\n");
        while(!keys_pwr_pressed()) {
            serial_poll();
            wireless_poll(); // receive sensor data
            history_poll();
            alarm_poll();
            vTaskDelay(100 / portTICK_PERIOD_MS);
            
            if(millis() > 3000) { // pwr not pressed, go back to sleep
                printf("going back to sleep\n");
                esp_sleep_enable_ext0_wakeup(GPIO_NUM_0, LOW);
                esp_sleep_enable_timer_wakeup(60 * 1000 * 1000);
                esp_deep_sleep_start();
            }
        }
        //while(!digitalRead(0)); // wait for button to release
    }
    
    mdns_setup();
    printf("mdns_setup_done, %ld\n", millis());

    web_setup();
    printf("web_setup_done, %ld\n", millis());

    int ss = CONFIG_ARDUINO_LOOP_STACK_SIZE;
    if (ss < 16384)
        printf("WARNING STACK TOO SMALL, %ld\n", millis());
    printf("Stack Size %d\n", ss);
    
    accel_read();
    display_setup();
    printf("display_setup  %ld\n", millis());
    
    menu_setup();
    printf("menu_setup  %ld\n", millis());

    printf("Minimum free heap size: %" PRIu32 " bytes\n", esp_get_minimum_free_heap_size());

    extio_set(EXTIO_LED, false);

    int dt=1;
    for(;;) {
        uint32_t t1 = millis();
        wireless_poll();
        extio_poll();
        keys_poll();
        serial_poll();
        nmea_poll();

//        signalk_poll();
        pypilot_client_poll();
        alarm_poll();
        buzzer_poll();
        web_poll();

        display_poll();
        history_poll();
        // sleep remainder of second

//        vTaskDelay(1);
        dt = millis() - t1;
//        printf("dt %d\n", dt);

        const int period = 50;
        if (dt < period) {
            //printf("delay %d\n", dt);
            //delay(period - dt);
            vTaskDelay((period-dt) / portTICK_PERIOD_MS);
        }
    }
}

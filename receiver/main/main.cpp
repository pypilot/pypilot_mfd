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
#include "dns_server.h"

#include "settings.h"
#include "wireless.h"
#include "display.h"
#include "serial.h"
#include "nmea.h"
#include "zeroconf.h"
#include "web.h"
#include "../../sensors/common/dhcp.h"

/*
  test unlock -
  unlock from serial console -
 */

#define TAG "main"

extern "C" void vApplicationStackOverflowHook(TaskHandle_t, char *name) {
    ESP_LOGE(TAG, "STACK OVERFLOW in %s", name ? name : "(unknown)");
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
    unsigned major_rev = chip_info.revision / 100;
    unsigned minor_rev = chip_info.revision % 100;
    ESP_LOGI(TAG, "This is %s chip with %d CPU core(s), %s%s%s%s, silicon revision v%d.%d, ",
             CONFIG_IDF_TARGET,
             chip_info.cores,
             (chip_info.features & CHIP_FEATURE_WIFI_BGN) ? "WiFi/" : "",
             (chip_info.features & CHIP_FEATURE_BT) ? "BT" : "",
             (chip_info.features & CHIP_FEATURE_BLE) ? "BLE" : "",
             (chip_info.features & CHIP_FEATURE_IEEE802154) ? ", 802.15.4 (Zigbee/Thread)" : "",
             major_rev, minor_rev);
    uint32_t flash_size;
    if(esp_flash_get_size(NULL, &flash_size) != ESP_OK) {
        ESP_LOGE(TAG, "Get flash size failed");
        return;
    }
    ESP_LOGI(TAG, "%" PRIu32 "MB %s flash", flash_size / (uint32_t)(1024 * 1024),
             (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

    settings_load();
    t0 = esp_timer_get_time();
    ESP_LOGI(TAG, "settings_load_done, %lld", t0);

    serial_setup();
    t0 = esp_timer_get_time();
    ESP_LOGI(TAG, "serial_setup_done, %lld", t0);
    
    wireless_setup();
    t0 = esp_timer_get_time();
    ESP_LOGI(TAG, "wireless_setup_done, %lld", t0);

    mdns_setup();
    t0 = esp_timer_get_time();
    ESP_LOGI(TAG, "mdns_setup_done, %lld", t0);

    display_setup();
    t0 = esp_timer_get_time();
    ESP_LOGI(TAG, "display_setup  %lld", t0);

    web_setup();
    t0 = esp_timer_get_time();
    ESP_LOGI(TAG, "web_setup  %lld", t0);

    //dns_server_config_t config = DNS_SERVER_CONFIG_SINGLE("*" /* all A queries */, "WIFI_AP_DEF" /* softAP netif ID */);
    //start_dns_server(&config);
    dhcp_set_captiveportal_url();
    t0 = esp_timer_get_time();
    ESP_LOGI(TAG, "dhcp_set_captiveportal  %lld", t0);
    
    int dt=1;
    for(;;) {
        t0 = esp_timer_get_time();
        wireless_poll();
        nmea_poll();
        serial_poll();
        web_poll();
        display_poll();
        
        // sleep remainder
        dt = (esp_timer_get_time() - t0)/1000;
        const int period = 200; // for now 10hz loop (should this change?)
        if (dt < period) {
            //ESP_LOGI(TAG, "delay %d", dt);
            vTaskDelay((period-dt) / portTICK_PERIOD_MS);
        }
    }
}

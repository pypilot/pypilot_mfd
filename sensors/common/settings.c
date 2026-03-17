/* Copyright (C) 2026 Sean D'Epagnier <seandepagnier@gmail.com>
 *
 * This Program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 */

#include "nvs_flash.h"

char wifi_psk[64];
uint8_t wifi_channel;
uint8_t output_rate;

void read_settings()
{
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK( nvs_flash_erase() );
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK( ret );

    nvs_handle_t nvs;
    esp_err_t res = nvs_open("SETTINGS", NVS_READWRITE, &nvs);
    if (res != ESP_OK) {
        printf("Unable to open NVS namespace: %d", res);
        return;
    }

    size_t len = 0;
    wifi_psk[0] = 0;
    res = nvs_get_str(nvs, "psk", 0, &len);
    if (res != ESP_OK && res != ESP_ERR_NVS_NOT_FOUND)
        printf("Unable to read NVS key: %d", res);
    else if(len < sizeof wifi_psk - 1) {
        res = nvs_get_str(nvs, "psk", wifi_psk, &len);
        if (res != ESP_OK && res != ESP_ERR_NVS_NOT_FOUND)
            printf("Unable to read NVS key: %d", res);
        else if(len < sizeof wifi_psk - 1)
            wifi_psk[len] = 0;
    }

    res = nvs_get_u8(nvs, "channel", &wifi_channel);
    if (res != ESP_OK && res != ESP_ERR_NVS_NOT_FOUND)
        printf("Unable to read NVS key: %d", res);
    if(wifi_channel != 6 && wifi_channel != 11)
        wifi_channel = 1;

    res = nvs_get_u8(nvs, "output_rate", &output_rate);
    if (res != ESP_OK && res != ESP_ERR_NVS_NOT_FOUND)
        printf("Unable to read NVS key: %d", res);
    if(output_rate != 1 && output_rate != 2 && output_rate != 5)
        output_rate = 10;
    
    nvs_close(nvs);
}

esp_err_t write_settings(const char *psk, int channel, int rate)
{
    nvs_handle_t nvs;
    esp_err_t err = nvs_open("SETTINGS", NVS_READWRITE, &nvs);
    if (err != ESP_OK)
        return err;

    err = nvs_set_str(nvs, "psk", psk);
    if (err == ESP_OK) err = nvs_set_u8(nvs, "channel", channel);
    if (err == ESP_OK) err = nvs_set_u8(nvs, "output_rate", rate);
    if (err == ESP_OK) err = nvs_commit(nvs);

    nvs_commit(nvs);
    nvs_close(nvs);
    return err;
}

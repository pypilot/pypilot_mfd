/* Copyright (C) 2026 Sean D'Epagnier <seandepagnier@gmail.com>
 *
 * This Program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_timer.h>

#include "dns_server.h"

#include "settings.h"
#include "web.h"

uint64_t wireless_ap_enabled;
dns_server_handle_t dns_handle;

// Global copy of chip
esp_now_peer_info_t chip;

#define UNLOCK_ID 0x1B21
typedef struct {
    uint16_t id;             // packet identifier
} packet_unlock_t;

#if 0
uint16_t crc16(const uint8_t *data_p, int length)
{
    uint8_t x;
    uint16_t crc = 0xFFFF;

    while (length--) {
        x = crc >> 8 ^ *data_p++;
        x ^= x >> 4;
        crc = (crc << 8) ^ ((uint16_t)(x << 12)) ^ ((uint16_t)(x << 5)) ^ ((uint16_t)x);
    }
    return crc;
}
#endif

static void reboot()
{
    printf("Reset ESP\n");
    vTaskDelay(30);
    abort();
}

void enable_ap_poll(void)
{
    // timeout after 5 minutes and disable ap again
    if(!wireless_ap_enabled || esp_timer_get_time() - wireless_ap_enabled < 300e6)
        return;

    printf("disabling AP\n");
    wireless_ap_enabled = 0;

    web_stop();

    stop_dns_server(dns_handle);

    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
}

void enable_ap(int enable_psk)
{
    if(wireless_ap_enabled) {
        wireless_ap_enabled = esp_timer_get_time();
        return;
    }
    wireless_ap_enabled = esp_timer_get_time();

    printf("enabling AP");

    wifi_config_t ap_cfg = {
        .ap = {
            .ssid = "wind_tx",
            .password = "",
            .ssid_len = 0,
            .channel = wifi_channel,
            .authmode = WIFI_AUTH_WPA2_PSK,
            .ssid_hidden = 0,
            .max_connection = 4,
            .beacon_interval = 100,
            .csa_count = 3,
            .dtim_period = 1,
            .pairwise_cipher = WIFI_CIPHER_TYPE_CCMP,
            .ftm_responder = 0,
            .pmf_cfg = {.capable = false, .required = false},
            .sae_pwe_h2e = WPA3_SAE_PWE_UNSPECIFIED,
            .transition_disable = 0,
            .sae_ext = 0,
            .bss_max_idle_cfg = {.period = 0, .protected_keep_alive = false},
            .gtk_rekey_interval = 0
        }
    };

    uint8_t my_mac[6];
    ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_IF_STA, my_mac));

    char macstr[16];
    snprintf(macstr, sizeof macstr, " %02x%02x", my_mac[0], my_mac[1]);

    strncat((char*)ap_cfg.ap.ssid, macstr, sizeof ap_cfg.ap.ssid - 1);
    
    if (strlen((char *)wifi_psk) >= 8 && enable_psk)
        memcpy(ap_cfg.ap.password, wifi_psk, sizeof ap_cfg.ap.password);
    else
        ap_cfg.ap.authmode = WIFI_AUTH_OPEN;
        
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_cfg));
    //    printf("using channel %d\n", wifi_channel);
    web_setup();

    // Start the DNS server that will redirect all queries to the softAP IP
    dns_server_config_t dns_config = DNS_SERVER_CONFIG_SINGLE("*" /* all A queries */, "WIFI_AP_DEF" /* softAP netif ID */);
    dns_handle = start_dns_server(&dns_config);
}

static void on_espnow_data(const esp_now_recv_info_t *recv_info, const uint8_t *data, int data_len)
{
    uint8_t my_mac[6];
    ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_IF_STA, my_mac));

    // not for me
    if (memcmp(recv_info->des_addr, my_mac, 6))
        return;
    
    if(data_len != sizeof(packet_unlock_t)) {
        printf("wrong packet size\n");
        return;
    }

    packet_unlock_t *packet = (packet_unlock_t*)data;
    if(packet->id != UNLOCK_ID)
        printf("ID mismatch\n");

    enable_ap(1);
}

static TaskHandle_t send_task;
static void on_espnow_send(const wifi_tx_info_t *info, esp_now_send_status_t status)
{
    vTaskNotifyGiveFromISR(send_task, 0);
}

void wireless_wait_sent() {
    ulTaskNotifyTake(pdTRUE, 1);
}

static void espnow_init(void)
{
    send_task = xTaskGetCurrentTaskHandle();
    /* Initialize ESPNOW and register sending and receiving callback function. */
    //esp_wifi_set_channel(chip.channel, WIFI_SECOND_CHAN_NONE);
    if (esp_now_init() == ESP_OK) {
        printf("ESPNow Init Success\n");
    } else {
        printf("ESPNow Init Failed\n");
        reboot();
    }

    ESP_ERROR_CHECK( esp_now_register_recv_cb(on_espnow_data) );
    ESP_ERROR_CHECK( esp_now_register_send_cb(on_espnow_send) );
#if 0
#if CONFIG_ESPNOW_ENABLE_POWER_SAVE
    ESP_ERROR_CHECK( esp_now_set_wake_window(CONFIG_ESPNOW_WAKE_WINDOW) );
    ESP_ERROR_CHECK( esp_wifi_connectionless_module_set_wake_interval(CONFIG_ESPNOW_WAKE_INTERVAL) );
#endif
#endif
    ESP_ERROR_CHECK( esp_now_add_peer(&chip) );
}

/* WiFi should start before using ESPNOW */
void wifi_init(void)
{
    if(wifi_channel != 6 && wifi_channel != 11)
        wifi_channel = 1; // allow only 1, 6, and 11
    
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_create_default_wifi_ap();
    
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_start());
    ESP_ERROR_CHECK( esp_wifi_set_channel(wifi_channel, WIFI_SECOND_CHAN_NONE));

    int mapped_tx_power = 80;
    switch(tx_power) {
    case 8:  mapped_tx_power = 34; break;
    case 13: mapped_tx_power = 52; break;
    case 16: mapped_tx_power = 66; break;
    case 20: mapped_tx_power = 80; break;
    }
    ESP_ERROR_CHECK( esp_wifi_set_max_tx_power(mapped_tx_power) );

    memset(&chip, 0, sizeof(chip));
    for(int ii=0; ii<6; ii++)
        chip.peer_addr[ii] = 0xff;
    chip.ifidx = WIFI_IF_STA;
    chip.channel = wifi_channel;
    chip.encrypt = 0;        // no encryption
    
    espnow_init();
}

// send data
void wireless_send_data(uint8_t *packet, int len)
{
    const uint8_t *peer_addr = chip.peer_addr;
    esp_err_t result = esp_now_send(peer_addr, packet, len);

    if (result == ESP_OK) {
        //printf("Success\n");
        return;
    } else if (result == ESP_ERR_ESPNOW_NOT_INIT)
        // How did we get so far!!
        printf("ESPNOW not Init.\n");
    else if (result == ESP_ERR_ESPNOW_ARG)
       printf("Invalid Argument\n");
    else if (result == ESP_ERR_ESPNOW_INTERNAL)
        printf("Internal Error\n");
    else if (result == ESP_ERR_ESPNOW_NO_MEM)
        printf("ESP_ERR_ESPNOW_NO_MEM\n");
    else if (result == ESP_ERR_ESPNOW_NOT_FOUND)
        printf("Peer not found.\n");
    else if (result == ESP_ERR_ESPNOW_IF)
        printf("Interface mismatch / interface disabled\n");
    else
        printf("Not sure what happened: %d\n", result);

    reboot();
}

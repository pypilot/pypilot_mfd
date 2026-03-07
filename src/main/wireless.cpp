/* Copyright (C) 2026 Sean D'Epagnier <seandepagnier@gmail.com>
 *
 * This Program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 */

#include <string.h>
#include <math.h>

#include <map>

#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_timer.h>
#include <esp_log.h>

#include "settings.h"
#include "wireless.h"
#include "display.h"
#include "zeroconf.h"
#include "sensors.h"
#include "utils.h"

#define TAG "wireless"

// Global copy of chip
esp_now_peer_info_t chip;

#define MAX_DATA_LEN 16

// crc is not really needed with espnow, however it adds a layer of protection against code changes
#define WIND_ID 0xf179

struct wind_packet_t {
    uint16_t id;
    uint16_t angle;
    uint16_t period;  // period of cups rotation in 100uS
    uint16_t accelx, accely;
    uint16_t crc16;
} __attribute__((packed));

#define AIR_ID 0xa41b
struct air_packet_t {
    uint16_t id;
    int16_t pressure;       // in pascals + 100,000,  eg 0 is 1000mbar,  1 is 1000.01 mbar
    int16_t temperature;    // in 100th/deg C
    uint16_t rel_humidity;  // in 100th %
    uint16_t air_quality;   // in 100th %
    uint16_t reserved;
    uint16_t battery_voltage; // in 100th of volts
    uint16_t crc16;
} __attribute__((packed));

#define WATER_ID 0xc946
struct water_packet_t {
    uint16_t id;
    uint16_t speed;        // in 100th knot
    uint16_t depth;        // in 100th meter
    uint16_t temperature;  // in 100th/deg C
    uint16_t crc16;
} __attribute__((packed));

#define LIGHTNING_UV_ID 0xd255
struct lightning_uv_packet_t {
    uint16_t id;
    uint16_t distance;
    uint16_t uva, uvb, uvc;
    uint16_t visible;
    uint16_t crc16;
} __attribute__((packed));

#define CHANNEL_ID 0x0a21
struct packet_channel_t {
    uint16_t id;  // packet identifier
    uint16_t channel;
    uint16_t crc16;
} __attribute__((packed));

static void connect_handler(void* arg, esp_event_base_t event_base,
                            int32_t event_id, void* event_data)
{
    printf("Wifi Connected\n");
}

bool wifi_connected;

esp_event_handler_instance_t instance;
static void wifi_event_handler(void *arg,
                               esp_event_base_t event_base,
                               int32_t event_id,
                               void *event_data)
{
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
        case WIFI_EVENT_STA_START:
            ESP_LOGI(TAG, "STA start, connecting...");
            esp_wifi_connect();
            break;

        case WIFI_EVENT_STA_CONNECTED:
            ESP_LOGI(TAG, "STA connected to AP");
            wifi_connected = true;
            break;

        case WIFI_EVENT_STA_DISCONNECTED:
            ESP_LOGI(TAG, "STA disconnected, reconnecting...");
            wifi_connected = false;
            esp_wifi_connect();
            break;

        default:
            break;
        }
    } else if (event_base == IP_EVENT) {
        switch (event_id) {
        case IP_EVENT_STA_GOT_IP: {
            ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
            ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
            break;
        }
        default:
            break;
        }
    }
}

static bool wifi_initialized = false;

static void wifi_global_init(void)
{
    if (wifi_initialized)
        return;

    ESP_ERROR_CHECK(esp_netif_init());

    esp_err_t err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE)
        ESP_ERROR_CHECK(err);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

    ESP_ERROR_CHECK(esp_event_handler_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL));

    ESP_ERROR_CHECK(esp_event_handler_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, connect_handler, NULL));

    esp_netif_create_default_wifi_sta();
    esp_netif_create_default_wifi_ap();

    wifi_initialized = true;
}

static void setup_wifi(void)
{
    wifi_global_init();

#ifndef CONFIG_IDF_TARGET_ESP32S3
    force_wifi_ap_mode = 1;
#endif

    ESP_ERROR_CHECK(esp_wifi_stop());

    if (force_wifi_ap_mode || settings.wifi_mode == WIFI_MODE_AP) {
        wifi_config_t ap_cfg = {
            .ap = {
#ifndef CONFIG_IDF_TARGET_ESP32S3
                .ssid = "wind_receiver",
#else
                .ssid = settings.ap_ssid.c_str(),
#endif
                .password = "", // settings.ap_psk.c_str()  TODO: fix this after testing
                .ssid_len = 0,
                .channel = 1, // settings.wifi_channel  TODO: fix this after testing
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

        if (strlen((char *)ap_cfg.ap.password) == 0)
            ap_cfg.ap.authmode = WIFI_AUTH_OPEN;

        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_cfg));
        ESP_ERROR_CHECK(esp_wifi_start());
    } else if (settings.wifi_mode == WIFI_MODE_STA && !settings.client_ssid.empty()) {
        wifi_config_t sta_cfg = {};
        strncpy((char *)sta_cfg.sta.ssid,
                settings.client_ssid.c_str(),
                sizeof(sta_cfg.sta.ssid) - 1);
        strncpy((char *)sta_cfg.sta.password,
                settings.client_psk.c_str(),
                sizeof(sta_cfg.sta.password) - 1);

        sta_cfg.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
        sta_cfg.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;
        sta_cfg.sta.threshold.authmode = WIFI_AUTH_OPEN;
        sta_cfg.sta.channel = 0;

        if (settings.wifi_channel > 0) {
            sta_cfg.sta.channel = settings.wifi_channel;
            sta_cfg.sta.scan_method = WIFI_FAST_SCAN;

            wifi_country_t myWiFi = {
                .cc = {'X', 'X', 'X'},
                .schan = settings.wifi_channel,
                .nchan = 1,
                .max_tx_power = 0,
                .policy = WIFI_COUNTRY_POLICY_MANUAL
            };
            ESP_ERROR_CHECK(esp_wifi_set_country(&myWiFi));
        }

        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_cfg));
        ESP_ERROR_CHECK(esp_wifi_start());
        ESP_ERROR_CHECK(esp_wifi_connect());
    } else {
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_start());
        esp_wifi_disconnect();
    }

    signalk_discovered = 0;
    pypilot_discovered = 0;
}

static uint16_t crc16(const uint8_t *data_p, int length) {
    uint8_t x;
    uint16_t crc = 0xFFFF;

    while (length--) {
        x = crc >> 8 ^ *data_p++;
        x ^= x >> 4;
        crc = (crc << 8) ^ ((uint16_t)(x << 12)) ^ ((uint16_t)(x << 5)) ^ ((uint16_t)x);
    }
    return crc;
}

int wireless_message_count;

struct esp_now_data_t {
    uint8_t mac[6];
    int len;
    uint8_t data[MAX_DATA_LEN];
} esp_now_rb[256];

volatile uint8_t rb_start, rb_end;
volatile SemaphoreHandle_t esp_now_sem;

static void DataRecvWind(struct esp_now_data_t &data) {
    if (data.len != sizeof(wind_packet_t)) {
        printf("received invalid wind packet %d\n", data.len);
        return;
    }

    uint16_t crc = crc16(data.data, data.len - 2);
    wind_packet_t *packet = (wind_packet_t *)data.data;
    if (crc != packet->crc16) {
        printf("espnow wind data packet crc failed %x %x\n", crc, packet->crc16);
        return;
    }

    float dir;
    if (packet->angle == 0xffff || packet->angle > 16384)
        dir = NAN;  // invalid
    else
        dir = 360 - packet->angle * 360.0f / 16384.0f;

    if (packet->period > 100) { // period of 100uS cups is about 193 knots for 100 rotations/s
        //        knots = 19350.0 / packet->period;
        float knots = 25800.0 / packet->period;  // corrected from preliminary calibration
        wind_knots = knots * .3 + wind_knots * .7;
    } else
        wind_knots = 0;

    float accel_x = packet->accelx * 4.0f / 16384 * 9.8f * 1.944f;
    float accel_y = packet->accely * 4.0f / 16384 * 9.8f * 1.944f;
    if (settings.compensate_wind_with_accelerometer && dir >= 0) {
        uint64_t t = esp_timer_get_time();
        static int lastt;
        float dt = (t - lastt) / 1e6;
        lastt = t;

        // TODO: combine inertial sensors from autopilot somehow?
        static float vx, vy;
        vx += accel_x * dt;
        vy += accel_y * dt;

        vx *= .9;  // decay

        float wvx = wind_knots * cos(deg2rad(dir));
        float wvy = wind_knots * sin(deg2rad(dir));

        wvx -= vx;
        wvy -= vy;

        wind_knots = hypotf(wvx, wvy);
        dir = rad2deg(atan2f(wvx, wvy));
        if (dir < 0)
            dir += 360;
    }

    //    uint32_t t1 = millis();
    sensors_wind_update(data.mac, dir, wind_knots, accel_x, accel_y);
}


static void DataRecvAir(struct esp_now_data_t &data) {
    if (data.len != sizeof(air_packet_t))
        printf("received invalid air packet %d\n", data.len);

    uint16_t crc = crc16(data.data, data.len - 2);
    air_packet_t *packet = (air_packet_t *)data.data;
    if (crc != packet->crc16) {
        printf("espnow air data packet crc failed %x %x\n", crc, packet->crc16);
        return;
    }

    sensors_air_update(data.mac, 1.0f + packet->pressure * .00001f, packet->rel_humidity * .01f, packet->temperature * .01f, packet->air_quality * .01f, packet->battery_voltage * .01f);
}

static void DataRecvWater(struct esp_now_data_t &data) {
    if (data.len != sizeof(water_packet_t))
        printf("received invalid water packet %d\n", data.len);

    uint16_t crc = crc16(data.data, data.len - 2);
    water_packet_t *packet = (water_packet_t *)data.data;
    if (crc != packet->crc16) {
        printf("espnow air data packet crc failed %x %x\n", crc, packet->crc16);
        return;
    }

    sensors_water_update(data.mac, packet->speed * .01f, packet->depth * .01f, packet->temperature * .01f);
}

static void DataRecvLightningUV(struct esp_now_data_t &data) {
    if (data.len != sizeof(lightning_uv_packet_t))
        printf("received invalid lightning UV packet %d\n", data.len);

    uint16_t crc = crc16(data.data, data.len - 2);
    lightning_uv_packet_t *packet = (lightning_uv_packet_t *)data.data;
    if (crc != packet->crc16) {
        printf("espnow air data packet crc failed %x %x\n", crc, packet->crc16);
        return;
    }

    lightning_distance = packet->distance * .01f;
    sensors_lightning_update(data.mac, lightning_distance);

    // no nmea or signalk to send

    //display_data_update(LIGHTNING_DISTANCE, wt.distance, ESP_DATA);
    printf("LIGHTNING!!! %f", lightning_distance);
}

// callback when data is recv from Master
// This function is run from the wifi thread, so post to a queue
void OnDataRecv(const esp_now_recv_info *recv_info, const uint8_t *data, int data_len) {
    uint8_t *mac_addr = recv_info->src_addr;
#if 0
    char macStrt[18];
    snprintf(macStrt, sizeof(macStrt), "%02x:%02x:%02x:%02x:%02x:%02x",
             mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
    // printf("Last Packet Recv from: %s %d\n", macStrt, data_len);
    //print(" Last Packet Recv Data: "); print(*data);

#endif
    if (data_len > MAX_DATA_LEN)
        return;

    if (xSemaphoreTake(esp_now_sem, (TickType_t)1) == pdTRUE) {
        esp_now_data_t &pos = esp_now_rb[rb_start];
        memcpy(pos.mac, mac_addr, 6);
        pos.len = data_len;
        memcpy(pos.data, data, data_len);
        xSemaphoreGive(esp_now_sem);
        rb_start+=1;
    }// else
//        printf("unable to get esp now semaphore");
}

// Init ESP Now with fallback
static void InitESPNow() {
    printf("InitESPNow %d\n", chip.channel);
    memset(&chip, 0, sizeof(chip));
    for (int ii = 0; ii < 6; ++ii)
        chip.peer_addr[ii] = (uint8_t)0xff;
    chip.channel = settings.wifi_channel;
    if (chip.channel > 12)
        chip.channel = 1;

    chip.encrypt = 0;

    esp_wifi_set_channel(chip.channel, WIFI_SECOND_CHAN_NONE);
    if (esp_now_init() == ESP_OK) {
        printf("ESPNow Init Success\n");
    } else {
        printf("ESPNow Init Failed\n");
        //ESP.restart();  // restart cpu on failure
        return;
    }

    // Once ESPNow is successfully Init, we will register for recv CB to
    // get recv packer info.
    esp_now_register_recv_cb(OnDataRecv);

    // chip not paired, attempt pair
    esp_err_t addStatus = esp_now_add_peer(&chip);
    if (addStatus != ESP_OK) {
        printf("ESP-NOW pair fail: %d\n", addStatus);
        return;
    }
}

static void receive_esp_now() {
    //printf("receive_esp_now %d %d\n", rb_start, rb_end);
    uint64_t t0 = esp_timer_get_time();
    while (rb_start != rb_end) {
        if (xSemaphoreTake(esp_now_sem, (TickType_t)10) != pdTRUE)
            return;
        esp_now_data_t first = esp_now_rb[rb_end];  // copy struct
        xSemaphoreGive(esp_now_sem);
        rb_end+=1;

        if (first.len > MAX_DATA_LEN) {
            printf("espnow wrong packet size %d", first.len);
            continue;
        }

        wireless_message_count++;

        uint16_t id = *(uint16_t *)first.data;
        switch (id) {
        case WIND_ID: DataRecvWind(first); break;
        case AIR_ID: DataRecvAir(first); break;
        case WATER_ID: DataRecvWater(first); break;
        case LIGHTNING_UV_ID: DataRecvLightningUV(first); break;
        default:
            printf("espnow packet ID mismatch %x\n", id);
        }

        if(esp_timer_get_time() - t0 > 20e6) { // taking too long
            printf("espnow receive failed to keep up\n");
            rb_end = rb_start;
            break;
        }
    }
}

void sendChannel() {
    packet_channel_t packet;
    packet.id = CHANNEL_ID;
    packet.channel = settings.wifi_channel;
    // compute checksum
    packet.crc16 = crc16((uint8_t *)&packet, sizeof(packet) - 2);

    const uint8_t *peer_addr = chip.peer_addr;
    esp_err_t result = esp_now_send(peer_addr, (uint8_t *)&packet, sizeof(packet));
    //print("Send Status: ");
    if (result != ESP_OK) {
        printf("Send failed Status: %d\n", result);
    }
}

static std::map<std::string, int> wifi_networks;
std::string wifi_networks_html;
//static uint64_t scanning = 0;
void wireless_scan_networks() {
    printf("scanning not implemented\n");
    /*
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    int32_t n = WiFi.scanNetworks(true);
    n++; // avoid warning unused
    scanning = esp_timer_get_time();
    if (!scanning) scanning++;
    // display on screen scanning for wifi networks progress
    */
}

void wireless_poll() {
    receive_esp_now();

#if 0
    if(!scanning)
        return;
    uint64_t t = esp_timer_get_time();
    if (t - scanning < 5e6)
        return;
    scanning = 0;
    int n = WiFi.scanComplete();
    for (uint8_t i = 0; i < n; i++) {
        std::string ssid = WiFi.SSID(i).c_str();
        int channel = WiFi.channel(i);
        if (ssid == settings.client_ssid)
            settings.channel = channel;
        if (wifi_networks.find(ssid) == wifi_networks.end()) {
            wifi_networks[ssid] = channel;
            std::string tr = "<tr><td>" + ssid + "</td><td>" + int_to_str(channel) + "</td>";
            tr += "<td>" + int_to_str(WiFi.RSSI(i)) + "</td><td>";
            //tr += WiFi.encryptionType(i) == ENC_TYPE_NONE ? "open" : "encrypted";
            tr += "</td></tr>";
            printf("wifi network %s\n", tr.c_str());
            wifi_networks_html += tr;
        }
    }
    WiFi.scanDelete();
    setup_wifi();
#endif
}

void wireless_program_channel() {
#if 0
    for (int c = 0; c <= 12; c++) {
        esp_now_deinit();
        WiFi.mode(WIFI_OFF);

        WiFi.mode(WIFI_STA);
        WiFi.disconnect();
        esp_wifi_set_channel(c, WIFI_SECOND_CHAN_NONE);
        chip.channel = c;
        if (esp_now_init() != ESP_OK)
            printf("ESPNow Init Failed");
        esp_err_t addStatus = esp_now_add_peer(&chip);
        if (addStatus == ESP_OK) {
            for (int i = 0; i < 15; i++) {
                sendChannel();
                vTaskDelay(50 / portTICK_PERIOD_MS);
            }
        }
        vTaskDelay(50 / portTICK_PERIOD_MS);
    }

    esp_now_deinit();
    WiFi.mode(WIFI_OFF);

    setup_wifi();
    InitESPNow();
#endif
}

void wireless_toggle_mode() {
    uint64_t t0 = esp_timer_get_time();
    static int last_wifi_toggle_time;
    if (t0 - last_wifi_toggle_time > 2e6) {
        force_wifi_ap_mode = !force_wifi_ap_mode;
        last_wifi_toggle_time = t0;
        setup_wifi();
    }
}

void wireless_setup() {
    //printf("wireless setup\n");
    // Init ESPNow with a fallback logic
    if(!esp_now_sem)
        esp_now_sem = xSemaphoreCreateMutex();

    setup_wifi();
    InitESPNow();
}

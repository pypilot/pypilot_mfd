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
#include "web.h"

#define TAG "wireless"

static void setup_wifi(void);

bool wifi_connected;
int wireless_message_count;

#define MAX_DATA_LEN 16
struct esp_now_data_t {
    uint8_t mac[6];
    int len;
    uint8_t data[MAX_DATA_LEN];
} esp_now_rb[256];

volatile uint8_t rb_start, rb_end;
volatile SemaphoreHandle_t esp_now_sem;

// Global copy of chip
static esp_now_peer_info_t chip;

// crc is not really needed with espnow, however it adds a layer of protection against code changes
#define WIND_ID 0xB179

struct wind_packet_t {
    uint16_t id;
    uint16_t angle;
    uint16_t period;  // period of cups rotation in 100uS
    int16_t accelx, accely, accelz;
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
} __attribute__((packed));

#define WATER_ID 0xc946
struct water_packet_t {
    uint16_t id;
    uint16_t speed;        // in 100th knot
    uint16_t depth;        // in 100th meter
    uint16_t temperature;  // in 100th/deg C
} __attribute__((packed));

#define LIGHTNING_UV_ID 0xd255
struct lightning_uv_packet_t {
    uint16_t id;
    uint16_t distance;
    uint16_t uva, uvb, uvc;
    uint16_t visible;
} __attribute__((packed));

#define UNLOCK_ID 0x1B21
struct unlock_packet_t {
    uint16_t id;             // packet identifier
};

#define INFO_ID 0xC9D2
typedef struct __attribute__((packed)) {
    uint16_t id;
    uint16_t packet_count;
    uint32_t runtime;
    uint8_t sw_version, hw_version;
} info_packet_t;


// callback when data is recv from Master
// This function is run from the wifi thread, so post to a queue
static void on_espnow_data(const esp_now_recv_info *recv_info, const uint8_t *data, int data_len) {
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
    } else
        ESP_LOGW(TAG, "unable to get esp now semaphore");
}

// Init ESP Now with fallback
static void InitESPNow(int channel) {
    ESP_LOGI(TAG, "InitESPNow %d", channel);
    memset(&chip, 0, sizeof(chip));
    for (int ii = 0; ii < 6; ++ii)
        chip.peer_addr[ii] = (uint8_t)0xff;
    chip.channel = channel;
    if (chip.channel > 12)
        chip.channel = 1;

    chip.encrypt = 0;

    esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
    if (esp_now_init() == ESP_OK) {
        ESP_LOGI(TAG, "ESPNow Init Success");
    } else {
        ESP_LOGE(TAG, "ESPNow Init Failed");
        abort();  // restart cpu on failure
        return;
    }

    // Once ESPNow is successfully Init, we will register for recv CB to
    // get recv packer info.
    esp_now_register_recv_cb(on_espnow_data);

    // chip not paired, attempt pair
    esp_err_t addStatus = esp_now_add_peer(&chip);
    if (addStatus != ESP_OK) {
        ESP_LOGE(TAG, "ESP-NOW pair fail: %d", addStatus);
        return;
    }
}

static void connect_handler(void* arg, esp_event_base_t event_base,
                            int32_t event_id, void* event_data)
{
    ESP_LOGI(TAG, "Wifi Connected");
}

static void wifi_event_handler(void *arg,
                               esp_event_base_t event_base,
                               int32_t event_id,
                               void *event_data)
{
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
        case WIFI_EVENT_STA_START:
            ESP_LOGI(TAG, "STA start, connecting...");
            //esp_wifi_connect();
            break;

        case WIFI_EVENT_STA_CONNECTED:
            ESP_LOGI(TAG, "STA connected to AP");
            wifi_connected = true;
            break;

        case WIFI_EVENT_STA_DISCONNECTED:
            ESP_LOGI(TAG, "STA disconnected, reconnecting...");
            wifi_connected = false;
            //esp_wifi_connect();
            break;

        case WIFI_EVENT_SCAN_DONE: {
            
            uint16_t count = 0;
            esp_wifi_scan_get_ap_num(&count);
            ESP_LOGI(TAG, "Scanning found %d networks", count);

            if(count > 32) {
                ESP_LOGI(TAG, "Found more than 32 networks, using only 32");
                count = 32;
            }

            std::vector<wifi_ap_record_t> recs(count);
            if (esp_wifi_scan_get_ap_records(&count, recs.data()) == ESP_OK) {
                void web_scan_complete(const std::vector<wifi_ap_record_t> &recs);
                web_scan_complete(recs);
                for(auto i = recs.begin(); i!=recs.end(); i++)
                    printf("ssid: %s rssi: %d channel %d encryption %d\n",
                           i->ssid, i->rssi, i->primary, i->authmode);
                setup_wifi();
                web_update_wifi_networks();
            }
        } break;
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

static void wifi_global_init(void)
{
    static bool wifi_initialized = false;
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

struct wifi_mode_config_t {
    std::string mode, ssid, psk;
    uint8_t channel;
};

static wifi_mode_config_t wifi_config;

static void setup_wifi(void)
{
    wifi_global_init();

#ifndef CONFIG_IDF_TARGET_ESP32S3
    force_wifi_ap_mode = 1;
#endif

    ESP_ERROR_CHECK(esp_now_deinit());
    ESP_ERROR_CHECK(esp_wifi_stop());

    wifi_config.channel = settings.wifi_channel;
    if (force_wifi_ap_mode || settings.wifi_mode == "ap") {
        wifi_config.mode = "ap";
        wifi_config.ssid = settings.ap_ssid;
        wifi_config.psk = settings.ap_psk;

        wifi_config_t ap_cfg = {
            .ap = {
                .ssid = "",
                .password = "",
                .ssid_len = 0,
                .channel = settings.wifi_channel,
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

        strncpy((char*)ap_cfg.ap.ssid, settings.ap_ssid.c_str(), sizeof ap_cfg.ap.ssid);
        strncpy((char*)ap_cfg.ap.password, settings.ap_psk.c_str(), sizeof ap_cfg.ap.password);

        if (strlen((char *)ap_cfg.ap.password) == 0)
            ap_cfg.ap.authmode = WIFI_AUTH_OPEN;

        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_cfg));
        ESP_ERROR_CHECK(esp_wifi_start());
    } else if (settings.wifi_mode == "client" && !settings.client_ssid.empty()) {
        wifi_config.mode = "client";
        wifi_config.ssid = settings.client_ssid;
        wifi_config.psk = settings.client_psk;

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
        wifi_config.mode = "none";
        
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_start());
        esp_wifi_disconnect();
    }

    InitESPNow(settings.wifi_channel);

    signalk_discovered = 0;
    pypilot_discovered = 0;
}

void wireless_scan_networks()
{
    ESP_ERROR_CHECK(esp_now_deinit());
    ESP_ERROR_CHECK(esp_wifi_stop());
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    //ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    wifi_scan_config_t cfg{};
    cfg.scan_type = WIFI_SCAN_TYPE_ACTIVE;
    cfg.scan_time.active.min = 40;
    cfg.scan_time.active.max = 120; // thorough

    esp_err_t err = esp_wifi_scan_start(&cfg, false); // async
    if (err != ESP_OK)
        ESP_LOGE(TAG, "scan start failed: %s", esp_err_to_name(err));
}

#if 0
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
#endif

static void DataRecvWind(struct esp_now_data_t &data) {
    if (data.len != sizeof(wind_packet_t)) {
        ESP_LOGW(TAG, "received invalid wind packet %d\n", data.len);
        return;
    }

    wind_packet_t *packet = (wind_packet_t *)data.data;
    float dir;
    if (packet->angle == 0xffff || packet->angle >= 32768)
        dir = NAN;  // invalid
    else
        dir = 360 - packet->angle * 360.0f / 32768.0f;

    if (packet->period > 100) { // period of 100uS cups is about 193 knots for 100 rotations/s
        //        knots = 19350.0 / packet->period;
        float knots = 25800.0 / packet->period;  // corrected from preliminary calibration
        wind_knots = knots * .5 + wind_knots * .5;
    } else
        wind_knots = 0;

    float paccel_x = packet->accelx * 4.0f / 32768;
    float paccel_y = packet->accely * 4.0f / 32768;
    float paccel_z = packet->accelz * 4.0f / 32768;

    //    printf("recv %d %d %d %d %d\n", packet->angle, packet->period, packet->accelx, packet->accely, packet->accelz);

    if (settings.compensate_wind_with_accelerometer && dir >= 0) {

        float ax = paccel_x * 9.8f * 1.944f;
        float ay = paccel_y * 9.8f * 1.944f;
        uint64_t t = esp_timer_get_time();
        static int lastt;
        float dt = (t - lastt) / 1e6;
        lastt = t;

        // TODO: combine inertial sensors from autopilot somehow?
        static float vx, vy;
        vx += ax * dt;
        vy += ay * dt;

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
    sensors_wind_update(data.mac, dir, wind_knots, paccel_x, paccel_y, paccel_z);
}


static void DataRecvAir(struct esp_now_data_t &data) {
    if (data.len != sizeof(air_packet_t))
        ESP_LOGW(TAG, "received invalid air packet %d\n", data.len);

    air_packet_t *packet = (air_packet_t *)data.data;

    sensors_air_update(data.mac, 1.0f + packet->pressure * .00001f, packet->rel_humidity * .01f, packet->temperature * .01f, packet->air_quality * .01f, packet->battery_voltage * .01f);
}

static void DataRecvWater(struct esp_now_data_t &data) {
    if (data.len != sizeof(water_packet_t))
        printf("received invalid water packet %d\n", data.len);

    water_packet_t *packet = (water_packet_t *)data.data;

    sensors_water_update(data.mac, packet->speed * .01f, packet->depth * .01f, packet->temperature * .01f);
}

static void DataRecvLightningUV(struct esp_now_data_t &data) {
    if (data.len != sizeof(lightning_uv_packet_t))
        printf("received invalid lightning UV packet %d\n", data.len);

    lightning_uv_packet_t *packet = (lightning_uv_packet_t *)data.data;

    lightning_distance = packet->distance * .01f;
    sensors_lightning_update(data.mac, lightning_distance);

    // no nmea or signalk to send

    //display_data_update(LIGHTNING_DISTANCE, wt.distance, ESP_DATA);
    printf("LIGHTNING!!! %f", lightning_distance);
}

static void DataRecvInfo(struct esp_now_data_t &data) {
    if (data.len != sizeof(info_packet_t))
        printf("received invalid info UV packet %d\n", data.len);

    info_packet_t *packet = (info_packet_t *)data.data;

    // for now not using version
    sensors_info_update(data.mac, packet->runtime, packet->packet_count);
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
            ESP_LOGW(TAG, "espnow wrong packet size %d", first.len);
            continue;
        }

        wireless_message_count++;

        uint16_t id = *(uint16_t *)first.data;
        switch (id) {
        case WIND_ID: DataRecvWind(first); break;
        case AIR_ID: DataRecvAir(first); break;
        case WATER_ID: DataRecvWater(first); break;
        case LIGHTNING_UV_ID: DataRecvLightningUV(first); break;
        case INFO_ID: DataRecvInfo(first); break;
        default:
            ESP_LOGW(TAG, "espnow packet ID mismatch %x", id);
        }

        if(esp_timer_get_time() - t0 > 20e6) { // taking too long
            ESP_LOGW(TAG, "espnow receive failed to keep up");
            rb_end = rb_start;
            break;
        }
    }
}

void wireless_unlock_channel(const std::string &mac) {
    ESP_LOGI(TAG, "unlock sensor %s", mac.c_str());

    uint64_t peer_addr_int = mac_str_to_int(mac);
    uint8_t peer_addr[6];
    int_as_mac(peer_addr, peer_addr_int);

    unlock_packet_t packet;
    packet.id = UNLOCK_ID;

    if (!esp_now_is_peer_exist(peer_addr)) {
        esp_now_peer_info_t peer = {};
        memcpy(peer.peer_addr, peer_addr, 6);
        peer.channel = settings.wifi_channel;
        peer.ifidx = WIFI_IF_STA;
        peer.encrypt = false;
        ESP_ERROR_CHECK( esp_now_add_peer(&peer) );
    }

    esp_err_t result = esp_now_send((const uint8_t*)peer_addr, (uint8_t *)&packet, sizeof(packet));
    if (result != ESP_OK)
        printf("Send failed Status: %d\n", result);
}

void wireless_poll() {
    receive_esp_now();

    static uint64_t last_wifi_update;
    uint64_t t = esp_timer_get_time();

    if(t-last_wifi_update > 1e6) {
        if(settings.wifi_mode != wifi_config.mode ||
           wifi_config.channel != settings.wifi_channel ||
           (wifi_config.mode == "ap" &&
            (wifi_config.ssid != settings.ap_ssid ||
             wifi_config.psk != settings.ap_psk)) ||
           (wifi_config.mode == "client" &&
            (wifi_config.ssid != settings.client_ssid ||
             wifi_config.psk != settings.client_psk)))
            setup_wifi();
        last_wifi_update = t;
    }
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
    // Init ESPNow with a fallback logic
    if(!esp_now_sem)
        esp_now_sem = xSemaphoreCreateMutex();

    setup_wifi();
}

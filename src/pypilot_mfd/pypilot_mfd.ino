/* Copyright (C) 2024 Sean D'Epagnier <seandepagnier@gmail.com>
 *
 * This Program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 */

#include <esp_now.h>
#include <esp_wifi.h>
#include <WiFi.h>
#include <lwip/sockets.h>

#include "bmp280.h"
#include "zeroconf.h"
#include "web.h"
#include "display.h"
#include "nmea.h"
#include "signalk.h"
#include "pypilot_client.h"
#include "utils.h"

#include "settings.h"

bool wifi_ap_mode = false;
settings_t settings;

// Global copy of chip
esp_now_peer_info_t chip;


#define ID  0xf179
#define CHANNEL_ID 0x0a21

enum keys {KEY_PAGE_UP, KEY_SCALE, KEY_PAGE_DOWN, KEY_COUNT};
int key_pin[KEY_COUNT] = {25, 26, 27};


std::map<uint64_t, wind_transmitter_t> wind_transmitters;

struct packet_t {
    uint16_t id;
    uint16_t angle;
    uint16_t period;
    uint16_t accelx, accely;
    uint16_t crc16;
} __attribute__((packed));  

struct packet_channel_t {
  uint16_t id;             // packet identifier
  uint16_t channel;
  uint16_t crc16;
} __attribute__((packed));


int32_t getWiFiChannel(const char *ssid) {
  if (int32_t n = WiFi.scanNetworks()) {
      for (uint8_t i=0; i<n; i++) {
          if (!strcmp(ssid, WiFi.SSID(i).c_str())) {
              return WiFi.channel(i);
          }
      }
  }
  return 0;
}

void setup_wifi()
{
  //wifi_ap_mode=true;
    if(wifi_ap_mode) {
        //Set device in AP mode to begin with
        WiFi.mode(WIFI_AP);
        // configure device AP mode
        const char *SSID = "pypilot_mfd";
        bool result = WiFi.softAP(SSID, 0, settings.channel);
        if (!result)
            Serial.println("AP Config failed.");
        else
            Serial.println("AP Config Success. Broadcasting with AP: " + String(SSID));

        Serial.print("Soft-AP IP address = ");
        Serial.println(WiFi.softAPIP());

        // This is the mac address of the Slave in AP Mode
        Serial.print("AP MAC: ");
        Serial.println(WiFi.softAPmacAddress());

    } else {
        WiFi.mode(WIFI_AP_STA);
        WiFi.onEvent(WiFiStationGotIP, SYSTEM_EVENT_STA_GOT_IP);

        if(settings.ssid) {
            Serial.printf("connecting to SSID: %s  psk: %s\n", settings.ssid.c_str(), settings.psk.c_str());
        
            WiFi.begin(settings.ssid.c_str(), settings.psk.c_str());
        } else
            WiFi.disconnect();

        signalk_discovered = 0;
        pypilot_discovered = 0;
    }
}

void WiFiStationGotIP(WiFiEvent_t event, WiFiEventInfo_t info)
{
    String ip = WiFi.localIP().toString();
    printf("WIFI got IP: %s\n", ip.c_str());
    settings.channel = WiFi.channel();
} 

// Init ESP Now with fallback
void InitESPNow() {
      esp_wifi_set_channel(chip.channel, WIFI_SECOND_CHAN_NONE);
    if (esp_now_init() == ESP_OK) {
        Serial.println("ESPNow Init Success");
    } else {
        Serial.println("ESPNow Init Failed");
        ESP.restart(); // restart cpu on failure
    }

    // Once ESPNow is successfully Init, we will register for recv CB to
    // get recv packer info.
    esp_now_register_recv_cb(OnDataRecv); 

        // chip not paired, attempt pair
    esp_err_t addStatus = esp_now_add_peer(&chip);
    if (addStatus != ESP_OK) {
        Serial.print("ESP-NOW pair fail:");
        Serial.println(addStatus);
        return;
    }
}

uint16_t crc16(const uint8_t* data_p, int length)
{
    uint8_t x;
    uint16_t crc = 0xFFFF;

    while (length--){
        x = crc >> 8 ^ *data_p++;
        x ^= x>>4;
        crc = (crc << 8) ^ ((uint16_t)(x << 12)) ^ ((uint16_t)(x <<5)) ^ ((uint16_t)x);
    }
    return crc;
}

float lpdir = NAN, knots = 0;

void lowpass_direction(float dir)
{
    if(dir >= 0) {
        float ldir = dir;
        //ldir += settings.wind_offset;
        if(ldir > 360)
            ldir -= 360;
        // lowpass wind direction                                                                                                                                                                
        if(lpdir - ldir > 180)
            ldir += 360;
        else if(ldir - lpdir > 180)
            ldir -= 360;

          //  Serial.printf("%x %f %f\n", packet->id, ldir, lpdir);

        const float lp = .2;
        if(isnan(lpdir))
            lpdir = dir;
        else if(fabs(lpdir - ldir) < 180) // another test which should never fail                                                                                                                  
            lpdir = lp*ldir + (1-lp)*lpdir;

        if(lpdir >= 360)
            lpdir -= 360;
        else if(lpdir < 0)
            lpdir += 360;
    } else
        lpdir = NAN;
}

uint64_t cur_primary;
uint32_t cur_primary_time;

uint64_t mac_as_int(const uint8_t *mac_addr)
{
    return ((*(uint32_t*)mac_addr) << 16) + (*(uint16_t*)(mac_addr+4));
}

// callback when data is recv from Master
void OnDataRecv(const uint8_t *mac_addr, const uint8_t *data, int data_len) {
#if 0
    char macStrt[18];
    snprintf(macStrt, sizeof(macStrt), "%02x:%02x:%02x:%02x:%02x:%02x",
             mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
    Serial.print("Last Packet Recv from: "); Serial.print (macStrt);
    //Serial.print(" Last Packet Recv Data: "); Serial.print(*data);
    Serial.print(" Last Packet Recv Data Len: "); Serial.print(data_len);
    Serial.println("");
#endif
    if(data_len != sizeof(packet_t)) {
        Serial.println("wrong packet size");
        return;
    }
        
    packet_t *packet = (packet_t*)data;
    if(packet->id != ID) {
      Serial.println("ID mismatch");
    }
    
    uint16_t crc = crc16(data, data_len-2);
    if(crc != packet->crc16) {
      Serial.printf("crc failed %x %x\n", crc, packet->crc16);
        return;
    }

    float dir;  
    if(packet->angle == 0xffff)
        dir = -1; // invalid
    else
        dir = 360 - packet->angle*360.0f/16384.0f;

    if(packet->period > 100) // period of 100uS cups is about 193 knots
        knots = 19350.0 / packet->period;
    else
        knots = 0;

#if 0
    if(settings.compensate_accelerometer && dir >= 0) {
        unsigned long t = millis();
        static int lastt;
        float dt = (t - lastt)/1000.0;
        lastt = t;

        // TODO: combine inertial sensors from autopilot somehow
        float accel_x = packet->accelx*4.0f/16384 * 9.8f * 1.944f;
        float accel_y = packet->accely*4.0f/16384 * 9.8f * 1.944f;

        static float vx, vy;
        vx += accel_x*dt;
        vy += accel_y*dt;

        vx *= .9; // decay

        float wvx = knots*cos(radians(dir));
        float wvy = knots*sin(radians(dir));

        wvx -= vx;
        wvy -= vy;

        float wvx = knots*cos(radians(dir));
        float wvy = knots*sin(radians(dir));

        wvx -= vx;
        wvy -= vy;

        knots = hypot(wvx, wvy);
        dir = degrees(atan2(wvx, wvy));
        if(dir < 0)
          dir += 360;
    }
#endif
    uint64_t mac_int = mac_as_int(mac_addr);
    uint32_t t = millis();
    if(mac_int == cur_primary) {
        lowpass_direction(dir);

        char buf[128];
        if(!isnan(lpdir)) 
            snprintf(buf, sizeof buf, PSTR("MWV,%d.%02d,R,%d.%02d,N,A"), (int)lpdir, (uint16_t)(lpdir*100.0f)%100U, (int)knots, (int)(knots*100)%100);
        else // invalid wind direction (no magnet?)
            snprintf(buf, sizeof buf, PSTR("MWV,,R,%d.%02d,N,A"), (int)knots, (int)(knots*100)%100);

        nmea_send(buf);

        if(settings.wifi_data == SIGNALK) {
            if(!isnan(lpdir))
                signalk_send("environment.wind.angleApparent", M_PI/180.0f * lpdir);
            signalk_send("environment.wind.speedApparent", knots*.514444f);
        }

        display_data_update(WIND_SPEED, knots, ESP_DATA);
        display_data_update(WIND_DIRECTION, lpdir, ESP_DATA);
    }

    if(wind_transmitters.find(mac_int) != wind_transmitters.end()) {
        wind_transmitter_t &wt = wind_transmitters[mac_int];
        wt.dir = dir + wt.offset;
        wt.knots = knots;
        wt.t = t;
        if(dir >= 0) {
            if((wt.position == PRIMARY) ||
               (wt.position == SECONDARY && !cur_primary) ||
               (wt.position == PORT && (dir > 200 && dir < 340)) ||
               (wt.position == STARBOARD && (dir > 20 && dir < 160)))
                cur_primary = mac_int;
            if(cur_primary == mac_int)
                cur_primary_time = t;
        }
    } else {
        wind_transmitter_t wt;
        wt.dir = dir;
        wt.knots = knots;
        wt.offset = 0;
        wt.position = SECONDARY;
        wt.t = t;
        wind_transmitters[mac_int] = wt;
    }

    if(t - cur_primary_time > 1000)
        cur_primary = 0;
}

void sendChannel()
{
    packet_channel_t packet;
    packet.id = CHANNEL_ID;
    packet.channel = settings.channel;
    // compute checksum
    packet.crc16 = crc16((uint8_t *)&packet, sizeof(packet) - 2);

    const uint8_t *peer_addr = chip.peer_addr;
    esp_err_t result = esp_now_send(peer_addr, (uint8_t *)&packet, sizeof(packet));
    //Serial.print("Send Status: ");
    if (result != ESP_OK) {
        Serial.print("Send failed Status: ");
        Serial.println(result);
    }
}

void scan_devices()
{
    for(int c = 0; c <= 12; c++) {
        esp_now_deinit();
        WiFi.mode(WIFI_OFF);

        WiFi.mode(WIFI_STA);
        WiFi.disconnect();
        esp_wifi_set_channel(c, WIFI_SECOND_CHAN_NONE);
        chip.channel = c;
        if (esp_now_init() != ESP_OK)
            Serial.println("ESPNow Init Failed");
        esp_err_t addStatus = esp_now_add_peer(&chip);
        if (addStatus == ESP_OK) {
            for(int i=0; i<15; i++) {
                sendChannel();
                delay(50);
            }
        }
        delay(50);
    }

    esp_now_deinit();
    WiFi.mode(WIFI_OFF);

    chip.channel = settings.channel;

    setup_wifi();
    InitESPNow();
}

struct SerialLinebuffer
{
    SerialLinebuffer(HardwareSerial &s, data_source_e so)
        : serial(s), source(so) {}

    String readline() {
        String data = serial.readStringUntil('\n');
        if(!data)
            return "";
        if(data[buf.length()-1] == '\n') {
            data = buf + data;
            buf = "";
            return data;
        }

        buf += data;
        return "";
    }

    void read(bool enabled=true) {
        for(;;) {
            String line = readline();
            if(!line)
                break;

            if(enabled)
                nmea_parse_line(line.c_str(), source);
        }
    }

    String buf;
    HardwareSerial &serial;
    data_source_e source;
};

SerialLinebuffer SerialBuffer(Serial, USB_DATA);
SerialLinebuffer Serial2Buffer(Serial2, RS422_DATA);

static void read_serials()
{
    SerialBuffer.read(settings.input_usb);
    Serial2Buffer.read();
}

void setup()
{
    memset(&chip, 0, sizeof(chip));
    for (int ii = 0; ii < 6; ++ii)
        chip.peer_addr[ii] = (uint8_t)0xff;
    chip.channel = settings.channel;
    if(chip.channel > 12)
        chip.channel = 1;

    chip.channel = 6;
    chip.encrypt = 0; 

    settings.usb_baud_rate = 115200;
    Serial.begin(settings.usb_baud_rate == 38400 ? 38400 : 115200);
    Serial.setTimeout(0);

    Serial2.begin(settings.rs422_baud_rate == 4800 ? 4800 : 38400,
                  SERIAL_8N1, 16, 17);    //Hardware Serial of ESP32
    Serial2.setTimeout(0);

    if(!SPIFFS.begin(true))
        Serial.println("SPIFFS Mount Failed");
    else
        listDir(SPIFFS, "/", 0);

    if(settings_load())
        settings_store(".bak");
    else
        settings_load(".bak");
    settings.show_status=true;
    settings.landscape = false;

    settings.input_wifi = true;
    //settings.output_wifi = true;
    settings.wifi_data = NMEA_PYPILOT;
    settings.channel = 6;


    Serial.println("pypilot_mfd");

   // bmX280_setup();

    pinMode(0, INPUT_PULLUP);
    pinMode(35, INPUT_PULLUP);
    for(int i=0; i<KEY_COUNT; i++)
        pinMode(key_pin[i], INPUT_PULLUP);

    setup_wifi();

    // Init ESPNow with a fallback logic
    InitESPNow();
    mdns_setup();
    web_setup();

    display_setup();

    Serial.println("setup complete");
}

static void toggle_wifi_mode()
{
    uint32_t t0 = millis();
    static int last_wifi_toggle_time;
    if(t0 - last_wifi_toggle_time > 2000) {
        wifi_ap_mode = !wifi_ap_mode;
        last_wifi_toggle_time = t0;
        setup_wifi();
    }
}

void loop()
{
    uint32_t t0 = millis();

    static uint32_t tl;
    if(t0-tl > 60000) { // report memory statistics every 60 seconds
        Serial.printf("pypilot_mfd %d %d %d\n", t0 / 1000, ESP.getFreeHeap(), ESP.getHeapSize());
        tl = t0;
    }

    // read keys
    static uint32_t wifi_toggle_hold;
    if(!digitalRead(key_pin[KEY_PAGE_UP])) {
        if(!digitalRead(key_pin[KEY_PAGE_DOWN])) {
            if(!wifi_toggle_hold) {
                wifi_toggle_hold = t0;
            } else if(t0 - wifi_toggle_hold > 1000) {
                toggle_wifi_mode();
                wifi_toggle_hold = 0;
            }
        } else {
            display_change_page(-1);
            wifi_toggle_hold = 0;
        }
    } else if(!digitalRead(key_pin[KEY_PAGE_DOWN])) {
            display_change_page(1);
            wifi_toggle_hold = 0;
    } else  if(!digitalRead(key_pin[KEY_SCALE]))
        display_change_scale();

    if(!digitalRead(0)) {
        display_change_page(1);
     //   toggle_wifi_mode();
    }
    
    //read_serials();
    //read_pressure_temperature();
    nmea_poll();
    signalk_poll();
    //pypilot_client_poll();

    display_render();
    web_poll();

    // sleep remainder of second
    int dt = millis() - t0;
    const int period = 100;
    if(dt < period) {
        //printf("delay %d\n", dt);  
        delay(period-dt);
    }
}

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

#include <Wire.h>

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

#define WIND_ID  0xf179
#define CHANNEL_ID 0x0a21

enum keys {KEY_PAGE_UP, KEY_SCALE, KEY_PAGE_DOWN, KEY_PWR, KEY_COUNT};
int key_pin[KEY_COUNT] = {27, 32, 33, 25};

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

            // setting a custom "country" locks the wifi in a particular channel
            wifi_country_t myWiFi;
            //Country code (cc) set to 'X','X','X' is the standard, apparently.
            myWiFi.cc[0]='X';
            myWiFi.cc[1]='X';
            myWiFi.cc[2]='X';
            myWiFi.schan = settings.channel;
            myWiFi.nchan = 1;
            myWiFi.policy = WIFI_COUNTRY_POLICY_MANUAL;

            esp_wifi_set_country(&myWiFi);
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
    printf("InitESPNow %d\n", chip.channel);
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
    if(isnan(dir)) {
        lpdir = dir;
        return;
    }

    float ldir = dir;
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

    lpdir = resolv(lpdir);
}

uint64_t cur_primary;
uint32_t cur_primary_time;

static void DataRecvWind(const uint8_t *mac_addr, const uint8_t *data, int data_len)
{
    uint16_t crc = crc16(data, data_len-2);
    packet_t *packet = (packet_t*)data;
    if(crc != packet->crc16) {
        Serial.printf("espnow wind data packet crc failed %x %x\n", crc, packet->crc16);
        return;
    }

    float dir;  
    if(packet->angle == 0xffff)
        dir = NAN; // invalid
    else
        dir = 360 - packet->angle*360.0f/16384.0f;

    if(packet->period > 100) // period of 100uS cups is about 193 knots for 100 rotations/s
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
    if(wind_transmitters.find(mac_int) != wind_transmitters.end()) {
        wind_transmitter_t &wt = wind_transmitters[mac_int];
        wt.dir = resolv(dir + wt.offset);
        wt.knots = knots;
        wt.t = t;
        if(!isnan(dir)) {
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
        wt.dir = resolv(dir);
        wt.knots = knots;
        wt.offset = 0;
        wt.position = SECONDARY;
        wt.t = t;
        wind_transmitters[mac_int] = wt;
    }

    if(mac_int == cur_primary) {
        wind_transmitter_t &wt = wind_transmitters[mac_int];

        lowpass_direction(wt.dir);

        char buf[128];
        if(!isnan(lpdir)) 
            snprintf(buf, sizeof buf, PSTR("MWV,%d.%02d,R,%d.%02d,N,A"), (int)lpdir, (uint16_t)(lpdir*100.0f)%100U, (int)knots, (int)(knots*100)%100);
        else // invalid wind direction (no magnet?)
            snprintf(buf, sizeof buf, PSTR("MWV,,R,%d.%02d,N,A"), (int)knots, (int)(knots*100)%100);

        nmea_send(buf);
        //printf("lpdir %f\n", lpdir);

        if(settings.wifi_data == SIGNALK) {
            if(!isnan(lpdir))
                signalk_send("environment.wind.angleApparent", M_PI/180.0f * lpdir);
            signalk_send("environment.wind.speedApparent", knots*.514444f);
        }

        display_data_update(WIND_SPEED, knots, ESP_DATA);
        display_data_update(WIND_DIRECTION, lpdir, ESP_DATA);
    }

    if(t - cur_primary_time > 1000)
        cur_primary = 0;
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
    if(packet->id == WIND_ID)
        DataRecvWind(mac_addr, data, data_len);
    else
        printf("ID mismatch %x, %x\n",  packet->id, WIND_ID);
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

static std::map <String, int> wifi_networks;
static String wifi_networks_html;
static uint32_t scanning = 0;
void scan_wifi_networks() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    int32_t n = WiFi.scanNetworks(true));
    scanning = millis();
    if(!scanning) scanning++;
    // display on screen scanning for wifi networks progress
}

void scaning_poll()
{
    if(t - scanning > 5000) {
            scanning = 0;
            int n = WiFi.scanComplete();
            for (uint8_t i=0; i<n; i++) {
                String ssid = WiFi.SSID(i);
                int channel = WiFi.channel(i);
                if(ssid == settings.ssid)
                    settings.channel = channel;
                if(wifi_networks.find(ssid) == wifi_networks.end()) {
                    wifi_networks[ssid] = channel;
                    String tr = "<tr><td>" + ssid + "</td><td>" + channel + "</td>";
                    tr += "<td>" + WiFi.RSSI(i) + "</td><td>";
                    tr += WiFi.encryptionType(i) == ENC_TYPE_NONE ? "open" : "encrypted";
                    tr += "</td></tr>";
                    printf("wifi network %s\n", tr.c_str());
                    wifi_networks_html += tr;
                }
            }
            WiFi.scanDelete();
        }
        setup_wifi();
        return;
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
        int c;
        for(;;) {
            c = serial.read();
            if(c < 0)
                return "";
            if(c == '\n' || c == '\r') {
                if(buf.isEmpty())
                    continue;
                String data = buf;
                buf = "";
                return data;
            }
            buf += (char)c;
        }
    }

    void read(bool enabled=true) {
        for(;;) {
            String line = readline();
            if(line.isEmpty())
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
    uint32_t t0 = millis();
    SerialBuffer.read(settings.input_usb);
    uint32_t t1 = millis();
    Serial2Buffer.read();
    uint32_t t2 = millis();
}

#define DEVICE_ADDRESS 0x19  // 0x32  // LIS2DW12TR
#define LIS2DW12_CTRL 0x1F   // CTRL1 is CTRL+1

static void setup_accel()
{
    // init communication with accelerometer
    Wire.begin();
    Wire.beginTransmission(0x19);
    Wire.write(LIS2DW12_CTRL + 1);
    Wire.write(0x44);  // high performance
    Wire.endTransmission();

    Wire.beginTransmission(0x19);
    Wire.write(LIS2DW12_CTRL + 6);
    Wire.write(0x10);  // set +/- 4g FS, LP filter ODR/2
    Wire.endTransmission();

    // enable block data update (bit 3) and auto register address increment (bit 2)
    Wire.beginTransmission(0x19);
    Wire.write(LIS2DW12_CTRL + 2);
    Wire.write(0x08 | 0x04);
    Wire.endTransmission();
}

void read_accel()
{
    Wire.beginTransmission(0x19);
    Wire.write(0x28);
    Wire.endTransmission();
    Wire.requestFrom(0x19, 6);
    int i = 0;
    uint8_t data[6] = { 0 };
    while (Wire.available() && i < 6)
        data[i++] = Wire.read();
    if (i != 6)
        return;

    int16_t x = (data[1] << 8) | data[0];
    int16_t y = (data[3] << 8) | data[2];

    int rotation;
    if(abs(x) > abs(y)) {
        rotation = x < 0 ? 0 : 2;
    } else
        rotation = y < 0 ? 3 : 1;

    display_set_mirror_rotation(rotation);
    printf("accel %d %d %d\n", rotation, x, y);
}

void setup()
{
    memset(&chip, 0, sizeof(chip));
    for (int ii = 0; ii < 6; ++ii)
        chip.peer_addr[ii] = (uint8_t)0xff;
    chip.channel = settings.channel;
    if(chip.channel > 12)
        chip.channel = 1;

    //chip.channel = 6;
    chip.encrypt = 0; 

    settings.usb_baud_rate = 115200;
    Serial.begin(settings.usb_baud_rate == 38400 ? 38400 : 115200);
    Serial.setTimeout(0);

    if(!SPIFFS.begin(true))
        Serial.println("SPIFFS Mount Failed");
    else
        listDir(SPIFFS, "/", 0);

    if(settings_load())
        settings_store(".bak");
    else
        settings_load(".bak");

    Serial2.begin(settings.rs422_baud_rate == 4800 ? 4800 : 38400,
                  SERIAL_8N1, 16, 17);    //Hardware Serial of ESP32
    Serial2.setTimeout(0);

    //settings.channel = 6;

    Serial.println("pypilot_mfd");

    // buzzer
    ledcSetup(1, 4000, 10);
    ledcAttachPin(4, 1);
    ledcWrite(1, 512);
    delay(100);
    ledcWrite(1, 0);


    // bmX280_setup();
    pinMode(2, INPUT_PULLUP);  // strap for display

    for(int i=0; i<KEY_COUNT; i++)
        pinMode(key_pin[i], INPUT_PULLUP);

    setup_wifi();

    // Init ESPNow with a fallback logic
    InitESPNow();
    mdns_setup();
    web_setup();
    setup_accel();
    printf("web setup complete\n");
    delay(200);  ///  remove this??

    bmX280_setup();

    int ss = CONFIG_ARDUINO_LOOP_STACK_SIZE;
    if(ss < 16384)
        printf("WARNING STACK TOO SMALL\n");
    printf("Stack Size %d\n", ss);

    Serial.println("setup complete");

    read_accel();
    display_setup();
    printf("display setup complete\n");

    esp_log_level_set("*", ESP_LOG_NONE);
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

static void read_keys()
{
    static uint32_t key_time, timeout = 500;
    static bool key_pressed;
    uint32_t t0 = millis();
    if(key_pressed) {
        if(t0-key_time < timeout) // allow new presses to process
            return;  // ignore keys down until timeout
        key_pressed = false;
        timeout = 300; // faster repeat timeout
    }

    if(!digitalRead(key_pin[KEY_PAGE_UP])) {
        if(!digitalRead(key_pin[KEY_PAGE_DOWN]))
            toggle_wifi_mode();
        else
            display_change_page(-1);
    } else if(!digitalRead(key_pin[KEY_PAGE_DOWN]))
        display_change_page(1);
    else  if(!digitalRead(key_pin[KEY_SCALE]))
        display_change_scale();
    else if(!digitalRead(0)) {
        display_change_page(1);
     //   toggle_wifi_mode();
    } else {
        timeout = 500;
        return;
    }

    key_pressed = true;
    key_time = t0;
}

void loop()
{
    uint32_t t0 = millis();
    static uint32_t tl;
    if(t0-tl > 60000) { // report memory statistics every 60 seconds
        Serial.printf("pypilot_mfd %d %d %d\n", t0 / 1000, ESP.getFreeHeap(), ESP.getHeapSize());
        tl = t0;
    }

    if(scanning)
        scanning_poll();
    else {
        read_keys();
        read_serials();
        //read_pressure_temperature();
        nmea_poll();
        signalk_poll();
        pypilot_client_poll();

        web_poll();
    }
    display_render();

    // sleep remainder of second
    int dt = millis() - t0;
    const int period = 100;
    if(dt < period) {
//        printf("delay %d\n", dt);  
        delay(period-dt);
    }
}

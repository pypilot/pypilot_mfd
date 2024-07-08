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

#include <Arduino.h>
#include <Arduino_JSON.h>

#include "settings.h"
#include "wireless.h"
#include "utils.h"
#include "display.h"
#include "nmea.h"
#include "signalk.h"
#include "zeroconf.h"
//#include "web.h"

bool wifi_ap_mode = false;
settings_t settings;


template<class T>
struct transmitters {
    std::map<uint64_t, T> macs;
    uint64_t cur_primary;
    uint32_t cur_primary_time;

    transmitters() : cur_primary(0), cur_primary_time(0) {}

    T &update(uint8_t mac[6], bool &primary, float dir = 0) {
        uint64_t mac_int = mac_as_int(mac);
        uint32_t t = millis();
        primary = cur_primary == mac_int;
        T& wt = macs[mac_int];
        if (macs.size() == 1)
            wt.position = PRIMARY;  // if there are no known transmitters assign it as primary, more likely to ignore another boat nearby
        if (!isnan(dir)) {
            if ((wt.position == PRIMARY) || (wt.position == SECONDARY && !cur_primary) || (wt.position == PORT && (dir > 200 && dir < 340)) || (wt.position == STARBOARD && (dir > 20 && dir < 160)))
                cur_primary = mac_int;
            if (primary)
                cur_primary_time = t;
        }
        wt.t = t;
        if (t - cur_primary_time > 1000)
            cur_primary = 0;
        return wt;
    }
};

static transmitters<wind_transmitter_t> wind_transmitters;
static transmitters<air_transmitter_t> air_transmitters;
static transmitters<water_transmitter_t> water_transmitters;
static transmitters<lightning_uv_transmitter_t> lightning_uv_transmitters;

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

void WiFiStationGotIP(WiFiEvent_t event, WiFiEventInfo_t info) {
  String ip = WiFi.localIP().toString();
  printf("WIFI got IP: %s\n", ip.c_str());
  settings.channel = WiFi.channel();
}

static void setup_wifi() {
  //wifi_ap_mode=true;
  if (wifi_ap_mode) {
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

    if (settings.ssid) {
      Serial.printf("connecting to SSID: %s  psk: %s\n", settings.ssid.c_str(), settings.psk.c_str());

      // setting a custom "country" locks the wifi in a particular channel
      wifi_country_t myWiFi;
      //Country code (cc) set to 'X','X','X' is the standard, apparently.
      myWiFi.cc[0] = 'X';
      myWiFi.cc[1] = 'X';
      myWiFi.cc[2] = 'X';
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

float lpwind_dir = NAN, wind_knots = 0;

void lowpass_direction(float dir) {
  if (isnan(dir)) {
    lpwind_dir = dir;
    return;
  }

  float ldir = dir;
  if (ldir > 360)
    ldir -= 360;
  // lowpass wind direction
  if (lpwind_dir - ldir > 180)
    ldir += 360;
  else if (ldir - lpwind_dir > 180)
    ldir -= 360;
  //  Serial.printf("%x %f %f\n", packet->id, ldir, lpwind_dir);

  const float lp = .2;
  if (isnan(lpwind_dir))
    lpwind_dir = dir;
  else if (fabs(lpwind_dir - ldir) < 180)  // another test which should never fail
    lpwind_dir = lp * ldir + (1 - lp) * lpwind_dir;

  lpwind_dir = resolv(lpwind_dir);
}

static int message_count;

struct esp_now_data_t {
  uint8_t mac[6];
  int len;
  uint8_t data[MAX_DATA_LEN];
} esp_now_rb[256];
uint8_t rb_start, rb_end;
SemaphoreHandle_t esp_now_sem;

static void DataRecvWind(struct esp_now_data_t &data) {
  if (data.len != sizeof(wind_packet_t))
    printf("received invalid wind packet %d\n", data.len);

  uint16_t crc = crc16(data.data, data.len - 2);
  wind_packet_t *packet = (wind_packet_t *)data.data;
  if (crc != packet->crc16) {
    Serial.printf("espnow wind data packet crc failed %x %x\n", crc, packet->crc16);
    return;
  }

  float dir;
  if (packet->angle == 0xffff)
    dir = NAN;  // invalid
  else
    dir = 360 - packet->angle * 360.0f / 16384.0f;

  if (packet->period > 100)  // period of 100uS cups is about 193 knots for 100 rotations/s
    //        knots = 19350.0 / packet->period;
    wind_knots = 25800.0 / packet->period;  // corrected from preliminary calibration
  else
    wind_knots = 0;

  if (settings.compensate_wind_with_accelerometer && dir >= 0) {
    unsigned long t = millis();
    static int lastt;
    float dt = (t - lastt) / 1000.0;
    lastt = t;

    // TODO: combine inertial sensors from autopilot somehow
    float accel_x = packet->accelx * 4.0f / 16384 * 9.8f * 1.944f;
    float accel_y = packet->accely * 4.0f / 16384 * 9.8f * 1.944f;

    static float vx, vy;
    vx += accel_x * dt;
    vy += accel_y * dt;

    vx *= .9;  // decay

    float wvx = wind_knots * cos(radians(dir));
    float wvy = wind_knots * sin(radians(dir));

    wvx -= vx;
    wvy -= vy;

    wind_knots = hypotf(wvx, wvy);
    dir = degrees(atan2f(wvx, wvy));
    if (dir < 0)
      dir += 360;
  }

  bool primary;
  wind_transmitter_t &wt = wind_transmitters.update(data.mac, primary, dir);
  wt.dir = resolv(dir + wt.offset);
  wt.knots = wind_knots;

  if (!primary)
    return;

  lowpass_direction(wt.dir);

  char buf[128];
  if (!isnan(lpwind_dir))
    snprintf(buf, sizeof buf, PSTR("MWV,%d.%02d,R,%d.%02d,N,A"), (int)lpwind_dir, (uint16_t)(lpwind_dir * 100.0f) % 100U, (int)wind_knots, (int)(wind_knots * 100) % 100);
  else  // invalid wind direction (no magnet?)
    snprintf(buf, sizeof buf, PSTR("MWV,,R,%d.%02d,N,A"), (int)wind_knots, (int)(wind_knots * 100) % 100);

  nmea_send(buf);

  message_count++;
  //printf("lpwind_dir %f\n", lpwind_dir);

  if (settings.wifi_data == SIGNALK) {
    if (!isnan(lpwind_dir))
      signalk_send("environment.wind.angleApparent", M_PI / 180.0f * lpwind_dir);
    signalk_send("environment.wind.speedApparent", wind_knots * .514444f);
  }

  display_data_update(WIND_SPEED, wind_knots, ESP_DATA);
  if (!isnan(lpwind_dir))
    display_data_update(WIND_DIRECTION, lpwind_dir, ESP_DATA);
}


static void DataRecvAir(struct esp_now_data_t &data) {
  if (data.len != sizeof(air_packet_t))
    printf("received invalid air packet %d\n", data.len);

  uint16_t crc = crc16(data.data, data.len - 2);
  air_packet_t *packet = (air_packet_t *)data.data;
  if (crc != packet->crc16) {
    Serial.printf("espnow air data packet crc failed %x %x\n", crc, packet->crc16);
    return;
  }

  bool primary;
  air_transmitter_t &wt = air_transmitters.update(data.mac, primary);
  wt.pressure = 1.0f + packet->pressure * .00001f;
  wt.rel_humidity = packet->rel_humidity * .01f;
  wt.temperature = packet->temperature * .01f;
  wt.air_quality = packet->air_quality * .01f;

  if (!primary)
    return;

  char buf[128];
  snprintf(buf, sizeof buf, "MDA,,,%.5f,B,,,,,%.2f,,,,,,,,,,,", wt.pressure, wt.rel_humidity);
  nmea_send(buf);

  snprintf(buf, sizeof buf, "MTA,%.2f,C", wt.temperature);
  nmea_send(buf);

  if (settings.wifi_data == SIGNALK) {
    signalk_send("environment.pressure", wt.pressure * 100000);
    signalk_send("environment.relativeHumidity", wt.rel_humidity / 100);
    signalk_send("environment.temperature", wt.temperature);
  }

  display_data_update(BAROMETRIC_PRESSURE, wt.pressure, ESP_DATA);
  display_data_update(RELATIVE_HUMIDITY, wt.rel_humidity, ESP_DATA);
  display_data_update(AIR_TEMPERATURE, wt.temperature, ESP_DATA);
  display_data_update(AIR_QUALITY, wt.air_quality, ESP_DATA);
}

static void DataRecvWater(struct esp_now_data_t &data) {
  if (data.len != sizeof(water_packet_t))
    printf("received invalid water packet %d\n", data.len);

  uint16_t crc = crc16(data.data, data.len - 2);
  water_packet_t *packet = (water_packet_t *)data.data;
  if (crc != packet->crc16) {
    Serial.printf("espnow air data packet crc failed %x %x\n", crc, packet->crc16);
    return;
  }

  bool primary;
  water_transmitter_t &wt = water_transmitters.update(data.mac, primary);
  wt.speed = packet->speed * .01f;
  wt.depth = packet->depth * .01f;
  wt.temperature = packet->temperature * .01f;

  if (!primary)
    return;

  char buf[128];
  snprintf(buf, sizeof buf, "VHW,,,,,%.2f,N,,", wt.speed);
  nmea_send(buf);

  snprintf(buf, sizeof buf, "DBT,,,%.5f,M,,", wt.depth);
  nmea_send(buf);

  snprintf(buf, sizeof buf, "MTW,%.2f,C", wt.temperature);
  nmea_send(buf);

  if (settings.wifi_data == SIGNALK) {
    signalk_send("navigation.speedThroughWater", wt.speed);
    signalk_send("environment.depth.belowSurface", wt.depth);
    signalk_send("environment.water.temperature", wt.temperature);
  }

  display_data_update(WATER_SPEED, wt.speed, ESP_DATA);
  display_data_update(WATER_TEMPERATURE, wt.temperature, ESP_DATA);
  display_data_update(DEPTH, wt.depth, ESP_DATA);
}

static void DataRecvLightningUV(struct esp_now_data_t &data) {
  if (data.len != sizeof(lightning_uv_packet_t))
    printf("received invalid lightning UV packet %d\n", data.len);

  uint16_t crc = crc16(data.data, data.len - 2);
  lightning_uv_packet_t *packet = (lightning_uv_packet_t *)data.data;
  if (crc != packet->crc16) {
    Serial.printf("espnow air data packet crc failed %x %x\n", crc, packet->crc16);
    return;
  }

  bool primary;
  lightning_uv_transmitter_t &wt = lightning_uv_transmitters.update(data.mac, primary);
  wt.distance = packet->distance * .01f;

  if (!primary)
    return;

  // no nmea or signalk to send

  //display_data_update(LIGHTNING_DISTANCE, wt.distance, ESP_DATA);
  printf("LIGHTNING!!! %f", wt.distance);
}

// callback when data is recv from Master
// This function is run from the wifi thread, so post to a queue
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
  if (xSemaphoreTake(esp_now_sem, (TickType_t)10) == pdTRUE) {
    esp_now_data_t &pos = esp_now_rb[rb_start];
    memcpy(pos.mac, mac_addr, 6);
    pos.len = data_len;
    if (data_len <= MAX_DATA_LEN)
      memcpy(pos.data, data, data_len);
    rb_start++;
    xSemaphoreGive(esp_now_sem);
  } else
    printf("unable to get esp now semaphore");
}

// Init ESP Now with fallback
static void InitESPNow() {
  printf("InitESPNow %d\n", chip.channel);
  esp_wifi_set_channel(chip.channel, WIFI_SECOND_CHAN_NONE);
  if (esp_now_init() == ESP_OK) {
    Serial.println("ESPNow Init Success");
  } else {
    Serial.println("ESPNow Init Failed");
    ESP.restart();  // restart cpu on failure
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

static void receive_esp_now() {
  while (rb_start != rb_end) {
    if (xSemaphoreTake(esp_now_sem, (TickType_t)10) != pdTRUE)
      return;
    esp_now_data_t first = esp_now_rb[rb_end++];  // copy struct
    xSemaphoreGive(esp_now_sem);

    if (first.len > MAX_DATA_LEN) {
      printf("espnow wrong packet size %d", first.len);
      continue;
    }

    uint16_t id = *(uint16_t *)first.data;
    switch (id) {
      case WIND_ID: DataRecvWind(first); break;
      case AIR_ID: DataRecvAir(first); break;
      case WATER_ID: DataRecvWater(first); break;
      case LIGHTNING_UV_ID: DataRecvLightningUV(first); break;
      default:
        printf("espnow packet ID mismatch %x\n", id);
    }
  }
}

void sendChannel() {
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

static std::map<String, int> wifi_networks;
String wifi_networks_html;
static uint32_t scanning = 0;
void wireless_scan_networks() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  int32_t n = WiFi.scanNetworks(true);
  scanning = millis();
  if (!scanning) scanning++;
  // display on screen scanning for wifi networks progress
}

void wireless_poll() {
  uint32_t t0 = millis();
  static uint32_t tl;
  if (t0 - tl > 60000) {  // report memory statistics every 60 seconds
    Serial.printf("pypilot_mfd %d %d %d %d\n", t0 / 1000, ESP.getFreeHeap(), ESP.getHeapSize(), message_count);
    tl = t0;
    message_count = 0;
  }

  receive_esp_now();

  uint32_t t = millis();
  if (t - scanning < 5000)
    return;
  scanning = 0;
  int n = WiFi.scanComplete();
  for (uint8_t i = 0; i < n; i++) {
    String ssid = WiFi.SSID(i);
    int channel = WiFi.channel(i);
    if (ssid == settings.ssid)
      settings.channel = channel;
    if (wifi_networks.find(ssid) == wifi_networks.end()) {
      wifi_networks[ssid] = channel;
      String tr = "<tr><td>" + ssid + "</td><td>" + channel + "</td>";
      tr += String("<td>") + WiFi.RSSI(i) + "</td><td>";
      //tr += WiFi.encryptionType(i) == ENC_TYPE_NONE ? "open" : "encrypted";
      tr += "</td></tr>";
      printf("wifi network %s\n", tr.c_str());
      wifi_networks_html += tr;
    }
  }
  WiFi.scanDelete();
  setup_wifi();
}

void wireless_scan_devices() {
  for (int c = 0; c <= 12; c++) {
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
      for (int i = 0; i < 15; i++) {
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

void wireless_toggle_mode() {
  uint32_t t0 = millis();
  static int last_wifi_toggle_time;
  if (t0 - last_wifi_toggle_time > 2000) {
    wifi_ap_mode = !wifi_ap_mode;
    last_wifi_toggle_time = t0;
    setup_wifi();
  }
}

void wireless_setup() {
  memset(&chip, 0, sizeof(chip));
  for (int ii = 0; ii < 6; ++ii)
    chip.peer_addr[ii] = (uint8_t)0xff;
  chip.channel = settings.channel;
  if (chip.channel > 12)
    chip.channel = 1;

  chip.channel = 6;
  chip.encrypt = 0;

  // Init ESPNow with a fallback logic
  esp_now_sem = xSemaphoreCreateMutex();

  InitESPNow();

  setup_wifi();
}

String position_str(sensor_position p)
{
    switch(p) {
       case PRIMARY:   return "Primary";
       case SECONDARY: return "Secondary";
       case PORT:      return "Port";
       case STARBOARD: return "Starboard";
       case IGNORED:   return "Ignored";
    }
    return "Invalid";
}

String wireless_json_sensors()
{
    uint32_t t = millis();
    JSONVar windsensors;
    for(std::map<uint64_t, wind_transmitter_t>::iterator i = wind_transmitters.macs.begin(); i != wind_transmitters.macs.end(); i++) {
        JSONVar jwt;
        wind_transmitter_t &wt = i->second;

        jwt["position"] = position_str(wt.position);
        jwt["offset"] = wt.offset;
        jwt["dir"] = wt.dir;
        jwt["knots"] = wt.knots;
        jwt["dt"] = t - wt.t;
        windsensors[mac_int_to_str(i->first)] = jwt;
    }

    JSONVar sensors;
    if(wind_transmitters.macs.size() > 0)
        sensors["wind"] = windsensors;

    return JSON.stringify(sensors);
}

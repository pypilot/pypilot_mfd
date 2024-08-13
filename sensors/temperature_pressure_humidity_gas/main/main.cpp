/* Copyright (C) 2024 Sean D'Epagnier <seandepagnier@gmail.com>
 *
 * This Program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 */

// note 80mhz cpu speed is plenty!

#include <esp_now.h>
#include <esp_wifi.h>
#include <WiFi.h>
#include <Wire.h>

#include <EEPROM.h>

#include "bme680.h"

#define PIN_VCC  33
#define PIN_2V5  32
#define PIN_1V24 39

// Global copy of chip
esp_now_peer_info_t chip;

// data in packet
#define AIR_ID 0xa41b
#define CHANNEL_ID 0x0a21

struct air_packet_t {
  uint16_t id;
  int16_t pressure;       // in pascals + 100,000,  eg 0 is 1000mbar,  1 is 1000.01 mbar
  int16_t temperature;    // in 100th/deg C
  uint16_t rel_humidity;  // in 100th %
  uint16_t air_quality;   // in index
  uint16_t reserved;      // future data (0 for now)
  uint16_t voltage; // in 100th of volts
  uint16_t crc16;
} __attribute__((packed));

air_packet_t packet;

struct packet_channel_t {
    uint16_t id;             // packet identifier
    uint16_t channel;
    uint16_t crc16;
};

void reboot()
{
    Serial.println("Reset ESP");
    delay(30);
    ESP.restart();  // reset
}

// Init ESP Now with fallback
void InitESPNow()
{
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    esp_wifi_set_channel(chip.channel, WIFI_SECOND_CHAN_NONE);
    if (esp_now_init() == ESP_OK) {
        //Serial.println("ESPNow Init Success");
    } else {
        Serial.println("ESPNow Init Failed");
        reboot();
    }

    esp_now_register_recv_cb(OnDataRecv);

    // chip not paired, attempt pair
    esp_err_t addStatus = esp_now_add_peer(&chip);
    if (addStatus == ESP_OK)
    // Pair successf
    // Serial.println("Pair success");
        return;
    else if (addStatus == ESP_ERR_ESPNOW_NOT_INIT)
    // How did we get so far!!
        Serial.println("ESPNOW Not Init");
    else if (addStatus == ESP_ERR_ESPNOW_ARG)
        Serial.println("Invalid Argument");
    else if (addStatus == ESP_ERR_ESPNOW_FULL)
        Serial.println("Peer list full");
    else if (addStatus == ESP_ERR_ESPNOW_NO_MEM)
        Serial.println("Out of memory");
    else if (addStatus == ESP_ERR_ESPNOW_EXIST)
        Serial.println("Peer Exists");
    else
        Serial.println("Not sure what happened");
    reboot();
}

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

// send data
void sendData()
{
    // compute checksum
    packet.crc16 = crc16((uint8_t *)&packet, sizeof(packet) - 2);

    const uint8_t *peer_addr = chip.peer_addr;
    //Serial.print("Sending...");
    esp_err_t result = esp_now_send(peer_addr, (uint8_t *)&packet, sizeof(packet));
    //Serial.print("Send Status: ");
    if (result == ESP_OK) {
        //Serial.println("Success");
        return;
    } else if (result == ESP_ERR_ESPNOW_NOT_INIT)
        // How did we get so far!!
        Serial.println("ESPNOW not Init.");
    else if (result == ESP_ERR_ESPNOW_ARG)
       Serial.println("Invalid Argument");
    else if (result == ESP_ERR_ESPNOW_INTERNAL)
        Serial.println("Internal Error");
    else if (result == ESP_ERR_ESPNOW_NO_MEM)
        Serial.println("ESP_ERR_ESPNOW_NO_MEM");
    else if (result == ESP_ERR_ESPNOW_NOT_FOUND)
        Serial.println("Peer not found.");
    else
        Serial.println("Not sure what happened");

    reboot();
}

// callback when data is sent from Master to chip
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status)
{
    char macStr[18];
    snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
             mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
    Serial.print("Last Packet Sent to: ");
    Serial.println(macStr);
    Serial.print("Last Packet Send Status: ");
    Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}



void OnDataRecv(const uint8_t *mac_addr, const uint8_t *data, int data_len)
{
    if(data_len != sizeof(packet_channel_t)) {
        //Serial.println("wrong packet size");
        return;
    }

    packet_channel_t *packet = (packet_channel_t*)data;
    if(packet->id != CHANNEL_ID) {
        //Serial.println("ID mismatch");
    }

    uint16_t crc = crc16(data, data_len-2);
    if(crc != packet->crc16) {
        Serial.printf("crc failed %x %x\n", crc, packet->crc16);
            return;
    }

    if(chip.channel != packet->channel) {
        chip.channel = packet->channel;
        EEPROM.put(0, packet->channel);
        EEPROM.commit();
    }
}

void setup()
{
    delay(500);
    Serial.begin(115200);
    //Set device in STA mode to begin with
    Serial.println("BME680 Transmitter");
  
    memset(&chip, 0, sizeof(chip));
    for (int ii = 0; ii < 6; ++ii)
        chip.peer_addr[ii] = (uint8_t)0xff;
    EEPROM.begin(sizeof chip.channel);
    EEPROM.get(0, chip.channel);  // pick a channel
    EEPROM.end();
    if(chip.channel > 12 || chip.channel == 0)
        chip.channel = 6;

    chip.encrypt = 0;        // no encryption

    packet.id = AIR_ID;  // initialize packet ID

    pinMode(0, INPUT);

    adcAttachPin(26);

    // adc
       /* adcAttachPin(PIN_2V5);
        adcAttachPin(PIN_1V24);
        adcAttachPin(PIN_VCC);*/

    Wire.begin();
    bme680_setup();
    Serial.println("setup complete");
}

void loop()
{
   // printf("loop\n");
    unsigned int t0 = millis();
    InitESPNow();
    unsigned int t1 = millis();
    bme680_read();


    packet.pressure = int16_t(pressure - 100000);
    packet.temperature = int16_t((temperature)*100);
    packet.rel_humidity = uint16_t(rel_humidity*100);
    packet.air_quality = uint16_t(air_quality*100);
    packet.reserved = 0;

    sendData();
    // Prepare for sleep
    int val2v5 = analogRead(PIN_2V5);
    int val1v24 = analogRead(PIN_1V24);
    int valvcc = analogRead(PIN_VCC);

    val2v5 = 2800;

    //2.5 = scale * val2v5 + offset;
    //1.24 = scale * val1v24 + offset;
    //1.26 = scale*(val2v5 - val1v24)
    float scale = (2.5f-1.24f) / (val2v5 - val1v24);
    float offset = 2.5f - scale*val2v5;

    float vin = scale * valvcc + offset;
    // vin = 10/91 * voltage;
    float voltage = 91/10.0f * vin;
   // printf("values %d %d %d %d %f %f %f\n", analogRead(26), val2v5, val1v24, valvcc, voltage, scale, offset);
    packet.voltage = voltage * 100;


    delay(10); //Short delay to finish transmit before esp_now_deinit()
    if (esp_now_deinit() != ESP_OK)   //De-initialize ESP-NOW. (icall information of paired devices will be deleted)
        Serial.println("esp_now_deinit() failed"); 
    WiFi.mode(WIFI_OFF);
    int period = 10000;
    for(;;) {
        unsigned int tx = millis();
        int d = tx - t0;

        if (d >= period)
            break;

        d = period - d;
#if 0
        delay(d);
#else
        gpio_wakeup_enable(GPIO_NUM_34, digitalRead(34) ? GPIO_INTR_LOW_LEVEL : GPIO_INTR_HIGH_LEVEL);
    	esp_sleep_enable_gpio_wakeup();
        esp_sleep_enable_timer_wakeup(d * 1000);  //light sleep 100ms
        esp_light_sleep_start();
#endif
   //     printf("sleep time %d %d\n", d,  millis()-tx);
    }
#if 0 // for debugging
    Serial.printf("%d %d %d %d %d %d\n", packet.pressure, packet.temperature, packet.rel_humidity, packet.air_quality, t1 - t0, t1 - t0);
#endif
}

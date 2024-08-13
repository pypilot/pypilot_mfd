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

#include "driver/i2c.h"
#include <SPI.h>
#include <EEPROM.h>
#include <Wire.h>

// Global copy of chip
esp_now_peer_info_t chip;

// data in packet
#define ID 0xf179
#define CHANNEL_ID 0x0a21

struct packet_t {
    uint16_t id;             // packet identifier
    uint16_t angle;          // angle in 14 bits (or 0xffff for invalid)
    uint16_t period;         // time of rotation of cups in units of 100uS
    int16_t accelx, accely;  // acceleration in 4g range
    uint16_t crc16;   // not strictly required but extra check to ensure correct application
} __attribute__((packed));

packet_t packet;

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

volatile unsigned int rotation_count;
volatile uint16_t lastperiod;

void IRAM_ATTR isr_anemometer_count()
{
    static uint16_t lastt;
    uint16_t t = millis();
    uint16_t period = t - lastt;
    if (period > 10) {
        lastt = t;
        lastperiod += period;
        rotation_count++;
    }
}

#define DEVICE_ADDRESS 0x19  // 0x32  // LIS2DW12TR
#define LIS2DW12_CTRL 0x1F   // CTRL1 is CTRL+1
/*
void i2c_init()
{
    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = GPIO_NUM_21;  // the GPIO pin connected to SDA
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_io_num = GPIO_NUM_22;  // the GPIO pin connected to SCL
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = 100000;  // desired I2C clock frequency

    i2c_param_config(I2C_NUM_0, &conf);
    i2c_driver_install(I2C_NUM_0, conf.mode, 0, 0, 0);

    i2c_write(LIS2DW12_CTRL + 1, 0x44);  // high performance
    i2c_write(LIS2DW12_CTRL + 6, 0x10);  // set +/- 4g FS, LP filter ODR/2
    // enable block data update (bit 3) and auto register address increment (bit 2)
    i2c_write(LIS2DW12_CTRL + 2, 0x08 | 0x04);
}

void i2c_write(uint8_t address, uint8_t data) {
    uint8_t d[2] = { address, data };
    // i2c_master_write_to_device(I2C_NUM_0, DEVICE_ADDRESS, d, 2, pdMS_TO_TICKS(1000));
}

void i2c_read()
{
    uint8_t data[6] = { 0 };
    i2c_cmd_handle_t cmd;

    // Create I2C command to read 6 bytes from the device
    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (DEVICE_ADDRESS << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, 0x28, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (DEVICE_ADDRESS << 1) | I2C_MASTER_READ, true);
    i2c_master_read(cmd, data, 6, I2C_MASTER_LAST_NACK);
    i2c_master_stop(cmd);

    // Execute the command
    i2c_master_cmd_begin(I2C_NUM_0, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);

    // For example, print it to the console
    Serial.println("Received i2c: ");
    for (int i = 0; i < 6; i++) {
        Serial.print(data[i]);
        Serial.print(" ");
    }
    Serial.println();

    packet.accelx = (data[0] << 8) | data[1];
    //packet.accelz = (data[2] << 8) | data[3];
    packet.accely = (data[4] << 8) | data[5];
}
*/
bool parityCheck(uint16_t data)
{
    data ^= data >> 8;
    data ^= data >> 4;
    data ^= data >> 2;
    data ^= data >> 1;

    return (~data) & 1;
}

#define MT6816_CS 5
void MT6816_read()
{
    digitalWrite(MT6816_CS, LOW);
    uint16_t angle = SPI.transfer16(0x8300) << 8;
    digitalWrite(MT6816_CS, HIGH);

    digitalWrite(MT6816_CS, LOW);
    angle |= SPI.transfer16(0x8400) & 0xff;
    digitalWrite(MT6816_CS, HIGH);

    if (!parityCheck(angle)) {
        Serial.println("parity failed");
        return;
    }

    if (angle & 0x2)
        packet.angle = 0xffff;
    else
        packet.angle = angle >> 2;
}

static portMUX_TYPE my_mutex;

void convert_speed()
{
    static portMUX_TYPE _spinlock = portMUX_INITIALIZER_UNLOCKED;

    taskENTER_CRITICAL(&_spinlock);
    uint16_t period = lastperiod;
    uint16_t rcount = rotation_count;
    rotation_count = 0;
    lastperiod = 0;
    taskEXIT_CRITICAL(&_spinlock);

#if 0
    if(rcount)
        printf("period count %d %d\n", period, rcount);
#endif
    static uint16_t nowindcount;
    const int nowindtimeout = 30;
    if (rcount) { 
        if (nowindcount != nowindtimeout && period < 6000)
            packet.period = period*10/rcount;  // packet period is units of 100uS   
        nowindcount = 0;
    } else {
        if (nowindcount < nowindtimeout)
            nowindcount++;
        else
            packet.period = 0;
    }
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

static void setup_accel()
{
    // init communication with accelerometer
//    Wire.setClock(10000);
    Wire.begin();
    Wire.beginTransmission(DEVICE_ADDRESS);
    Wire.write(LIS2DW12_CTRL + 1);
    Wire.write(0x44);  // high performance
    Wire.endTransmission();

    Wire.beginTransmission(DEVICE_ADDRESS);
    Wire.write(LIS2DW12_CTRL + 6);
    Wire.write(0x10);  // set +/- 4g FS, LP filter ODR/2
    Wire.endTransmission();

    // enable block data update (bit 3) and auto register address increment (bit 2)
    Wire.beginTransmission(DEVICE_ADDRESS);
    Wire.write(LIS2DW12_CTRL + 2);
    Wire.write(0x08 | 0x04);
    Wire.endTransmission();
}

void setup()
{
    Serial.begin(115200);
    //Set device in STA mode to begin with
    Serial.println("Wind Transmitter");
  
    memset(&chip, 0, sizeof(chip));
    for (int ii = 0; ii < 6; ++ii)
        chip.peer_addr[ii] = (uint8_t)0xff;
    EEPROM.begin(sizeof chip.channel);
    EEPROM.get(0, chip.channel);  // pick a channel
    EEPROM.end();
    if(chip.channel > 12 || chip.channel == 0)
        chip.channel = 6;

    chip.encrypt = 0;        // no encryption

    packet.id = ID;  // initialize packet ID

    // init communication with angular hall sensor
    SPI.begin();
    SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE3));
    pinMode(MT6816_CS, OUTPUT);
    digitalWrite(MT6816_CS, HIGH);

    pinMode(0, INPUT);

    // setup interrupt to read wind speed pulses
    const int interruptPin = 34;
    pinMode(interruptPin, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(interruptPin), isr_anemometer_count, RISING);

    setup_accel();
    Serial.println("setup complete");
}

void read_accel()
{
    Wire.beginTransmission(DEVICE_ADDRESS);
    Wire.write(0x28);
    Wire.endTransmission();
    Wire.requestFrom(DEVICE_ADDRESS, 6);
    int i = 0;
    uint8_t data[6] = { 0 };
    while (Wire.available() && i < 6)
        data[i++] = Wire.read();
        //printf("read %d\n", i);
    if (i != 6)
        return;

    packet.accelx = (data[3] << 8) | data[2];
    packet.accely = (data[5] << 8) | data[4];
}

void loop()
{
    unsigned int t0 = millis();
    InitESPNow();
    MT6816_read();
    read_accel();
    convert_speed();
    unsigned int t1 = millis();
    sendData();

    // Prepare for sleep
    delay(10); //Short delay to finish transmit before esp_now_deinit()
    if (esp_now_deinit() != ESP_OK)   //De-initialize ESP-NOW. (all information of paired devices will be deleted)
        Serial.println("esp_now_deinit() failed"); 
    WiFi.mode(WIFI_OFF);

    int period = 100;
    for(;;) {
        unsigned int tx = millis();
        int d = tx - t0;

        if (d >= period)
            break;

        d = period - d;
#if 1
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
    float angle = (packet.angle == 0xffff) ? NAN : packet.angle * 360.0 / 16384;
    Serial.printf("%.02f %.02f %d %.02f %d %d\n", packet.accelx * 4.0 / 32768, packet.accely * 4.0 / 32768, packet.period, angle, t1 - t0, t1 - t0);

#endif
    //printf("pos %d\n", digitalRead(34));
}

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

#include "Adafruit_VEML7700.h"
#include "SparkFun_AS7331.h"

bool have_lightning = false;
bool have_visible = false;
bool have_uv = false;

// Global copy of chip
esp_now_peer_info_t chip;

// data in packet
#define CHANNEL_ID 0x0a21

#define LIGHTNING_UV_ID 0xd255
struct lightning_uv_packet_t {
    uint16_t id;
    uint16_t lightning_distance, lightning_energy;
    uint16_t uva, uvb, uvc;
    uint32_t visible;
    uint16_t crc16;
} __attribute__((packed));

lightning_uv_packet_t packet;

struct packet_channel_t {
    uint16_t id;             // packet identifier
    uint16_t channel;
    uint16_t crc16;
};

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

void reboot()
{
    Serial.println("Reset ESP");
    delay(30);
    abort();
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

void OnDataRecv(const esp_now_recv_info *recv_info, const uint8_t *data, int data_len){
    //uint8_t *mac_addr = recv_info->src_addr;

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


#include "SparkFun_AS3935.h"

#define INDOOR 0x12 
#define OUTDOOR 0xE
#define LIGHTNING_INT 0x08
#define DISTURBER_INT 0x04
#define NOISE_INT 0x01

// SPI
SparkFun_AS3935 lightning;


// Interrupt pin for lightning detection 
const int lightningInt = 12; 
// Chip select pin 
int spiCS = 5; 

// Values for modifying the IC's settings. All of these values are set to their
// default values. 
byte noiseFloor = 2;
byte watchDogVal = 2;
byte spike = 2;
byte lightningThresh = 1; 


// This variable holds the number representing the lightning or non-lightning
// event issued by the lightning detector. 
byte intVal = 0; 

static void setup_lightning_sensor()
{
      pinMode(lightningInt, INPUT); 

    // init communication with angular hall sensor
    SPI.begin();
  if( !lightning.beginSPI(spiCS) ) { 
    Serial.println ("Lightning Detector did not start up, freezing!"); 
    return;
  }
  //else
      //    Serial.println("Schmow-ZoW, Lightning Detector Ready!\n");

  // "Disturbers" are events that are false lightning events. If you find
  // yourself seeing a lot of disturbers you can have the chip not report those
  // events on the interrupt lines. 

  lightning.maskDisturber(true); 

  int maskVal = lightning.readMaskDisturber();
/*  Serial.print("Are disturbers being masked: "); 
  if (maskVal == 1)
    Serial.println("YES"); 
  else if (maskVal == 0)
  Serial.println("NO"); */

  // The lightning detector defaults to an indoor setting (less
  // gain/sensitivity), if you plan on using this outdoors 
  // uncomment the following line:

  lightning.setIndoorOutdoor(OUTDOOR); 

  int enviVal = lightning.readIndoorOutdoor();
/*  Serial.print("Are we set for indoor or outdoor: ");  
  if( enviVal == INDOOR )
    Serial.println("Indoor.");  
  else if( enviVal == OUTDOOR )
    Serial.println("Outdoor.");  
  else 
    Serial.println(enviVal, BIN); 
*/
  // Noise floor setting from 1-7, one being the lowest. Default setting is
  // two. If you need to check the setting, the corresponding function for
  // reading the function follows.    

  lightning.setNoiseLevel(noiseFloor);  

  int noiseVal = lightning.readNoiseLevel();
//  Serial.print("Noise Level is set at: ");
//  Serial.println(noiseVal);

  // Watchdog threshold setting can be from 1-10, one being the lowest. Default setting is
  // two. If you need to check the setting, the corresponding function for
  // reading the function follows.    

  lightning.watchdogThreshold(watchDogVal); 

  int watchVal = lightning.readWatchdogThreshold();
//  Serial.print("Watchdog Threshold is set to: ");
//  Serial.println(watchVal);

  // Spike Rejection setting from 1-11, one being the lowest. Default setting is
  // two. If you need to check the setting, the corresponding function for
  // reading the function follows.    
  // The shape of the spike is analyzed during the chip's
  // validation routine. You can round this spike at the cost of sensitivity to
  // distant events. 

  lightning.spikeRejection(spike); 

  int spikeVal = lightning.readSpikeRejection();
//  Serial.print("Spike Rejection is set to: ");
//  Serial.println(spikeVal);


  // This setting will change when the lightning detector issues an interrupt.
  // For example you will only get an interrupt after five lightning strikes
  // instead of one. Default is one, and it takes settings of 1, 5, 9 and 16.   
  // Followed by its corresponding read function. Default is zero. 

  lightning.lightningThreshold(lightningThresh); 

  uint8_t lightVal = lightning.readLightningThreshold();
//  Serial.print("The number of strikes before interrupt is triggerd: "); 
//  Serial.println(lightVal); 

  // When the distance to the storm is estimated, it takes into account other
  // lightning that was sensed in the past 15 minutes. If you want to reset
  // time, then you can call this function. 

  //lightning.clearStatistics(); 

  // The power down function has a BIG "gotcha". When you wake up the board
  // after power down, the internal oscillators will be recalibrated. They are
  // recalibrated according to the resonance frequency of the antenna - which
  // should be around 500kHz. It's highly recommended that you calibrate your
  // antenna before using these two functions, or you run the risk of schewing
  // the timing of the chip. 

  //lightning.powerDown(); 
  //delay(1000);
  //if( lightning.wakeUp() ) 
   // Serial.println("Successfully woken up!");  
  //else 
    //Serial.println("Error recalibrating internal osciallator on wake up."); 
  
  // Set too many features? Reset them all with the following function.
  // lightning.resetSettings();
  have_lightning = true;
}

Adafruit_VEML7700 veml = Adafruit_VEML7700();
SfeAS7331ArdI2C myUVSensor;

volatile bool newDataReady = false;

void IRAM_ATTR dataReadyInterrupt() {
    newDataReady = true;
}

static RTC_DATA_ATTR uint8_t lux_gain = 0;
const uint8_t veml_gains[] = {VEML7700_GAIN_1_8, VEML7700_GAIN_1_4,
    VEML7700_GAIN_1, VEML7700_GAIN_2};

static void setup_visible_light_sensor()
{
    // init communication with accelerometer
//    Wire.setClock(10000);
    if(lux_gain > 3)
        lux_gain = 3;
    if (!veml.begin(veml_gains[lux_gain])) {
        Serial.println("VELM7700 Sensor not found");
        return;
    }

    have_visible = true;
    
//    Serial.println("Sensor began.");
}

void setup_uv_light_sensor()
{
  // Initialize sensor and run default setup.
  if(myUVSensor.begin() == false) {
    Serial.println("UV Sensor failed to begin. Please check your wiring!");
    return;
  }


// Configure Interrupt.
  pinMode(13, INPUT);
  attachInterrupt(digitalPinToInterrupt(13), dataReadyInterrupt, RISING);
  // Set the delay between measurements so that the processor can read out the 
  // results without interfering with the ADC.
  // Set break time to 900us (112 * 8us) to account for the time it takes to poll data.
  if(kSTkErrOk != myUVSensor.setBreakTime(112)) {
    Serial.println("Sensor did not set break time properly.");
    return;
  }

  // Set measurement mode and change device operating mode to measure.
  if(myUVSensor.prepareMeasurement(MEAS_MODE_CMD) == false) {
    Serial.println("Sensor did not get set properly.");
    return;
  }

  Serial.println("Set mode to continuous. Starting measurement...");

  // Begin measurement.
  if(kSTkErrOk != myUVSensor.setStartState(true)) {
      Serial.println("Error starting reading!");
      return;
  }

  have_uv = true;
}

void setup()
{
    Wire.begin();
    setup_uv_light_sensor();
    setup_visible_light_sensor();

//    Serial.begin(115200);

    pinMode(34, INPUT_PULLUP); 
  
    memset(&chip, 0, sizeof(chip));
    for (int ii = 0; ii < 6; ++ii)
        chip.peer_addr[ii] = (uint8_t)0xff;
//    EEPROM.begin(sizeof chip.channel);
//    EEPROM.get(0, chip.channel);  // pick a channel
//    EEPROM.end();
//    if(chip.channel > 12 || chip.channel == 0)
        chip.channel = 6;

    chip.encrypt = 0;        // no encryption

    packet.id = LIGHTNING_UV_ID;  // initialize packet ID

    setup_lightning_sensor();

}


void read_lightning()
{    
    packet.lightning_distance = packet.lightning_energy = 0;
    if(digitalRead(lightningInt) != HIGH)
        return;

    // Hardware has alerted us to an event, now we read the interrupt register
    // to see exactly what it is. 
    intVal = lightning.readInterruptReg();
    if(intVal == NOISE_INT){
      Serial.println("Noise."); 
    }
    else if(intVal == DISTURBER_INT){
      Serial.println("Disturber."); 
    }
    else if(intVal == LIGHTNING_INT){
      Serial.println("Lightning Strike Detected!"); 
      // Lightning! Now how far away is it? Distance estimation takes into
      // account previously seen events. 
      byte distance = lightning.distanceToStorm(); 
      Serial.print("Approximately: "); 
      Serial.print(distance); 
      Serial.println("km away!"); 

          packet.lightning_distance = distance;

      // "Lightning Energy" and I do place into quotes intentionally, is a pure
      // number that does not have any physical meaning. 
      long lightEnergy = lightning.lightningEnergy(); 
      Serial.print("Lightning Energy: "); 
      Serial.println(lightEnergy); 

          packet.lightning_energy = lightEnergy>>4;

    }
//    SPI.end();
    //lightning.powerDown(); 


//    pinMode(18, INPUT_PULLUP);
//    pinMode(19, INPUT_PULLUP);
//    pinMode(23, INPUT_PULLUP);
}


void read_visible_light()
{
    // to read lux using automatic method, specify VEML_LUX_AUTO
    //    float lux = veml.readLux(VEML_LUX_AUTO);
    uint16_t ALS = veml.readALS(false);
    float lux = veml.computeLux(ALS, true);
    
    packet.visible = lux*10;

    printf("Lux = %d %u %f %ld\n", lux_gain, ALS, lux, millis());

    if(ALS <= 100) {
        if(lux_gain == 3)
            return;
        lux_gain++;
    } else if(ALS > 1000) {
        if(lux_gain == 0)
            return;
        lux_gain--;
    } else
        return;

    /*
    veml.setGain(veml_gains[lux_gain]);
    delay(40);
    read_visible_light();
    */
}

void read_uv_light()
{
    // If an interrupt has been generated...
    if(!newDataReady) {
        printf("no uv data ready\n");
        return;
    }
    printf("uv  isr %d\n",  digitalRead(13));

    newDataReady = false;

    if(kSTkErrOk != myUVSensor.readAllUV())
      Serial.println("Error reading UV.");
  
    float uva = myUVSensor.getUVA();
    float uvb = myUVSensor.getUVB();
    float uvc = myUVSensor.getUVC();


    printf("hve UV %f %f %f\n", uva, uvb, uvc);

    packet.uva = uva;
    packet.uvb = uvb;
    packet.uvc = uvc;
}
#include "esp_sleep.h"
#include "driver/rtc_io.h"

void loop()
{
    if(have_lightning)
        read_lightning();

    unsigned int t0 = millis();
    InitESPNow();
    unsigned int t1 = millis();
    if(have_visible)
        read_visible_light();

    if(have_uv) {
        read_uv_light();
        bool t = true;
        myUVSensor.setPowerDownState(t);
    }
    sendData();

    // Prepare for sleep
    delay(10); //Short delay to finish transmit before esp_now_deinit()
    if (esp_now_deinit() != ESP_OK)   //De-initialize ESP-NOW. (all information of paired devices will be deleted)
        Serial.println("esp_now_deinit() failed"); 
    WiFi.mode(WIFI_OFF);


#if 1
        // for testing  seconds
            esp_sleep_enable_timer_wakeup(3 * 1000 * 1000);
            printf("sleeping %ld\n", millis());
            delay(100);
#else
        // sleep 60s if there was light, else 10 minute
        if(packet.visible < 100)
            esp_sleep_enable_timer_wakeup(10 * 60 * 1000 * 1000);
        else
            esp_sleep_enable_timer_wakeup(60 * 1000 * 1000);
#endif


        esp_deep_sleep_start();

   //     printf("sleep time %d %d\n", d,  millis()-tx);
}

/* Copyright (C) 2024 Sean D'Epagnier <seandepagnier@gmail.com>
 *
 * This Program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 */

// note 80mhz cpu speed is plenty!
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"

#include <math.h>
#include <string.h>

#include "esp_log.h"
#include "esp_netif.h"
#include "esp_sleep.h"
#include <esp_now.h>
#include <esp_wifi.h>
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_timer.h"

#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "soc/gpio_struct.h"
#include "soc/gpio_reg.h"

const gpio_num_t interruptPin = GPIO_NUM_34;

// data in packet
//#define I2C_MASTER  // nonblocking i2c, can read accel while booting wifi should save 1ms per loop (slightly less power??)
//#define LOW_POWER   // power down wireless while not working (save a lot of power?)
#define DEBUG

// Global copy of chip
esp_now_peer_info_t chip;

#define ID 0xf179
#define LIS2DW_ADDRESS0 0x18
#define LIS2DW_ADDRESS1 0x19

struct packet_t {
    uint16_t id;             // packet identifier
    uint16_t angle;          // angle in 14 bits (or 0xffff for invalid)
    uint16_t period;         // time of rotation of cups in units of 100uS
    int16_t accelx, accely;  // acceleration in 4g range
    uint16_t crc16;   // not strictly required but extra check to ensure correct application
} __attribute__((packed));

volatile packet_t packet;

#include "../../common/common.h"
spi_device_handle_t spi;
uint16_t spi_transfer16(uint16_t d)
{
    spi_transaction_t t = {
        .flags = SPI_TRANS_USE_RXDATA,
        .length = 16,
        .user = 0,
        .tx_buffer = &d,
    };
    spi_device_polling_transmit(spi, &t);

    return *(uint16_t*)t.rx_data;
}

volatile unsigned int rotation_count;
volatile uint32_t lastperiod;

void IRAM_ATTR isr_anemometer_count(void *)
{
#if 0
    // Pins 32-39 use status1 on ESP32
    uint32_t status1 = GPIO.status1.intr_st;   // latched interrupt status for GPIO32-39

    // GPIO34 corresponds to bit (34 - 32) = 2 in status1
    const uint32_t bit = 1u << (interruptPin - 32);

    if (!(status1 & bit))
        return;
    
    // Clear the interrupt *before* doing anything else
    GPIO.status1_w1tc.intr_st = bit;
#endif
    
    static uint32_t lastt;
    uint32_t t = esp_timer_get_time();
    uint32_t period = t - lastt;
    if (period > 10000) {
        lastt = t;
        lastperiod += period;
        rotation_count=rotation_count+1;
    }
}

#define LIS2DW12_CTRL 0x1F   // CTRL1 is CTRL+1
bool parityCheck(uint16_t data)
{
    data ^= data >> 8;
    data ^= data >> 4;
    data ^= data >> 2;
    data ^= data >> 1;

    return (~data) & 1;
}

#define MT6816_CS GPIO_NUM_5

static int parity_failures;
static void mt6816_init()
{
#if 1
    spi_bus_config_t buscfg={
        .mosi_io_num=23,
        .miso_io_num=19,
        .sclk_io_num=18,
        .quadwp_io_num=-1,
        .quadhd_io_num=-1,
        .max_transfer_sz=32,//2
    };
    
    spi_device_interface_config_t devcfg={
        .mode = 3,          //SPI mode 3
        .clock_speed_hz = 1000000,
        .spics_io_num = -1, // cs manually controlled
        .flags = 0,//SPI_DEVICE_POSITIVE_CS,
        .queue_size = 1,
    };
    //Attach the EEPROM to the SPI bus
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_DISABLED));
    ESP_ERROR_CHECK(spi_bus_add_device(SPI2_HOST, &devcfg, &spi));
#else
    SPI.begin();
    SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE3));
#endif

#if 1
    gpio_config_t cs_cfg = {
        .pin_bit_mask = (1ULL<<MT6816_CS),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&cs_cfg);
    gpio_set_level(MT6816_CS, 1);
#else    
    pinMode(MT6816_CS, OUTPUT);
    digitalWrite(MT6816_CS, HIGH);
#endif
}

void mt6816_read()
{
    gpio_set_level(MT6816_CS, 0);
    uint16_t angle = spi_transfer16(0x0083) & 0xff00;
    gpio_set_level(MT6816_CS, 1);

    gpio_set_level(MT6816_CS, 0);
    angle |= spi_transfer16(0x0084) >> 8;
    gpio_set_level(MT6816_CS, 1);

    if (!parityCheck(angle)) {
#ifdef DEBUG
        printf("parity failed\n");
#endif
        parity_failures++;
        if(parity_failures > 5)
            abort();
        return;
    }

    if (angle & 0x2)
        packet.angle = 0xffff;
    else
        packet.angle = angle >> 2;
}

void speed_init() {
#if 1
    gpio_config_t cs_cfg = {
        .pin_bit_mask = (1ULL<<interruptPin),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,              // GPIO34 can't pull up internally
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_POSEDGE,                 // <-- IMPORTANT
    };
    gpio_config(&cs_cfg);

    gpio_install_isr_service(ESP_INTR_FLAG_LEVEL3 | ESP_INTR_FLAG_IRAM);
    gpio_isr_handler_add(interruptPin, isr_anemometer_count, (void*)interruptPin);
    //gpio_isr_register(isr_anemometer_count, 0, ESP_INTR_FLAG_LEVEL3, 0);
#else
    pinMode(interruptPin, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(interruptPin), isr_anemometer_count, RISING);
#endif
}

void convert_speed()
{
    static portMUX_TYPE _spinlock = portMUX_INITIALIZER_UNLOCKED;

    taskENTER_CRITICAL(&_spinlock);
    uint32_t period = lastperiod;
    uint16_t rcount = rotation_count;
    rotation_count = 0;
    lastperiod = 0;
    taskEXIT_CRITICAL(&_spinlock);

#ifdef DEBUG
    if(rcount)
        printf("period count %ld %d\n", period, rcount);
#endif
    static uint16_t nowindcount;
    const int nowindtimeout = 30;
    if (rcount) { 
        if (nowindcount != nowindtimeout && period < 6000000)
            packet.period = period/(100*rcount);  // packet period is units of 100uS   
        nowindcount = 0;
    } else {
        if (nowindcount < nowindtimeout)
            nowindcount++;
        else
            packet.period = 0;
    }
}


// Global handles needed for transactions
#ifdef I2C_MASTER
uint8_t i2c_data[6] = { 0 };

i2c_master_dev_handle_t lis2dw12_0, lis2dw12_1;
i2c_master_dev_handle_t lis2dw12_addr = NULL;
// Callback that runs when a non-blocking transfer finishes
bool IRAM_ATTR i2c_finish_callback(i2c_master_dev_handle_t dev_hd, 
                                          const i2c_master_event_data_t *event_data, 
                                          void *arg) {
    if(event_data->event == I2C_EVENT_DONE) {
        packet.accelx = (i2c_data[3] << 8) | i2c_data[2];
        packet.accely = (i2c_data[5] << 8) | i2c_data[4];
    }
    return false; 
}
#else
static uint8_t lis2dw12_addr;
#endif

static void lis2dw12_init()
{
    i2c_setup();
#ifdef I2C_MASTER
    i2c_add_device(&lis2dw12_0, LIS2DW_ADDRESS0);
    i2c_add_device(&lis2dw12_1, LIS2DW_ADDRESS1);
#else
    uint8_t lis2dw12_0 = LIS2DW_ADDRESS0;
    uint8_t lis2dw12_1 = LIS2DW_ADDRESS1;
#endif
    uint8_t whoami[1] = {0};
    i2c_read(lis2dw12_0, 0x0F, whoami, 1);
    if(whoami[0] == 0x44)
        lis2dw12_addr = lis2dw12_0;
    else {
        i2c_read(lis2dw12_1, 0x0F, whoami, 1);
        if(whoami[0] == 0x44)
            lis2dw12_addr = lis2dw12_1;
        else {
            lis2dw12_addr = 0;
            return;
        }
    }

    i2c_write_byte(lis2dw12_addr, LIS2DW12_CTRL + 1, 0x46);
    i2c_write_byte(lis2dw12_addr, LIS2DW12_CTRL + 6, 0x10);
    i2c_write_byte(lis2dw12_addr, LIS2DW12_CTRL + 2, 0x08 | 0x04);
}

void lis2dw12_read()
{
    // every 100 reads, read whoami again to make sure the device is working
    static int reads;
    if(++reads >= 100) {
        uint8_t whoami[1] = {0};
        i2c_read(lis2dw12_addr, 0x0F, whoami, 1);
        if(whoami[0] != 0x44)
            lis2dw12_init();            

        reads = 0;
    }
    
    packet.accelx = 0;
    packet.accely = 0;
    if(!lis2dw12_addr)
        return;
    
    uint8_t reg_addr = 0x28+2;
#ifdef I2C_MASTER
    // nonblocking,  isr will update
    i2c_master_transmit_receive(lis2dw12_addr, &reg_addr, 1, i2c_data, 6-2, -1); 
#else
    uint8_t data[6];
    i2c_read(lis2dw12_addr, reg_addr, data+2, 6-2);
    packet.accelx = (data[3] << 8) | data[2];
    packet.accely = (data[5] << 8) | data[4];
#endif
}

void setup(void)
{
    //Set device in STA mode to begin with
#ifdef DEBUG
    printf("Wind Transmitter\n");
#endif  
    memset(&chip, 0, sizeof(chip));
    for (int ii = 0; ii < 6; ++ii)
        chip.peer_addr[ii] = (uint8_t)0xff;

    read_channel();
    if(chip.channel != 1 && chip.channel != 11)
        chip.channel = 6;

    chip.channel=1;

    //chip.ifidx = ESPNOW_WIFI_IF;  ???
    chip.encrypt = 0;        // no encryption

    packet.id = ID;  // initialize packet ID

    // init communication with angular hall sensor
    mt6816_init();

    // setup interrupt to read wind speed pulses
    speed_init();
    lis2dw12_init();

    wifi_init();
    espnow_init();
    esp_log_level_set("wifi", ESP_LOG_NONE);

#ifdef DEBUG
    printf("setup complete\n");
#endif
}

void loop()
{
    static uint64_t t0 = 0;
    lis2dw12_read();
#ifdef LOW_POWER
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA)); // or WIFI_MODE_APSTA if needed
    ESP_ERROR_CHECK(esp_wifi_start());
#endif
    uint64_t t1 = esp_timer_get_time();        
    mt6816_read();
    uint64_t t2 = esp_timer_get_time();        
    convert_speed();
    uint64_t t3 = esp_timer_get_time();

#if 0    
    static int xa, xp = 1000;
    packet.angle = xa+=10;
    packet.period = xp+=5;
    if(xa >= 16384)
        xa = 0;
    if(xp >= 20000)
        xp = 500;
#endif    
    sendData();

    uint64_t t4 = esp_timer_get_time();        

    
    // Prepare for sleep
    vTaskDelay(5); //Short delay to finish transmit before esp_now_deinit()
    //    if (esp_now_deinit() != ESP_OK)   //De-initialize ESP-NOW. (all information of paired devices will be deleted)
    //        printf("esp_now_deinit() failed"); 

#ifdef LOW_POWER
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_NULL)); // effectively "Wi-Fi off"
#endif
    int period = 100;
    for(;;) {
        uint64_t tx = esp_timer_get_time();        
        uint64_t d = (tx - t0) / 1000;

        if (d >= period)
            break;

        d = period - d;
#ifdef LOW_POWER
        // wake up on pin change for isr to count cup revolutions
        gpio_wakeup_enable(GPIO_NUM_34, gpio_get_level(GPIO_NUM_34) ? GPIO_INTR_LOW_LEVEL : GPIO_INTR_HIGH_LEVEL);
    	esp_sleep_enable_gpio_wakeup();
        esp_sleep_enable_timer_wakeup(d);
        esp_light_sleep_start();
#else
        vTaskDelay(d / portTICK_PERIOD_MS);
        //vTaskDelay(100/portTICK_PERIOD_MS);
#endif
   //     printf("sleep time %d %d\n", d,  millis()-tx);
    }
    t0 = esp_timer_get_time();

#ifdef DEBUG // for debugging
    float angle = (packet.angle == 0xffff) ? NAN : packet.angle * 360.0 / 16384;
    printf("result: %.02f %.02f %d %.02f %llu %llu %llu %llu\n", packet.accelx * 4.0 / 32768, packet.accely * 4.0 / 32768, packet.period, angle, t1 - t0, t2-t1,t3-t2,t4-t3);
    //printf("result %.02f %d\n", angle, gpio_get_level(GPIO_NUM_34));
#endif
}

extern "C" void app_main(void)
{
    setup();
    for(;;)
        loop();
}

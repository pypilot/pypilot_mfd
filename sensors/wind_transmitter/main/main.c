/* Copyright (C) 2026 Sean D'Epagnier <seandepagnier@gmail.com>
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

#include <esp_wifi.h>

#include "esp_log.h"
#include "esp_netif.h"
#include "esp_sleep.h"
#include "esp_timer.h"

#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "soc/gpio_struct.h"
#include "soc/gpio_reg.h"

#include "../../common/web.h"
#include "../../common/settings.h"
#include "../../common/wireless.h"
#include "../../common/lis2dw.h"
#include "../../common/dhcp.h"

const gpio_num_t interruptPin = GPIO_NUM_34;

// data in packet
//#define LOW_POWER   // power down wireless while not working (save a lot of power?)
//#define DEBUG

#define ID 0xB179
typedef struct __attribute__((packed)) {
    uint16_t id;             // packet identifier
    uint16_t angle;          // angle in 14 bits (or 0xffff for invalid)
    uint16_t period;         // time of rotation of cups in units of 100uS
    int16_t accelx, accely, accelz;  // acceleration in 4g range
} packet_t;

volatile packet_t packet;

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
        .max_transfer_sz=32,
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

    gpio_config_t cs_cfg = {
        .pin_bit_mask = (1ULL<<MT6816_CS),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&cs_cfg);
    gpio_set_level(MT6816_CS, 1);
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
            packet.angle = 0x8000;
        return;
    }

    if (angle & 0x2)
        packet.angle = 0xffff;
    else {
        angle += 16384; // rotate 90 degrees
        packet.angle = angle >> 1; // store angle using 15 bits, high bit indicates error
    }
}

void speed_init() {
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

void setup(void)
{
    //Set device in STA mode to begin with
#ifdef DEBUG
    printf("Wind Transmitter\n");
#endif
    read_settings();

    packet.id = ID;  // initialize packet ID

    // init communication with angular hall sensor
    mt6816_init();

    // setup interrupt to read wind speed pulses
    speed_init();
    lis2dw12_init();

    wifi_init();
    //    esp_log_level_set("wifi", ESP_LOG_NONE);

    dhcp_set_captiveportal_url();

#ifdef DEBUG
    printf("setup complete\n");
#endif
}

static int jitter = 0;
uint64_t ts;
void loop()
{
    uint64_t t0 = esp_timer_get_time();
    
    int16_t x, y, z;
    lis2dw12_read(&x, &y, &z);
    packet.accelx = x;
    packet.accely = y;
    packet.accelz = z;

#ifdef LOW_POWER
    //    if(!wireless_ap_enabled)
        ESP_ERROR_CHECK(esp_wifi_start());
#endif

    // enable access point if inverted long enough
    static int inverted_count;
    if(z < -6000)
        if(inverted_count > 3*rate) // 3 seconds
            enable_ap(0);
        else
            inverted_count++;
    else
        inverted_count = 0;

    enable_ap_poll();
    convert_speed();
    mt6816_read();

    wireless_send_data((uint8_t*)&packet, sizeof packet);

    if(wireless_ap_enabled) {
        char msg[128];
        snprintf(msg, sizeof msg,
                 "{\"angle\":%d,\"period\":%d,\"accel\":"
                 "{\"x\":%d,\"y\":%d,\"z\":%d}}",
                 packet.angle, packet.period, packet.accelx, packet.accely, packet.accelz);
        web_broadcast(msg);
    }
    
    wireless_wait_sent(); // finish transmit before sleep

    int period = 1e6/rate;
    jitter += 7400;
    jitter %= 4000;

    uint64_t te = ts + period;// + jitter - 2000;
    for(;;) {
        uint64_t tx = esp_timer_get_time();
        if (tx >= te)
            break;

        uint64_t d = te-tx;
#ifdef LOW_POWER
        // ap enabled or first 60 seconds dont go in low power
        if(!wireless_ap_enabled && t0 >= 60e6) {// && d > 10000) {
            //            d -= 2000;
            esp_wifi_stop();
            // wake up on pin change for isr to count cup revolutions
            gpio_wakeup_enable(GPIO_NUM_34, gpio_get_level(GPIO_NUM_34) ?
                               GPIO_INTR_LOW_LEVEL : GPIO_INTR_HIGH_LEVEL);
            esp_sleep_enable_gpio_wakeup();
            esp_sleep_enable_timer_wakeup(d);
            esp_light_sleep_start();
        } else
#endif
            vTaskDelay(d / 1000 / portTICK_PERIOD_MS);
        //printf("sleep time %llu %llu\n", d, esp_timer_get_time()-tx);
    }

    if(ts < t0 - period/2 || ts > t0 + period/2)
        ts=t0;
    else
        ts += period;

#ifdef DEBUG // for debugging
    float angle = (packet.angle == 0xffff) ? NAN : packet.angle * 360.0 / 32768;
    printf("result: %.02f %.02f %d %.02f\n",
           packet.accelx * 4.0 / 32768, packet.accely * 4.0 / 32768,
           packet.period, angle);
    //printf("result %.02f %d\n", angle, gpio_get_level(GPIO_NUM_34));
#endif
}

void app_main(void)
{   
    setup();
    for(;;)
        loop();
}

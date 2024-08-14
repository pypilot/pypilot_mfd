/* Copyright (C) 2024 Sean D'Epagnier <seandepagnier@gmail.com>
 *
 * This Program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 */


// note 80mhz cpu speed is plenty!
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"

#include "string.h"

#include "esp_netif.h"
#include <esp_now.h>
#include <esp_wifi.h>
#include "esp_timer.h"

#include "driver/i2c.h"
#include "driver/spi_master.h"

// Global copy of chip
esp_now_peer_info_t chip;

// data in packet
#define I2C_MASTER_NUM I2C_NUM_0
#define ID 0xf179
#define CHANNEL_ID 0x0a21

#define I2C_MASTER_TIMEOUT_MS 1000

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

spi_device_handle_t spi;

/* WiFi should start before using ESPNOW */
static void wifi_init(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_AP) );
    ESP_ERROR_CHECK( esp_wifi_start());
//    ESP_ERROR_CHECK( esp_wifi_set_channel(CONFIG_ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE));

#if CONFIG_ESPNOW_ENABLE_LONG_RANGE
    ESP_ERROR_CHECK( esp_wifi_set_protocol(ESPNOW_WIFI_IF, WIFI_PROTOCOL_11B|WIFI_PROTOCOL_11G|WIFI_PROTOCOL_11N|WIFI_PROTOCOL_LR) );
#endif

    esp_wifi_disconnect();
}

void reboot()
{
    printf("Reset ESP\n");
    vTaskDelay(30);
    abort();
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
    //print("Sending...");
    esp_err_t result = esp_now_send(peer_addr, (uint8_t *)&packet, sizeof(packet));
    //print("Send Status: ");
    if (result == ESP_OK) {
        //printf("Success\n");
        return;
    } else if (result == ESP_ERR_ESPNOW_NOT_INIT)
        // How did we get so far!!
        printf("ESPNOW not Init.\n");
    else if (result == ESP_ERR_ESPNOW_ARG)
       printf("Invalid Argument\n");
    else if (result == ESP_ERR_ESPNOW_INTERNAL)
        printf("Internal Error\n");
    else if (result == ESP_ERR_ESPNOW_NO_MEM)
        printf("ESP_ERR_ESPNOW_NO_MEM\n");
    else if (result == ESP_ERR_ESPNOW_NOT_FOUND)
        printf("Peer not found.\n");
    else
        printf("Not sure what happened\n");

    reboot();
}

volatile unsigned int rotation_count;
volatile uint32_t lastperiod;

void IRAM_ATTR isr_anemometer_count(void *)
{
    static uint16_t lastt;
    uint32_t t = esp_timer_get_time();
    uint32_t period = t - lastt;
    if (period > 10) {
        lastt = t;
        lastperiod = lastperiod + period;
        rotation_count = rotation_count + 1;
    }
}

#define DEVICE_ADDRESS 0x19  // 0x32  // LIS2DW12TR
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

void MT6816_read()
{
    gpio_set_level(MT6816_CS, 0);
    //digitalWrite(MT6816_CS, LOW);
    uint16_t angle = spi_transfer16(0x8300) << 8;
    //uint16_t angle = SPI.transfer16(0x8300) << 8;
    gpio_set_level(MT6816_CS, 1);
    //digitalWrite(MT6816_CS, HIGH);

    gpio_set_level(MT6816_CS, 0);
    //digitalWrite(MT6816_CS, LOW);
    angle |= spi_transfer16(0x8400) & 0xff;
    //angle |= SPI.transfer16(0x8400) & 0xff;
    gpio_set_level(MT6816_CS, 1);
    //digitalWrite(MT6816_CS, HIGH);

    if (!parityCheck(angle)) {
        printf("parity failed\n");
        return;
    }

    if (angle & 0x2)
        packet.angle = 0xffff;
    else
        packet.angle = angle >> 2;
}

//static portMUX_TYPE my_mutex;
void convert_speed()
{
    static portMUX_TYPE _spinlock = portMUX_INITIALIZER_UNLOCKED;

    taskENTER_CRITICAL(&_spinlock);
    uint32_t period = lastperiod;
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
            packet.period = period/(100*rcount);  // packet period is units of 100uS   
        nowindcount = 0;
    } else {
        if (nowindcount < nowindtimeout)
            nowindcount++;
        else
            packet.period = 0;
    }
}

void OnDataRecv(const esp_now_recv_info *recv_info, const uint8_t *data, int data_len) {
    //uint8_t *mac_addr = recv_info->src_addr;
    if(data_len != sizeof(packet_channel_t)) {
        //printf("wrong packet size\n");
        return;
    }

    packet_channel_t *packet = (packet_channel_t*)data;
    if(packet->id != CHANNEL_ID) {
        //printf("ID mismatch\n");
    }

    uint16_t crc = crc16(data, data_len-2);
    if(crc != packet->crc16) {
        printf("crc failed %x %x\n", crc, packet->crc16);
            return;
    }

    if(chip.channel != packet->channel) {
        chip.channel = packet->channel;
//        EEPROM.put(0, packet->channel);
//        EEPROM.commit();
    }
}

static esp_err_t i2c_write_byte(uint8_t reg_addr, uint8_t data)
{
    int ret;
    uint8_t write_buf[2] = {reg_addr, data};

    ret = i2c_master_write_to_device(I2C_MASTER_NUM, DEVICE_ADDRESS, write_buf, sizeof(write_buf), I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);

    return ret;
}

static esp_err_t i2c_read(uint8_t reg_addr, uint8_t *data, size_t len)
{
    return i2c_master_write_read_device(I2C_MASTER_NUM, DEVICE_ADDRESS, &reg_addr, 1, data, len, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
}

static void setup_accel()
{
    i2c_port_t i2c_master_port = I2C_MASTER_NUM;

    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = 21,
        .scl_io_num = 22,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master = {.clk_speed = 100000}
    };

    i2c_param_config(i2c_master_port, &conf);

    i2c_driver_install(i2c_master_port, conf.mode, 0, 0, 0);
    i2c_write_byte(LIS2DW12_CTRL + 1, 0x44);
    i2c_write_byte(LIS2DW12_CTRL + 6, 0x10);
    i2c_write_byte(LIS2DW12_CTRL + 2, 0x08 | 0x04);
}

static void init_mt6816()
{
#if 1
    spi_bus_config_t buscfg={
        .mosi_io_num=23,
        .miso_io_num=19,
        .sclk_io_num=18,
        .quadwp_io_num=-1,
        .quadhd_io_num=-1,
        .max_transfer_sz=2
    };
    
    spi_device_interface_config_t devcfg={
        .mode = 3,          //SPI mode 3
        .clock_speed_hz = 1000000,
        .spics_io_num = -1, // cs manually controlled
        .flags = SPI_DEVICE_POSITIVE_CS,
        .queue_size = 1,
    };
    //Attach the EEPROM to the SPI bus
    ESP_ERROR_CHECK(spi_bus_initialize(HSPI_HOST, &buscfg, SPI_DMA_DISABLED));
    ESP_ERROR_CHECK(spi_bus_add_device(HSPI_HOST, &devcfg, &spi));
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

static void espnow_init(void)
{
    /* Initialize ESPNOW and register sending and receiving callback function. */
    esp_wifi_set_channel(chip.channel, WIFI_SECOND_CHAN_NONE);
    if (esp_now_init() == ESP_OK) {
        //printf("ESPNow Init Success\n");
    } else {
        printf("ESPNow Init Failed\n");
        reboot();
    }

    ESP_ERROR_CHECK( esp_now_register_recv_cb(OnDataRecv) );

#if CONFIG_ESPNOW_ENABLE_POWER_SAVE
    ESP_ERROR_CHECK( esp_now_set_wake_window(CONFIG_ESPNOW_WAKE_WINDOW) );
    ESP_ERROR_CHECK( esp_wifi_connectionless_module_set_wake_interval(CONFIG_ESPNOW_WAKE_INTERVAL) );
#endif
    /* Set primary master key. */
//    ESP_ERROR_CHECK( esp_now_set_pmk((uint8_t *)CONFIG_ESPNOW_PMK) );

    ESP_ERROR_CHECK( esp_now_add_peer(&chip) );
}

void setup()
{
    //Set device in STA mode to begin with
    printf("Wind Transmitter\n");
  
    memset(&chip, 0, sizeof(chip));
    for (int ii = 0; ii < 6; ++ii)
        chip.peer_addr[ii] = (uint8_t)0xff;
//    EEPROM.begin(sizeof chip.channel);
//    EEPROM.get(0, chip.channel);  // pick a channel
//    EEPROM.end();
    if(chip.channel > 12 || chip.channel == 0)
        chip.channel = 6;

    //chip.ifidx = ESPNOW_WIFI_IF;  ???
    chip.encrypt = 0;        // no encryption

    packet.id = ID;  // initialize packet ID

    // init communication with angular hall sensor
    init_mt6816();

//    pinMode(0, INPUT);

    // setup interrupt to read wind speed pulses
#if 1
    const gpio_num_t interruptPin = GPIO_NUM_34;
    gpio_config_t cs_cfg = {
        .pin_bit_mask = (1ULL<<interruptPin),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE
    };
    gpio_config(&cs_cfg);
    gpio_set_intr_type(interruptPin, GPIO_INTR_POSEDGE);
    
    gpio_isr_register(isr_anemometer_count, 0, ESP_INTR_FLAG_LEVEL3, 0);
#else
    pinMode(interruptPin, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(interruptPin), isr_anemometer_count, RISING);
#endif
    setup_accel();
    printf("setup complete\n");
}

void read_accel()
{
    uint8_t data[6] = { 0 };
    i2c_read(0x28, data, 6);

    packet.accelx = (data[3] << 8) | data[2];
    packet.accely = (data[5] << 8) | data[4];
}

void loop()
{
    uint64_t t0 = esp_timer_get_time()/1000L;        
    wifi_init();
    espnow_init();

    MT6816_read();
    read_accel();
    convert_speed();
    uint64_t t1 = esp_timer_get_time()/1000L;        
    sendData();

    // Prepare for sleep
    vTaskDelay(10); //Short delay to finish transmit before esp_now_deinit()
    if (esp_now_deinit() != ESP_OK)   //De-initialize ESP-NOW. (all information of paired devices will be deleted)
        printf("esp_now_deinit() failed"); 

    //WiFi.mode(WIFI_OFF);

    int period = 100;
    for(;;) {
        uint64_t tx = esp_timer_get_time()/1000L;        
        int d = tx - t0;

        if (d >= period)
            break;

        d = period - d;
#if 1
        vTaskDelay(d / portTICK_PERIOD_MS);
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
    printf("%.02f %.02f %d %.02f %d %d\n", packet.accelx * 4.0 / 32768, packet.accely * 4.0 / 32768, packet.period, angle, t1 - t0, t1 - t0);

#endif
    //printf("pos %d\n", digitalRead(34));
}

extern "C" void app_main(void)
{
    setup();
    for(;;)
        loop();
}


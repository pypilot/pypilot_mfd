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
#include <esp_sleep.h>
#include "esp_timer.h"
#include <string.h>

#include "bme680.h"

#define PIN_VCC  33
#define PIN_2V5  32
#define PIN_1V24 39

// Global copy of chip
esp_now_peer_info_t chip;

// data in packet
#define ID 0xa41b

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

#include "../../common/common.h"

//#define ADC_CONTINUOUS

#ifdef ADC_CONTINUOUS
#include "esp_log.h"
#include "esp_adc/adc_continuous.h"
static const char *TAG = "ADC_CONTINOUS";

adc_continuous_handle_t adc_handle = NULL;

#else

#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

adc_oneshot_unit_handle_t adc1_handle;

#endif

const int READ_LEN = 256;

void init_adc()
{
#ifdef ADC_CONTINUOUS
    adc_continuous_handle_cfg_t adc_config = {
        .max_store_buf_size = 1024,
        .conv_frame_size = READ_LEN,
    };
    ESP_ERROR_CHECK(adc_continuous_new_handle(&adc_config, &adc_handle));

    adc_continuous_config_t dig_cfg = {
        .sample_freq_hz = 20 * 1000,
        .conv_mode = ADC_CONV_SINGLE_UNIT_1,
        .format = ADC_DIGI_OUTPUT_FORMAT_TYPE1,
    };

    adc_digi_pattern_config_t adc_pattern[SOC_ADC_PATT_LEN_MAX] = {0};
    dig_cfg.pattern_num = 2;
    int channel[3] = {ADC_CHANNEL_3, ADC_CHANNEL_4, ADC_CHANNEL_5};
    for (int i = 0; i < dig_cfg.pattern_num; i++) {
        adc_pattern[i].atten = ADC_ATTEN_DB_12;
        adc_pattern[i].channel = channel[i] & 0x7;
        adc_pattern[i].unit = ADC_UNIT_1;
        adc_pattern[i].bit_width = SOC_ADC_DIGI_MAX_BITWIDTH;

        ESP_LOGI(TAG, "adc_pattern[%d].atten is :%"PRIx8, i, adc_pattern[i].atten);
        ESP_LOGI(TAG, "adc_pattern[%d].channel is :%"PRIx8, i, adc_pattern[i].channel);
        ESP_LOGI(TAG, "adc_pattern[%d].unit is :%"PRIx8, i, adc_pattern[i].unit);
    }
    dig_cfg.adc_pattern = adc_pattern;
    ESP_ERROR_CHECK(adc_continuous_config(adc_handle, &dig_cfg));
    ESP_ERROR_CHECK(adc_continuous_start(adc_handle));
#else
    //-------------ADC1 Init---------------//
    adc_oneshot_unit_init_cfg_t init_config1 = {
       .unit_id = ADC_UNIT_1,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));

    //-------------ADC1 Config---------------//
    adc_oneshot_chan_cfg_t config = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    // pin 1V24 30 ADC1_3
    // pin 2V5 32 ADC1_4
    // pin VCC 33 ADC1_5
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL_3, &config));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL_4, &config));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL_5, &config));

    //-------------ADC1 Calibration Init---------------//
#if 0    
    adc_cali_handle_t adc1_cali_chan3_handle = NULL;
    adc_cali_handle_t adc1_cali_chan4_handle = NULL;
    adc_cali_handle_t adc1_cali_chan5_handle = NULL;
    bool do_calibration1_chan0 = example_adc_calibration_init(ADC_UNIT_1, EXAMPLE_ADC1_CHAN0, EXAMPLE_ADC_ATTEN, &adc1_cali_chan0_handle);
    bool do_calibration1_chan1 = example_adc_calibration_init(ADC_UNIT_1, EXAMPLE_ADC1_CHAN1, EXAMPLE_ADC_ATTEN, &adc1_cali_chan1_handle);
#endif
#endif
}

#ifndef ADC_CONTINUOUS
uint16_t read_adc(adc_channel_t channel, int samples)
{
    uint32_t total = 0;
    for(int i=0; i<samples; i++) {
        int v;
        ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, channel, &v));
        total += v;
    }
    return total / samples;
}
#endif

extern "C" void app_main(void)
{
//    vTaskDelay(1500 / portTICK_PERIOD_MS); // remove after debugged
    //Set device in STA mode to begin with
    printf("BME680 Transmitter!\n");
  
    read_channel();

    packet.id = ID;  // initialize packet ID

    init_adc();
    i2c_setup();

    bme680_setup();
    printf("setup complete\n");

    // printf("loop\n");
    wifi_init();
    espnow_init();
    
    bme680_read();

    packet.pressure = int16_t(pressure - 100000);
    packet.temperature = int16_t((temperature)*100);
    packet.rel_humidity = uint16_t(rel_humidity*100);
    packet.air_quality = uint16_t(air_quality*100);
    packet.reserved = 0;

    int val1v24, val2v5, valvcc;
#ifdef ADC_CONTINUOUS
    esp_err_t ret;
    uint32_t ret_num = 0;
    uint8_t result[READ_LEN] = {0};
    memset(result, 0xcc, READ_LEN);

    uint32_t values[3] = {0}, counts[3] = {0};

    ret = adc_continuous_read(adc_handle, result, READ_LEN, &ret_num, 0);
    if (ret == ESP_OK) {
        ESP_LOGI("TASK", "ret is %x, ret_num is %"PRIu32" bytes", ret, ret_num);
        for (int i = 0; i < ret_num; i += SOC_ADC_DIGI_RESULT_BYTES) {
            adc_digi_output_data_t *p = (adc_digi_output_data_t *)&result[i];
            uint32_t chan_num = p->type1.channel;
            uint32_t data = p->type1.data;
            /* Check the channel number validation, the data is invalid if the channel num exceed the maximum channel */
            if (chan_num < SOC_ADC_CHANNEL_NUM(ADC_UNIT_1)) {
                values[chan_num] += data;
                counts[chan_num] ++;
            } else {
                ESP_LOGW(TAG, "Invalid data [1_%"PRIu32"_%"PRIx32"]", chan_num, data);
            }
        }
    }
    ESP_ERROR_CHECK(adc_continuous_stop(adc_handle));
    ESP_ERROR_CHECK(adc_continuous_deinit(adc_handle));


    val2v5 = values[0] / counts[0];
    val1v24 = values[1] / counts[1];
    valvcc = values[2] / counts[2];
#else
    const int samples = 16;
//    int32_t t0 = esp_timer_get_time();
    val1v24 = read_adc(ADC_CHANNEL_3, samples);
//    int32_t t1 = esp_timer_get_time();
//    printf("took %ld\n", t1-t0);
//    val2v5 = read_adc(ADC_CHANNEL_4, samples);
    valvcc = read_adc(ADC_CHANNEL_5, samples);
   
    val2v5 = 2800;
#endif
        
    //2.5 = scale * val2v5 + offset;
    //1.24 = scale * val1v24 + offset;
    //1.26 = scale*(val2v5 - val1v24)
    float scale = (2.5f-1.24f) / (val2v5 - val1v24);
    float offset = 2.5f - scale*val2v5;
    
    float vin = scale * valvcc + offset;
    // vin = 10/91 * voltage;
    float voltage = 91/10.0f * vin;
//    printf("values %d %d %d %f %f %f\n", val2v5, val1v24, valvcc, voltage, scale, offset);
    packet.voltage = voltage * 100;

#if 1 
    sendData();
#else
    // continuous debugging
    for(;;) {
        sendData();
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
#endif
    //Short delay to finish transmit before esp_now_deinit()
    vTaskDelay(10 / portTICK_PERIOD_MS);

    if (esp_now_deinit() != ESP_OK)   //De-initialize ESP-NOW. (icall information of paired devices will be deleted)
        printf("esp_now_deinit() failed\n"); 

    int period = 10000;
    
    // Prepare for sleep
    esp_sleep_enable_timer_wakeup(period * 1000);
    esp_deep_sleep_start();
}

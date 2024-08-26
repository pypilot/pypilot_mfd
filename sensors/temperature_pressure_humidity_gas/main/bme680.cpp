/* Copyright (C) 2022 Sean D'Epagnier <seandepagnier@gmail.com>
 *
 * This Program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 */

#include <list>

#include <math.h>
#include <string.h>
#include <stdint.h>
#include <driver/i2c.h>

#include "rom/ets_sys.h"
#include "bme68x.h"

float pressure, temperature, rel_humidity, air_quality;


#define DEVICE_ADDRESS 0x76

#define I2C_MASTER_TIMEOUT_MS 1000

/******************************************************************************/
/*!                 Macro definitions                                         */
/*! BME68X shuttle board ID */
#define BME68X_SHUTTLE_ID  0x93

/******************************************************************************/
/*!                Static variable definition                                 */
static uint8_t dev_addr;

/******************************************************************************/
/*!                User interface functions                                   */

/*!
 * I2C read function map to COINES platform
 */
BME68X_INTF_RET_TYPE bme68x_i2c_read(uint8_t reg_addr, uint8_t *reg_data, uint32_t len, void *intf_ptr)
{
    ESP_ERROR_CHECK(i2c_master_write_read_device((i2c_port_t)0, 0x76, &reg_addr, 1, reg_data, len, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS));
    return BME68X_INTF_RET_SUCCESS;
}

/*!
 * I2C write function map to COINES platform
 */
BME68X_INTF_RET_TYPE bme68x_i2c_write(uint8_t reg_addr, const uint8_t *reg_data, uint32_t len, void *intf_ptr)
{
    uint8_t write_buf[len+1];
    write_buf[0] = reg_addr;
    memcpy(write_buf+1, reg_data, len);
    
    int ret = i2c_master_write_to_device((i2c_port_t)0, DEVICE_ADDRESS, write_buf, len+1, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
    return BME68X_INTF_RET_SUCCESS;
}

/*!
 * SPI read function map to COINES platform
 */
BME68X_INTF_RET_TYPE bme68x_spi_read(uint8_t reg_addr, uint8_t *reg_data, uint32_t len, void *intf_ptr)
{
    return !BME68X_INTF_RET_SUCCESS;
}

/*!
 * SPI write function map to COINES platform
 */
BME68X_INTF_RET_TYPE bme68x_spi_write(uint8_t reg_addr, const uint8_t *reg_data, uint32_t len, void *intf_ptr)
{
    return !BME68X_INTF_RET_SUCCESS;
}

/*!
 * Delay function map to COINES platform
 */
void bme68x_delay_us(uint32_t period, void *intf_ptr)
{
    ets_delay_us(period);
}

void bme68x_check_rslt(const char api_name[], int8_t rslt)
{
    switch (rslt)
    {
        case BME68X_OK:

            /* Do nothing */
            break;
        case BME68X_E_NULL_PTR:
            printf("API name [%s]  Error [%d] : Null pointer\r\n", api_name, rslt);
            break;
        case BME68X_E_COM_FAIL:
            printf("API name [%s]  Error [%d] : Communication failure\r\n", api_name, rslt);
            break;
        case BME68X_E_INVALID_LENGTH:
            printf("API name [%s]  Error [%d] : Incorrect length parameter\r\n", api_name, rslt);
            break;
        case BME68X_E_DEV_NOT_FOUND:
            printf("API name [%s]  Error [%d] : Device not found\r\n", api_name, rslt);
            break;
        case BME68X_E_SELF_TEST:
            printf("API name [%s]  Error [%d] : Self test error\r\n", api_name, rslt);
            break;
        case BME68X_W_NO_NEW_DATA:
            printf("API name [%s]  Warning [%d] : No new data found\r\n", api_name, rslt);
            break;
        default:
            printf("API name [%s]  Error [%d] : Unknown error code\r\n", api_name, rslt);
            break;
    }
}

int8_t bme68x_interface_init(struct bme68x_dev *bme, uint8_t intf)
{
    int8_t rslt = BME68X_OK;

    if (bme != NULL)
    {
        /* Bus configuration : I2C */
        if (intf == BME68X_I2C_INTF)
        {
            //printf("I2C Interface\n");
            dev_addr = BME68X_I2C_ADDR_LOW;
            bme->read = bme68x_i2c_read;
            bme->write = bme68x_i2c_write;
            bme->intf = BME68X_I2C_INTF;
        }
        /* Bus configuration : SPI */
        else if (intf == BME68X_SPI_INTF)
        {
            printf("SPI Interface not supported\n");
        }

        bme->delay_us = bme68x_delay_us;
        bme->intf_ptr = &dev_addr;
        bme->amb_temp = 25; /* The ambient temperature in deg C is used for defining the heater temperature */
    }
    else
    {
        rslt = BME68X_E_NULL_PTR;
    }

    return rslt;
}


#include "bme68x.h"
#define SAMPLE_COUNT  UINT8_C(300)

    struct bme68x_dev bme;

    struct bme68x_conf conf;

    struct bme68x_heatr_conf heatr_conf;

    int8_t rslt;
    struct bme68x_data data[3];
    uint32_t time_ms = 0;
    uint8_t n_fields;


        /* Heater temperature in degree Celsius */
    uint16_t temp_prof[10] = { 200, 240, 280, 320, 360, 360, 320, 280, 240, 200 };

    /* Heating duration in milliseconds */
    uint16_t dur_prof[10] = { 100, 100, 100, 100, 100, 100, 100, 100, 100, 100 };

void bme680_setup()
{

    /* Interface preference is updated as a parameter
     * For I2C : BME68X_I2C_INTF
     * For SPI : BME68X_SPI_INTF
     */
    rslt = bme68x_interface_init(&bme, BME68X_I2C_INTF);
    bme68x_check_rslt("bme68x_interface_init", rslt);

    rslt = bme68x_init(&bme);
    bme68x_check_rslt("bme68x_init", rslt);

    /* Check if rslt == BME68X_OK, report or handle if otherwise */
    rslt = bme68x_get_conf(&conf, &bme);
    bme68x_check_rslt("bme68x_get_conf", rslt);

    /* Check if rslt == BME68X_OK, report or handle if otherwise */
    conf.filter = BME68X_FILTER_OFF;
    conf.odr = BME68X_ODR_NONE; /* This parameter defines the sleep duration after each profile */
    conf.os_hum = BME68X_OS_16X;
    conf.os_pres = BME68X_OS_1X;
    conf.os_temp = BME68X_OS_2X;
    rslt = bme68x_set_conf(&conf, &bme);
    bme68x_check_rslt("bme68x_set_conf", rslt);

    /* Check if rslt == BME68X_OK, report or handle if otherwise */
    heatr_conf.enable = BME68X_ENABLE;
    heatr_conf.heatr_temp_prof = temp_prof;
    heatr_conf.heatr_dur_prof = dur_prof;
    heatr_conf.profile_len = 10;
    rslt = bme68x_set_heatr_conf(BME68X_SEQUENTIAL_MODE, &heatr_conf, &bme);
    bme68x_check_rslt("bme68x_set_heatr_conf", rslt);

    /* Check if rslt == BME68X_OK, report or handle if otherwise */
    rslt = bme68x_set_op_mode(BME68X_SEQUENTIAL_MODE, &bme);
    bme68x_check_rslt("bme68x_set_op_mode", rslt);

    /* Check if rslt == BME68X_OK, report or handle if otherwise */
    //printf("Sample, TimeStamp(ms), Temperature(deg C), Pressure(Pa), Humidity(%%), Gas resistance(ohm), Status, Profile index, Measurement index\n");


}
		
int burn_in_cycles = 300;
int gas_recal_period = 3600;;
float ph_slope = 0.03;
float gas_ceil = 0;
std::list <float> gas_cal_data;

int gas_recal_step = 0;

void average_gas_data()
{
    float t = 0, c = 0;

    for(std::list<float>::iterator i = gas_cal_data.begin(); i != gas_cal_data.end(); i++)
        t += *i, c++;
    gas_ceil = t / c;
}

// python version: https://github.com/thstielow/raspi-bme680-iaq
float getIAQ(bme68x_data *data)
{
	float temp = data->temperature;
	float press = data->pressure;
	float hum = data->humidity;
	float R_gas = data->gas_resistance;
		
	// calculate stauration density and absolute humidity
   	float	rho_max = (6.112* 100 * exp((17.62 * temp)/(243.12 + temp)))/(461.52 * (temp + 273.15));
	float hum_abs = hum * 10 * rho_max;
		
    // compensate exponential impact of humidity on resistance
	float comp_gas = R_gas * exp(ph_slope * hum_abs);
		
	if(burn_in_cycles > 0) {
		// check if burn-in-cycles are recorded
		burn_in_cycles --;		//count down cycles
		if(comp_gas > gas_ceil) {//if value exceeds current ceiling, add to calibration list and update ceiling
			gas_cal_data.empty();
            gas_cal_data.push_back(comp_gas);
			gas_ceil = comp_gas;
        }
		return 0; //return None type as sensor burn-in is not yet completed
    }

	// adapt calibration
	if(comp_gas > gas_ceil) {
        gas_cal_data.push_back(comp_gas);
        if(gas_cal_data.size() > 100)
            gas_cal_data.pop_front();
		average_gas_data();
    }
			
	// calculate and print relative air quality on a scale of 0-100%
	// use quadratic ratio for steeper scaling at high air quality
	// clip air quality at 100%
    float d = comp_gas / gas_ceil;
	float AQ = d*d*100;
	if(AQ > 100)
        AQ = 100;		

	// for compensating negative drift (dropping resistance) of the gas sensor:
	// delete oldest value from calibration list and add current value
	gas_recal_step ++;
	if(gas_recal_step >= gas_recal_period) {
		gas_recal_step = 0;
		gas_cal_data.push_back(comp_gas);
		gas_cal_data.pop_front();
        average_gas_data();
    }
		
	return AQ;
}

uint16_t sample_count = 1;

void bme680_read()
{
        /* Calculate delay period in microseconds */
     uint32_t del_period;
       del_period = bme68x_get_meas_dur(BME68X_SEQUENTIAL_MODE, &conf, &bme);

    del_period += (heatr_conf.heatr_dur_prof[0] * 1000);
        bme.delay_us(del_period, bme.intf_ptr);

//        time_ms = millis();

        rslt = bme68x_get_data(BME68X_SEQUENTIAL_MODE, data, &n_fields, &bme);
        bme68x_check_rslt("bme68x_get_data", rslt);

        //printf("fields %d\n", n_fields);
        /* Check if rslt == BME68X_OK, report or handle if otherwise */
        for (uint8_t i = 0; i < n_fields; i++)
        {
            /*
            printf("%u, %lu, %.2f, %.2f, %.2f, %.2f, 0x%x, %d, %d\n",
                   sample_count,
                   (long unsigned int)time_ms + (i * (del_period / 2000)),
                   data[i].temperature,
                   data[i].pressure,
                   data[i].humidity,
                   data[i].gas_resistance,
                   data[i].status,
                   data[i].gas_index,
                   data[i].meas_index);*/
            float comp_gas = logf(data[i].gas_resistance) + 0.04f * data[i].humidity;
            //printf("gas reading %d %f %f\n", data[i].gas_index, comp_gas, data[i].gas_resistance);

            pressure = data[i].pressure;
            temperature = data[i].temperature-4.5;
            rel_humidity = data[i].humidity;
            air_quality = getIAQ(data+i);
            sample_count++;
        }
        rslt = bme68x_set_op_mode(BME68X_SLEEP_MODE, &bme);
}

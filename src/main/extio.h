/* Copyright (C) 2024 Sean D'Epagnier <seandepagnier@gmail.com>
 *
 * This Program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 */

#define EXTIO_LED     1
#define EXTIO_DISP        2
#define EXTIO_BL      4
#define EXTIO_ENA_USB         8
#define EXTIO_ENA_NMEA0      16
#define EXTIO_ENA_NMEA1      32
#define EXTIO_ENA_NMEA2      64
#define EXTIO_ENA_NMEA  128

#ifdef CONFIG_IDF_TARGET_ESP32S3
#define EXTIO_LED_num -1
#else
#define EXTIO_LED_num 26
#endif

#ifdef CONFIG_IDF_TARGET_ESP32S3
#define EXTIO_NMEA_num -1
#else
#define EXTIO_NMEA_num 14
#endif


void extio_set(uint8_t val, bool on=true);
void extio_clr(uint8_t val);
void extio_poll();
void extio_setup();


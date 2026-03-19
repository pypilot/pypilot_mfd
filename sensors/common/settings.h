/* Copyright (C) 2026 Sean D'Epagnier <seandepagnier@gmail.com>
 *
 * This Program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 */

extern char wifi_psk[64];
extern uint8_t wifi_channel, rate, tx_power;

void read_settings();
esp_err_t write_settings(int channel, int rate, int tx_power, const char *psk);


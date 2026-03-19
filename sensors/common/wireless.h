/* Copyright (C) 2026 Sean D'Epagnier <seandepagnier@gmail.com>
 *
 * This Program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 */

void enable_ap_poll(void);
void enable_ap(int);
void wifi_init(void);
void wireless_send_data(uint8_t *packet, int len);
void wireless_wait_sent();

extern uint64_t wireless_ap_enabled;


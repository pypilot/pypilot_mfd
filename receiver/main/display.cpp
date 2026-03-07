/* Copyright (C) 2026 Sean D'Epagnier <seandepagnier@gmail.com>
 *
 * This Program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 */

#include <stdio.h>
#include <math.h>

#include <unordered_set>

#include <TFT_eSPI.h>  // Hardware-specific library

#include "settings.h"
#include "display.h"
#include "utils.h"
#include "sensors.h"

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite dsp = TFT_eSprite(&tft);

#define WHITE 0xffff
#define BLACK 0

std::string display_get_item_label(display_item_e item)
{
    return "<item label>";
}

void display_data_update(display_item_e item, float value, data_source_e)
{
}

bool display_data_get(display_item_e item, float &value, std::string &source, uint64_t &time)
{
    time = 0;
    value = 0;
    source = "";

    return false;
}

void display_setup() {
    tft.init();

    printf("free=%u largest=%u\n",
  heap_caps_get_free_size(MALLOC_CAP_8BIT),
  heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));

 dsp.createSprite(tft.width(), tft.height()); // big RAM use

 printf("free=%u largest=%u\n",
  heap_caps_get_free_size(MALLOC_CAP_8BIT),
  heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
 
 //    std::unordered_set<std::string> keys = settings_keys();
    dsp.setSwapBytes(true); // if you use pushImage / 16-bit data
    
    dsp.setRotation(0);
    dsp.fillScreen(TFT_BLACK);

    tft.setTextSize(1);
}

void display_poll()
{
#if 0
    static bool last_force_wifi_ap_mode;

    if (force_wifi_ap_mode != last_force_wifi_ap_mode) {
        tft.fillScreen(TFT_BLACK);
        last_force_wifi_ap_mode = force_wifi_ap_mode;
    }

    if (force_wifi_ap_mode) {
        tft.setTextColor(TFT_RED, TFT_BLACK);
        tft.drawString("WIFI AP", 0, 0, 4);

        tft.drawString("windAP", 0, 20, 4);
        tft.drawString("http://wind.local", 0, 60, 2);
        return;
    }
#endif

    dsp.setTextColor(TFT_GREEN, TFT_BLACK);
    char buf[16];
    static int coords[3][2] = { 0 };
    dsp.fillTriangle(coords[0][0], coords[0][1], coords[1][0], coords[1][1], coords[2][0], coords[2][1], BLACK);

    static int xp = 0;
    static int yp = 0;
    dsp.fillRect(xp, yp, 60, 25, BLACK);  // clear heading text area

    dsp.drawEllipse(67, 67, 67, 67, WHITE);

    if (lpwind_dir >= 0) {
        const uint8_t xc = 67, yc = 67, r = 64;
        float wind_rad = deg2rad(lpwind_dir);
        int x = r * sinf(wind_rad);
        int y = -r * cosf(wind_rad);
        int s = 4;
        xp = s * cosf(wind_rad);
        yp = s * sinf(wind_rad);

        coords[0][0] = xc - xp;
        coords[0][1] = yc - yp;
        coords[1][0] = xc + x;
        coords[1][1] = yc + y;
        coords[2][0] = xc + xp;
        coords[2][1] = yc + yp;

        xp = -31.0 * sinf(wind_rad), yp = 20.0 * cosf(wind_rad);
        static float nxp = 0, nyp = 0;
        nxp = (xp + 15 * nxp) / 16;
        nyp = (yp + 15 * nyp) / 16;
        xp = xc + nxp - 31, yp = yc + nyp - 20;

        snprintf(buf, sizeof buf, "%03d", (int)lpwind_dir);

        dsp.fillTriangle(coords[0][0], coords[0][1], coords[1][0], coords[1][1], coords[2][0], coords[2][1], WHITE);

        dsp.drawString(buf, xp, yp, 4);
    }

    float iknots;
    float iknotf = 10 * modff(wind_knots, &iknots);

    snprintf(buf, sizeof buf, "%02d.%d", (int)iknots, (int)iknotf);
    dsp.drawString(buf, 0, 150, 6);

    dsp.fillRect(0, 195, 135, 50, BLACK);  // clear heading text area
    int accel_yi = accel_y * 135/2;
    int accel_xi = accel_x * 30;
    float x0 = 135/2.0f;
    if(accel_yi > 0)
        x0 -= accel_yi;
    float y0 = 195;
    if(accel_xi > 0)
        y0 += accel_xi;

    dsp.fillRect(x0, y0, abs(accel_yi), 30-abs(accel_xi), TFT_GREEN);

    dsp.setTextColor(TFT_BLUE);
    snprintf(buf, sizeof buf, "%.0f", rad2deg(fabsf(asinf(accel_y))));
    int w = dsp.textWidth(buf, 4);
    dsp.drawString(buf, 135/2-w/2, 195, 4);
    
    dsp.fillRect(0, 220, 135, 35, BLACK);  // c area
    snprintf(buf, sizeof buf, "%.0f", rad2deg(fabsf(asinf(accel_x))));
    dsp.drawString(buf, 0, 220, 4);
    
    dsp.pushSprite(0, 0); // push to screen
}

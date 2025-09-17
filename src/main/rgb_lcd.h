/* Copyright (C) 2025 Sean D'Epagnier <seandepagnier@gmail.com>
 *
 * This Program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 */

#if !defined(__linux__)
static uint8_t *framebuffers[3];
static int vsynccount;

#define DRAW_LCD_PIXEL_CLOCK_HZ     (18 * 1000 * 1000)
#define DRAW_PIN_NUM_HSYNC          -1//47
#define DRAW_PIN_NUM_VSYNC          -1//48
#define DRAW_PIN_NUM_DE             45
#define DRAW_PIN_NUM_PCLK           21
#define DRAW_PIN_NUM_DATA0          13 // B0
#define DRAW_PIN_NUM_DATA1          14 // B1
#define DRAW_PIN_NUM_DATA2          10 // G0
#define DRAW_PIN_NUM_DATA3          11 // G1
#define DRAW_PIN_NUM_DATA4          12 // G2
#define DRAW_PIN_NUM_DATA5          3 // R0
#define DRAW_PIN_NUM_DATA6          46 // R1
#define DRAW_PIN_NUM_DATA7          9 // R2
#define DRAW_PIN_NUM_DISP_EN        -1

#define DRAW_LCD_NUM_FB             3

#include <stdio.h>
#include <string.h>
#include "sdkconfig.h"
#include "rtc_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"

// we use two semaphores to sync the VSYNC event and the LVGL task, to avoid potential tearing effect

static bool IRAM_ATTR example_on_vsync_event(esp_lcd_panel_handle_t panel, const esp_lcd_rgb_panel_event_data_t *event_data, void *user_data)  
{
    vsynccount++;
    return pdTRUE;
}
static esp_lcd_panel_handle_t panel_handle = NULL;
#endif

static std::string last_color_scheme = "none";
void draw_setup(int r)
{
    rotation = r;

#ifdef CONFIG_IDF_TARGET_ESP32S3
    printf("draw: Install RGB LCD panel driver\n");
    esp_lcd_rgb_panel_config_t panel_config = {
        .clk_src = LCD_CLK_SRC_DEFAULT,
        .timings = {
            .pclk_hz = DRAW_LCD_PIXEL_CLOCK_HZ,
            .h_res = DRAW_LCD_H_RES,
            .v_res = DRAW_LCD_V_RES,
            // The following parameters should refer to LCD spec
            .hsync_pulse_width = 8,
            .hsync_back_porch = 4,
            .hsync_front_porch = 8,
            .vsync_pulse_width = 4,
            .vsync_back_porch = 16,
            .vsync_front_porch = 16,
            .flags =
            {
                .hsync_idle_low = false,
                .vsync_idle_low = false,
                .de_idle_high = false,
                .pclk_active_neg = true,
                //.pclk_idle_high = false,
            },
            //.flags.pclk_active_neg = true,
            //.flags.de_idle_high = true,
            //.flags.pclk_idle_high = true,
        },
        .data_width = 8, // RGB332 in parallel mode
        .bits_per_pixel = 8,
        .num_fbs = DRAW_LCD_NUM_FB,
        .bounce_buffer_size_px = 0,//16*DRAW_LCD_H_RES,
        .sram_trans_align = 0,
        .psram_trans_align = 64,

        .hsync_gpio_num = DRAW_PIN_NUM_HSYNC,
        .vsync_gpio_num = DRAW_PIN_NUM_VSYNC,
        .de_gpio_num = DRAW_PIN_NUM_DE,
        .pclk_gpio_num = DRAW_PIN_NUM_PCLK,
        .disp_gpio_num = DRAW_PIN_NUM_DISP_EN,
        .data_gpio_nums = {
            DRAW_PIN_NUM_DATA0,
            DRAW_PIN_NUM_DATA1,
            DRAW_PIN_NUM_DATA2,
            DRAW_PIN_NUM_DATA3,
            DRAW_PIN_NUM_DATA4,
            DRAW_PIN_NUM_DATA5,
            DRAW_PIN_NUM_DATA6,
            DRAW_PIN_NUM_DATA7,
        },
        .flags = {
            .disp_active_low = (uint32_t)NULL,
            .refresh_on_demand = false,
            .fb_in_psram = true,
            .double_fb = false,
            .no_fb = false,
            .bb_invalidate_cache = (uint32_t)NULL,
        }
    };
    ESP_ERROR_CHECK(esp_lcd_new_rgb_panel(&panel_config, &panel_handle));

    printf("draw: Register event callbacks\n");
    esp_lcd_rgb_panel_event_callbacks_t cbs = {
        .on_vsync = example_on_vsync_event,
        .on_bounce_empty = 0,
        .on_bounce_frame_finish = 0,
    };
    ESP_ERROR_CHECK(esp_lcd_rgb_panel_register_event_callbacks(panel_handle, &cbs, NULL));

    printf("draw: Initialize RGB LCD panel\n");
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));

    ESP_ERROR_CHECK(esp_lcd_rgb_panel_get_frame_buffer(panel_handle, 3, (void**)&framebuffers[0], (void**)&framebuffers[1], (void**)&framebuffers[2]));
    framebuffer = framebuffers[0];

    printf("got the framebuffers %p %p %p\n", framebuffers[0], framebuffers[1], framebuffers[2]);
    
#if 0
    printf("SETTING GPIO HIGH\n");
    extio_set(EXTIO_DISP);
#endif

    memset(framebuffers[0], 0, DRAW_LCD_V_RES*DRAW_LCD_H_RES);
    memset(framebuffers[1], 0, DRAW_LCD_V_RES*DRAW_LCD_H_RES);
    memset(framebuffers[2], 0, DRAW_LCD_V_RES*DRAW_LCD_H_RES);
#else
    // emulation on linux
    //framebuffers[0] = (uint8_t*)malloc(DRAW_LCD_H_RES*DRAW_LCD_V_RES);
    //framebuffers[1] = (uint8_t*)malloc(DRAW_LCD_H_RES*DRAW_LCD_V_RES);
    //framebuffers[2] = (uint8_t*)malloc(DRAW_LCD_H_RES*DRAW_LCD_V_RES);
    framebuffer = (uint8_t*)malloc(DRAW_LCD_H_RES*DRAW_LCD_V_RES); //framebuffers[0];
#endif
}

static void putpixel(int x, int y, uint8_t c)
{    
    if(x < 0 || y < 0 || x >= DRAW_LCD_H_RES || y >= DRAW_LCD_V_RES)
        return;
    if(c >= GRAYS) {
        printf("putpixel gradient out of range %d\n", c);
        return;
    }
    if(c == 0) // nothing to do
        return;

    uint8_t value = palette[color][c];
    if(c == GRAYS-1)
        framebuffer[DRAW_LCD_H_RES*y+x] = value;
    else
#if 0
        uint8_t value = palette[color][GRAYS-1];

        uint8_t fv = framebuffer[DRAW_LCD_H_RES*y+x];

        uint8_t rf = (fv>>5);
        uint8_t gf = ((fv>>2)&0x7);
        uint8_t bf = ((fv&0x3)<<1);
        uint8_t rv = (value>>5);
        uint8_t gv = ((value>>2)&0x7);
        uint8_t bv = ((value&0x3)<<1);

        uint8_t r = rv*c/7 + rf*(7-c)/7;
        uint8_t g = gv*c/7 + gf*(7-c)/7;
        uint8_t b = bv*c/7 + bf*(7-c)/7;

        if(r > 7) r = 7;
        if(g > 7) g = 7;
        if(b > 7) b = 7;

        framebuffer[DRAW_LCD_H_RES*y+x] = (r<<5) | (g<<2) | (b>>1);
#else
        framebuffer[DRAW_LCD_H_RES*y+x] |= value;
#endif

}

#if 1
// produce 8 bit color from 3 bit per channel mapped onto rgb332
uint8_t rgb3(uint8_t r, uint8_t g, uint8_t b)
{
    b >>= 1;
    return (r << 5) | (g << 2) | b;
}
#else
// produce 8 bit color from 3 bit per channel mapped onto rgb2321 with last bit shared for red and blue
uint8_t rgb3(uint8_t r, uint8_t g, uint8_t b)
{
    uint8_t l = (r&&b) ? (b&1) : 0;
    r >>= 1;
    b >>= 1;
    return (r << 6) | (g << 3) | (b << 1) | l;
}
#endif

// b from 0-8;  convert enum to rgb232+1
static uint8_t compute_color(color_e c, uint8_t b)
{
    uint8_t v=0;
        // default
    switch(c) {
    case WHITE:   v = rgb3(b, b, b); break;
    case RED:     v = rgb3(b, 0, 0); break;
    case GREEN:   v = rgb3(0, b, 0); break;
    case BLUE:    v = rgb3(0, 0, b); break;
    case CYAN:    v = rgb3(0, b, b); break;
    case MAGENTA: v = rgb3(b, 0, b); break;
    case YELLOW:  v = rgb3(b, b, 0); break;
    case GREY:    v = rgb3(b/2, b/2, b/2); break;
    case ORANGE:  v = rgb3(b, b/2, 0); break;
    case BLACK:   v = rgb3(0, 0, 0); break;
    default: break;
    }

#ifdef CONFIG_IDF_TARGET_ESP32S3
    if(settings.color_scheme == "light")
        v = ~v;
    else if(settings.color_scheme == "sky")
        v ^= rgb3(0, 3, 7);
    if(settings.color_scheme == "mars")
        v ^= rgb3(7, 1, 0);
#endif    
    return v;
}

static inline void draw_scanline(int x, int y, int value, int count) {
    // easy with 8bpp each byte is a pixel
    memset(framebuffer + DRAW_LCD_H_RES*y+x, value, count);
}

static inline void invert_scanline(int x, int y, int count)
{
    for(int x0 = x; x0<x+count; x0++)
        framebuffer[DRAW_LCD_H_RES*y+x0] = ~framebuffer[DRAW_LCD_H_RES*y+x0];
}

void draw_clear(bool display_on)
{
    uint8_t c = 0;
#ifdef CONFIG_IDF_TARGET_ESP32S3
    // rebuild palette if needed
    if(settings.color_scheme != last_color_scheme)
    {
#endif
        for(int i=0; i<COLOR_COUNT; i++)
            for(int j=0; j<GRAYS; j++)
                palette[i][j] = compute_color((color_e)i, j);
#ifdef CONFIG_IDF_TARGET_ESP32S3
        last_color_scheme = settings.color_scheme;
    }        
    
    extio_set(EXTIO_DISP, display_on);
    extio_set(EXTIO_BL,   display_on); // enable backlight driver

    if(settings.color_scheme == "light")
        c = 0xff;
    else if(settings.color_scheme == "dusk")
        c = 0x40;
#endif

    memset(framebuffer, c, DRAW_LCD_H_RES*DRAW_LCD_V_RES);
}

static uint32_t t0start;

#ifndef __linux__
void draw_send_buffer()
{
    uint32_t t1 = esp_timer_get_time();
    esp_lcd_panel_draw_bitmap(panel_handle, 0, 0, DRAW_LCD_H_RES, DRAW_LCD_V_RES, framebuffer);
        
    uint32_t t2 = esp_timer_get_time();

    if(framebuffer == framebuffers[0])
        framebuffer = framebuffers[1];
    else if(framebuffer == framebuffers[1])
        framebuffer = framebuffers[2];
    else
        framebuffer = framebuffers[0];
        
    //printf("frame %d %lu %lu\n", vsynccount, t1-t0start, t2-t1);
    vsynccount=0;
    t0start = esp_timer_get_time();
}
#endif

/* Copyright (C) 2024 Sean D'Epagnier <seandepagnier@gmail.com>
 *
 * This Program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 */

#ifndef __linux__
#include <Arduino.h>

#else

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#define PROGMEM

#endif
#include <sys/time.h>
#include <string>
#include <list>


#include "settings.h"
#include "draw.h"
#include "extio.h"

#ifdef USE_U8G2

#include <U8g2lib.h>
//U8G2_ST75256_JLX256160_F_4W_SW_SPI u8g2(U8G2_R1, /* clock=*/15, /* data=*/13, /* cs=*/5, /* dc=*/12, /* reset=*/14);
//U8G2_ST75256_JLX256160_F_4W_SW_SPI u8g2(U8G2_R1, /* clock=*/18, /* data=*/23, /* cs=*/5, /* dc=*/12, /* reset=*/14);
U8G2_ST75256_JLX256160_F_4W_HW_SPI u8g2(U8G2_R1, /* cs=*/5, /* dc=*/12, /* reset=*/13);
//U8G2_ST75256_JLX256160_2_4W_HW_SPI u8g2(U8G2_R1, /* cs=*/5, /* dc=*/12, /* reset=*/13);

void draw_setup(int rotation)
{
    u8g2.begin();
    u8g2.enableUTF8Print();
    u8g2.setFontPosTop();

    if(settings.mirror == 2)
        settings.mirror = 0;
    //settings.mirror = !digitalRead(2);

    if(settings.mirror)
        rotation ^= 2;

    switch(rotation) {
        case 0: u8g2.setDisplayRotation(U8G2_R0); break;
        case 1: u8g2.setDisplayRotation(U8G2_R1); break;
        case 2: u8g2.setDisplayRotation(U8G2_R2); break;
        case 3: u8g2.setDisplayRotation(U8G2_R3); break;
    };        

    if(settings.mirror)
        u8g2.setFlipMode(true);
}

void draw_thick_line(int x0, int y0, int x1, int y1, int w)
{
    if(w < 2)
        draw_line(x0, y0, x1, y1);

    int ex = x1-x0, ey = y1-y0;
    int d = sqrt(ex*ex + ey*ey);

     if(d == 0)
        return;

    int ex0 = (ex*w/d+(ex > 0 ? 1 : -1))/2;
    int ey0 = (ey*w/d+(ey > 0 ? 1 : -1))/2;
    int ex1 = ex*w/d/2;
    int ey1 = ey*w/d/2;

    int ax = x0+ey0, ay = y0-ex0;
    int bx = x0-ey1, by = y0+ex1;
    int cx = x1+ey0, cy = y1-ex0;
    int dx = x1-ey1, dy = y1+ex1;
 
    u8g2.drawTriangle(ax, ay, bx, by, cx, cy);
    u8g2.drawTriangle(bx, by, dx, dy, cx, cy);
}

void draw_circle(int x, int y, int r, int thick)
{
    r -= thick;
    if(r <= 0) {
        thick += r-1;
        r = 1;
    }
    for (int i = -(thick+1)/2; i <= thick/2; i++)
        for (int j = -(thick+1)/1; j <= thick/2; j++)
            u8g2.drawCircle(x + i, y + j, r);
}

void draw_line(int x1, int y1, int x2, int y2, bool)
{
    u8g2.drawLine(x1, y1, x2, y2);
}

void draw_box(int x, int y, int w, int h, bool invert)
{
    if(invert)
        u8g2.setDrawColor(2);
    u8g2.drawBox(x, y, w, h);
    if(invert)
        u8g2.setDrawColor(settings.invert ? 0 : 1);
}

void draw_triangle(int x1, int y1, int x2, int y2, int x3, int y3)
{
    u8g2.drawTriangle(x1, y1, x2, y2, x3, y3);
}

const uint8_t *getFont(int &ht) {
    if(ht < 7) return 0;
    if(ht < 11) return ht = 7, u8g2_font_5x7_tf;
    if (ht < 13) return ht = 11, u8g2_font_helvB08_tf;
    if (ht < 15) return ht = 13, u8g2_font_helvB10_tf;
    if (ht < 18) return ht = 15, u8g2_font_helvB12_tf;
    if (ht < 24) return ht = 18, u8g2_font_helvB14_tf;
    if (ht < 28) return ht = 24, u8g2_font_helvB18_tf;
    if (ht < 30) return ht = 28, u8g2_font_helvB24_tf;
    if (ht < 35) return ht = 30, u8g2_font_inb24_mf;
    if (ht < 40) return ht = 35, u8g2_font_inb27_mf;
    if (ht < 44) return ht = 40, u8g2_font_inb30_mf;
    if (ht < 48) return ht = 44, u8g2_font_inb33_mf;
    if (ht < 52) return ht = 48, u8g2_font_inb38_mf;
    if (ht < 56) return ht = 52, u8g2_font_inb42_mf;
    return ht = 56, u8g2_font_inb46_mf;
}

bool draw_set_font(int &ht)
{
    const uint8_t *font = getFont(ht);
    if(!font)
        return false;
    u8g2.setFont(font);
    return true;
}

int draw_text_width(const std::string &str)
{
    return u8g2.getStrWidth(str.c_str());
}

void draw_text(int x, int y, const std::string &str)
{
    u8g2.drawUTF8(x, y, str.c_str());
}

void draw_color(color_e color)
{
    // ignored since monochrome
}

void draw_clear(bool display_on)
{
    u8g2.setContrast(160 + settings.contrast);

    if(!display_on) {
        u8g2.clearBuffer();
        draw_send_buffer();
        return;
    }

    if(settings.invert) {
        u8g2.setDrawColor(1);
        int w = u8g2.getDisplayWidth();
        int h = u8g2.getDisplayHeight();
        u8g2.drawBox(0, 0, w, h);
        u8g2.setDrawColor(0);
    } else {
        u8g2.clearBuffer();
        u8g2.setDrawColor(1);
    }
}

void draw_send_buffer()
{
    u8g2.sendBuffer();
}

#endif

#ifdef USE_RGB_LCD     // color lcd

#include "fonts.h"

static uint8_t color;

#define GRAYS 8 // 3bpp insternal

// for now 256 entrees, could make it less
static uint8_t palette[COLOR_COUNT][GRAYS];

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
uint8_t compute_color(color_e c, uint8_t b)
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

uint8_t *framebuffer;
static uint8_t *framebuffers[3];
static int vsynccount;
static int rotation;

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

// The pixel number in horizontal and vertical
#define DRAW_LCD_H_RES              800
#define DRAW_LCD_V_RES              480

#define DRAW_LCD_NUM_FB             3


#ifdef CONFIG_IDF_TARGET_ESP32S3

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

#else
    // emulation on linux
    framebuffers[0] = (uint8_t*)malloc(DRAW_LCD_H_RES*DRAW_LCD_V_RES);
    framebuffers[1] = (uint8_t*)malloc(DRAW_LCD_H_RES*DRAW_LCD_V_RES);
    framebuffers[2] = (uint8_t*)malloc(DRAW_LCD_H_RES*DRAW_LCD_V_RES);
    framebuffer = framebuffers[0];
#endif
    memset(framebuffers[0], 0, DRAW_LCD_V_RES*DRAW_LCD_H_RES);
    memset(framebuffers[1], 0, DRAW_LCD_V_RES*DRAW_LCD_H_RES);
    memset(framebuffers[2], 0, DRAW_LCD_V_RES*DRAW_LCD_H_RES);
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
        uint8_t value = palette[color][7];

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

#define putpixeli(x, y, c) putpixel(x, y, ((GRAYS-1)-(c)))

static void convert_coords(int &x, int &y)
{
    switch(rotation) {
    case 3: {
        int t = x;
        x = y;
        y = DRAW_LCD_V_RES - t - 1;
    } break;
    case 2: // rotate 180
        x = DRAW_LCD_H_RES - x - 1;
        y = DRAW_LCD_V_RES - y - 1;
        break;
    case 1: {
        int t = x;
        x = DRAW_LCD_H_RES - y - 1;
        y = t;
    } break;
    default: // no conversion
        break;
    }
}

void draw_line(int x0, int y0, int x1, int y1, bool convert)
{
    if(convert) {
        convert_coords(x0, y0);
        convert_coords(x1, y1);
    }
    
    // http://members.chello.at/~easyfilter/bresenham.c
    /* draw a black (0) anti-aliased line on white (255) background */
    int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, x2;
    long dx = abs(x1-x0), dy = abs(y1-y0), err = dx*dx+dy*dy;
    long e2 = err == 0 ? 1 : 0xffff7fl/sqrtf(err);     /* multiplication factor */

    dx *= e2; dy *= e2; err = dx-dy;                       /* error value e_xy */
    for ( ; ; ){                                                 /* pixel loop */
        putpixeli(x0,y0,abs(err-dx+dy)>>21);
        e2 = err; x2 = x0;
        if (2*e2 >= -dx) {                                            /* x step */
            if (x0 == x1) break;
            if (e2+dy < 0xff0000l) putpixeli(x0,y0+sy,(e2+dy)>>21);
            err -= dy; x0 += sx;
        }
        if (2*e2 <= dy) {                                             /* y step */
            if (y0 == y1) break;
            if (dx-e2 < 0xff0000l) putpixeli(x2+sx,y0,(dx-e2)>>21);
            err += dx; y0 += sy;
        }
    }
}

#define MAX(a, b) ((a>b) ? (a) : (b))
#define MIN(a, b) ((a<b) ? (a) : (b))
void draw_thick_line(int x0, int y0, int x1, int y1, int wd)
{
    convert_coords(x0, y0);
    convert_coords(x1, y1);
#if 0
    int dx = abs(x1-x0), sx = x0 < x1 ? 1 : -1;
    int dy = abs(y1-y0), sy = y0 < y1 ? 1 : -1;
    int err = dx-dy, e2, x2, y2;                           /* error value e_xy */
    int ed = dx+dy == 0 ? 1 : sqrtf((float)dx*dx+(float)dy*dy);

    for (wd = (wd+1)/2; ; ) {                                    /* pixel loop */
        putpixel(x0, y0, MAX(0,MIN(GRAYS-1, (GRAYS-1)*(wd*ed-abs(err-dx+dy))/ed)));
        e2 = err; x2 = x0;
        if (2*e2 >= -dx) {                                            /* x step */
            for (e2 += dy, y2 = y0; e2 < ed*wd && (y1 != y2 || dx > dy); e2 += dx)
                putpixel(x0, y2 += sy, MAX(0,MIN(GRAYS-1, (GRAYS-1)*(wd*ed-abs(e2))/ed)));
            if (x0 == x1) break;
            e2 = err; err -= dy; x0 += sx;
        }
        if (2*e2 <= dy) {                                             /* y step */
            for (e2 = dx-e2; e2 < ed*wd && (x1 != x2 || dx < dy); e2 += dy)
                putpixel(x2 += sx, y0, MAX(0,MIN(GRAYS-1, (GRAYS-1)*(wd*ed-abs(e2))/ed)));
            if (y0 == y1) break;
            err += dx; y0 += sy;
        }
    }
#elif 0 // this is twice faster than above :(
    int ex = x1-x0, ey = y1-y0;
    int d = sqrt(ex*ex + ey*ey);
    if(d == 0)
        return;

    ex = ex*wd/d/2;
    ey = ey*wd/d/2;

    int ax = x0+ey, ay = y0-ex;
    int bx = x0-ey, by = y0+ex;
    int cx = x1+ey, cy = y1-ex;
    int dx = x1-ey, dy = y1+ex;
    draw_triangle(ax, ay, bx, by, cx, cy);
    draw_triangle(bx, by, dx, dy, cx, cy);
#else // like 8 times faster but needs antialiasing
    if(y0>y1) {
        int t = y0;
        y0 = y1;
        y1 = t;
        t = x0;
        x0 = x1;
        x1 = t;
    }

    int ax = abs(x1-x0), sw;
    float dx = float(x1-x0) / (y1-y0);
    if(ax == 0)
        sw = ax;
    else {
        sw = wd * hypotf(x1-x0, y1-y0) / ax;
        if(sw > ax)
            sw = ax;
    }

    float x = x0-sw/2;
    uint8_t value = palette[color][7];
    for(int y=y0; y<= y1; y++) {
        memset(framebuffer + DRAW_LCD_H_RES*y+(int)x, value, sw);
        x += dx;
    }
#endif
}

void draw_circle_thin(int xm, int ym, int r)
{
   convert_coords(xm, ym);
                       /* draw a black anti-aliased circle on white background */
   int x = -r, y = 0;           /* II. quadrant from bottom left to top right */
   int i, x2, e2, err = 2-2*r;                             /* error of 1.step */
   r = 1-err;
   do {
       i = (GRAYS-1)*abs(err-2*(x+y)-2)/r;               /* get blend value of pixel */
       putpixeli(xm-x, ym+y, i);                             /*   I. Quadrant */
       putpixeli(xm-y, ym-x, i);                             /*  II. Quadrant */
       putpixeli(xm+x, ym-y, i);                             /* III. Quadrant */
       putpixeli(xm+y, ym+x, i);                             /*  IV. Quadrant */
      e2 = err; x2 = x;                                    /* remember values */
      if (err+y > 0) {                                              /* x step */
          i = (GRAYS-1)*(err-2*x-1)/r;                              /* outward pixel */
          if (i < 256) {
              putpixeli(xm-x, ym+y+1, i);
              putpixeli(xm-y-1, ym-x, i);
              putpixeli(xm+x, ym-y-1, i);
              putpixeli(xm+y+1, ym+x, i);
         }
         err += ++x*2+1;
      }
      if (e2+x2 <= 0) {                                             /* y step */
          i = (GRAYS-1)*(2*y+3-e2)/r;                                /* inward pixel */
          if (i < 256) {
              putpixeli(xm-x2-1, ym+y, i);
              putpixeli(xm-y, ym-x2-1, i);
              putpixeli(xm+x2+1, ym-y, i);
              putpixeli(xm+y, ym+x2+1, i);
         }
         err += ++y*2+1;
      }
   } while (x < 0);
}

void draw_circle_orig(int xm, int ym, int r, int th)
{
    /* draw anti-aliased ellipse inside rectangle with thick line... could be optimized considerably */
    //r--; // todo: is this correct??
    convert_coords(xm, ym);
    int x0 = xm - r, x1 = xm + r, y0 = ym - r, y1 = ym + r;
    
    int a = abs(x1-x0), b = abs(y1-y0), b1 = b&1;  /* outer diameter */
    int a2 = a-2*th, b2 = b-2*th;                            /* inner diameter */
    int dx = 4*(a-1)*b*b, dy = 4*(b1-1)*a*a;                /* error increment */
    float i = a+b2, err = b1*a*a, dx2, dy2, e2;
    float ed; 
                                                     /* thick line correction */
    if (th < 1.5) return draw_circle_thin(xm, ym, r);
    if ((th-1)*(2*b-th) > a*a) b2 = sqrtf(a*(b-a)*i*a2)/(a-th);       
    if ((th-1)*(2*a-th) > b*b) { a2 = sqrtf(b*(a-b)*i*b2)/(b-th); th = (a-a2)/2; }
    if (a == 0 || b == 0) return draw_line(x0,y0, x1,y1);
    if (x0 > x1) { x0 = x1; x1 += a; }        /* if called with swapped points */
    if (y0 > y1) y0 = y1;                                  /* .. exchange them */
    if (b2 <= 0) th = a;                                     /* filled ellipse */
    e2 = 0/*th-floorf(th)*/; th = x0+th-e2;
    dx2 = 4*(a2+2*e2-1)*b2*b2; dy2 = 4*(b1-1)*a2*a2; e2 = dx2*e2;
    y0 += (b+1)>>1; y1 = y0-b1;                              /* starting pixel */
    a = 8*a*a; b1 = 8*b*b; a2 = 8*a2*a2; b2 = 8*b2*b2;
   
    do {
        for (;;) {                           
            if (err < 0 || x0 > x1) { i = x0; break; }
            i = MIN(dx,dy); ed = MAX(dx,dy);
            if (y0 == y1+1 && 2*err > dx && a > b1) ed = a/4;           /* x-tip */
            else ed += 2*ed*i*i/(4*ed*ed+i*i+1)+1;/* approx ed=sqrt(dx*dx+dy*dy) */
            i = (GRAYS-1)*err/ed;                             /* outside anti-aliasing */
            putpixeli(x0,y0, i); putpixeli(x0,y1, i);
            putpixeli(x1,y0, i); putpixeli(x1,y1, i);
            if (err+dy+a < dx) { i = x0+1; break; }
            x0++; x1--; err -= dx; dx -= b1;                /* x error increment */
      }
      for (; i < th && 2*i <= x0+x1; i++) {                /* fill line pixel */
          putpixeli(i,y0,0); putpixeli(x0+x1-i,y0,0); 
          putpixeli(i,y1,0); putpixeli(x0+x1-i,y1,0);
      }    
      while (e2 > 0 && x0+x1 >= 2*th) {               /* inside anti-aliasing */
         i = MIN(dx2,dy2); ed = MAX(dx2,dy2);
         if (y0 == y1+1 && 2*e2 > dx2 && a2 > b2) ed = a2/4;         /* x-tip */
         else  ed += 2*ed*i*i/(4*ed*ed+i*i);                 /* approximation */
         i = (GRAYS-1)-(GRAYS-1)*e2/ed;             /* get intensity value by pixel error */
         putpixeli(th,y0, i); putpixeli(x0+x1-th,y0, i);
         putpixeli(th,y1, i); putpixeli(x0+x1-th,y1, i);
         if (e2+dy2+a2 < dx2) break; 
         th++; e2 -= dx2; dx2 -= b2;                     /* x error increment */
      }
      e2 += dy2 += a2;
      y0++; y1--; err += dy += a;                                   /* y step */
   } while (x0 < x1);
   
   if (y0-y1 <= b)             
   {
      if (err > dy+a) { y0--; y1++; err -= dy -= a; }
      for (; y0-y1 <= b; err += dy += a) { /* too early stop of flat ellipses */
         i = (GRAYS-1)*4*err/b1;           /* -> finish tip of ellipse */
         putpixeli(x0,y0, i); putpixeli(x1,y0++, i);
         putpixeli(x0,y1, i); putpixeli(x1,y1--, i);
      }
   }
}

uint8_t *sp_malloc(int size)
{
#ifdef __linux__
    return (uint8_t*)malloc(size);
#else
    return (uint8_t*)heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
#endif    
}

struct circle_cache_t
{
    circle_cache_t(int size, uint8_t *d) : sz(size) {
        gettimeofday(&tv, 0);
        enc = sp_malloc(sz);
        memcpy(enc, d, sz);
    }

    uint8_t *get_data() {
        gettimeofday(&tv, 0);
        return enc;
    }

    int age() {
        timeval tv2;
        gettimeofday(&tv2, 0);
        return 1000000*(tv2.tv_sec - tv.tv_sec) + (tv2.tv_usec - tv.tv_usec);
    }
        
    struct timeval tv;
    int sz;
    uint8_t *enc;
};

static std::map <std::pair <int, int>, circle_cache_t > circle_cache;
static void putpixelb(uint8_t *buf, int r, int x, int y, uint8_t c)
{
    if(x<0 || y < 0 || x> 2*r || y > r)
        return;
    int d = 7-c;
    if(d<0) d = 0;
    if(d>7) d = 7; // should not normally get hit
    buf[(2*r+1)*y+x] = d;
}

void draw_circle(int xm, int ym, int r, int th)
{
    //printf("draw circle %d %d %d %d\n", xm, ym, r, th);
//    draw_circle_orig(xm, ym, r, th);
//    return;
    
    if (th < 1.5) return draw_circle_thin(xm, ym, r);
    if(r < 20) {
        draw_circle_orig(xm, ym, r, th);        
        return;
    }

    std::pair pair = std::make_pair(r, th);
    if(circle_cache.find(pair) != circle_cache.end()) {
        convert_coords(xm, ym);
        circle_cache_t &c = circle_cache.at(pair);
        uint8_t *buf = c.get_data();
        int sz = c.sz;
        int x = 0, y = 0;
        for(int i=0; i<sz; i++) {
            if(buf[i] & 0x80) {
                uint8_t len = (buf[i] & ~0x80) + 1;
                x+=len%(2*r+1);
                y+=len/(2*r+1);
                if(x > 2*r) {
                    x -= 2*r+1;
                    y++;
                }
            } else {
                uint8_t c = buf[i]&0x7;
                uint8_t len = (buf[i]>>3) + 1;

                uint8_t value = palette[color][c];
                int len1 = len, len2=0;
                if(len1 + x > 2*r) {
                    len1 = 2*r+1 - x;
                    len2 = len - len1;
                }

                int idx1 = DRAW_LCD_H_RES*(ym-r+y);
                int idx2 = DRAW_LCD_H_RES*(ym+r-y);
                int lenr = len1;
                int xp = xm - r + x;
                if(xp < 0) {
                    lenr += xp;
                    xp = 0;
                } else {
                    idx1 += xp;
                    idx2 += xp;
                }
                
                if(xp + lenr >= DRAW_LCD_H_RES)
                    lenr = DRAW_LCD_H_RES - 1 - xp;

                if(lenr > 0) {
                    if(c == GRAYS-1) {
                        if(idx1 > 0 && idx1 + lenr < DRAW_LCD_H_RES*DRAW_LCD_V_RES)
                            memset(framebuffer + idx1, value, lenr);
                        if(idx2 > 0 && idx2 + lenr < DRAW_LCD_H_RES*DRAW_LCD_V_RES)
                            memset(framebuffer + idx2, value, lenr);
                    } else {
                        if(idx1 > 0 && idx1 + lenr < DRAW_LCD_H_RES*DRAW_LCD_V_RES)
                            for(int i=0; i<lenr; i++)
                                framebuffer[idx1++] |= value;
                        if(idx2 > 0 && idx2 + lenr < DRAW_LCD_H_RES*DRAW_LCD_V_RES)
                            for(int i=0; i<lenr; i++)
                                framebuffer[idx2++] |= value;
                    }
                }
                x+=len1;
                if(x > 2*r) {
                    x -= 2*r+1;
                    y++;
                }
                
                // len2
                if(len2) {
                    idx1 = DRAW_LCD_H_RES*(ym-r+y);
                    idx2 = DRAW_LCD_H_RES*(ym+r-y);

                    int lenr = len2;
                    xp = xm - r + x;
                    if(xp < 0) {
                        lenr += xp;
                        xp = 0;
                    } else {
                        idx1 += xp;
                        idx2 += xp;
                    }
                
                    if(xp + lenr >= DRAW_LCD_H_RES)
                        lenr = DRAW_LCD_H_RES - 1 - xp;

                    if(lenr > 0) {
                        if(c == GRAYS-1) {
                            if(idx1 > 0 && idx1 + lenr < DRAW_LCD_H_RES*DRAW_LCD_V_RES)
                                memset(framebuffer + idx1, value, lenr);
                            if(idx2 > 0 && idx2 + lenr < DRAW_LCD_H_RES*DRAW_LCD_V_RES)
                                memset(framebuffer + idx2, value, lenr);
                        } else {
                            if(idx1 > 0 && idx1 + lenr < DRAW_LCD_H_RES*DRAW_LCD_V_RES)
                                for(int i=0; i<lenr; i++)
                                    framebuffer[idx1++] |= value;
                            if(idx2 > 0 && idx2 + lenr < DRAW_LCD_H_RES*DRAW_LCD_V_RES)
                                for(int i=0; i<lenr; i++)
                                    framebuffer[idx2++] |= value;
                        }
                    }
                    x+=len2;
                }
            }
        }
        return;
    }

    // render to buffer 1/2 circle
    uint8_t *buf = sp_malloc(2*(r+1)*(r+1));
    memset(buf, 0, 2*(r+1)*(r+1));
    /* draw anti-aliased ellipse inside rectangle with thick line... could be optimized considerably */
    int x0 = 0, x1 = 2*r, y0 = 0, y1 = 2*r;
    
    int a = abs(x1-x0), b = abs(y1-y0), b1 = b&1;  /* outer diameter */
    int a2 = a-2*th, b2 = b-2*th;                            /* inner diameter */
    int dx = 4*(a-1)*b*b, dy = 4*(b1-1)*a*a;                /* error increment */
    float i = a+b2, err = b1*a*a, dx2, dy2, e2;
    float ed;
    int oth = th;
                                                     /* thick line correction */
    if ((th-1)*(2*b-th) > a*a) b2 = sqrtf(a*(b-a)*i*a2)/(a-th);       
    if ((th-1)*(2*a-th) > b*b) { a2 = sqrtf(b*(a-b)*i*b2)/(b-th); th = (a-a2)/2; }
//    if (a == 0 || b == 0) return draw_line(x0,y0, x1,y1);
    if (x0 > x1) { x0 = x1; x1 += a; }        /* if called with swapped points */
    if (y0 > y1) y0 = y1;                                  /* .. exchange them */
    if (b2 <= 0) th = a;                                     /* filled ellipse */
    e2 = 0/*th-floorf(th)*/; th = x0+th-e2;
    dx2 = 4*(a2+2*e2-1)*b2*b2; dy2 = 4*(b1-1)*a2*a2; e2 = dx2*e2;
    y0 += (b+1)>>1; y1 = y0-b1;                              /* starting pixel */
    a = 8*a*a; b1 = 8*b*b; a2 = 8*a2*a2; b2 = 8*b2*b2;
   
    do {
        for (;;) {                           
            if (err < 0 || x0 > x1) { i = x0; break; }
            i = MIN(dx,dy); ed = MAX(dx,dy);
            if (y0 == y1+1 && 2*err > dx && a > b1) ed = a/4;           /* x-tip */
            else ed += 2*ed*i*i/(4*ed*ed+i*i+1)+1;/* approx ed=sqrt(dx*dx+dy*dy) */
            i = (GRAYS-1)*err/ed;                             /* outside anti-aliasing */
            //putpixelb(buf, r, x0,y0, i);
            putpixelb(buf, r, x0,y1, i);
            //putpixelb(buf, r, x1,y0, i);
            putpixelb(buf, r, x1,y1, i);
            if (err+dy+a < dx) { i = x0+1; break; }
            x0++; x1--; err -= dx; dx -= b1;                /* x error increment */
      }
      for (; i < th && 2*i <= x0+x1; i++) {                /* fill line pixel */
          //putpixelb(buf, r, i,y0,0); putpixelb(buf, r, x0+x1-i,y0,0); 
          putpixelb(buf, r, i,y1,0); putpixelb(buf, r, x0+x1-i,y1,0);
      }    
      while (e2 > 0 && x0+x1 >= 2*th) {               /* inside anti-aliasing */
         i = MIN(dx2,dy2); ed = MAX(dx2,dy2);
         if (y0 == y1+1 && 2*e2 > dx2 && a2 > b2) ed = a2/4;         /* x-tip */
         else  ed += 2*ed*i*i/(4*ed*ed+i*i);                 /* approximation */
         i = (GRAYS-1)-(GRAYS-1)*e2/ed;             /* get intensity value by pixel error */
         //putpixelb(buf, r, th,y0, i); putpixelb(buf, r, x0+x1-th,y0, i);
         putpixelb(buf, r, th,y1, i); putpixelb(buf, r, x0+x1-th,y1, i);
         if (e2+dy2+a2 < dx2) break; 
         th++; e2 -= dx2; dx2 -= b2;                     /* x error increment */
      }
      e2 += dy2 += a2;
      y0++; y1--; err += dy += a;                                   /* y step */
   } while (x0 < x1);

#if 0 // seems to be not needed
   if (y0-y1 <= b)             
   {
      if (err > dy+a) { y0--; y1++; err -= dy -= a; }
      for (; y0-y1 <= b; err += dy += a) { /* too early stop of flat ellipses */
         i = (GRAYS-1)*4*err/b1;           /* -> finish tip of ellipse */
         putpixelb(buf, r, x0,y0, i); putpixelb(buf, r, x1,y0++, i);
         putpixelb(buf, r, x0,y1, i); putpixelb(buf, r, x1,y1--, i);
      }
   }
#endif

   // now encode the buffer
   uint8_t *rbuf = sp_malloc(2*r*(r+1));
   int cnt = 1, cur = buf[0], sz=0;
   for(int i=1; i<2*(r+1)*(r+1); i++) {
       if(cur && (cur != buf[i] || cnt==16)) {
           // encode up to 16 runs for c>0
           rbuf[sz++] = cur | ((cnt-1)<<3);
           cur = buf[i];
           cnt = 1;
       } else if(!cur && (cur != buf[i] || cnt == 128)) {
           // encode up to 128 runs for c == 0
           rbuf[sz++] = 0x80 | (cnt-1);
           cur = buf[i];
           cnt = 1;
       } else
           cnt++;
   }
   free(buf);
   circle_cache.emplace(pair, circle_cache_t(sz, rbuf));
   free(rbuf);

#if 1
   // free obsolete circles from cache
   std::list <std::map <std::pair <int, int>, circle_cache_t >::iterator> obsolete;
   for(std::map <std::pair <int, int>, circle_cache_t >::iterator it = circle_cache.begin(); it!=circle_cache.end(); it++) {
       if(it->second.age() > 1000000)
           obsolete.push_back(it);
   }

   for(std::list <std::map <std::pair <int, int>, circle_cache_t >::iterator>::iterator it = obsolete.begin(); it != obsolete.end(); it++) {
       free((*it)->second.enc);
       circle_cache.erase(*it);
   }
#endif
   draw_circle(xm, ym, r, oth);
}

// rasterize a flat triangle
static void draw_flat_tri(int x1, int y1, int x2, int x3, int y2)
{
    if (x2 > x3) {
        int tx = x3;
        x3 = x2;
        x2 = tx;
    }

    float inv = 1.0f / (y2-y1);
    float invslope1 = (x2 - x1)*inv;
    float invslope2 = (x3 - x1)*inv;

    int ys;
    if(y1 < y2) {
        ys = 1;
    } else {
        ys = -1;
        invslope1 = -invslope1;
        invslope2 = -invslope2;
    }

    draw_line(x1, y1, x2, y2, false);
    draw_line(x1, y1, x3, y2, false);

    float curx2 = x1;
    float curx3 = x1;

    int y = y1;
    uint8_t value = palette[color][7];
    
    for (;;) {
        // Left edge antialiasing
        int icurx2 = curx2;
        int icurx3 = curx3;

#if 0
        for (int x = icurx2+1;x <= icurx3; x++)
            putpixel(x, y, (GRAYS-1));
#else
        // fill scanline
        memset(framebuffer + DRAW_LCD_H_RES*y+icurx2+1, value, icurx3-icurx2);
#endif
        if(y == y2)
            break;
        y+=ys;

        curx2 += invslope1;
        curx3 += invslope2;
    }
}

// draw a triangle with the verticies in order from top to bottom
static void draw_tri(int x1, int y1, int x2, int y2, int x3, int y3)
{
    if (y2 == y3)
        draw_flat_tri(x1, y1, x2, x3, y2);
    /* check for trivial case of top-flat triangle */
    else if (y1 == y2)
        draw_flat_tri(x3, y3, x1, x2, y1);
    else {
        /* general case - split the triangle in a topflat and bottom-flat one */
        int x4 = x1 + (float)(y2-y1) / (float)(y3-y1) * (x3 - x1);
        draw_flat_tri(x1, y1, x2, x4, y2);
        draw_flat_tri(x3, y3, x2, x4, y2);
    }
}

void draw_triangle(int x1, int y1, int x2, int y2, int x3, int y3)
{
    convert_coords(x1, y1);
    convert_coords(x2, y2);
    convert_coords(x3, y3);

    if(x1 < 0 || x2 < 0 || x3 < 0 ||
       x1 > DRAW_LCD_H_RES || x2 > DRAW_LCD_H_RES || x3 > DRAW_LCD_H_RES ||
       y1 < 0 || y2 < 0 || y3 < 0 ||
       y1 > DRAW_LCD_V_RES || y2 > DRAW_LCD_V_RES || y3 > DRAW_LCD_V_RES) {
        printf("triangle out of screen\n");
        return;
    }

    // reorder coordinates so x1, y1 at top, and x2 to left
    if(y1 <= y2 && y1 <= y3) {
        if(y2 <= y3)
            draw_tri(x1, y1, x2, y2, x3, y3);
        else
            draw_tri(x1, y1, x3, y3, x2, y2);
    } else if(y2 <= y1 && y2 <= y3) {
        if(y1 <= y3)
            draw_tri(x2, y2, x1, y1, x3, y3);
        else
            draw_tri(x2, y2, x3, y3, x1, y1);
    } else {
        if(y1 <= y2)
            draw_tri(x3, y3, x1, y1, x2, y2);
        else
            draw_tri(x3, y3, x2, y2, x1, y1);
    }
}

void draw_box(int x0, int y0, int w, int h, bool invert)
{
    switch(rotation) {
    case 3: {
        int t = x0;
        x0 = y0;
        y0 = DRAW_LCD_V_RES - t - w-1;
        t = w, w = h, h = t;
    } break;
    case 2: // rotate 180
        x0 = DRAW_LCD_H_RES - x0 - w-1;
        y0 = DRAW_LCD_V_RES - y0 - h-1;
        break;
    case 1: {
        int t = x0;
        x0 = DRAW_LCD_H_RES - y0 - h-1;
        y0 = t;
        t = w, w = h, h = t;
    } break;
    default: // no conversion
        break;
    }

    if(x0 < 0 || y0 < 0 || (x0+w) >= DRAW_LCD_H_RES || (y0+h) >= DRAW_LCD_V_RES) {
        printf("draw_ box out of range %d %d %d %d\n", x0, y0, w, h);
        return;
    }

    if(invert) {
        for(int y = y0; y<y0+h; y++)
            for(int x = x0; x<x0+w; x++)
                framebuffer[DRAW_LCD_H_RES*y+x] = ~framebuffer[DRAW_LCD_H_RES*y+x];
    } else {
        uint8_t value = palette[color][GRAYS-1];
        //value = rgb3(4|2|1,4|2,4|2|1);
        for(int y = y0; y<y0+h; y++)
            for(int x = x0; x<x0+w; x++)
                framebuffer[DRAW_LCD_H_RES*y+x] = value;
    }
}

static int cur_font = 0;
bool draw_set_font(int &ht)
{
    for(int i=FONT_COUNT-1; i >= 0; i--) {
        if(ht >= fonts[i].h) {
            cur_font = i;
            ht = fonts[i].h;
            return true;
        }
    }

    return false;
}

struct rglyph_t {
    rglyph_t(int size, uint8_t *buf) : sz(size) {
        data = sp_malloc(size);
        memcpy(data, buf, size);
    }
    uint8_t *data;
    int sz;
};

static std::map<std::pair<int, char>, rglyph_t> r_glyphs;
static int render_glyph(char c, int x, int y)
{
    if(c < FONT_MIN || c > FONT_MAX)
        return 0;
    
    const character &ch = fonts[cur_font].font_data[c-FONT_MIN];

    int w = ch.w, h = ch.h;
    if(rotation == 1 || rotation == 3) {
        int t = w;
        w = h;
        h = t;
    }

    // convert to correct corner of glyph
    switch(rotation) {
    case 3:
        y -= h;
        break;
    case 2:
        x -= w;
        y -= h;
        break;
    case 1:
        x -= w;
        break;
    case 0:
        // no conversion
        break;
    }
    if(x < 0 || x + w >= DRAW_LCD_H_RES) // off screen no partial render
        return ch.w;

    std::pair pair = std::make_pair(cur_font, c);
    if(r_glyphs.find(pair) == r_glyphs.end()) {
        // data is in original rotation, re-encode with correct rotation
        uint8_t *buf = sp_malloc(w*h+1);

        int i=0;
        int bx = 0, by = 0;
        while(i<ch.size) {
            int v = ch.data[i++];
            int g = v&(GRAYS-1), cnt;
            if(v & 0x80) // extended
                cnt = (ch.data[i++]+1)*16 + ((v&0x78)>>3);
            else
                cnt = (v>>3)+1;
            int cx, cy;
            for(int j=0; j<cnt; j++) {
                switch(rotation) {
                case 3: cx = by, cy = h-1-bx;     break;
                case 2: cx = w-1-bx, cy = h-1-by; break;
                case 1: cx = w-1-by, cy = bx;      break;
                default: cx = bx, cy = by;         break;
                }
                    
                buf[w*cy+cx] = g;
                if(++bx >= ch.w) {
                    bx = 0;
                    by++;
                }
            }
        }

        // now the character is in a buffer, we need to re-compress it again
        uint8_t *rbuf = sp_malloc(w*h);
        int cnt = 1, last = buf[0];
        int sz = 0;
        buf[w*h] = 0xff; // force write on last byte
        for(int i=1; i<=w*h; i++) {
            if(buf[i] != last || cnt == 4096) {
                if(cnt <= 16)
                    rbuf[sz++] = last | ((cnt-1)<<3);
                else {
                    rbuf[sz++] = last | ((cnt&0xf)<<3) | 0x80;
                    rbuf[sz++] = (cnt >> 4) - 1;
                }
                last = buf[i];
                cnt = 1;
            } else
                cnt++;
        }

        free(buf);
        r_glyphs.emplace(pair, rglyph_t(sz, rbuf));
        free(rbuf);
    }

    rglyph_t &rg = r_glyphs.at(pair);
    // we have rotated buffer data;
//    y+=ch.yoff;
    int i=0;
    int xc = 0;
    while(i<rg.sz) {
        int v = rg.data[i++];
        int g = v&(GRAYS-1), cnt;
        if(v & 0x80) // extended
            cnt = (rg.data[i++]+1)*16 + ((v&0x78)>>3);
        else
            cnt = (v>>3)+1;

        uint8_t value = palette[color][g];
#if 0 // unoptimized (better blending)
            for(int j=0; j<cnt; j++) {
                putpixel(x+xc, y, g);
                if(++xc >= ch.w) {
                    xc = 0;
                    y++;
                }
            }
#else
            if(g && y >= 0) {
                for(int j=0; j<cnt;) {
                    int len = MIN(cnt-j, w-xc);
                    if(y < DRAW_LCD_V_RES)
                        memset(framebuffer + DRAW_LCD_H_RES*y+x+xc, value, len);
                    xc += len;
                    j += len;
                    if(xc == w) {
                        xc = 0;
                        y++;
                    }
                }
            } else {
                y += cnt/w;
                xc += cnt%w;
                if(xc >= w) {
                    xc-=w;
                    y++;
                }
            }
#endif
    }
    return ch.w;
}

int draw_text_width(const std::string &str)
{
    int w = 0;
    for(int i=0; i<str.length(); i++) {
        char c = str[i];
        if(c < FONT_MIN || c > FONT_MAX)
            return 0;
        w += fonts[cur_font].font_data[c-FONT_MIN].w;
    }
    return w;
}

void draw_text(int x, int y, const std::string &str)
{
    //printf("draw text %d %d %s\n", x, y, str.c_str());
    convert_coords(x, y);
    for(int i=0; i<str.length(); i++) {
        switch(rotation) {
        case 3:
            y -= render_glyph(str[i], x, y);
            break;
        case 2: // rotate 180
            x -= render_glyph(str[i], x, y);
            break;
        case 1:
            y += render_glyph(str[i], x, y);
            break;
        default: // no conversion
            x += render_glyph(str[i], x, y);
            break;
        }
    }
}

void draw_color(color_e c)
{
    color = c;
}

static uint32_t t0start;
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

void draw_send_buffer()
{
#ifndef __linux__
    uint32_t t1 = esp_timer_get_time();
    esp_lcd_panel_draw_bitmap(panel_handle, 0, 0, DRAW_LCD_H_RES, DRAW_LCD_V_RES, framebuffer);
        
    uint32_t t2 = esp_timer_get_time();
#endif
    if(framebuffer == framebuffers[0])
        framebuffer = framebuffers[1];
    else if(framebuffer == framebuffers[1])
        framebuffer = framebuffers[2];
    else
        framebuffer = framebuffers[0];
        
#ifndef __linux__
    //printf("frame %d %lu %lu\n", vsynccount, t1-t0start, t2-t1);
    vsynccount=0;
    t0start = esp_timer_get_time();
#endif
}

#endif

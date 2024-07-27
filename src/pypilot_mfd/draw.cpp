/* Copyright (C) 2024 Sean D'Epagnier <seandepagnier@gmail.com>
 *
 * This Program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 */

#include <Arduino.h>

#include "settings.h"
#include "draw.h"

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

    if(settings.mirror == 2) {
        settings.mirror = digitalRead(2);
        //settings.mirror = 0;
    }

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

void draw_thick_line(int x1, int y1, int x2, int y2, int w)
{
    int ex = x2-x1, ey = y2-y1;
    int d = sqrt(ex*ex + ey*ey);
    if(d == 0)
        return;
    ex = ex*w/d/2;
    ey = ey*w/d/2;

    int ax = x1+ey, ay = y1-ex;
    int bx = x1-ey, by = y1+ex;
    int cx = x2+ey, cy = y2-ex;
    int dx = x2-ey, dy = y2+ex;
    u8g2.drawTriangle(ax, ay, bx, by, cx, cy);
    u8g2.drawTriangle(bx, by, dx, dy, cx, cy);
}

void draw_circle(int x, int y, int r, int thick)
{
    if(r <= 0) {
        thick += r-1;
        r = 1;
    }
    for (int i = -thick; i <= thick; i++)
        for (int j = -thick; j <= thick; j++)
            u8g2.drawCircle(x + i, y + j, r);
}

void draw_line(int x1, int y1, int x2, int y2)
{
    u8g2.drawLine(x1, y1, x2, y2);
}

void draw_box(int x, int y, int w, int h, bool invert=false)
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

int draw_text_width(const char *str)
{
    return u8g2.getStrWidth(str);
}

void draw_text(int x, int y, const char *str)
{
    u8g2.drawUTF8(x, y, str);
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

#ifdef USE_LVGL     // color lcd

static uint8_t color;

// for now 256 entrees, could make it less
static uint8_t palette[COLOR_COUNT][8];

// produce 8 bit color from 3 bit per channel mapped onto rgb232 with last bit shared for red and blue
uint8_t rgb2321(uint8_t r, uint8_t g, uint8_t b)
{
    uint8_t l = r&&b ? (b&1) : 0;
    r >>= 1;
    b >>= 1;
    return (r << 6) | (g << 3) | (b << 1) | l;
}

// b from 0-8;
uint8_t compute_color(color_e c, uint8_t b)
{
    // convert enum to rgb232+1
    switch(color) {
    case WHITE:   return rgb2321(b, b, b);
    case RED:     return rgb2321(b, 0, 0);
    case GREEN:   return rgb2321(0, b, 0);
    case BLUE:    return rgb2321(0, 0, b);
    case CYAN:    return rgb2321(0, b, b);
    case MAGENTA: return rgb2321(b, 0, b);
    case YELLOW:  return rgb2321(b, b, 0);
    case GREY:    return rgb2321(b/2, b/2, b/2);
    case ORANGE:  return rgb2321(b, b/2, 0);
    }
}

void draw_setup()
{
    for(int i=0; i<COLOR_COUNT; i++)
        for(int j=0; j<8; j++)
            palette[i][j]=compute_color(i, j);
}

// put pixel using 3 bit gradient mapped into current color
static void putpixel(int x, int y, uint8_t c)
{
    uint8_t value = palette[color][c];
    /// write to mem here
}

void draw_line(int x1, int y1, int x2, int y2)
{
    // http://members.chello.at/~easyfilter/bresenham.c
    /* draw a black (0) anti-aliased line on white (255) background */
    int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, x2;
    long dx = abs(x1-x0), dy = abs(y1-y0), err = dx*dx+dy*dy;
    long e2 = err == 0 ? 1 : 0xffff7fl/sqrtf(err);     /* multiplication factor */

    dx *= e2; dy *= e2; err = dx-dy;                       /* error value e_xy */
    for ( ; ; ){                                                 /* pixel loop */
      putpixel(x0,y0,abs(err-dx+dy)>>21);
      e2 = err; x2 = x0;
      if (2*e2 >= -dx) {                                            /* x step */
         if (x0 == x1) break;
         if (e2+dy < 0xff0000l) putpixel(x0,y0+sy,(e2+dy)>>21);
         err -= dy; x0 += sx;
      }
      if (2*e2 <= dy) {                                             /* y step */
         if (y0 == y1) break;
         if (dx-e2 < 0xff0000l) putpixel(x2+sx,y0,(dx-e2)>>21);
         err += dx; y0 += sy;
    }
}

#define MAX(a, b) ((a>b) ? (a) : (b))
void draw_thick_line(int x1, int y1, int x2, int y2, int wd)
{
    int dx = abs(x1-x0), sx = x0 < x1 ? 1 : -1;
    int dy = abs(y1-y0), sy = y0 < y1 ? 1 : -1;
    int err = dx-dy, e2, x2, y2;                           /* error value e_xy */
    float ed = dx+dy == 0 ? 1 : sqrtf((float)dx*dx+(float)dy*dy);

    for (wd = (wd+1)/2; ; ) {                                    /* pixel loop */
      putpixel(x0, y0, MAX(0,7*(abs(err-dx+dy)/ed-wd+1)));
      e2 = err; x2 = x0;
      if (2*e2 >= -dx) {                                            /* x step */
         for (e2 += dy, y2 = y0; e2 < ed*wd && (y1 != y2 || dx > dy); e2 += dx)
            putpixel(x0, y2 += sy, MAX(0,7*(abs(e2)/ed-wd+1)));
         if (x0 == x1) break;
         e2 = err; err -= dy; x0 += sx;
      }
      if (2*e2 <= dy) {                                             /* y step */
         for (e2 = dx-e2; e2 < ed*wd && (x1 != x2 || dx < dy); e2 += dy)
            putpixel(x2 += sx, y0, MAX(0,7*(abs(e2)/ed-wd+1)));
         if (y0 == y1) break;
         err += dx; y0 += sy;
      }
    }
}


// TODO adapt http://members.chello.at/~easyfilter/bresenham.js  plotEllipseRectWidth for thick
void draw_circle(int x, int y, int r, int thick=0)
{
                       /* draw a black anti-aliased circle on white background */
   int x = -r, y = 0;           /* II. quadrant from bottom left to top right */
   int i, x2, e2, err = 2-2*r;                             /* error of 1.step */
   r = 1-err;
   do {
      i = 7*abs(err-2*(x+y)-2)/r;               /* get blend value of pixel */
      putpixel(xm-x, ym+y, i);                             /*   I. Quadrant */
      putpixel(xm-y, ym-x, i);                             /*  II. Quadrant */
      putpixel(xm+x, ym-y, i);                             /* III. Quadrant */
      putpixel(xm+y, ym+x, i);                             /*  IV. Quadrant */
      e2 = err; x2 = x;                                    /* remember values */
      if (err+y > 0) {                                              /* x step */
         i = 7*(err-2*x-1)/r;                              /* outward pixel */
         if (i < 256) {
            putpixel(xm-x, ym+y+1, i);
            putpixel(xm-y-1, ym-x, i);
            putpixel(xm+x, ym-y-1, i);
            putpixel(xm+y+1, ym+x, i);
         }
         err += ++x*2+1;
      }
      if (e2+x2 <= 0) {                                             /* y step */
         i = 7*(2*y+3-e2)/r;                                /* inward pixel */
         if (i < 256) {
            putpixel(xm-x2-1, ym+y, i);
            putpixel(xm-y, ym-x2-1, i);
            putpixel(xm+x2+1, ym-y, i);
            putpixel(xm+y, ym+x2+1, i);
         }
         err += ++y*2+1;
      }
   } while (x < 0);
}

// rasterize a flat triangle
static void draw_flat_tri(int x1, int y1, int x2, int x3, int y2)
{
    if (x2 > x3) {
        int tx = x3, ty = y3;
        x3 = x2, y3 = y2;
        x2 = tx, y2 = ty;
    }

    float invslope1 = (x2 - x1) / (y2 - y1);
    float invslope2 = (x3 - x1) / (y3 - y1);

    int ys;
    if(y1 < y2) {
        ys = 1;
    } else {
        ys = -1;
        invslope1 = -invslope1;
        invslope2 = -invslope2;
    }

    float curx1 = x1;
    float curx2 = x1;

    for (int y = y1; y <= y2; y+=ys) {
        // Left edge antialiasing
        int icurx1 = curx1;
        putpixel(icurx1, y, 7*(1.0f - (curx1 - icurx1)));
        int icurx2 = curx2;
        putpixel(icurx2, y, 7*(1.0f - (curx2 - icurx2)));
        for (int x = icurx1+1; x < icurx2; x++)
            putpixel(x, y, 7);

        curx1 += invslope1;
        curx2 += invslope2;
    }
}

// draw a triangle with the verticies in order from top to bottom
static void draw_tri(int x1, int y1, int x2, int y2, int x3, int y3)
{
        if (y2 == y3)
        {
            draw_flat_tri(x1, y1, x2, x3, y2);
        }
        /* check for trivial case of top-flat triangle */
        else if (y1 == y2)
        {
            draw_flat_tri(x3, y3, x1, x2, y1);
        }
        else
        {
            /* general case - split the triangle in a topflat and bottom-flat one */
            int x4 = x1 + (float)(y2-y1) / (float)(y3-y1) * (x3 - x1);
            draw_flat_tri(x1, y1, x2, x4, y2);
            draw_flat_tri(x3, y3, x2, x4, y2);
        }

}

void draw_triangle(int x1, int y1, int x2, int y2, int x3, int y3)
{
    // reorder coordinates so x1, y1 at top, and x2 to left
    if(y1 < y2 && y1 < y3) {
        if(y2 < y3)
            draw_tri(x1, y1, x2, y2, x3, y3);
        else
            draw_tri(x1, y1, x3, y3, x2, y2);
    } else if(y2 < y1 && y2 < y3) {
        if(y1 < y3)
            draw_tri(x2, y2, x1, y1, x3, y3);
        else
            draw_tri(x2, y2, x3, y3, x1, y1);
    } else {
        if(y1 < y2)
            draw_tri(x3, y3, x1, y1, x2, y2);
        else
            draw_tri(x3, y3, x2, y2, x1, y1);
    }
}

void draw_box(int x, int y, int w, int h, bool invert=false)
{
    // implement using memset, and memcpy for inverting then for the right rotation
}

bool draw_set_font(int &ht)
{
    // need fonts up to 150pt
}

static int get_glyph(char c)
{
}

static int render_glyph(char c)
{

}

int draw_text_width(const char *str)
{
    int w = 0;
    while(*str++) {
        w += get_glyph(*str);
    }
    return w;
}

void draw_text(int x, int y, const char *str)
{
    while(*str++) {
        x += render_glyph(*str);
    }
}

void draw_color(color_e c)
{
    color = c;
}

#endif

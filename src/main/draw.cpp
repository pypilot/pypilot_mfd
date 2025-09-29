/* Copyright (C) 2024 Sean D'Epagnier <seandepagnier@gmail.com>
 *
 * This Program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 */

#include <math.h>
#ifndef __linux__
#include <Arduino.h>

#else

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#define PROGMEM

#endif

#include <sys/time.h>
#include <string>
#include <list>

#include "settings.h"
#include "draw.h"
#include "extio.h"

#ifdef USE_U8G2
#include "u8g2drv.h"
#else

#include "fonts.h"


#define GRAYS 8 // 8 shades for each color
// for now 256 entrees, could make it less
static uint8_t palette[COLOR_COUNT][GRAYS];
static uint8_t color;

uint8_t *framebuffer;
static int rotation;

#ifdef USE_JLX256160
#include "jlx256160.h"
#else
#include "rgb_lcd.h"
#endif

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
#if 0 // render using putpixel
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
#elif 1 // this is twice faster than above using triangles..
    int ex = x1-x0, ey = y1-y0;
    int d = sqrtf((float)(ex*ex + ey*ey));
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
#else // about 8 times faster but has no antialiasing (yet)
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
    uint8_t value = palette[color][GRAYS-1];
    for(int y=y0; y<= y1; y++) {
        draw_scanline(x, y, value, sw);
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
#if defined(__linux__) || defined(CONFIG_IDF_TARGET_ESP32)
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
    if(r < 20
#if defined(USE_JLX256160)
       || 1  // TODO: fix below/profile for this driver the faster circle or not needed??  just need make memset to draw_scanline
#endif
        ) {
        draw_circle_orig(xm, ym, r, th);        
        return;
    }

    std::pair pair = std::make_pair(r, th);
    // render circle from cache
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

    // render to buffer 1/2 circle into cache
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

   // now encode the buffer with runlength compression
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

   // now draw it using the cache we just made
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
    uint8_t value = palette[color][GRAYS-1];
    
    for (;;) {
        // Left edge antialiasing
        int icurx2 = curx2;
        int icurx3 = curx3;

#if 0 // works in all cases, slower with antialiasing
        for (int x = icurx2+1;x <= icurx3; x++)
            putpixel(x, y, (GRAYS-1));
#else
        // fill scanline, no antialiasing??
        draw_scanline(icurx2+1, y, value, icurx3-icurx2);
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

    if(x0 < 0 || y0 < 0 || (x0+w) > DRAW_LCD_H_RES || (y0+h) > DRAW_LCD_V_RES) {
        printf("draw box out of range %d %d %d %d\n", x0, y0, w, h);
        return;
    }

    if(invert) {
        for(int y = y0; y<y0+h; y++)
            invert_scanline(x0, y, w);
    } else {
        uint8_t value = palette[color][GRAYS-1];
        for(int y = y0; y<y0+h; y++)
            draw_scanline(x0, y, value, w);
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
    if(x < 0 || x + w > DRAW_LCD_H_RES) { // off screen no partial render
      printf("off screen %d %d %d %d\n", x, y, w, c);
        return ch.w;
    }

    std::pair pair = std::make_pair(cur_font, c);
    if(r_glyphs.find(pair) == r_glyphs.end()) {
        // data is in original rotation, re-encode with correct rotation
        uint8_t *buf = sp_malloc(w*h+1);
	
        int i=0;
        int bx = 0, by = 0;
        while(i<ch.size) {
            int v = ch.data[i++];
            int g = v&(GRAYS-1), cnt;
	    if(h < 11) // flatten to monochrome for small font
	      g = g>2 ? GRAYS-1 : 0;
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
        // TODO make faster rendering work?
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
                    draw_scanline(x+xc, y, value, len);
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
#endif

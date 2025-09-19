/* Copyright (C) 2025 Sean D'Epagnier <seandepagnier@gmail.com>
 *
 * This Program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 */


#define DRAW_LCD_H_RES              256
#define DRAW_LCD_V_RES              160

#ifdef CONFIG_IDF_TARGET_ESP32
#include <SPI.h>

#define GRAYSCALE

#define RS 12
#define RESET 13
#define CS 5

static uint8_t spibuf[16], spibufpos, spirs_state;

static void flush_lcdcmd() {
    if(spibufpos) {
        digitalWrite(RS, spirs_state);
        digitalWrite(CS, LOW);
        //for(int i=0; i<spibufpos; i++)
        //SPI.transfer(spibuf[i]);
        SPI.writeBytes(spibuf, spibufpos);
        digitalWrite(CS, HIGH);
    }
    spibufpos = 0;
}    

static void lcdcmd(uint8_t rs, uint8_t d) {
#if 0
    if(spirs_state != rs || spibufpos == sizeof spibuf)
        flush_lcdcmd();
    spirs_state = rs;
    spibuf[spibufpos++] = d;
#else
    digitalWrite(RS, rs);
    SPI.transfer(d);
#endif
}

static void cmd(uint8_t d) { lcdcmd(LOW, d); }
static void data(uint8_t d) { lcdcmd(HIGH, d); }
#endif

static uint8_t compute_color(color_e c, uint8_t b)
{
    return b;
}

void draw_setup(int r)
{
    rotation = r;

    for(int i=0; i<COLOR_COUNT; i++)
        for(int j=0; j<GRAYS; j++)
            palette[i][j] = compute_color((color_e)i, j);
    
#ifdef CONFIG_IDF_TARGET_ESP32
    // only a single framebuffer needed 2bpp,  spi ram or internal??
    int size = DRAW_LCD_H_RES*DRAW_LCD_V_RES/4;
//    framebuffer = sp_malloc(size);
    framebuffer = (uint8_t*)heap_caps_malloc(size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    printf("FRAMEBUFFER %p\n", framebuffer);
    
    pinMode(RS, OUTPUT);
    pinMode(CS, OUTPUT);
    pinMode(RESET, OUTPUT);

    digitalWrite(CS, HIGH);

    digitalWrite(RESET, LOW);
    delay(5);
    digitalWrite(RESET, HIGH);
//    pinMode(RESET, INPUT);

    SPI.begin(18, 19, 23, CS);
    //SPI.setClockDivider( SPI_CLOCK_DIV2 );
    SPI.setFrequency(4000000);
    digitalWrite(CS, LOW);
    SPI.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));

    // initialize SPI lcd here and put in gray mode
    delay(20); // needed??

    cmd( 0x30 );    /* select 00 commands */
    cmd( 0x94 );    /* sleep out */

    //cmd( 0x30 );    /* select 00 commands */
    cmd( 0xae );    /* display off */

    cmd( 0x31 );    /* select 01 commands */
    cmd( 0xd7);
    data(0x9f );    /* disable auto read */  

    //cmd( 0x31 );    /* select 01 commands */
    cmd( 0x32 );    /* analog circuit set */
    data( 0x00 );    /* code example: OSC Frequency adjustment */
    data( 0x01 );    /* Frequency on booster capacitors 1 = 6KHz? */
    data( 0x00 );    /* Bias: 1: 1/13, 2: 1/12, 3: 1/11, 4:1/10, 5:1/9 */

//    cmd(0x51);
//    data(0xfa);

#if 1
   cmd(0xf0); // frame rate 5hz
   uint8_t rate = 14;
    data( rate );
    data( rate );
    data( rate );
    data( rate );
#endif
    
#if 1
    //cmd( 0x31 );    /* select 01 commands */
    cmd( 0x20 );    /* gray levels */
    data( 0x0 );
    data( 0x0 );
    data( 0x0 );
    data( 0x07 );
    data( 0x07 );
    data( 0x07 );
    data( 0x0 );
    data( 0x0 );
    data( 0xc );
    data( 0x0 );
    data( 0x0 );
    data( 0xc );
    data( 0xc );
    data( 0xc );
    data( 0x0 );
    data( 0x0 );
#endif 
    cmd( 0x30 );                /* select 00 commands */
    cmd(0x75);
    data(0);
    data(0x20);        /* row range */
    cmd(0x15);
    data(0);
    data(64);        /* col range */
  
    //cmd( 0x30 );                /* select 00 commands */
    cmd( 0xbc);
    data( 0x5 );            /* data scan dir  ( CHANGED FOR MIRROR VERSION ) */

    //cmd( 0x30 );                /* select 00 commands */
    cmd( 0x08 );                /* data format LSB top */

    //cmd( 0x30 );                /* select 00 commands */ 
    cmd( 0xca );                /* display control, 3 args follow  */
    data( 0x0 );                /* 0x0: no clock division, 0x4: divide clock */
    data( 0x9f );                /* 1/160 duty value from the DS example code */
    data( 0x20 );                /* nline off */ 

    //cmd( 0x30 );                /* select 00 commands */ 
    cmd( 0xf0);
#ifdef GRAYSCALE
    data( 0x11 );        /* 4 gray mode */
#else
    data( 0x10 );        /* bw mode */
#endif

    //cmd( 0x30 );                /* select 00 commands */
    cmd( 0x81);
    data( 0x3f );
    data( 0x06 );    /* contrast */

    //cmd( 0x30 );                /* select 00 commands */
    cmd( 0x20 );
    data( 0x0b );        /* Power control: Regulator, follower & booster on */

//    cmd(0xaf); // display on

    SPI.endTransaction();
    digitalWrite(CS, HIGH);
    delay(100);


    digitalWrite(CS, LOW);
    SPI.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));

    cmd(0x30);
    cmd(0x94);
    delay(10);
    cmd(0xaf);
    
    SPI.endTransaction();

    //flush_lcdcmd();
#else
    // emulation on linux
    framebuffer = (uint8_t*)malloc(DRAW_LCD_H_RES*DRAW_LCD_V_RES/4);
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

    // for now shift c from 3 to 2 bits until we can directly support 4 gray
    uint8_t value = c >> 1;
    uint8_t mask = value << ((x&0x3)<<1);
    framebuffer[(DRAW_LCD_H_RES*y+x)>>2] |= mask;
}



static uint8_t getpixel(int x, int y)
{
    return (framebuffer[(DRAW_LCD_H_RES*y+x)>>2] >> ((x&0x3)<<1)) & 0x3;
}


// draw a scanline of 2bpp
static inline void draw_scanline(int x, int y, int value, int count)
{
    if(count < 3) {  // logic below assumes minimum of 3 pixels
        for(int xi=x; xi<x+count; xi++)
            putpixel(xi, y, value);
        return;
    }

    // 2bpp have to do more work
    value >>= 1; // convert 8 to 4 gray for now
    if(value == 0)
        return;

    const uint8_t masks[4] = {0x00, 0x55, 0xAA, 0xFF};
    uint8_t mask = masks[value];

    int start_count = 4-(x&3);
    if(start_count<4) {
        const uint8_t start_masks[] = {0x00, 0xC0, 0xF0, 0xFC};
        framebuffer[(DRAW_LCD_H_RES*y+x)>>2] |= mask & start_masks[start_count];
        x+=start_count;
        count-=start_count;
    }

    int cd = count>>2;
    if(cd) {
        memset(framebuffer+((DRAW_LCD_H_RES*y+x)>>2), mask, cd);
        cd <<= 2;
        x += cd;
        count -= cd;
    }
    int end_count = count&3;
    if(end_count) {
        const uint8_t end_masks[] = {0x00, 0x03, 0x0F, 0x3F};
        framebuffer[(DRAW_LCD_H_RES*y+x)>>2] |= mask & end_masks[end_count];
    }
}

static inline void invert_scanline(int x, int y, int count)
{
    return;
    if(count < 3)  // logic below assumes minimum of 3 pixel, dont invert narrower than that for now
        return;

    int start_count = 4-(x&3);
    if(start_count<4) {
        const uint8_t start_masks[] = {0x00, 0xC0, 0xF0, 0xFC};
        framebuffer[(DRAW_LCD_H_RES*y+x)>>2] ^= start_masks[start_count];
        x+=start_count;
        count-=start_count;
    }

    int cd = count>>2;
    if(cd) {
        uint8_t *fb = framebuffer+((DRAW_LCD_H_RES*y+x)>>2);
        for(int i = 0; i < cd; i++)
            fb[i] = ~fb[i];
        cd <<= 2;
        x += cd;
        count -= cd;
    }
    int end_count = count&3;
    if(end_count) {
        const uint8_t end_masks[] = {0x00, 0x03, 0x0F, 0x3F};
        framebuffer[(DRAW_LCD_H_RES*y+x)>>2] ^= end_masks[end_count];
    }
}

void draw_clear(bool display_on)
{
    uint8_t c = 0;
#ifdef CONFIG_IDF_TARGET_ESP32
    if(settings.invert)
        c = 0xff;
#endif
    memset(framebuffer, c, DRAW_LCD_H_RES*DRAW_LCD_V_RES/4);
}

#ifndef __linux__

uint8_t pbuffer[256*160/4];

void draw_send_buffer()
{
    uint8_t *p = pbuffer;
#ifdef GRAYSCALE
    for(int x=0; x<256; x++)
        for(int y=0; y<160; y+=4) {
            uint8_t t = 0;
            for(int b=0; b<4; b++) {
                uint8_t v = getpixel(x, y+b);
                t |= (v<<(2*b));
            }

#if 0
            if (x< 10) {
                if(y < 16)
                    t = 0xff;
                else if(y>20&&y<32)
                    t = 0xAA;
                else if(y>40&&y<64)
                    t = 0x55;
            }
            else
                t = 0;
#endif
            *(p++) = t;
        }
#else
    for(int x=0; x<256; x++)
        for(int y=0; y<160; y+=8) {
            uint8_t t = 0;
            for(int b=0; b<8; b++) {
                uint8_t v = getpixel(x, y+b);
                if(v>1)
                    t |= (1<<b);
            }
            *(p++) = t;
        }
#endif

    digitalWrite(CS, LOW);
    SPI.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));

    
    cmd( 0x30 );                /* select 00 commands */

    static int contrast = 200;
//    printf("con %d\n", contrast++);
    cmd(0x30 );
    cmd(0x81 );
    data((contrast & 0x1f)<<1);
    data(contrast >> 5);
    SPI.endTransaction();
    digitalWrite(CS, HIGH);

    digitalWrite(CS, LOW);
    SPI.beginTransaction(SPISettings(2000000, MSBFIRST, SPI_MODE0));
    cmd(0x75);
    data(0x1);
#ifdef GRAYSCALE
    data(0x28);
#else
    data(0x14);
#endif
    cmd(0x15);
    data(0x0);
    data(0xff);
    cmd(0x5c);
    digitalWrite(RS, HIGH);

#ifdef GRAYSCALE
    SPI.writeBytes(pbuffer, 256*160/4);
#else
    SPI.writeBytes(pbuffer, 256*160/8);
#endif

    SPI.endTransaction();
    digitalWrite(CS, HIGH);
    digitalWrite(RS, LOW);
    
}
#endif

/* Copyright (C) 2025 Sean D'Epagnier <seandepagnier@gmail.com>
 *
 * This Program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 */

#define DRAW_LCD_H_RES              240
#define DRAW_LCD_V_RES              160

#define RS 12
#define RESET 13
#define CS 5

static uint8_t spibuf[16], spibufpos, spirs_state;

static void lcdcmd(uint8_t rs, uint8_t d) {
    if(spirs_state != rs || spibufpos == sizeof spibuf) {
        digitalWrite(RS, spirs_state);
        if(spibufpos) {
            //digitalWrite(CS, LOW);
            for(int i=0; i<spibufpos; i++)
                SPI.transfer(spibuf[i]);
            //digitalWrite(CS, HIGH);
        }
        spibufpos = 0;
    }        
    spirs_state = rs;
    spibuf[spibufpos++] = d;
}

static void cmd(uint8_t d) { lcdcmd(LOW, d); }
static void data(uint8_t d) { lcdcmd(HIGH, d); }

void draw_setup(int r)
{
    rotation = r;

    // only a single framebuffer needed 2bpp,  spi ram or internal??
    framebuffer = sp_malloc(DRAW_LCD_H_RES*DRAW_LCD_V_RES/4);
    
    pinMode(RS, OUTPUT);
    //pinMode(CS, OUTPUT);
    pinMode(RESET, OUTPUT);
    digitalWrite(RESET, LOW);
    delay(5);
    digitalWrite(RESET, HIGH);
    pinMode(RESET, INPUT);
    
    SPI.begin(18, 19, 23, 5);
    //SPI.setClockDivider( SPI_CLOCK_DIV2 );
    SPI.setFrequency(4000000);

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
    
    //cmd( 0x31 );    /* select 01 commands */
    cmd( 0x20 );    /* gray levels */
    data( 0x1 );
    data( 0x3 );
    data( 0x5 );
    data( 0x7 );
    data( 0x9);
    data( 0xb );
    data( 0xd );
    data( 0x10 );
    data( 0x11 );
    data( 0x13 );
    data( 0x15 );
    data( 0x17 );
    data( 0x19 );
    data( 0x1b );
    data( 0x1d );
    data( 0x1f );
 
    cmd( 0x30 );                /* select 00 commands */
    cmd(0x75);
    data(0);
    data(0x28);        /* row range */
    cmd(0x15);
    data(0);
    data(0xFF);        /* col range */
  
    //cmd( 0x30 );                /* select 00 commands */
    cmd( 0xbc);
    data( 0x2 );            /* data scan dir  ( CHANGED FOR MIRROR VERSION ) */
    data( 0xa6 );                /* ??? */

    //cmd( 0x30 );                /* select 00 commands */
    cmd( 0x0c );                /* data format LSB top */

    //cmd( 0x30 );                /* select 00 commands */ 
    cmd( 0xca );                /* display control, 3 args follow  */
    data( 0x0 );                /* 0x0: no clock division, 0x4: divide clock */
    data( 159 );                /* 1/160 duty value from the DS example code */
    data( 0x20 );                /* nline off */ 

    //cmd( 0x30 );                /* select 00 commands */ 
    cmd( 0xf0);
    data( 0x11 );        /* 4 gray mode */

    //cmd( 0x30 );                /* select 00 commands */
    cmd( 0x81);
    data( 0x18);
    data( 0x5 );    /* contrast */

    //cmd( 0x30 );                /* select 00 commands */
    cmd( 0x20 );
    data( 0x0b );        /* Power control: Regulator, follower & booster on */
    delay(100);

    cmd(0xaf); // display on

    // flip sequence
    cmd( 0x30 );                /* select 00 commands */
    cmd( 0xbc);
    data( 0x2 );            /* data scan dir */
    data( 0xa6 );                /* ??? */

    //cmd( 0x30 );                /* select 00 commands */
    cmd( 0x0c );                /* data format MSB top */
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
    framebuffer[(DRAW_LCD_H_RES/4*y+x)>>2] |= mask;
}

static uint8_t compute_color(color_e c, uint8_t b)
{
    return b>>1;
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

    int start_count = 3-(x&3);
    if(start_count) {
        const uint8_t start_masks[] = {0x00, 0x03, 0x0F, 0x3F};
        framebuffer[(DRAW_LCD_H_RES*y+x0)>>2] |= mask & start_masks[start_count];
        x+=start_count;
        count-=start_count;
    }

    int cd = count>>2;
    if(cd) {
        memset(framebuffer+(DRAW_LCD_H_RES*y+x)>>2, mask, cd);
        cd <<= 2;
        x += cd;
        count -= cd;
    }
    int end_count = count&3;
    if(end_count) {
        const uint8_t end_masks[] = {0x00, 0xC0, 0xF0, 0xFC};
        framebuffer[(DRAW_LCD_H_RES*y+x0)>>2] |= mask & end_masks[end_count];
    }
}

static inline void invert_scanline(int x, int y, int count)
{
    if(count < 3)  // logic below assumes minimum of 3 pixel, dont invert narrower than that for now
        return;

    int start_count = 3-(x&3);
    if(start_count) {
        const uint8_t start_masks[] = {0x00, 0x03, 0x0F, 0x3F};
        framebuffer[(DRAW_LCD_H_RES*y+x0)>>2] ^= start_masks[start_count];
        x+=start_count;
        count-=start_count;
    }

    int cd = count>>2;
    if(cd) {
        uint8_t *fb = framebuffer+(DRAW_LCD_H_RES*y+x)>>2;
        for(int i = 0; i < cd; i++)
            fb[i] = ~fb[i];
        cd <<= 2;
        x += cd;
        count -= cd;
    }
    int end_count = count&3;
    if(end_count) {
        const uint8_t end_masks[] = {0x00, 0xC0, 0xF0, 0xFC};
        framebuffer[(DRAW_LCD_H_RES*y+x0)>>2] ^= end_masks[end_count];
    }
}

void draw_clear(bool display_on)
{
    uint8_t c = 0;
    if(settings.invert)
        c = 0xff;
    memset(framebuffer, c, DRAW_LCD_H_RES*DRAW_LCD_V_RES/4);
}

void draw_send_buffer()
{
    // flush framebuffer to spi device here

    cmd(0x30 );
    cmd(0x81 );
    cmd(contrast & 0x1f);
    cmd(congrast >> 5);

    digitalWrite(RS, HIGH);
//    digitalWrite(CS, LOW);

    cmd(0x5C); // drite data

    int len = DRAW_LCD_H_RES*DRAW_LCD_V_RES/4;
    for(int i=0; i<len; i++) {
        SPI.transfer(framebuffer[i]);
    }
//    digitalWrite(CS, HIGH);
}

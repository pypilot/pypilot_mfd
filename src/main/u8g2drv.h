/* Copyright (C) 2025 Sean D'Epagnier <seandepagnier@gmail.com>
 *
 * This Program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 */

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

/* Copyright (C) 2024 Sean D'Epagnier <seandepagnier@gmail.com>
 *
 * This Program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 */


#ifdef CONFIG_IDF_TARGET_ESP32
#if 1
# define USE_U8G2
# else
//#define USE_TFT_ESPI // only small wind display supported (bottom of file)
# define USE_JLX256160 // for now, eventually get from boot partition, and do at runtime
# endif

// eventually make these constants variables so the same firmware can support different hardware and displays

#define DRAW_LCD_H_RES              240
#define DRAW_LCD_V_RES              160

#else

// The pixel number in horizontal and vertical
#define DRAW_LCD_H_RES              800
#define DRAW_LCD_V_RES              480

#endif

enum color_e {WHITE, RED, GREEN, BLUE, CYAN, MAGENTA, YELLOW, GREY, ORANGE, BLACK, COLOR_COUNT};

void draw_setup(int rotation);
void draw_thick_line(int x1, int y1, int x2, int y2, int w);
void draw_circle(int x, int y, int r, int thick=0);
void draw_line(int x1, int y1, int x2, int y2, bool convert=true);
void draw_box(int x, int y, int w, int h, bool invert=false);
void draw_triangle(int x1, int y1, int x2, int y2, int x3, int y3);
bool draw_set_font(int &ht);
int draw_text_width(const std::string &str);
void draw_text(int x, int y, const std::string &str);
void draw_color(color_e color);
void draw_clear(bool display_on);
void draw_send_buffer();

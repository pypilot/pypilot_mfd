#include <string>
#include <cstring>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <GL/glut.h>
#include <sys/time.h>

#include "draw.h"

// g++ -g -o testdraw testdraw.cpp draw.cpp -lGL -lGLU -lglut && ./testdraw

extern uint8_t *framebuffer;

uint32_t framebuffer32[DRAW_LCD_H_RES*DRAW_LCD_V_RES];

void convert_buffer()
{
#ifdef USE_JLX256160
    // convert 2bpp framebuffer to rgb888
    for(int i=0; i<DRAW_LCD_H_RES*DRAW_LCD_V_RES; i++) {
        int ind = i/4;
        uint8_t g = (framebuffer[ind] >> (2*(i&3))) & 0x3;
        g = (g<<6) | (g<<4) | (g<<2) | g; // extend
        uint32_t v = (g<<16) | (g<<8) | g;
        framebuffer32[i] = v;
    }
#else
    // convert rgb332 back to rgb888
    for(int i=0; i<DRAW_LCD_H_RES*DRAW_LCD_V_RES; i++) {
        uint8_t value = framebuffer[i];
        uint8_t r = (value&0xe0)>>5;
        uint8_t g = (value&0x1c)>>2;
        uint8_t b = (value&0x03);

        // extend
        r = (r<<5) | (r<<2) | (r>>1);
        g = (g<<5) | (g<<2) | (g>>1);
        b = (b<<6) | (b<<4) | (b<<2) | b;

        uint32_t v = (b<<16) | (g<<8) | r;
        framebuffer32[i] = v;
    }
#endif
}

void display() {
    // draw the current object using the current texture.

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    draw_clear(true);

    draw_color(GREEN);
    draw_box(DRAW_LCD_H_RES/4, DRAW_LCD_H_RES/4, DRAW_LCD_H_RES/8, DRAW_LCD_H_RES/8);

    draw_color(RED);
    static int ri=100;
    ri++;
    draw_circle(DRAW_LCD_H_RES/3, DRAW_LCD_V_RES/2, DRAW_LCD_H_RES/4, 6 );
    draw_thick_line(10,10,100, 30, 10);
    if(ri>80)
        ri = 40;
    static float r;
    r+=.1;
    float s=sin(r), c = cos(r);
    draw_color(MAGENTA);
    int x0 = DRAW_LCD_H_RES/4;
    int tw = DRAW_LCD_H_RES/80;
    draw_triangle(x0+c*tw, x0+s*tw-c*tw, x0-c*tw, x0-s*tw-c*tw, x0-s*x0/2, x0+c*x0/2);

    int ht = DRAW_LCD_V_RES/13;
    draw_color(GREEN);
    draw_set_font(ht);
    struct timeval tv, tv2;
    gettimeofday(&tv, 0);
    draw_text(20, 20, "Hello World!");
    gettimeofday(&tv2, 0);
    printf("text time %d\n", 1000000*(tv2.tv_sec - tv.tv_sec) + (tv2.tv_usec - tv.tv_usec));

    draw_box(0, 20, 200, 30, true);

    convert_buffer();

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, DRAW_LCD_H_RES, DRAW_LCD_V_RES, 0, GL_RGBA, GL_UNSIGNED_BYTE, framebuffer32);

    glBegin(GL_QUADS);
    glTexCoord2f(0, 1), glVertex2f(0, 0);
    glTexCoord2f(1, 1), glVertex2f(1, 0);
    glTexCoord2f(1, 0), glVertex2f(1, 1);
    glTexCoord2f(0, 0), glVertex2f(0, 1);
    glEnd();
    glDisable(GL_TEXTURE_2D);
    glutSwapBuffers();
}

void reshape (int w, int h)
{
    glViewport (0, 0, w, h);
    glMatrixMode (GL_PROJECTION);
    glLoadIdentity ();
    glMatrixMode (GL_MODELVIEW);
    glLoadIdentity();
    gluOrtho2D(0, 1, 0, 1);
}

void key(unsigned char k, int x, int y)
{
    exit(0);
}

void refresh(int value)
{
    glutPostRedisplay();
    glutTimerFunc(value, refresh, value);
}

int main(int argc, char *argv[])
{
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(1024, 768);
    glutInitWindowPosition(20, 20);
    glutCreateWindow("testdraw");

    draw_setup(0);
    
    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(key);

    glutTimerFunc(100, refresh, 20);

    glutMainLoop();
}

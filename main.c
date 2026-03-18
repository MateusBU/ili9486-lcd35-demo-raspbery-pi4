/*
 * main.c — ILI9486 Demo with FRAMEBUFFER
 *
 * Every scene is animated using fb_clear() + fb_*() + fb_flush()
 * Display is only touched once per frame → zero flickering
 *
 * Compile: gcc -O2 -o demo main.c ili9486.c -lwiringPi -lm
 * Run:     sudo ./demo
 */

#include "ili9486.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <wiringPi.h>

static inline uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

static uint32_t millis_now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

// ─── Intro ────────────────────────────────────────────────────────────────
static void demo_intro(void) {
    printf("  [1/4] Intro...\n");
    fb_clear(rgb(10, 10, 30));
    fb_draw_string(20,  40, "ILI9486 DEMO",      COLOR_CYAN,   rgb(10,10,30), 3);
    fb_draw_string(32,  90, "Raspberry Pi 4",    COLOR_WHITE,  rgb(10,10,30), 2);
    fb_draw_string(28, 120, "Framebuffer active",COLOR_YELLOW, rgb(10,10,30), 1);
    fb_draw_line(10, 140, 310, 140, COLOR_CYAN);
    fb_draw_line(10, 142, 310, 142, rgb(0,100,150));
    fb_draw_string(20, 460, "No flickering!", COLOR_GREEN, rgb(10,10,30), 1);
    fb_flush();
    delay(2500);
}

// ─── Bouncing ball ────────────────────────────────────────────────────────
static void demo_bola(void) {
    printf("  [2/4] Bouncing ball...\n");
    float bx=160, by=100, vx=2.8f, vy=2.2f;
    int r=20;
    uint16_t bg = rgb(10,10,25);

    for (int frame=0; frame<300; frame++) {
        uint32_t t0 = millis_now();

        bx+=vx; by+=vy;
        if (bx-r < 0)           { bx=r;            vx= fabsf(vx);  }
        if (bx+r >= LCD_WIDTH)  { bx=LCD_WIDTH-r-1; vx=-fabsf(vx); }
        if (by-r < 0)           { by=r;             vy= fabsf(vy);  }
        if (by+r >= LCD_HEIGHT) { by=LCD_HEIGHT-r-1;vy=-fabsf(vy); }

        fb_clear(bg);
        for (int gx=20;gx<LCD_WIDTH; gx+=40)
        for (int gy=20;gy<LCD_HEIGHT;gy+=40)
            fb_pixel(gx,gy,rgb(30,30,60));

        fb_fill_circle((int16_t)(bx+4),(int16_t)(by+4),r,rgb(0,0,15));
        for (int cr=r;cr>0;cr-=2) {
            float t=(float)(r-cr)/r;
            fb_fill_circle((int16_t)bx,(int16_t)by,cr,
                rgb((uint8_t)(255*(1-t*0.3f)),(uint8_t)(80*t),(uint8_t)(50*(1-t))));
        }

        char buf[32];
        snprintf(buf,sizeof(buf),"Frame %03d",frame);
        fb_draw_string(4,4,buf,COLOR_GRAY,bg,1);

        fb_flush();
        uint32_t el=millis_now()-t0;
        if (el<33) delay(33-el);
    }
}

// ─── Sine waves ───────────────────────────────────────────────────────────
static void demo_ondas(void) {
    printf("  [3/4] Waves...\n");
    uint16_t bg=rgb(5,5,20);

    for (int frame=0;frame<200;frame++) {
        uint32_t t0=millis_now();
        fb_clear(bg);

        float phase=frame*0.08f;
        typedef struct{float amp,freq,ph;uint16_t color;}Wave;
        Wave waves[]={
            {60,0.025f,phase,       COLOR_CYAN},
            {40,0.040f,phase*1.3f,  COLOR_GREEN},
            {30,0.060f,phase*0.7f,  COLOR_MAGENTA},
        };

        for (int o=0;o<3;o++) {
            int prev_y=-1;
            for (int x=0;x<LCD_WIDTH;x++) {
                int y=(int)(LCD_HEIGHT/2+waves[o].amp*sinf(waves[o].freq*x+waves[o].ph));
                y=y<0?0:(y>=LCD_HEIGHT?LCD_HEIGHT-1:y);
                if (prev_y>=0) {
                    int y0=prev_y<y?prev_y:y, y1=prev_y<y?y:prev_y;
                    for (int fy=y0;fy<=y1;fy++) fb_pixel(x,fy,waves[o].color);
                } else fb_pixel(x,y,waves[o].color);
                prev_y=y;
            }
        }
        fb_draw_line(0,LCD_HEIGHT/2,LCD_WIDTH-1,LCD_HEIGHT/2,rgb(40,40,40));
        fb_draw_string(4,4,"Sine waves",COLOR_WHITE,bg,1);
        fb_flush();
        uint32_t el=millis_now()-t0;
        if(el<33) delay(33-el);
    }
}

// ─── Dashboard ────────────────────────────────────────────────────────────
//
// Anti-flicker strategy:
//   1. First frame: draw complete static background + fb_flush() (only once)
//   2. Subsequent frames: redraw ONLY the regions that change and call
//      fb_flush_rect() on each region individually.
//      Background, titles and borders are NOT resent.
//
// Dynamic regions (the only ones that change frame to frame):
//   R1 — Temperature value     (x=16,  y=60,  w=128, h=18)
//   R2 — Temperature bar       (x=55,  y=125, w=38,  h=80)
//   R3 — Humidity value        (x=175, y=60,  w=128, h=18)
//   R4 — Humidity bar          (x=228, y=125, w=38,  h=80)
//   R5 — Sparklines            (x=5,   y=255, w=310, h=90)
//   R6 — Footer (frame counter)(x=4,   y=462, w=120, h=10)

// Dynamic region dimensions
#define R_TVAL_X  16
#define R_TVAL_Y  60
#define R_TVAL_W  128
#define R_TVAL_H  18

#define R_TBAR_X  55
#define R_TBAR_Y  125
#define R_TBAR_W  38
#define R_TBAR_H  80

#define R_UVAL_X  175
#define R_UVAL_Y  60
#define R_UVAL_W  128
#define R_UVAL_H  18

#define R_UBAR_X  228
#define R_UBAR_Y  125
#define R_UBAR_W  38
#define R_UBAR_H  80

#define R_SPARK_X 5
#define R_SPARK_Y 255
#define R_SPARK_W 310
#define R_SPARK_H 90

#define R_FOOT_X  4
#define R_FOOT_Y  462
#define R_FOOT_W  130
#define R_FOOT_H  10

static void dashboard_draw_static(uint16_t bg) {
    // Called ONCE — draws everything that doesn't change
    fb_clear(bg);

    // Header
    fb_fill_rect(0,0,LCD_WIDTH,28,rgb(0,70,140));
    fb_draw_string(55,8,"SENSOR DASHBOARD",COLOR_WHITE,rgb(0,70,140),1);
    fb_draw_line(0,28,LCD_WIDTH,28,COLOR_CYAN);

    // Temperature block background
    fb_fill_rect(8,38,145,75,rgb(35,10,10));
    fb_draw_rect(8,38,145,75,rgb(200,50,50));
    fb_draw_string(16,44,"TEMPERATURE",rgb(255,120,120),rgb(35,10,10),1);

    // Humidity block background
    fb_fill_rect(167,38,145,75,rgb(10,10,40));
    fb_draw_rect(167,38,145,75,rgb(50,100,255));
    fb_draw_string(175,44,"HUMIDITY %",rgb(100,160,255),rgb(10,10,40),1);

    // Bar borders
    fb_draw_rect(R_TBAR_X, R_TBAR_Y, R_TBAR_W, R_TBAR_H+2, COLOR_GRAY);
    fb_draw_string(48,215,"TEMP",COLOR_GRAY,bg,1);
    fb_draw_rect(R_UBAR_X, R_UBAR_Y, R_UBAR_W, R_UBAR_H+2, COLOR_GRAY);
    fb_draw_string(221,215,"HMDT",COLOR_GRAY,bg,1);

    // Sparklines area
    fb_draw_rect(4,240,LCD_WIDTH-8,110,COLOR_GRAY);
    fb_draw_string(8,244,"Temp (history):",COLOR_GRAY,bg,1);
    fb_draw_string(8,330,"Hmdt (history):",COLOR_GRAY,bg,1);
}

static void demo_dashboard(void) {
    printf("  [4/4] Dashboard...\n");
    float temp=23.5f, umid=60.0f;
    uint16_t bg=rgb(12,12,25);
    #define HIST 150
    float hist_t[HIST]={0}, hist_u[HIST]={0};
    int hpos=0;

    // ── Frame 0: static background, full flush (only full flush) ─────────
    dashboard_draw_static(bg);
    fb_flush();

    for (int frame=0;frame<150;frame++) {
        uint32_t t0=millis_now();

        temp+=((rand()%100)-50)*0.05f;
        umid+=((rand()%100)-50)*0.10f;
        if(temp<15) temp=15;
        if(temp>45) temp=45;
        if(umid<30) umid=30;
        if(umid>90) umid=90;
        hist_t[hpos%HIST]=temp; hist_u[hpos%HIST]=umid; hpos++;

        uint16_t tc=temp>35?COLOR_RED:(temp>28?COLOR_ORANGE:COLOR_GREEN);

        // ── R1: temperature value ──────────────────────────────────────
        fb_fill_rect(R_TVAL_X, R_TVAL_Y, R_TVAL_W, R_TVAL_H, rgb(35,10,10));
        char tbuf[16]; snprintf(tbuf,sizeof(tbuf),"%.1f C",temp);
        fb_draw_string(R_TVAL_X, R_TVAL_Y, tbuf, tc, rgb(35,10,10), 2);
        fb_flush_rect(R_TVAL_X, R_TVAL_Y, R_TVAL_W, R_TVAL_H);

        // ── R2: temperature bar ────────────────────────────────────────
        int tbh=(int)((temp-15)/30.0f*R_TBAR_H);
        fb_fill_rect(R_TBAR_X, R_TBAR_Y, R_TBAR_W, R_TBAR_H, rgb(25,25,25));
        if(tbh>0) fb_fill_rect(R_TBAR_X, R_TBAR_Y+R_TBAR_H-tbh, R_TBAR_W, tbh, tc);
        fb_flush_rect(R_TBAR_X, R_TBAR_Y, R_TBAR_W, R_TBAR_H);

        // ── R3: humidity value ─────────────────────────────────────────
        fb_fill_rect(R_UVAL_X, R_UVAL_Y, R_UVAL_W, R_UVAL_H, rgb(10,10,40));
        char ubuf[16]; snprintf(ubuf,sizeof(ubuf),"%.1f%%",umid);
        fb_draw_string(R_UVAL_X, R_UVAL_Y, ubuf, COLOR_CYAN, rgb(10,10,40), 2);
        fb_flush_rect(R_UVAL_X, R_UVAL_Y, R_UVAL_W, R_UVAL_H);

        // ── R4: humidity bar ───────────────────────────────────────────
        int ubh=(int)((umid-30)/60.0f*R_UBAR_H);
        fb_fill_rect(R_UBAR_X, R_UBAR_Y, R_UBAR_W, R_UBAR_H, rgb(25,25,25));
        if(ubh>0) fb_fill_rect(R_UBAR_X, R_UBAR_Y+R_UBAR_H-ubh, R_UBAR_W, ubh, COLOR_BLUE);
        fb_flush_rect(R_UBAR_X, R_UBAR_Y, R_UBAR_W, R_UBAR_H);

        // ── R5: sparklines ─────────────────────────────────────────────
        fb_fill_rect(R_SPARK_X, R_SPARK_Y, R_SPARK_W, R_SPARK_H, bg);
        int pts=hpos<HIST?hpos:HIST;
        for (int i=1;i<pts;i++) {
            int i0=(hpos-pts+i-1+HIST)%HIST, i1=(hpos-pts+i+HIST)%HIST;
            int W=R_SPARK_W-4;
            int tx0=R_SPARK_X+2+(i-1)*W/HIST;
            int tx1=R_SPARK_X+2+i*W/HIST;
            // Temperature sparkline (y range: R_SPARK_Y+5 .. R_SPARK_Y+38)
            int ty0=R_SPARK_Y+38-(int)((hist_t[i0]-15)/30.0f*33);
            int ty1=R_SPARK_Y+38-(int)((hist_t[i1]-15)/30.0f*33);
            fb_draw_line(tx0,ty0,tx1,ty1,COLOR_GREEN);
            // Humidity sparkline (y range: R_SPARK_Y+50 .. R_SPARK_Y+83)
            int uy0=R_SPARK_Y+83-(int)((hist_u[i0]-30)/60.0f*33);
            int uy1=R_SPARK_Y+83-(int)((hist_u[i1]-30)/60.0f*33);
            fb_draw_line(tx0,uy0,tx1,uy1,COLOR_CYAN);
        }
        fb_flush_rect(R_SPARK_X, R_SPARK_Y, R_SPARK_W, R_SPARK_H);

        // ── R6: footer ─────────────────────────────────────────────────
        fb_fill_rect(R_FOOT_X, R_FOOT_Y, R_FOOT_W, R_FOOT_H, bg);
        char fr[32]; snprintf(fr,sizeof(fr),"Frame %03d/150",frame+1);
        fb_draw_string(R_FOOT_X, R_FOOT_Y, fr, COLOR_GRAY, bg, 1);
        fb_flush_rect(R_FOOT_X, R_FOOT_Y, R_FOOT_W, R_FOOT_H);

        // Cap at ~30fps
        uint32_t el=millis_now()-t0;
        if(el<33) delay(33-el);
    }
}

// ─── Color squares ────────────────────────────────────────────────────────
// Draws a grid of squares, each with a different color and its name.
// Text is black on light colors, white on dark colors (readability).

static void demo_color_squares(void) {
    printf("  [5/5] Color squares...\n");

    typedef struct {
        uint16_t    color;
        const char *name;
    } ColorEntry;

    // 15 colors — 3 columns × 5 rows
    static const ColorEntry colors[] = {
        {0xF800, "RED"      },
        {0x07E0, "GREEN"    },
        {0x001F, "BLUE"     },
        {0xFFE0, "YELLOW"   },
        {0x07FF, "CYAN"     },
        {0xF81F, "MAGENTA"  },
        {0xFD20, "ORANGE"   },
        {0xFFFF, "WHITE"    },
        {0x8410, "GRAY"     },
        {0x0000, "BLACK"    },
        {0x780F, "PURPLE"   },
        {0x0410, "DKGREEN"  },
        {0x000F, "NAVY"     },
        {0x7800, "MAROON"   },
        {0xFEA0, "GOLD"     },
    };

    const int COLS    = 3;
    const int ROWS    = 5;
    const int MARGIN  = 6;
    const int SQ_W    = (LCD_WIDTH  - (COLS + 1) * MARGIN) / COLS;   // ~96px
    const int SQ_H    = (LCD_HEIGHT - (ROWS + 1) * MARGIN) / ROWS;   // ~87px

    fb_clear(rgb(20, 20, 20));

    for (int i = 0; i < ROWS * COLS; i++) {
        int col = i % COLS;
        int row = i / COLS;

        int x = MARGIN + col * (SQ_W + MARGIN);
        int y = MARGIN + row * (SQ_H + MARGIN);

        uint16_t bg  = colors[i].color;
        uint16_t fg;

        // Approximate luminance in RGB565 → decide if text is black or white
        uint8_t r5 = (bg >> 11) & 0x1F;
        uint8_t g6 = (bg >>  5) & 0x3F;
        uint8_t b5 = (bg >>  0) & 0x1F;
        // Normalize to 0-255 and apply luminance weights
        uint32_t lum = (uint32_t)r5 * 8 * 299
                     + (uint32_t)g6 * 4 * 587
                     + (uint32_t)b5 * 8 * 114;
        fg = (lum > 128000) ? COLOR_BLACK : COLOR_WHITE;

        // Filled square
        fb_fill_rect(x, y, SQ_W, SQ_H, bg);

        // Thin border to separate similar-colored squares
        fb_draw_rect(x, y, SQ_W, SQ_H, fg == COLOR_BLACK ? rgb(180,180,180) : rgb(60,60,60));

        // Center text inside the square
        int name_len  = 0;
        const char *p = colors[i].name;
        while (*p++) name_len++;
        int char_w    = 6 * 2;  // scale 2: each char is 6px wide
        int char_h    = 8 * 2;  // scale 2: each char is 8px tall
        int txt_x     = x + (SQ_W - name_len * char_w) / 2;
        int txt_y     = y + (SQ_H - char_h) / 2;

        fb_draw_string(txt_x, txt_y, colors[i].name, fg, bg, 2);
    }

    fb_flush();
    delay(4000);
}

// ─── main ─────────────────────────────────────────────────────────────────
int main(void) {
    printf("=== ILI9486 Demo with Framebuffer ===\n");
    if (lcd_init() < 0) { fprintf(stderr,"LCD init failed\n"); return 1; }
    demo_intro();
    // demo_bola();
    // demo_ondas();
    demo_dashboard();
    demo_color_squares();
    fb_clear(COLOR_BLACK);
    fb_draw_string(30,210,"Demo complete!",  COLOR_GREEN, COLOR_BLACK, 2);
    fb_draw_string(40,240,"No flickering :)", COLOR_CYAN,  COLOR_BLACK, 2);
    fb_flush();
    printf("=== Done ===\n");
    lcd_close();
    return 0;
}
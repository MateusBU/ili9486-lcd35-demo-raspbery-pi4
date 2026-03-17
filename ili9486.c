/*
 * ili9486.c — Driver for 3.5" ILI9486 display via SPI on Raspberry Pi 4
 *
 * Dependencies:
 *   sudo apt install wiringpi libwiringpi-dev   (or use lgpio/pigpio)
 *   Interface: Raspberry Pi SPI0
 *
 * Compile:
 *   gcc -O2 -o demo main.c ili9486.c -lwiringPi
 */

#include "ili9486.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>

#include <math.h>
#include <wiringPi.h>

// ─── 5×8 Font (ASCII 32-127) ──────────────────────────────────────────────
// Basic subset – each byte is an 8-bit column (bit7 = top)

static const uint8_t font5x8[][5] = {
    {0x00,0x00,0x00,0x00,0x00}, // ' '
    {0x00,0x00,0x5F,0x00,0x00}, // '!'
    {0x00,0x07,0x00,0x07,0x00}, // '"'
    {0x14,0x7F,0x14,0x7F,0x14}, // '#'
    {0x24,0x2A,0x7F,0x2A,0x12}, // '$'
    {0x23,0x13,0x08,0x64,0x62}, // '%'
    {0x36,0x49,0x55,0x22,0x50}, // '&'
    {0x00,0x05,0x03,0x00,0x00}, // '\''
    {0x00,0x1C,0x22,0x41,0x00}, // '('
    {0x00,0x41,0x22,0x1C,0x00}, // ')'
    {0x14,0x08,0x3E,0x08,0x14}, // '*'
    {0x08,0x08,0x3E,0x08,0x08}, // '+'
    {0x00,0x50,0x30,0x00,0x00}, // ','
    {0x08,0x08,0x08,0x08,0x08}, // '-'
    {0x00,0x60,0x60,0x00,0x00}, // '.'
    {0x20,0x10,0x08,0x04,0x02}, // '/'
    {0x3E,0x51,0x49,0x45,0x3E}, // '0'
    {0x00,0x42,0x7F,0x40,0x00}, // '1'
    {0x42,0x61,0x51,0x49,0x46}, // '2'
    {0x21,0x41,0x45,0x4B,0x31}, // '3'
    {0x18,0x14,0x12,0x7F,0x10}, // '4'
    {0x27,0x45,0x45,0x45,0x39}, // '5'
    {0x3C,0x4A,0x49,0x49,0x30}, // '6'
    {0x01,0x71,0x09,0x05,0x03}, // '7'
    {0x36,0x49,0x49,0x49,0x36}, // '8'
    {0x06,0x49,0x49,0x29,0x1E}, // '9'
    {0x00,0x36,0x36,0x00,0x00}, // ':'
    {0x00,0x56,0x36,0x00,0x00}, // ';'
    {0x08,0x14,0x22,0x41,0x00}, // '<'
    {0x14,0x14,0x14,0x14,0x14}, // '='
    {0x00,0x41,0x22,0x14,0x08}, // '>'
    {0x02,0x01,0x51,0x09,0x06}, // '?'
    {0x32,0x49,0x79,0x41,0x3E}, // '@'
    {0x7E,0x11,0x11,0x11,0x7E}, // 'A'
    {0x7F,0x49,0x49,0x49,0x36}, // 'B'
    {0x3E,0x41,0x41,0x41,0x22}, // 'C'
    {0x7F,0x41,0x41,0x22,0x1C}, // 'D'
    {0x7F,0x49,0x49,0x49,0x41}, // 'E'
    {0x7F,0x09,0x09,0x09,0x01}, // 'F'
    {0x3E,0x41,0x49,0x49,0x7A}, // 'G'
    {0x7F,0x08,0x08,0x08,0x7F}, // 'H'
    {0x00,0x41,0x7F,0x41,0x00}, // 'I'
    {0x20,0x40,0x41,0x3F,0x01}, // 'J'
    {0x7F,0x08,0x14,0x22,0x41}, // 'K'
    {0x7F,0x40,0x40,0x40,0x40}, // 'L'
    {0x7F,0x02,0x0C,0x02,0x7F}, // 'M'
    {0x7F,0x04,0x08,0x10,0x7F}, // 'N'
    {0x3E,0x41,0x41,0x41,0x3E}, // 'O'
    {0x7F,0x09,0x09,0x09,0x06}, // 'P'
    {0x3E,0x41,0x51,0x21,0x5E}, // 'Q'
    {0x7F,0x09,0x19,0x29,0x46}, // 'R'
    {0x46,0x49,0x49,0x49,0x31}, // 'S'
    {0x01,0x01,0x7F,0x01,0x01}, // 'T'
    {0x3F,0x40,0x40,0x40,0x3F}, // 'U'
    {0x1F,0x20,0x40,0x20,0x1F}, // 'V'
    {0x3F,0x40,0x38,0x40,0x3F}, // 'W'
    {0x63,0x14,0x08,0x14,0x63}, // 'X'
    {0x07,0x08,0x70,0x08,0x07}, // 'Y'
    {0x61,0x51,0x49,0x45,0x43}, // 'Z'
    {0x00,0x7F,0x41,0x41,0x00}, // '['
    {0x02,0x04,0x08,0x10,0x20}, // '\\'
    {0x00,0x41,0x41,0x7F,0x00}, // ']'
    {0x04,0x02,0x01,0x02,0x04}, // '^'
    {0x40,0x40,0x40,0x40,0x40}, // '_'
    {0x00,0x01,0x02,0x04,0x00}, // '`'
    {0x20,0x54,0x54,0x54,0x78}, // 'a'
    {0x7F,0x48,0x44,0x44,0x38}, // 'b'
    {0x38,0x44,0x44,0x44,0x20}, // 'c'
    {0x38,0x44,0x44,0x48,0x7F}, // 'd'
    {0x38,0x54,0x54,0x54,0x18}, // 'e'
    {0x08,0x7E,0x09,0x01,0x02}, // 'f'
    {0x0C,0x52,0x52,0x52,0x3E}, // 'g'
    {0x7F,0x08,0x04,0x04,0x78}, // 'h'
    {0x00,0x44,0x7D,0x40,0x00}, // 'i'
    {0x20,0x40,0x44,0x3D,0x00}, // 'j'
    {0x7F,0x10,0x28,0x44,0x00}, // 'k'
    {0x00,0x41,0x7F,0x40,0x00}, // 'l'
    {0x7C,0x04,0x18,0x04,0x78}, // 'm'
    {0x7C,0x08,0x04,0x04,0x78}, // 'n'
    {0x38,0x44,0x44,0x44,0x38}, // 'o'
    {0x7C,0x14,0x14,0x14,0x08}, // 'p'
    {0x08,0x14,0x14,0x18,0x7C}, // 'q'
    {0x7C,0x08,0x04,0x04,0x08}, // 'r'
    {0x48,0x54,0x54,0x54,0x20}, // 's'
    {0x04,0x3F,0x44,0x40,0x20}, // 't'
    {0x3C,0x40,0x40,0x20,0x7C}, // 'u'
    {0x1C,0x20,0x40,0x20,0x1C}, // 'v'
    {0x3C,0x40,0x30,0x40,0x3C}, // 'w'
    {0x44,0x28,0x10,0x28,0x44}, // 'x'
    {0x0C,0x50,0x50,0x50,0x3C}, // 'y'
    {0x44,0x64,0x54,0x4C,0x44}, // 'z'
};

// ─── SPI file descriptor ──────────────────────────────────────────────────
static int      spi_fd    = -1;
static uint32_t spi_speed = SPI_SPEED_HZ;

// ─── Helpers GPIO ─────────────────────────────────────────────────────────

static inline void dc_cmd(void)  { digitalWrite(PIN_DC, LOW);  }
static inline void dc_data(void) { digitalWrite(PIN_DC, HIGH); }

// ─── SPI: ioctl ────────────────────────────
static void spi_write_byte(uint8_t byte) {
    uint8_t rx;
    struct spi_ioc_transfer tr = {
        .tx_buf        = (unsigned long)&byte,
        .rx_buf        = (unsigned long)&rx,
        .len           = 1,
        .speed_hz      = spi_speed,
        .bits_per_word = 8,
        .delay_usecs   = 0,
    };
    ioctl(spi_fd, SPI_IOC_MESSAGE(1), &tr);
}

static void spi_write_buf(const uint8_t *buf, uint32_t len) {
    uint32_t done = 0;
    while (done < len) {
        uint32_t chunk = len - done;
        if (chunk > 4096) chunk = 4096;
        uint8_t rx[4096];
        struct spi_ioc_transfer tr = {
            .tx_buf        = (unsigned long)(buf + done),
            .rx_buf        = (unsigned long)rx,
            .len           = chunk,
            .speed_hz      = spi_speed,
            .bits_per_word = 8,
            .delay_usecs   = 0,
        };
        ioctl(spi_fd, SPI_IOC_MESSAGE(1), &tr);
        done += chunk;
    }
}

// ─── Public API ──────────────────────────────────────────────────────────

void lcd_send_cmd(uint8_t cmd) {
    dc_cmd();
    spi_write_byte(cmd);
}

void lcd_send_data(uint8_t data) {
    dc_data();
    spi_write_byte(data);
}

void lcd_reset(void) {
    digitalWrite(PIN_RST, HIGH); delay(10);
    digitalWrite(PIN_RST, LOW);  delay(20);
    digitalWrite(PIN_RST, HIGH); delay(150);
}

int lcd_init(void) {
    if (wiringPiSetupGpio() < 0) {
        fprintf(stderr, "[LCD] Erro ao inicializar wiringPi GPIO\n");
        return -1;
    }

    // Sets GPIO states BEFORE opening SPI
    pinMode(PIN_DC,  OUTPUT); digitalWrite(PIN_DC,  HIGH);
    pinMode(PIN_RST, OUTPUT); digitalWrite(PIN_RST, HIGH);
    pinMode(PIN_CS,  OUTPUT); digitalWrite(PIN_CS,  LOW);
    pinMode(7,       OUTPUT); digitalWrite(7,       HIGH); // Touch CS

    delay(50); // stabilizes logic levels

    spi_fd = open(SPI_DEV, O_RDWR);
    if (spi_fd < 0) {
        fprintf(stderr, "[LCD] Not possible to open %s\n", SPI_DEV);
        return -1;
    }
    uint8_t  mode  = SPI_MODE_0;
    uint8_t  bits  = 8;
    uint32_t speed = SPI_SPEED_HZ;
    ioctl(spi_fd, SPI_IOC_WR_MODE,          &mode);
    ioctl(spi_fd, SPI_IOC_WR_BITS_PER_WORD, &bits);
    ioctl(spi_fd, SPI_IOC_WR_MAX_SPEED_HZ,  &speed);
    spi_speed = speed;
    printf("[LCD] SPI opened: %s @ %u MHz\n", SPI_DEV, speed / 1000000);

    // Hard reset with generous delays
    digitalWrite(PIN_RST, LOW);
    delay(50);
    digitalWrite(PIN_RST, HIGH);
    delay(200); // datasheet requires minimum 120ms after reset

    // Software Reset + mandatory 120ms wait (datasheet)
    lcd_send_cmd(0x01);
    delay(200);

    // Sleep Out + mandatory 120ms wait (datasheet)
    lcd_send_cmd(0x11);
    delay(200);

    // COLMOD: 16bpp RGB565
    lcd_send_cmd(0x3A);
    lcd_send_data(0x55);
    delay(20);

    // MADCTL: MX=1, BGR=1 → portrait, correct colors
    lcd_send_cmd(0x36);
    lcd_send_data(0x48);
    delay(10);

    // Display ON
    lcd_send_cmd(0x29);
    delay(100);

    printf("[LCD] ILI9486 initialized!\n");
    lcd_fill_screen(0x0000);
    return 0;
}

void lcd_close(void) {
    if (spi_fd >= 0) { close(spi_fd); spi_fd = -1; }
}

// ─── Graphics primitives (write directly to display via SPI) ────────────

void lcd_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    lcd_send_cmd(ILI9486_CASET);
    lcd_send_data(x0 >> 8); lcd_send_data(x0 & 0xFF);
    lcd_send_data(x1 >> 8); lcd_send_data(x1 & 0xFF);
    lcd_send_cmd(ILI9486_PASET);
    lcd_send_data(y0 >> 8); lcd_send_data(y0 & 0xFF);
    lcd_send_data(y1 >> 8); lcd_send_data(y1 & 0xFF);
    lcd_send_cmd(ILI9486_RAMWR);
}

void lcd_draw_pixel(uint16_t x, uint16_t y, uint16_t color) {
    if (x >= LCD_WIDTH || y >= LCD_HEIGHT) return;
    lcd_set_window(x, y, x, y);
    dc_data();
    spi_write_byte(color >> 8);
    spi_write_byte(color & 0xFF);
}

void lcd_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    if (x >= LCD_WIDTH || y >= LCD_HEIGHT) return;
    if (x + w > LCD_WIDTH)  w = LCD_WIDTH  - x;
    if (y + h > LCD_HEIGHT) h = LCD_HEIGHT - y;
    lcd_set_window(x, y, x + w - 1, y + h - 1);
    uint8_t hi = color >> 8, lo = color & 0xFF;
    uint8_t buf[256];
    for (int i = 0; i < 256; i += 2) { buf[i] = hi; buf[i+1] = lo; }
    dc_data();
    uint32_t total = (uint32_t)w * h * 2;
    while (total > 0) {
        uint32_t n = total > 256 ? 256 : total;
        spi_write_buf(buf, n);
        total -= n;
    }
}

void lcd_fill_screen(uint16_t color) {
    lcd_fill_rect(0, 0, LCD_WIDTH, LCD_HEIGHT, color);
}

void lcd_draw_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) {
    int16_t dx=abs(x1-x0), sx=x0<x1?1:-1;
    int16_t dy=-abs(y1-y0), sy=y0<y1?1:-1;
    int16_t err=dx+dy, e2;
    while(1){
        lcd_draw_pixel(x0,y0,color);
        if(x0==x1&&y0==y1) break;
        e2=2*err;
        if(e2>=dy){err+=dy;x0+=sx;}
        if(e2<=dx){err+=dx;y0+=sy;}
    }
}

void lcd_draw_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    lcd_draw_line(x,     y,     x+w-1, y,     color);
    lcd_draw_line(x,     y+h-1, x+w-1, y+h-1, color);
    lcd_draw_line(x,     y,     x,     y+h-1, color);
    lcd_draw_line(x+w-1, y,     x+w-1, y+h-1, color);
}

void lcd_draw_circle(int16_t cx, int16_t cy, int16_t r, uint16_t color) {
    int16_t x=0,y=r,d=3-2*r;
    while(y>=x){
        lcd_draw_pixel(cx+x,cy+y,color); lcd_draw_pixel(cx-x,cy+y,color);
        lcd_draw_pixel(cx+x,cy-y,color); lcd_draw_pixel(cx-x,cy-y,color);
        lcd_draw_pixel(cx+y,cy+x,color); lcd_draw_pixel(cx-y,cy+x,color);
        lcd_draw_pixel(cx+y,cy-x,color); lcd_draw_pixel(cx-y,cy-x,color);
        d+=(d<0)?4*x+6:4*(x-y--)+10; x++;
    }
}

void lcd_fill_circle(int16_t cx, int16_t cy, int16_t r, uint16_t color) {
    for(int16_t dy=-r;dy<=r;dy++)
        lcd_fill_rect(cx-(int16_t)sqrt((double)(r*r-dy*dy)),cy+dy,
                      2*(int16_t)sqrt((double)(r*r-dy*dy))+1,1,color);
}

void lcd_draw_char(uint16_t x, uint16_t y, char c, uint16_t fg, uint16_t bg, uint8_t scale) {
    if(c<32||c>122) c='?';
    const uint8_t *glyph=font5x8[c-32];
    for(uint8_t col=0;col<5;col++){
        uint8_t line=glyph[col];
        for(uint8_t row=0;row<8;row++){
            uint16_t color=(line&(1<<row))?fg:bg;
            lcd_fill_rect(x+col*scale,y+row*scale,scale,scale,color);
        }
    }
}

void lcd_draw_string(uint16_t x, uint16_t y, const char *str,
                     uint16_t fg, uint16_t bg, uint8_t scale) {
    uint16_t cx=x;
    while(*str){
        if(*str=='\n'){y+=8*scale+2;cx=x;}
        else{lcd_draw_char(cx,y,*str,fg,bg,scale);cx+=(5+1)*scale;}
        str++;
    }
}


// ═══════════════════════════════════════════════════════════════════════════
//  FRAMEBUFFER — double-buffer in RAM, single flush per frame
//  Eliminates flickering: draw everything to the buffer, then send at once.
//
//  Typical usage:
//      fb_clear(COLOR_BLACK);       // clears buffer (without touching display)
//      fb_fill_rect(...);           // draws to buffer
//      fb_draw_string(...);         // text to buffer
//      fb_flush();                  // sends everything to display at once
// ═══════════════════════════════════════════════════════════════════════════

// Global buffer — 320×480 pixels × 2 bytes = 307200 bytes (~300 KB)
uint16_t fb[LCD_WIDTH * LCD_HEIGHT];

// Big-endian transmission buffer (ILI9486 expects big-endian)
static uint8_t _txbuf[LCD_WIDTH * LCD_HEIGHT * 2];

// Converts fb[] (little-endian uint16) to _txbuf[] (big-endian bytes)
static void _fb_to_txbuf(int x, int y, int w, int h) {
    int out = 0;
    for (int row = y; row < y + h; row++) {
        const uint16_t *src = &fb[row * LCD_WIDTH + x];
        for (int col = 0; col < w; col++) {
            _txbuf[out++] = src[col] >> 8;
            _txbuf[out++] = src[col] & 0xFF;
        }
    }
}

// ─── Full flush ───────────────────────────────────────────────────────────
// Converts and sends the entire framebuffer in ONE single ioctl call.
// Much more stable than 480 separate calls (one per line).
void fb_flush(void) {
    _fb_to_txbuf(0, 0, LCD_WIDTH, LCD_HEIGHT);

    // Set window = full screen
    lcd_send_cmd(ILI9486_CASET);
    lcd_send_data(0x00); lcd_send_data(0x00);
    lcd_send_data((LCD_WIDTH-1) >> 8); lcd_send_data((LCD_WIDTH-1) & 0xFF);
    lcd_send_cmd(ILI9486_PASET);
    lcd_send_data(0x00); lcd_send_data(0x00);
    lcd_send_data((LCD_HEIGHT-1) >> 8); lcd_send_data((LCD_HEIGHT-1) & 0xFF);
    lcd_send_cmd(ILI9486_RAMWR);

    // Set window = full screen
    digitalWrite(PIN_DC, HIGH);
    spi_write_buf(_txbuf, LCD_WIDTH * LCD_HEIGHT * 2);
}

// ─── Partial flush ────────────────────────────────────────────────────────
void fb_flush_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    if (x + w > LCD_WIDTH)  w = LCD_WIDTH  - x;
    if (y + h > LCD_HEIGHT) h = LCD_HEIGHT - y;
    if (w == 0 || h == 0) return;

    _fb_to_txbuf(x, y, w, h);

    lcd_send_cmd(ILI9486_CASET);
    lcd_send_data(x >> 8); lcd_send_data(x & 0xFF);
    lcd_send_data((x+w-1) >> 8); lcd_send_data((x+w-1) & 0xFF);
    lcd_send_cmd(ILI9486_PASET);
    lcd_send_data(y >> 8); lcd_send_data(y & 0xFF);
    lcd_send_data((y+h-1) >> 8); lcd_send_data((y+h-1) & 0xFF);
    lcd_send_cmd(ILI9486_RAMWR);

    digitalWrite(PIN_DC, HIGH);
    spi_write_buf(_txbuf, (uint32_t)w * h * 2);
}

// ─── Framebuffer primitives ────────────────────────────────────────────

void fb_clear(uint16_t color) {
    // Fills entire buffer with color (using optimized memset for colors
    // where hi==lo, otherwise simple loop)
    uint8_t hi = color >> 8, lo = color & 0xFF;
    if (hi == lo) {
        memset(fb, hi, sizeof(fb));
    } else {
        for (int i = 0; i < LCD_WIDTH * LCD_HEIGHT; i++) fb[i] = color;
    }
}

void fb_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    if (x >= LCD_WIDTH || y >= LCD_HEIGHT) return;
    if (x + w > LCD_WIDTH)  w = LCD_WIDTH  - x;
    if (y + h > LCD_HEIGHT) h = LCD_HEIGHT - y;
    for (uint16_t row = 0; row < h; row++) {
        uint16_t *dst = &fb[(y + row) * LCD_WIDTH + x];
        for (uint16_t col = 0; col < w; col++) dst[col] = color;
    }
}

void fb_draw_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) {
    int16_t dx =  abs(x1-x0), sx = x0<x1 ? 1:-1;
    int16_t dy = -abs(y1-y0), sy = y0<y1 ? 1:-1;
    int16_t err = dx+dy, e2;
    while (1) {
        fb_pixel(x0, y0, color);
        if (x0==x1 && y0==y1) break;
        e2 = 2*err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void fb_draw_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    fb_draw_line(x,       y,       x+w-1, y,       color);
    fb_draw_line(x,       y+h-1,   x+w-1, y+h-1,   color);
    fb_draw_line(x,       y,       x,     y+h-1,   color);
    fb_draw_line(x+w-1,   y,       x+w-1, y+h-1,   color);
}

void fb_draw_circle(int16_t cx, int16_t cy, int16_t r, uint16_t color) {
    int16_t x=0, y=r, d=3-2*r;
    while (y>=x) {
        fb_pixel(cx+x,cy+y,color); fb_pixel(cx-x,cy+y,color);
        fb_pixel(cx+x,cy-y,color); fb_pixel(cx-x,cy-y,color);
        fb_pixel(cx+y,cy+x,color); fb_pixel(cx-y,cy+x,color);
        fb_pixel(cx+y,cy-x,color); fb_pixel(cx-y,cy-x,color);
        d += (d<0) ? 4*x+6 : 4*(x-y--)+10;
        x++;
    }
}

void fb_fill_circle(int16_t cx, int16_t cy, int16_t r, uint16_t color) {
    for (int16_t dy=-r; dy<=r; dy++) {
        int16_t dx = (int16_t)sqrt((double)(r*r - dy*dy));
        fb_fill_rect(cx-dx, cy+dy, 2*dx+1, 1, color);
    }
}

void fb_draw_char(uint16_t x, uint16_t y, char c, uint16_t fg, uint16_t bg, uint8_t scale) {
    if (c < 32 || c > 122) c = '?';
    const uint8_t *glyph = font5x8[c - 32];
    for (uint8_t col=0; col<5; col++) {
        uint8_t line = glyph[col];
        for (uint8_t row=0; row<8; row++) {
            uint16_t color = (line & (1<<row)) ? fg : bg;
            fb_fill_rect(x+col*scale, y+row*scale, scale, scale, color);
        }
    }
}

void fb_draw_string(uint16_t x, uint16_t y, const char *s,
                    uint16_t fg, uint16_t bg, uint8_t scale) {
    uint16_t cx = x;
    while (*s) {
        if (*s == '\n') { y += 8*scale+2; cx = x; }
        else { fb_draw_char(cx, y, *s, fg, bg, scale); cx += (5+1)*scale; }
        s++;
    }
}
#ifndef ILI9486_H
#define ILI9486_H

#include <stdint.h>

// ─── Pinos SPI (BCM numbering) ───────────────────────────────────────────────
#define PIN_DC   24   // Data/Command (GPIO24)
#define PIN_RST   27            // Reset        (GPIO27)
#define PIN_CS    8             // Chip Select  (CE0 = GPIO8)
// MOSI = GPIO10, SCLK = GPIO11 (gerenciados pelo SPI do kernel)

// ─── Configuração SPI ────────────────────────────────────────────────────────
#define SPI_DEV       "/dev/spidev0.0"
#define SPI_SPEED_HZ  64000000  // 64 MHz — Pi4 aguenta; reduza para 32000000 se instável

// ─── Dimensões do display ────────────────────────────────────────────────────
#define LCD_WIDTH   320
#define LCD_HEIGHT  480

// ─── Cores RGB565 ────────────────────────────────────────────────────────────
#define COLOR_BLACK   0x0000
#define COLOR_WHITE   0xFFFF
#define COLOR_RED     0xF800
#define COLOR_GREEN   0x07E0
#define COLOR_BLUE    0x001F
#define COLOR_YELLOW  0xFFE0
#define COLOR_CYAN    0x07FF
#define COLOR_MAGENTA 0xF81F
#define COLOR_ORANGE  0xFD20
#define COLOR_GRAY    0x8410

// ─── Comandos ILI9486 ────────────────────────────────────────────────────────
#define ILI9486_NOP        0x00
#define ILI9486_SWRESET    0x01
#define ILI9486_SLPOUT     0x11
#define ILI9486_DISPON     0x29
#define ILI9486_CASET      0x2A
#define ILI9486_PASET      0x2B
#define ILI9486_RAMWR      0x2C
#define ILI9486_MADCTL     0x36
#define ILI9486_COLMOD     0x3A
#define ILI9486_PGAMCTRL   0xE0
#define ILI9486_NGAMCTRL   0xE1
#define ILI9486_PWCTRL1    0xC0
#define ILI9486_PWCTRL2    0xC1
#define ILI9486_PWCTRL3    0xC2
#define ILI9486_VMCTRL1    0xC5
#define ILI9486_IFCTRL     0xF6
#define ILI9486_IFMODE     0xB0

// ─── Framebuffer (double-buffer sem piscar) ──────────────────────────────────
// Todas as funções fb_* desenham no buffer em RAM.
// Chame fb_flush() ou fb_flush_rect() para enviar ao display de uma vez.
#define FB_SIZE (LCD_WIDTH * LCD_HEIGHT)   // 153600 pixels = 307200 bytes

extern uint16_t fb[FB_SIZE];   // buffer de pixels RGB565

// converte (x,y) em índice no buffer
static inline int fb_idx(int x, int y) { return y * LCD_WIDTH + x; }

// Escreve pixel direto no buffer (sem enviar ao display)
static inline void fb_pixel(int x, int y, uint16_t c) {
    if ((unsigned)x < LCD_WIDTH && (unsigned)y < LCD_HEIGHT)
        fb[fb_idx(x,y)] = c;
}

// Envia buffer inteiro ao display — chame uma vez por frame
void fb_flush(void);

// Envia apenas uma região retangular (mais rápido se só parte mudou)
void fb_flush_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h);

// Preenche o buffer (sem enviar)
void fb_clear(uint16_t color);
void fb_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);

// Primitivas que desenham no buffer
void fb_draw_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color);
void fb_draw_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void fb_draw_circle(int16_t cx, int16_t cy, int16_t r, uint16_t color);
void fb_fill_circle(int16_t cx, int16_t cy, int16_t r, uint16_t color);
void fb_draw_char(uint16_t x, uint16_t y, char c, uint16_t fg, uint16_t bg, uint8_t scale);
void fb_draw_string(uint16_t x, uint16_t y, const char *s, uint16_t fg, uint16_t bg, uint8_t scale);

// ─── API legada (escreve direto no display, sem buffer) ──────────────────────
int  lcd_init(void);
void lcd_close(void);
void lcd_reset(void);
void lcd_send_cmd(uint8_t cmd);
void lcd_send_data(uint8_t data);
void lcd_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
void lcd_fill_screen(uint16_t color);
void lcd_flush(void);          // envia framebuffer inteiro ao display (chame 1x por frame)
void lcd_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void lcd_draw_pixel(uint16_t x, uint16_t y, uint16_t color);
void lcd_draw_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color);
void lcd_draw_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void lcd_draw_circle(int16_t cx, int16_t cy, int16_t r, uint16_t color);
void lcd_fill_circle(int16_t cx, int16_t cy, int16_t r, uint16_t color);
void lcd_draw_char(uint16_t x, uint16_t y, char c, uint16_t fg, uint16_t bg, uint8_t scale);
void lcd_draw_string(uint16_t x, uint16_t y, const char *str, uint16_t fg, uint16_t bg, uint8_t scale);

#endif // ILI9486_H
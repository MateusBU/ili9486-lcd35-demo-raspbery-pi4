# ili9486-lcd35-demo-raspberry-pi4

Demo and examples for the 3.5" ILI9486 SPI LCD display on Raspberry Pi 4.

## Hardware

| Signal | Raspberry Pi 4 (BCM) |
|--------|----------------------|
| MOSI   | GPIO10               |
| SCLK   | GPIO11               |
| CS     | GPIO8 (CE0)          |
| DC     | GPIO24               |
| RST    | GPIO25               |
| Touch CS | GPIO7 (HIGH = disabled) |

> SPI interface: `/dev/spidev0.0` at **64 MHz** (reduce to 32 MHz if unstable)

## Dependencies

```bash
sudo apt update
sudo apt install wiringpi libwiringpi-dev
```

> Alternatively, use `lgpio` or `pigpio` as GPIO backend.

## Build

```bash
gcc -O2 -o demo main.c ili9486.c -lwiringPi -lm
```

## Run

```bash
sudo ./demo
```

## Demo Scenes

The demo runs 5 scenes in sequence:

1. **Intro** — static splash screen with display info
2. **Bouncing Ball** — gradient ball with shadow, ~300 frames at 30 fps
3. **Sine Waves** — 3 animated sine waves (cyan, green, magenta)
4. **Sensor Dashboard** — simulated temperature & humidity with sparklines, partial flush per region
5. **Color Squares** — 15-color RGB565 grid with auto text contrast

## Driver API

### Initialization

```c
int  lcd_init(void);   // initialize SPI + GPIO + ILI9486 registers
void lcd_close(void);  // close SPI file descriptor
```

### Framebuffer (recommended — flicker-free)

All `fb_*` functions draw into a RAM buffer (`uint16_t fb[320*480]`).  
Call `fb_flush()` once per frame to send the buffer to the display in a single SPI transfer.

```c
void fb_clear(uint16_t color);
void fb_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void fb_draw_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color);
void fb_draw_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void fb_draw_circle(int16_t cx, int16_t cy, int16_t r, uint16_t color);
void fb_fill_circle(int16_t cx, int16_t cy, int16_t r, uint16_t color);
void fb_draw_char(uint16_t x, uint16_t y, char c, uint16_t fg, uint16_t bg, uint8_t scale);
void fb_draw_string(uint16_t x, uint16_t y, const char *s, uint16_t fg, uint16_t bg, uint8_t scale);

void fb_flush(void);                                              // full screen
void fb_flush_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h); // partial update
```

### Legacy API (direct to display, no buffer)

```c
void lcd_fill_screen(uint16_t color);
void lcd_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void lcd_draw_pixel(uint16_t x, uint16_t y, uint16_t color);
void lcd_draw_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color);
void lcd_draw_circle(int16_t cx, int16_t cy, int16_t r, uint16_t color);
void lcd_draw_string(uint16_t x, uint16_t y, const char *str, uint16_t fg, uint16_t bg, uint8_t scale);
```

### Color helpers

```c
// Predefined RGB565 colors
COLOR_BLACK, COLOR_WHITE, COLOR_RED, COLOR_GREEN, COLOR_BLUE,
COLOR_YELLOW, COLOR_CYAN, COLOR_MAGENTA, COLOR_ORANGE, COLOR_GRAY

// Custom color macro
static inline uint16_t rgb(uint8_t r, uint8_t g, uint8_t b);
```

## Display Specs

| Property   | Value         |
|------------|---------------|
| Controller | ILI9486       |
| Resolution | 320 × 480     |
| Color      | RGB565 16-bit |
| Interface  | SPI (4-wire)  |
| Size       | 3.5 inch      |

## License

MIT
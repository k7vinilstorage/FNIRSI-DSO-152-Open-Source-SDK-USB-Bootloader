#include "dso152_lcd.h"
#include "dso152_hw.h"

#define GPIOA_BASE      0x40010800
#define GPIOA_CRH      (*(volatile uint32_t*)(GPIOA_BASE + 0x04))
#define GPIOA_BSRR     (*(volatile uint32_t*)(GPIOA_BASE + 0x10))
#define GPIOA_BRR      (*(volatile uint32_t*)(GPIOA_BASE + 0x14))

#define GPIOB_BASE      0x40010C00
#define GPIOB_CRL      (*(volatile uint32_t*)(GPIOB_BASE + 0x00))
#define GPIOB_CRH      (*(volatile uint32_t*)(GPIOB_BASE + 0x04))
#define GPIOB_ODR      (*(volatile uint32_t*)(GPIOB_BASE + 0x0C))

#define GPIOC_BASE      0x40011000
#define GPIOC_CRH      (*(volatile uint32_t*)(GPIOC_BASE + 0x04))
#define GPIOC_BSRR     (*(volatile uint32_t*)(GPIOC_BASE + 0x10))
#define GPIOC_BRR      (*(volatile uint32_t*)(GPIOC_BASE + 0x14))

#define RCC_APB2ENR    (*(volatile uint32_t*)(0x40021000 + 0x18))
#define AFIO_MAPR      (*(volatile uint32_t*)(0x40010000 + 0x04))

static int16_t screen_width = 320;
static int16_t screen_height = 240;

extern const uint8_t font8x8_basic[96][8];

static inline void lcd_write_cmd(uint16_t cmd) {
    GPIOC_BRR = (1 << PIN_LCD_RS);             // RS = 0 (Command)
    GPIOB_ODR = cmd;
    __asm__ volatile("nop");
    GPIOC_BRR = (1 << PIN_LCD_WR);             // WR = 0
    __asm__ volatile("nop; nop; nop; nop;");
    GPIOC_BSRR = (1 << PIN_LCD_WR);            // WR = 1 (Latch on rising edge)
    __asm__ volatile("nop; nop;");
}

static inline void lcd_write_data(uint16_t data) {
    GPIOC_BSRR = (1 << PIN_LCD_RS);            // RS = 1 (Data)
    GPIOB_ODR = data;
    __asm__ volatile("nop");
    GPIOC_BRR = (1 << PIN_LCD_WR);             // WR = 0
    __asm__ volatile("nop; nop; nop; nop;");
    GPIOC_BSRR = (1 << PIN_LCD_WR);            // WR = 1 (Latch on rising edge)
    __asm__ volatile("nop; nop;");
}

static inline void lcd_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    lcd_write_cmd(0x2A); // CASET
    lcd_write_data(x0 >> 8);
    lcd_write_data(x0 & 0xFF);
    lcd_write_data(x1 >> 8);
    lcd_write_data(x1 & 0xFF);

    lcd_write_cmd(0x2B); // RASET
    lcd_write_data(y0 >> 8);
    lcd_write_data(y0 & 0xFF);
    lcd_write_data(y1 >> 8);
    lcd_write_data(y1 & 0xFF);

    lcd_write_cmd(0x2C); // RAMWR
}

int16_t st7789_get_width(void)  { return screen_width; }
int16_t st7789_get_height(void) { return screen_height; }

void st7789_set_rotation(uint8_t rotation) {
    lcd_write_cmd(0x36);
    if (rotation == 0) {
        screen_width = 240; screen_height = 320;
        lcd_write_data(0x08); // Portrait
    } else if (rotation == 1) {
        screen_width = 320; screen_height = 240;
        lcd_write_data(0x70); // Landscape
    } else if (rotation == 2) {
        screen_width = 240; screen_height = 320;
        lcd_write_data(0xC8); // Portrait 180
    } else {
        screen_width = 320; screen_height = 240;
        lcd_write_data(0xA0); // Landscape 180
    }
}

void st7789_init(void) {
    RCC_APB2ENR |= (1 << 0) | (1 << 2) | (1 << 3) | (1 << 4); // AFIO, GPIOA, GPIOB, GPIOC

    // Free PA15, PB3, PB4 from JTAG while keeping SWD
    AFIO_MAPR = (AFIO_MAPR & ~(7 << 24)) | (2 << 24);

    // Port B: 16-bit Push-Pull Outputs 50MHz
    GPIOB_CRL = 0x33333333;
    GPIOB_CRH = 0x33333333;
    GPIOB_ODR = 0x0000;

    // PA8 (RST), PA15 (CS)
    GPIOA_CRH &= ~(0xFu << ((8 - 8) * 4));
    GPIOA_CRH |= (0x3u << ((8 - 8) * 4));
    GPIOA_CRH &= ~(0xFu << ((15 - 8) * 4));
    GPIOA_CRH |= (0x3u << ((15 - 8) * 4));

    // PC13 (RS), PC14 (WR), PC15 (RD)
    GPIOC_CRH &= ~(0xFu << ((13 - 8) * 4));
    GPIOC_CRH |= (0x3u << ((13 - 8) * 4));
    GPIOC_CRH &= ~(0xFu << ((14 - 8) * 4));
    GPIOC_CRH |= (0x3u << ((14 - 8) * 4));
    GPIOC_CRH &= ~(0xFu << ((15 - 8) * 4));
    GPIOC_CRH |= (0x3u << ((15 - 8) * 4));

    // Idle states: CS=0, RD=1, WR=1
    GPIOA_BRR = (1 << PIN_LCD_CS);
    GPIOC_BSRR = (1 << PIN_LCD_RD) | (1 << PIN_LCD_WR);

    // Hardware reset
    GPIOA_BSRR = (1 << PIN_LCD_RST);
    delay_ms(10);
    GPIOA_BRR = (1 << PIN_LCD_RST);
    delay_ms(20);
    GPIOA_BSRR = (1 << PIN_LCD_RST);
    delay_ms(50);

    // ST7789 Initialization Sequence
    lcd_write_cmd(0x11); delay_ms(120);

    st7789_set_rotation(1); // Default Landscape 320x240

    lcd_write_cmd(0x3A); lcd_write_data(0x55); // 16-bit RGB565

    lcd_write_cmd(0xB2); // Porch setting
    lcd_write_data(0x0C); lcd_write_data(0x0C); lcd_write_data(0x00);
    lcd_write_data(0x33); lcd_write_data(0x33);

    lcd_write_cmd(0xB7); lcd_write_data(0x35); // Gate control
    lcd_write_cmd(0xBB); lcd_write_data(0x28); // VCOM setting
    lcd_write_cmd(0xC0); lcd_write_data(0x2C); // LCM control
    lcd_write_cmd(0xC2); lcd_write_data(0x01); // VDV/VRH enable
    lcd_write_cmd(0xC3); lcd_write_data(0x0B); // VRH set
    lcd_write_cmd(0xC4); lcd_write_data(0x20); // VDV set
    lcd_write_cmd(0xC6); lcd_write_data(0x0F); // Frame rate 60Hz
    lcd_write_cmd(0xD0); lcd_write_data(0xA4); lcd_write_data(0xA1); // Power control 1

    // Gamma
    lcd_write_cmd(0xE0);
    lcd_write_data(0xD0); lcd_write_data(0x01); lcd_write_data(0x08);
    lcd_write_data(0x0F); lcd_write_data(0x11); lcd_write_data(0x2A);
    lcd_write_data(0x36); lcd_write_data(0x55); lcd_write_data(0x44);
    lcd_write_data(0x3A); lcd_write_data(0x0B); lcd_write_data(0x06);
    lcd_write_data(0x11); lcd_write_data(0x20);

    lcd_write_cmd(0xE1);
    lcd_write_data(0xD0); lcd_write_data(0x02); lcd_write_data(0x07);
    lcd_write_data(0x0A); lcd_write_data(0x0B); lcd_write_data(0x18);
    lcd_write_data(0x34); lcd_write_data(0x43); lcd_write_data(0x4A);
    lcd_write_data(0x2B); lcd_write_data(0x1B); lcd_write_data(0x1C);
    lcd_write_data(0x22); lcd_write_data(0x1F);

    lcd_write_cmd(0x21); // Display Inversion ON
    lcd_write_cmd(0x13); delay_ms(10); // Normal Display ON
    lcd_write_cmd(0x29); delay_ms(30); // Display ON

    st7789_fill(COLOR_BLACK);
    dso152_backlight(true);
}

void st7789_fill(uint16_t color) {
    st7789_fill_rect(0, 0, screen_width, screen_height, color);
}

void st7789_draw_pixel(int16_t x, int16_t y, uint16_t color) {
    if (x < 0 || x >= screen_width || y < 0 || y >= screen_height) return;
    lcd_set_window(x, y, x, y);
    lcd_write_data(color);
}

void st7789_fill_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    if (x >= screen_width || y >= screen_height || w <= 0 || h <= 0) return;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > screen_width)  w = screen_width - x;
    if (y + h > screen_height) h = screen_height - y;
    if (w <= 0 || h <= 0) return;

    lcd_set_window(x, y, x + w - 1, y + h - 1);
    uint32_t total = (uint32_t)w * h;
    while (total--) {
        lcd_write_data(color);
    }
}

void st7789_draw_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    st7789_fill_rect(x, y, w, 1, color);
    st7789_fill_rect(x, y + h - 1, w, 1, color);
    st7789_fill_rect(x, y, 1, h, color);
    st7789_fill_rect(x + w - 1, y, 1, h, color);
}

void st7789_draw_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) {
    int16_t dx = (x1 > x0) ? (x1 - x0) : (x0 - x1);
    int16_t dy = (y1 > y0) ? (y1 - y0) : (y0 - y1);
    int16_t sx = (x0 < x1) ? 1 : -1;
    int16_t sy = (y0 < y1) ? 1 : -1;
    int16_t err = dx - dy;

    while (1) {
        st7789_draw_pixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int16_t e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 < dx)  { err += dx; y0 += sy; }
    }
}

void st7789_draw_circle(int16_t x0, int16_t y0, int16_t r, uint16_t color) {
    int16_t f = 1 - r;
    int16_t ddF_x = 1;
    int16_t ddF_y = -2 * r;
    int16_t x = 0;
    int16_t y = r;

    st7789_draw_pixel(x0, y0 + r, color);
    st7789_draw_pixel(x0, y0 - r, color);
    st7789_draw_pixel(x0 + r, y0, color);
    st7789_draw_pixel(x0 - r, y0, color);

    while (x < y) {
        if (f >= 0) {
            y--;
            ddF_y += 2;
            f += ddF_y;
        }
        x++;
        ddF_x += 2;
        f += ddF_x;

        st7789_draw_pixel(x0 + x, y0 + y, color);
        st7789_draw_pixel(x0 - x, y0 + y, color);
        st7789_draw_pixel(x0 + x, y0 - y, color);
        st7789_draw_pixel(x0 - x, y0 - y, color);
        st7789_draw_pixel(x0 + y, y0 + x, color);
        st7789_draw_pixel(x0 - y, y0 + x, color);
        st7789_draw_pixel(x0 + y, y0 - x, color);
        st7789_draw_pixel(x0 - y, y0 - x, color);
    }
}

void st7789_fill_circle(int16_t x0, int16_t y0, int16_t r, uint16_t color) {
    st7789_draw_line(x0, y0 - r, x0, y0 + r, color);
    int16_t f = 1 - r;
    int16_t ddF_x = 1;
    int16_t ddF_y = -2 * r;
    int16_t x = 0;
    int16_t y = r;

    while (x < y) {
        if (f >= 0) {
            y--;
            ddF_y += 2;
            f += ddF_y;
        }
        x++;
        ddF_x += 2;
        f += ddF_x;

        st7789_draw_line(x0 + x, y0 - y, x0 + x, y0 + y, color);
        st7789_draw_line(x0 - x, y0 - y, x0 - x, y0 + y, color);
        st7789_draw_line(x0 + y, y0 - x, x0 + y, y0 + x, color);
        st7789_draw_line(x0 - y, y0 - x, x0 - y, y0 + x, color);
    }
}

void st7789_draw_char(int16_t x, int16_t y, char c, uint16_t fg, uint16_t bg, uint8_t size) {
    if (c < 32 || c > 126) c = ' ';
    uint8_t char_idx = c - 32;

    for (int8_t row = 0; row < 8; row++) {
        uint8_t line = font8x8_basic[char_idx][row];
        for (int8_t col = 0; col < 8; col++) {
            if (line & (0x80 >> col)) {
                if (size == 1) {
                    st7789_draw_pixel(x + col, y + row, fg);
                } else {
                    st7789_fill_rect(x + col * size, y + row * size, size, size, fg);
                }
            } else if (bg != fg) {
                if (size == 1) {
                    st7789_draw_pixel(x + col, y + row, bg);
                } else {
                    st7789_fill_rect(x + col * size, y + row * size, size, size, bg);
                }
            }
        }
    }
}

void st7789_draw_string(int16_t x, int16_t y, const char *str, uint16_t fg, uint16_t bg, uint8_t size) {
    while (*str) {
        st7789_draw_char(x, y, *str++, fg, bg, size);
        x += 8 * size;
        if (x + 8 * size > screen_width) break;
    }
}

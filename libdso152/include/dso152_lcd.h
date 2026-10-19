#ifndef DSO152_LCD_H
#define DSO152_LCD_H

#include <stdint.h>
#include <stdbool.h>

/* RGB565 Common Colors */
#define COLOR_BLACK       0x0000
#define COLOR_WHITE       0xFFFF
#define COLOR_RED         0xF800
#define COLOR_GREEN       0x07E0
#define COLOR_BLUE        0x001F
#define COLOR_CYAN        0x07FF
#define COLOR_MAGENTA     0xF81F
#define COLOR_YELLOW      0xFFE0
#define COLOR_ORANGE      0xFD20
#define COLOR_DARKBLUE    0x000F
#define COLOR_NAVY        0x000F
#define COLOR_GRAY        0x8410
#define COLOR_DARKGRAY    0x31A6
#define COLOR_LIGHTGRAY   0xC618

void st7789_init(void);
void st7789_set_rotation(uint8_t rotation);
int16_t st7789_get_width(void);
int16_t st7789_get_height(void);

void st7789_fill(uint16_t color);
void st7789_draw_pixel(int16_t x, int16_t y, uint16_t color);
void st7789_draw_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color);
void st7789_draw_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
void st7789_fill_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
void st7789_draw_circle(int16_t x0, int16_t y0, int16_t r, uint16_t color);
void st7789_fill_circle(int16_t x0, int16_t y0, int16_t r, uint16_t color);
void st7789_draw_char(int16_t x, int16_t y, char c, uint16_t fg, uint16_t bg, uint8_t size);
void st7789_draw_string(int16_t x, int16_t y, const char *str, uint16_t fg, uint16_t bg, uint8_t size);

#endif /* DSO152_LCD_H */

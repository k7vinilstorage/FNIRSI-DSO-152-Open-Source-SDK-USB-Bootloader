#ifndef ST7789_BOOT_H
#define ST7789_BOOT_H

#include <stdint.h>
#include <stdbool.h>

#define LCD_WIDTH   320
#define LCD_HEIGHT  240

/* RGB565 Colors */
#define COLOR_BLACK       0x0000
#define COLOR_WHITE       0xFFFF
#define COLOR_BLUE        0x001F
#define COLOR_NAVY        0x000F
#define COLOR_DARKBLUE    0x0112
#define COLOR_RED         0xF800
#define COLOR_GREEN       0x07E0
#define COLOR_CYAN        0x07FF
#define COLOR_YELLOW      0xFFE0
#define COLOR_ORANGE      0xFD20
#define COLOR_GRAY        0x8410
#define COLOR_DARKGRAY    0x31A6

void st7789_boot_init(void);
void st7789_boot_fill(uint16_t color);
void st7789_boot_draw_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void st7789_boot_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void st7789_boot_draw_char(uint16_t x, uint16_t y, char c, uint16_t fg, uint16_t bg);
void st7789_boot_draw_string(uint16_t x, uint16_t y, const char *str, uint16_t fg, uint16_t bg);
void st7789_boot_show_screen(void);
void st7789_boot_update_progress(uint32_t current, uint32_t total);
void st7789_boot_show_success(void);

#endif /* ST7789_BOOT_H */

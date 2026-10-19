#include "st7789_boot.h"
#include "stm32f1xx.h"

#define PIN_LCD_CS      15  // PA15
#define PIN_LCD_RST     8   // PA8
#define PIN_LCD_BL      9   // PC9
#define PIN_LCD_RS      13  // PC13
#define PIN_LCD_WR      14  // PC14
#define PIN_LCD_RD      15  // PC15

static void delay_us(uint32_t us) {
    uint32_t count = us * 12;
    while (count--) {
        __asm__ volatile ("nop");
    }
}

static void delay_ms(uint32_t ms) {
    while (ms--) {
        delay_us(1000);
    }
}

static inline void lcd_write_cmd(uint16_t cmd) {
    GPIOC->BRR = (1 << PIN_LCD_RS);
    GPIOB->ODR = cmd;
    __asm__ volatile("nop");
    GPIOC->BRR = (1 << PIN_LCD_WR);
    __asm__ volatile("nop; nop; nop; nop;");
    GPIOC->BSRR = (1 << PIN_LCD_WR);
    __asm__ volatile("nop; nop;");
}

static inline void lcd_write_data(uint16_t data) {
    GPIOC->BSRR = (1 << PIN_LCD_RS);
    GPIOB->ODR = data;
    __asm__ volatile("nop");
    GPIOC->BRR = (1 << PIN_LCD_WR);
    __asm__ volatile("nop; nop; nop; nop;");
    GPIOC->BSRR = (1 << PIN_LCD_WR);
    __asm__ volatile("nop; nop;");
}

static inline void lcd_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    lcd_write_cmd(0x2A);
    lcd_write_data(x0 >> 8);
    lcd_write_data(x0 & 0xFF);
    lcd_write_data(x1 >> 8);
    lcd_write_data(x1 & 0xFF);

    lcd_write_cmd(0x2B);
    lcd_write_data(y0 >> 8);
    lcd_write_data(y0 & 0xFF);
    lcd_write_data(y1 >> 8);
    lcd_write_data(y1 & 0xFF);

    lcd_write_cmd(0x2C);
}

void st7789_boot_fill(uint16_t color) {
    lcd_set_window(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1);
    uint32_t total = (uint32_t)LCD_WIDTH * LCD_HEIGHT;
    while (total--) {
        lcd_write_data(color);
    }
}

void st7789_boot_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    if (x >= LCD_WIDTH || y >= LCD_HEIGHT) return;
    if (x + w > LCD_WIDTH) w = LCD_WIDTH - x;
    if (y + h > LCD_HEIGHT) h = LCD_HEIGHT - y;
    
    lcd_set_window(x, y, x + w - 1, y + h - 1);
    uint32_t total = (uint32_t)w * h;
    while (total--) {
        lcd_write_data(color);
    }
}

void st7789_boot_draw_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    st7789_boot_fill_rect(x, y, w, 1, color);
    st7789_boot_fill_rect(x, y + h - 1, w, 1, color);
    st7789_boot_fill_rect(x, y, 1, h, color);
    st7789_boot_fill_rect(x + w - 1, y, 1, h, color);
}

extern const uint8_t font8x8_basic[96][8];

void st7789_boot_draw_char(uint16_t x, uint16_t y, char c, uint16_t fg, uint16_t bg) {
    if (c < 32 || c > 126) c = ' ';
    uint8_t char_idx = c - 32;

    lcd_set_window(x, y, x + 7, y + 7);
    for (uint8_t row = 0; row < 8; row++) {
        uint8_t b = font8x8_basic[char_idx][row];
        for (uint8_t col = 0; col < 8; col++) {
            if (b & (0x80 >> col)) {
                lcd_write_data(fg);
            } else {
                lcd_write_data(bg);
            }
        }
    }
}

void st7789_boot_draw_string(uint16_t x, uint16_t y, const char *str, uint16_t fg, uint16_t bg) {
    while (*str) {
        st7789_boot_draw_char(x, y, *str++, fg, bg);
        x += 8;
        if (x + 8 > LCD_WIDTH) break;
    }
}

void st7789_boot_init(void) {
    /* 1. Clocks for GPIOA, GPIOB, GPIOC, AFIO */
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_IOPBEN | RCC_APB2ENR_IOPCEN | RCC_APB2ENR_AFIOEN;

    /* Disable JTAG-DP to free PA15, PB3, PB4 while keeping SW-DP enabled */
    AFIO->MAPR = (AFIO->MAPR & ~AFIO_MAPR_SWJ_CFG) | AFIO_MAPR_SWJ_CFG_JTAGDISABLE;

    /* Port B: All 16 pins as Push-Pull Output 50MHz (Data Bus) */
    GPIOB->CRL = 0x33333333;
    GPIOB->CRH = 0x33333333;
    GPIOB->ODR = 0x0000;

    /* PA8 (RST), PA15 (CS) as Push-Pull Output 50MHz */
    GPIOA->CRH &= ~(0xFu << ((8 - 8) * 4));
    GPIOA->CRH |= (0x3u << ((8 - 8) * 4));
    GPIOA->CRH &= ~(0xFu << ((15 - 8) * 4));
    GPIOA->CRH |= (0x3u << ((15 - 8) * 4));

    /* PC9 (BL), PC13 (RS), PC14 (WR), PC15 (RD) as Push-Pull Output 50MHz */
    GPIOC->CRH &= ~(0xFu << ((9 - 8) * 4));
    GPIOC->CRH |= (0x3u << ((9 - 8) * 4));
    GPIOC->CRH &= ~(0xFu << ((13 - 8) * 4));
    GPIOC->CRH |= (0x3u << ((13 - 8) * 4));
    GPIOC->CRH &= ~(0xFu << ((14 - 8) * 4));
    GPIOC->CRH |= (0x3u << ((14 - 8) * 4));
    GPIOC->CRH &= ~(0xFu << ((15 - 8) * 4));
    GPIOC->CRH |= (0x3u << ((15 - 8) * 4));

    /* Idle pin states: CS = 0 (selected), RD = 1, WR = 1, BL = 0 (ON) */
    GPIOA->BRR = (1 << PIN_LCD_CS);
    GPIOC->BSRR = (1 << PIN_LCD_RD) | (1 << PIN_LCD_WR);
    GPIOC->BRR = (1 << PIN_LCD_BL); // Backlight ON

    /* Hardware reset ST7789 */
    GPIOA->BSRR = (1 << PIN_LCD_RST);
    delay_ms(10);
    GPIOA->BRR = (1 << PIN_LCD_RST);
    delay_ms(20);
    GPIOA->BSRR = (1 << PIN_LCD_RST);
    delay_ms(50);

    /* ST7789 Initialization */
    lcd_write_cmd(0x11); // Sleep Out
    delay_ms(120);

    lcd_write_cmd(0x36); // MADCTL
    lcd_write_data(0x70); // Landscape (MV=1, MX=1, BGR=1)

    lcd_write_cmd(0x3A); // COLMOD 16-bit RGB565
    lcd_write_data(0x55);

    lcd_write_cmd(0xB2); // Porch setting
    lcd_write_data(0x0C); lcd_write_data(0x0C); lcd_write_data(0x00);
    lcd_write_data(0x33); lcd_write_data(0x33);

    lcd_write_cmd(0xB7); // Gate control
    lcd_write_data(0x35);

    lcd_write_cmd(0xBB); // VCOM setting
    lcd_write_data(0x28);

    lcd_write_cmd(0xC0); // LCM control
    lcd_write_data(0x2C);

    lcd_write_cmd(0xC2); // VDV/VRH enable
    lcd_write_data(0x01);

    lcd_write_cmd(0xC3); // VRH set
    lcd_write_data(0x0B);

    lcd_write_cmd(0xC4); // VDV set
    lcd_write_data(0x20);

    lcd_write_cmd(0xC6); // Frame rate control (60Hz)
    lcd_write_data(0x0F);

    lcd_write_cmd(0xD0); // Power control 1
    lcd_write_data(0xA4); lcd_write_data(0xA1);

    /* Gamma Control */
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

    lcd_write_cmd(0x21); // Display inversion ON
    lcd_write_cmd(0x13); // Normal display ON
    delay_ms(10);
    lcd_write_cmd(0x29); // Display ON
    delay_ms(30);

    st7789_boot_show_screen();
}

void st7789_boot_show_screen(void) {
    /* Dark blue background */
    st7789_boot_fill(COLOR_DARKBLUE);

    /* Header banner */
    st7789_boot_fill_rect(0, 0, LCD_WIDTH, 36, COLOR_NAVY);
    st7789_boot_fill_rect(0, 36, LCD_WIDTH, 2, COLOR_CYAN);

    st7789_boot_draw_string(24, 10, "FNIRSI DSO-152 USB BOOTLOADER", COLOR_WHITE, COLOR_NAVY);

    /* Status card */
    st7789_boot_fill_rect(16, 50, LCD_WIDTH - 32, 130, COLOR_BLACK);
    st7789_boot_draw_rect(16, 50, LCD_WIDTH - 32, 130, COLOR_CYAN);

    st7789_boot_draw_string(28, 62, "STATUS: USB DISK READY", COLOR_GREEN, COLOR_BLACK);
    st7789_boot_draw_string(28, 86, "DRIVE:  [DSO BOOT]", COLOR_YELLOW, COLOR_BLACK);
    st7789_boot_draw_string(28, 110, "FLASH:  0x08004000 (48 KB)", COLOR_WHITE, COLOR_BLACK);
    st7789_boot_draw_string(28, 134, "DROP:   .bin or .hex to update", COLOR_CYAN, COLOR_BLACK);
    st7789_boot_draw_string(28, 154, "AUTO-REBOOT ON COMPLETION", COLOR_DARKGRAY, COLOR_BLACK);

    /* Footer */
    st7789_boot_draw_string(20, 200, "Hold OK on boot to enter this mode", COLOR_DARKGRAY, COLOR_DARKBLUE);
    st7789_boot_draw_string(20, 218, "Hold POWER 1s to shutdown", COLOR_DARKGRAY, COLOR_DARKBLUE);
}

void st7789_boot_update_progress(uint32_t current, uint32_t total) {
    if (total == 0) return;
    if (current > total) current = total;
    uint32_t percent = (current * 100) / total;

    st7789_boot_draw_string(28, 62, "STATUS: FLASHING FIRMWARE... ", COLOR_ORANGE, COLOR_BLACK);

    /* Draw progress bar */
    uint16_t bar_x = 28;
    uint16_t bar_y = 145;
    uint16_t bar_w = LCD_WIDTH - 56;
    uint16_t bar_h = 16;

    st7789_boot_draw_rect(bar_x, bar_y, bar_w, bar_h, COLOR_WHITE);
    uint16_t fill_w = ((bar_w - 4) * percent) / 100;
    st7789_boot_fill_rect(bar_x + 2, bar_y + 2, fill_w, bar_h - 4, COLOR_GREEN);
    st7789_boot_fill_rect(bar_x + 2 + fill_w, bar_y + 2, (bar_w - 4) - fill_w, bar_h - 4, COLOR_BLACK);
}

void st7789_boot_show_success(void) {
    st7789_boot_fill_rect(16, 50, LCD_WIDTH - 32, 130, COLOR_BLACK);
    st7789_boot_draw_rect(16, 50, LCD_WIDTH - 32, 130, COLOR_GREEN);

    st7789_boot_draw_string(40, 75, "UPDATE SUCCESSFUL!", COLOR_GREEN, COLOR_BLACK);
    st7789_boot_draw_string(40, 105, "Flashing Complete.", COLOR_WHITE, COLOR_BLACK);
    st7789_boot_draw_string(40, 135, "Rebooting into application...", COLOR_YELLOW, COLOR_BLACK);
}

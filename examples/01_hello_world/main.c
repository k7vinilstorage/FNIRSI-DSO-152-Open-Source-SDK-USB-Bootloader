#include "dso152.h"
#include <stdio.h>

static void draw_btn_indicator(int16_t x, int16_t y, const char *label, bool pressed) {
    uint16_t bg = pressed ? COLOR_GREEN : COLOR_DARKGRAY;
    uint16_t fg = pressed ? COLOR_BLACK : COLOR_WHITE;
    st7789_fill_rect(x, y, 42, 20, bg);
    st7789_draw_rect(x, y, 42, 20, pressed ? COLOR_WHITE : COLOR_GRAY);
    st7789_draw_string(x + 5, y + 6, label, fg, bg, 1);
}

int main(void) {
    // 1. Initialize DSO152 hardware (power latch, 72MHz clock, buttons, LCD, AFE)
    dso152_init();

    // 2. Clear screen to dark background
    st7789_fill(COLOR_BLACK);

    // 3. Draw Header
    st7789_fill_rect(0, 0, 320, 26, COLOR_BLUE);
    st7789_draw_string(20, 8, "FNIRSI DSO-152 - HELLO WORLD", COLOR_WHITE, COLOR_BLUE, 1);

    // 4. System info
    st7789_draw_string(10, 36, "SDK: libdso152 v1.0", COLOR_CYAN, COLOR_BLACK, 1);
    st7789_draw_string(10, 50, "MCU: CH32F103 / STM32F103C8T6 @ 72MHz", COLOR_LIGHTGRAY, COLOR_BLACK, 1);
    st7789_draw_string(10, 64, "LCD: 320x240 ST7789 (16-bit Parallel)", COLOR_LIGHTGRAY, COLOR_BLACK, 1);
    st7789_draw_string(10, 78, "Flash Base: 0x08004000 (App Space)", COLOR_YELLOW, COLOR_BLACK, 1);

    // 5. Primitives Demo Box
    st7789_draw_rect(10, 95, 145, 60, COLOR_GRAY);
    st7789_draw_string(15, 99, "Graphics Primitives", COLOR_WHITE, COLOR_BLACK, 1);
    st7789_fill_rect(18, 115, 24, 24, COLOR_RED);
    st7789_fill_circle(62, 127, 12, COLOR_GREEN);
    st7789_draw_circle(96, 127, 12, COLOR_CYAN);
    st7789_draw_line(116, 115, 145, 139, COLOR_YELLOW);
    st7789_draw_line(116, 139, 145, 115, COLOR_MAGENTA);

    // 6. Color Swatches
    st7789_draw_rect(165, 95, 145, 60, COLOR_GRAY);
    st7789_draw_string(170, 99, "RGB565 Palette", COLOR_WHITE, COLOR_BLACK, 1);
    const uint16_t colors[] = { COLOR_RED, COLOR_GREEN, COLOR_BLUE, COLOR_YELLOW, COLOR_CYAN, COLOR_MAGENTA };
    for (int i = 0; i < 6; i++) {
        st7789_fill_rect(172 + (i * 22), 120, 18, 20, colors[i]);
    }

    // 7. Interactive Buttons section
    st7789_draw_string(10, 165, "Button Tester (Press any button):", COLOR_WHITE, COLOR_BLACK, 1);
    st7789_draw_string(10, 222, "Hold PWR to shut down | Hold OK to bootloader", COLOR_ORANGE, COLOR_BLACK, 1);

    uint32_t last_cal_toggle = 0;
    uint32_t ok_hold_start = 0;
    dso152_buttons_t last_b = {0};

    // Draw initial button widgets
    draw_btn_indicator(10,  190, "LEFT",  false);
    draw_btn_indicator(58,  190, "OK",    false);
    draw_btn_indicator(106, 190, "RIGHT", false);
    draw_btn_indicator(154, 190, "UP",    false);
    draw_btn_indicator(202, 190, "DOWN",  false);
    draw_btn_indicator(250, 190, "MODE",  false);

    while (1) {
        dso152_buttons_t b = dso152_btn_read();

        // Check power button hold for clean power off
        dso152_btn_check_power_hold();

        // Check OK hold for 2s to reboot to bootloader
        if (b.ok) {
            if (ok_hold_start == 0) {
                ok_hold_start = get_ticks();
            } else if ((get_ticks() - ok_hold_start) > 2000) {
                st7789_fill(COLOR_BLUE);
                st7789_draw_string(60, 110, "REBOOTING TO BOOTLOADER...", COLOR_WHITE, COLOR_BLUE, 1);
                delay_ms(500);
                dso152_reboot_to_bootloader();
            }
        } else {
            ok_hold_start = 0;
        }

        // Toggle calibration signal PA0 every 500ms
        uint32_t now = get_ticks();
        if ((now - last_cal_toggle) >= 500) {
            last_cal_toggle = now;
            dso152_cal_toggle();
        }

        // Update button indicators only on state change (zero flicker!)
        if (b.left != last_b.left)   draw_btn_indicator(10,  190, "LEFT",  b.left);
        if (b.ok != last_b.ok)       draw_btn_indicator(58,  190, "OK",    b.ok);
        if (b.right != last_b.right) draw_btn_indicator(106, 190, "RIGHT", b.right);
        if (b.up != last_b.up)       draw_btn_indicator(154, 190, "UP",    b.up);
        if (b.down != last_b.down)   draw_btn_indicator(202, 190, "DOWN",  b.down);
        if (b.mode != last_b.mode)   draw_btn_indicator(250, 190, "MODE",  b.mode);

        last_b = b;
        delay_ms(10);
    }

    return 0;
}

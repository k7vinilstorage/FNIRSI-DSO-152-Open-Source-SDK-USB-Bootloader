#ifndef DSO152_H
#define DSO152_H

/**
 * @file dso152.h
 * @brief FNIRSI DSO-152 Open-Source C SDK
 * 
 * Comprehensive bare-metal SDK for developing custom firmware,
 * retro games, UI utilities, and oscilloscope tools for the
 * FNIRSI DSO-152 handheld oscilloscope.
 */

#include "dso152_hw.h"
#include "dso152_btn.h"
#include "dso152_lcd.h"
#include "dso152_afe.h"

/**
 * @brief Initialize all DSO152 hardware components:
 *        Power Latch, 72MHz Clock, SysTick, Buttons, ST7789 Display, Backlight, AFE & ADC.
 */
static inline void dso152_init(void) {
    dso152_hw_init();
    dso152_btn_init();
    st7789_init();
    dso152_afe_init();
}

#endif /* DSO152_H */

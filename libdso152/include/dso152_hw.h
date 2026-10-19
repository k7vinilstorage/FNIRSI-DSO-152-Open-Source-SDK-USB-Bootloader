#ifndef DSO152_HW_H
#define DSO152_HW_H

#include <stdint.h>
#include <stdbool.h>

#define CPU_FREQ_HZ     72000000UL

/* Hardware Pin Definitions */
#define PIN_CAL_OUT     0   // PA0: 1kHz Calibration Output
#define PIN_ANALOG_IN   1   // PA1: Scope ADC Input
#define PIN_AFE_RELAY   2   // PA2: Attenuation Relay (0 = Low Range, 1 = High Range)
#define PIN_AFE_ACDC    3   // PA3: Coupling (0 = DC, 1 = AC)
#define PIN_AFE_A0      4   // PA4: MUX A0
#define PIN_AFE_A1      5   // PA5: MUX A1
#define PIN_AFE_A2      6   // PA6: MUX A2
#define PIN_LCD_RST     8   // PA8: LCD Reset
#define PIN_LCD_CS      15  // PA15: LCD Chip Select

#define PIN_LCD_BL      9   // PC9: Backlight (Active-Low: 0 = ON, 1 = OFF)
#define PIN_PWR_LATCH   10  // PC10: Power Latch (Active-High: MUST BE 1 TO STAY ON!)
#define PIN_LCD_RS      13  // PC13: LCD Register Select (0 = Cmd, 1 = Data)
#define PIN_LCD_WR      14  // PC14: LCD Write Strobe
#define PIN_LCD_RD      15  // PC15: LCD Read Strobe

void dso152_hw_init(void);
void dso152_power_latch(bool on);
void dso152_power_off(void);
void dso152_backlight(bool on);
void dso152_reboot_to_bootloader(void);

void dso152_cal_out(bool high);
void dso152_cal_toggle(void);

uint32_t get_ticks(void);
void delay_ms(uint32_t ms);
void delay_us(uint32_t us);

#endif /* DSO152_HW_H */

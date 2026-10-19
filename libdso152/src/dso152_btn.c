#include "dso152_btn.h"
#include "dso152_hw.h"

#define GPIOA_BASE      0x40010800
#define GPIOA_CRH      (*(volatile uint32_t*)(GPIOA_BASE + 0x04))
#define GPIOA_IDR      (*(volatile uint32_t*)(GPIOA_BASE + 0x08))
#define GPIOA_BSRR     (*(volatile uint32_t*)(GPIOA_BASE + 0x10))

#define GPIOC_BASE      0x40011000
#define GPIOC_CRL      (*(volatile uint32_t*)(GPIOC_BASE + 0x00))
#define GPIOC_CRH      (*(volatile uint32_t*)(GPIOC_BASE + 0x04))
#define GPIOC_IDR      (*(volatile uint32_t*)(GPIOC_BASE + 0x08))
#define GPIOC_BSRR     (*(volatile uint32_t*)(GPIOC_BASE + 0x10))

#define RCC_APB2ENR    (*(volatile uint32_t*)(0x40021000 + 0x18))

static uint32_t pwr_press_start_time = 0;

void dso152_btn_init(void) {
    RCC_APB2ENR |= (1 << 2) | (1 << 4); // GPIOA, GPIOC

    /* PA9 (Down), PA10 (Run/Stop): Input with pull-up (0x8) */
    GPIOA_CRH &= ~(0xFu << ((9 - 8) * 4));
    GPIOA_CRH |= (0x8u << ((9 - 8) * 4));
    GPIOA_CRH &= ~(0xFu << ((10 - 8) * 4));
    GPIOA_CRH |= (0x8u << ((10 - 8) * 4));
    GPIOA_BSRR = (1 << 9) | (1 << 10);

    /* PC3 (Left), PC4 (Menu/OK), PC5 (Right), PC6 (Up), PC7 (Mode) */
    for (int p = 3; p <= 7; p++) {
        GPIOC_CRL &= ~(0xFu << (p * 4));
        GPIOC_CRL |= (0x8u << (p * 4));
        GPIOC_BSRR = (1 << p);
    }

    /* PC8 (Auto), PC11 (Power) */
    GPIOC_CRH &= ~(0xFu << ((8 - 8) * 4));
    GPIOC_CRH |= (0x8u << ((8 - 8) * 4));
    GPIOC_CRH &= ~(0xFu << ((11 - 8) * 4));
    GPIOC_CRH |= (0x8u << ((11 - 8) * 4));
    GPIOC_BSRR = (1 << 8) | (1 << 11);
}

dso152_buttons_t dso152_btn_read(void) {
    dso152_buttons_t b;
    uint32_t ida = GPIOA_IDR;
    uint32_t idc = GPIOC_IDR;

    // All buttons active-low
    b.down     = !(ida & (1 << BTN_DOWN_PIN));
    b.run      = !(ida & (1 << BTN_RUN_PIN));

    b.left     = !(idc & (1 << BTN_LEFT_PIN));
    b.ok       = !(idc & (1 << BTN_OK_PIN));
    b.right    = !(idc & (1 << BTN_RIGHT_PIN));
    b.up       = !(idc & (1 << BTN_UP_PIN));
    b.mode     = !(idc & (1 << BTN_MODE_PIN));
    b.auto_btn = !(idc & (1 << BTN_AUTO_PIN));
    b.pwr      = !(idc & (1 << BTN_PWR_PIN));

    return b;
}

void dso152_btn_check_power_hold(void) {
    if (!(GPIOC_IDR & (1 << BTN_PWR_PIN))) {
        if (pwr_press_start_time == 0) {
            pwr_press_start_time = get_ticks();
        } else if ((get_ticks() - pwr_press_start_time) > 800) {
            // Held for >800ms -> power off
            dso152_power_off();
        }
    } else {
        pwr_press_start_time = 0;
    }
}

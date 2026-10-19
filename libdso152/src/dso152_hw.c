#include "dso152_hw.h"

/* STM32F103 Peripheral Base Addresses */
#define RCC_BASE        0x40021000
#define RCC_CR         (*(volatile uint32_t*)(RCC_BASE + 0x00))
#define RCC_CFGR       (*(volatile uint32_t*)(RCC_BASE + 0x04))
#define RCC_APB2ENR    (*(volatile uint32_t*)(RCC_BASE + 0x18))

#define FLASH_R_BASE    0x40022000
#define FLASH_ACR      (*(volatile uint32_t*)(FLASH_R_BASE + 0x00))

#define GPIOA_BASE      0x40010800
#define GPIOA_CRL      (*(volatile uint32_t*)(GPIOA_BASE + 0x00))
#define GPIOA_CRH      (*(volatile uint32_t*)(GPIOA_BASE + 0x04))
#define GPIOA_ODR      (*(volatile uint32_t*)(GPIOA_BASE + 0x0C))
#define GPIOA_BSRR     (*(volatile uint32_t*)(GPIOA_BASE + 0x10))
#define GPIOA_BRR      (*(volatile uint32_t*)(GPIOA_BASE + 0x14))

#define GPIOC_BASE      0x40011000
#define GPIOC_CRL      (*(volatile uint32_t*)(GPIOC_BASE + 0x00))
#define GPIOC_CRH      (*(volatile uint32_t*)(GPIOC_BASE + 0x04))
#define GPIOC_BSRR     (*(volatile uint32_t*)(GPIOC_BASE + 0x10))
#define GPIOC_BRR      (*(volatile uint32_t*)(GPIOC_BASE + 0x14))

#define SYST_CSR       (*(volatile uint32_t*)0xE000E010)
#define SYST_RVR       (*(volatile uint32_t*)0xE000E014)
#define SYST_CVR       (*(volatile uint32_t*)0xE000E018)

#define SCB_AIRCR      (*(volatile uint32_t*)0xE000ED0C)

static volatile uint32_t ms_ticks = 0;

void SysTick_Handler(void) {
    ms_ticks++;
}

uint32_t get_ticks(void) {
    return ms_ticks;
}

void delay_us(uint32_t us) {
    // At 72MHz, roughly 24 iterations per microsecond
    uint32_t cycles = us * 24;
    __asm__ volatile (
        "1: subs %0, %0, #1 \n"
        "   bne 1b          \n"
        : "+r" (cycles)
    );
}

void delay_ms(uint32_t ms) {
    uint32_t start = ms_ticks;
    while ((ms_ticks - start) < ms) {
        __asm__ volatile ("nop");
    }
}

void dso152_power_latch(bool on) {
    if (on) {
        GPIOC_BSRR = (1 << PIN_PWR_LATCH); // PC10 HIGH
    } else {
        GPIOC_BRR = (1 << PIN_PWR_LATCH);  // PC10 LOW -> Cuts battery power!
    }
}

void dso152_power_off(void) {
    dso152_power_latch(false);
    while (1);
}

void dso152_backlight(bool on) {
    if (on) {
        GPIOC_BRR = (1 << PIN_LCD_BL);     // PC9 LOW = Backlight ON
    } else {
        GPIOC_BSRR = (1 << PIN_LCD_BL);    // PC9 HIGH = Backlight OFF
    }
}

void dso152_reboot_to_bootloader(void) {
    // Set magic key at top of RAM
    (*(volatile uint32_t*)0x20004FFC) = 0x12345678;
    // Issue System Reset
    SCB_AIRCR = 0x05FA0004;
    while (1);
}

void dso152_cal_out(bool high) {
    if (high) {
        GPIOA_BSRR = (1 << PIN_CAL_OUT);
    } else {
        GPIOA_BRR  = (1 << PIN_CAL_OUT);
    }
}

void dso152_cal_toggle(void) {
    if (GPIOA_ODR & (1 << PIN_CAL_OUT)) {
        GPIOA_BRR  = (1 << PIN_CAL_OUT);
    } else {
        GPIOA_BSRR = (1 << PIN_CAL_OUT);
    }
}

void dso152_hw_init(void) {
    /* 1. Latch power immediately */
    RCC_APB2ENR |= (1 << 4) | (1 << 2); // GPIOCEN, GPIOAEN
    GPIOC_CRH &= ~(0xFu << ((PIN_PWR_LATCH - 8) * 4));
    GPIOC_CRH |= (0x3u << ((PIN_PWR_LATCH - 8) * 4)); // Output push-pull 50MHz
    GPIOC_BSRR = (1 << PIN_PWR_LATCH); // PC10 = 1

    /* Backlight pin PC9 */
    GPIOC_CRH &= ~(0xFu << ((PIN_LCD_BL - 8) * 4));
    GPIOC_CRH |= (0x3u << ((PIN_LCD_BL - 8) * 4));
    GPIOC_BRR = (1 << PIN_LCD_BL); // Backlight ON

    /* Calibration pin PA0 */
    GPIOA_CRL &= ~(0xFu << (PIN_CAL_OUT * 4));
    GPIOA_CRL |= (0x3u << (PIN_CAL_OUT * 4));
    GPIOA_BRR = (1 << PIN_CAL_OUT);

    /* 2. Configure 72MHz System Clock from 8MHz HSE */
    RCC_CR |= (1 << 16); // HSEON
    uint32_t timeout = 50000;
    while (!(RCC_CR & (1 << 17)) && --timeout); // Wait HSERDY

    // 2 wait states for flash access at 72MHz + prefetch buffer
    FLASH_ACR = (1 << 4) | 0x02;

    if (RCC_CR & (1 << 17)) {
        // PLLMUL = 9 (0111b << 18), PLLSRC = HSE (1 << 16), APB1 = /2 (100b << 8)
        RCC_CFGR = (7 << 18) | (1 << 16) | (4 << 8);
        RCC_CR |= (1 << 24); // PLLON
        while (!(RCC_CR & (1 << 25))); // Wait PLLRDY

        // Switch to PLL
        RCC_CFGR |= 0x02;
        while ((RCC_CFGR & 0x0C) != 0x08);
    }

    /* 3. Configure SysTick for 1ms interrupts at 72MHz */
    SYST_RVR = (72000000 / 1000) - 1;
    SYST_CVR = 0;
    SYST_CSR = 0x07; // CLKSOURCE=CPU, TICKINT=1, ENABLE=1
}

#include "dso152_afe.h"
#include "dso152_hw.h"

#define GPIOA_BASE      0x40010800
#define GPIOA_CRL      (*(volatile uint32_t*)(GPIOA_BASE + 0x00))
#define GPIOA_BSRR     (*(volatile uint32_t*)(GPIOA_BASE + 0x10))
#define GPIOA_BRR      (*(volatile uint32_t*)(GPIOA_BASE + 0x14))

#define RCC_APB2ENR    (*(volatile uint32_t*)(0x40021000 + 0x18))
#define RCC_AHBENR     (*(volatile uint32_t*)(0x40021000 + 0x14))
#define RCC_CFGR       (*(volatile uint32_t*)(0x40021000 + 0x04))

#define ADC1_BASE       0x40012400
#define ADC1_SR        (*(volatile uint32_t*)(ADC1_BASE + 0x00))
#define ADC1_CR1       (*(volatile uint32_t*)(ADC1_BASE + 0x04))
#define ADC1_CR2       (*(volatile uint32_t*)(ADC1_BASE + 0x08))
#define ADC1_SMPR2     (*(volatile uint32_t*)(ADC1_BASE + 0x10))
#define ADC1_SQR1      (*(volatile uint32_t*)(ADC1_BASE + 0x2C))
#define ADC1_SQR3      (*(volatile uint32_t*)(ADC1_BASE + 0x34))
#define ADC1_DR        (*(volatile uint32_t*)(ADC1_BASE + 0x4C))

#define DMA1_BASE       0x40020000
#define DMA1_CCR1      (*(volatile uint32_t*)(DMA1_BASE + 0x08))
#define DMA1_CNDTR1    (*(volatile uint32_t*)(DMA1_BASE + 0x0C))
#define DMA1_CPAR1     (*(volatile uint32_t*)(DMA1_BASE + 0x10))
#define DMA1_CMAR1     (*(volatile uint32_t*)(DMA1_BASE + 0x14))
#define DMA1_ISR       (*(volatile uint32_t*)(DMA1_BASE + 0x00))
#define DMA1_IFCR      (*(volatile uint32_t*)(DMA1_BASE + 0x04))

void dso152_afe_init(void) {
    RCC_APB2ENR |= (1 << 2) | (1 << 9); // GPIOA, ADC1
    RCC_AHBENR  |= (1 << 0);            // DMA1

    // ADC Prescaler: 72MHz / 6 = 12MHz (max allowable is 14MHz)
    RCC_CFGR &= ~(3 << 14);
    RCC_CFGR |= (2 << 14);

    // PA0: Output Push-Pull 50MHz (Cal Out)
    GPIOA_CRL &= ~(0xFu << (0 * 4));
    GPIOA_CRL |= (0x3u << (0 * 4));

    // PA1: Analog Input (0x0)
    GPIOA_CRL &= ~(0xFu << (1 * 4));

    // PA2 (Relay), PA3 (Coupling), PA4 (A0), PA5 (A1), PA6 (A2): Output Push-Pull 50MHz (0x3)
    for (int p = 2; p <= 6; p++) {
        GPIOA_CRL &= ~(0xFu << (p * 4));
        GPIOA_CRL |= (0x3u << (p * 4));
    }

    // Default: 1V/div (Range 6), DC coupling
    dso152_afe_set_range(AFE_RANGE_1V);
    dso152_afe_set_coupling(false);

    // ADC1 Configuration: Independent mode, 1 conversion on Ch1 (PA1)
    ADC1_CR1 = (1 << 8); // Scan mode disabled
    ADC1_CR2 = 1;        // ADON = 1 (Power on)
    delay_ms(1);

    // Calibration
    ADC1_CR2 |= (1 << 3); // RSTCAL
    while (ADC1_CR2 & (1 << 3));
    ADC1_CR2 |= (1 << 2); // CAL
    while (ADC1_CR2 & (1 << 2));

    // Sample time: 55.5 cycles for Ch1
    ADC1_SMPR2 = (5 << 3);
    ADC1_SQR1 = 0; // 1 conversion
    ADC1_SQR3 = 1; // Channel 1 (PA1)
}

void dso152_afe_set_range(dso152_range_t range) {
    if (range > AFE_RANGE_20V) range = AFE_RANGE_20V;

    // Extracted truth table from stock firmware at 0x0800150c:
    // Range 0..4 (10mV..200mV): PA2 = 0
    // Range 5..10 (500mV..20V): PA2 = 1
    bool pa2 = (range >= 5);
    bool pa4 = (range == 1 || range == 3 || range == 6 || range == 8 || range == 10);
    bool pa5 = (range == 2 || range == 3 || range == 7 || range == 8);
    bool pa6 = (range == 4 || range == 9 || range == 10);

    if (pa2) GPIOA_BSRR = (1 << PIN_AFE_RELAY); else GPIOA_BRR = (1 << PIN_AFE_RELAY);
    if (pa4) GPIOA_BSRR = (1 << PIN_AFE_A0);    else GPIOA_BRR = (1 << PIN_AFE_A0);
    if (pa5) GPIOA_BSRR = (1 << PIN_AFE_A1);    else GPIOA_BRR = (1 << PIN_AFE_A1);
    if (pa6) GPIOA_BSRR = (1 << PIN_AFE_A2);    else GPIOA_BRR = (1 << PIN_AFE_A2);
}

void dso152_afe_set_coupling(bool ac_coupling) {
    if (ac_coupling) {
        GPIOA_BSRR = (1 << PIN_AFE_ACDC); // PA3 HIGH = AC Coupling
    } else {
        GPIOA_BRR  = (1 << PIN_AFE_ACDC); // PA3 LOW  = DC Coupling
    }
}

uint16_t dso152_adc_read_single(void) {
    ADC1_CR2 &= ~((1 << 8) | (1 << 1)); // Single mode, no DMA
    ADC1_CR2 |= 1; // Start conversion
    uint32_t to = 20000;
    while (!(ADC1_SR & (1 << 1)) && --to); // Wait EOC with timeout
    return (uint16_t)(ADC1_DR & 0xFFF);
}

void dso152_adc_start_dma(uint16_t *buffer, uint16_t length) {
    DMA1_CCR1 &= ~1;  // Disable DMA1 Ch1
    DMA1_IFCR = 0x0F; // Clear all DMA1 Channel 1 interrupt flags
    DMA1_CPAR1 = (uint32_t)&ADC1_DR;
    DMA1_CMAR1 = (uint32_t)buffer;
    DMA1_CNDTR1 = length;
    // Memory increment (1<<7), 16-bit PSIZE/MSIZE (1<<8 | 1<<10), TCIE (1<<1), EN (1<<0)
    DMA1_CCR1 = (1 << 7) | (1 << 8) | (1 << 10) | (1 << 1) | (1 << 0);

    ADC1_CR2 |= (1 << 8) | (1 << 1); // Enable DMA mode & Continuous conversion
    ADC1_CR2 |= 1;                   // Start conversion
}

bool dso152_adc_dma_is_ready(void) {
    if (DMA1_ISR & (1 << 1)) { // TCIF1
        DMA1_IFCR = (1 << 1);  // Clear TCIF1
        ADC1_CR2 &= ~(1 << 1); // Stop continuous conversion
        return true;
    }
    return false;
}

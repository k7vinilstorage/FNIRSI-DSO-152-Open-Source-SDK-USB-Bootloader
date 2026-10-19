#include "FNIRSI_DSO152.h"

/* ============================================================================
 * Direct STM32F103 / CH32F103 Register Definitions (Zero-overhead I/O)
 * ============================================================================ */
#define DSO_RCC_APB2ENR    (*(volatile uint32_t*)(0x40021000UL + 0x18UL))
#define DSO_RCC_CFGR       (*(volatile uint32_t*)(0x40021000UL + 0x04UL))
#define DSO_AFIO_MAPR      (*(volatile uint32_t*)(0x40010000UL + 0x04UL))

#define DSO_GPIOA_CRL      (*(volatile uint32_t*)(0x40010800UL + 0x00UL))
#define DSO_GPIOA_CRH      (*(volatile uint32_t*)(0x40010800UL + 0x04UL))
#define DSO_GPIOA_IDR      (*(volatile uint32_t*)(0x40010800UL + 0x08UL))
#define DSO_GPIOA_ODR      (*(volatile uint32_t*)(0x40010800UL + 0x0CUL))
#define DSO_GPIOA_BSRR     (*(volatile uint32_t*)(0x40010800UL + 0x10UL))
#define DSO_GPIOA_BRR      (*(volatile uint32_t*)(0x40010800UL + 0x14UL))

#define DSO_GPIOB_CRL      (*(volatile uint32_t*)(0x40010C00UL + 0x00UL))
#define DSO_GPIOB_CRH      (*(volatile uint32_t*)(0x40010C00UL + 0x04UL))
#define DSO_GPIOB_ODR      (*(volatile uint32_t*)(0x40010C00UL + 0x0CUL))

#define DSO_GPIOC_CRL      (*(volatile uint32_t*)(0x40011000UL + 0x00UL))
#define DSO_GPIOC_CRH      (*(volatile uint32_t*)(0x40011000UL + 0x04UL))
#define DSO_GPIOC_IDR      (*(volatile uint32_t*)(0x40011000UL + 0x08UL))
#define DSO_GPIOC_BSRR     (*(volatile uint32_t*)(0x40011000UL + 0x10UL))
#define DSO_GPIOC_BRR      (*(volatile uint32_t*)(0x40011000UL + 0x14UL))

#define DSO_ADC1_SR        (*(volatile uint32_t*)(0x40012400UL + 0x00UL))
#define DSO_ADC1_CR1       (*(volatile uint32_t*)(0x40012400UL + 0x04UL))
#define DSO_ADC1_CR2       (*(volatile uint32_t*)(0x40012400UL + 0x08UL))
#define DSO_ADC1_SMPR2     (*(volatile uint32_t*)(0x40012400UL + 0x10UL))
#define DSO_ADC1_SQR1      (*(volatile uint32_t*)(0x40012400UL + 0x2CUL))
#define DSO_ADC1_SQR3      (*(volatile uint32_t*)(0x40012400UL + 0x34UL))
#define DSO_ADC1_DR        (*(volatile uint32_t*)(0x40012400UL + 0x4CUL))

#define DSO_SCB_VTOR       (*(volatile uint32_t*)0xE000ED08UL)
#define DSO_SCB_AIRCR      (*(volatile uint32_t*)0xE000ED0CUL)
#define DSO_BOOTKEY_RAM    (*(volatile uint32_t*)0x20004FFCUL)

/* Hardware Pin Assignments */
#define PIN_CAL_OUT        0   // PA0
#define PIN_ADC_IN         1   // PA1
#define PIN_AFE_RELAY      2   // PA2
#define PIN_AFE_ACDC       3   // PA3
#define PIN_AFE_A0         4   // PA4
#define PIN_AFE_A1         5   // PA5
#define PIN_AFE_A2         6   // PA6
#define PIN_LCD_RST        8   // PA8
#define PIN_BTN_DOWN       9   // PA9
#define PIN_BTN_RUN        10  // PA10
#define PIN_LCD_CS         15  // PA15

#define PIN_BTN_LEFT       3   // PC3
#define PIN_BTN_OK         4   // PC4 (Menu / OK)
#define PIN_BTN_RIGHT      5   // PC5
#define PIN_BTN_UP         6   // PC6
#define PIN_BTN_MODE       7   // PC7
#define PIN_BTN_AUTO       8   // PC8
#define PIN_LCD_BL         9   // PC9  (Active-Low)
#define PIN_PWR_LATCH      10  // PC10 (Active-High)
#define PIN_BTN_PWR        11  // PC11
#define PIN_LCD_RS         13  // PC13
#define PIN_LCD_WR         14  // PC14
#define PIN_LCD_RD         15  // PC15

/* ============================================================================
 * Early Boot Hook: Relocate Vector Table & Latch Battery Power Immediately
 * Ensures FreeRTOS (SVC/PendSV/SysTick) and hardware interrupts work when
 * launched from the DSO BOOT USB Bootloader at 0x08004000.
 * ============================================================================ */
static void _dso152_latch_and_vtor(void) {
    // 1. Dynamically detect whether sketch is linked at 0x08004000 or 0x08000000
    uint32_t pc_addr = (uint32_t)&_dso152_latch_and_vtor;
    if (pc_addr >= 0x08004000UL && pc_addr < 0x08080000UL) {
        DSO_SCB_VTOR = 0x08004000UL;
        __asm__ volatile ("dsb\nisb" ::: "memory");
    }

    // 2. Immediately latch battery power (PC10 = HIGH) so regulator stays on
    DSO_RCC_APB2ENR |= (1UL << 4); // GPIOCEN
    DSO_GPIOC_CRH = (DSO_GPIOC_CRH & ~(0xFUL << ((PIN_PWR_LATCH - 8) * 4))) | (0x3UL << ((PIN_PWR_LATCH - 8) * 4));
    DSO_GPIOC_BSRR = (1UL << PIN_PWR_LATCH);
}

__attribute__((constructor(101)))
static void _dso152_early_boot_hook(void) {
    _dso152_latch_and_vtor();
}

#if defined(ARDUINO_ARCH_STM32)
extern "C" {
void hw_config_init(void);

// Override STM32duino's weak Reset_Handler so that even if a 48KB/64KB SRAM board
// variant is selected (or cached by Arduino IDE), MSP is clamped to 0x20004FF0
// (within the DSO-152's physical 20 KB SRAM, below the 0x20004FFC bootloader key)
// BEFORE the very first stack push instruction!
__attribute__((naked)) void Reset_Handler(void) {
    __asm__ volatile (
        ".syntax unified            \n"
        "ldr r0, =0x20004FF0        \n"
        "mov r1, sp                 \n"
        "cmp r1, r0                 \n"
        "it hi                      \n"
        "movhi sp, r0               \n"
        "bl SystemInit              \n"
        "ldr r0, =_sdata            \n"
        "ldr r1, =_edata            \n"
        "ldr r2, =_sidata           \n"
        "movs r3, #0                \n"
        "b 2f                       \n"
        "1:                         \n"
        "ldr r4, [r2, r3]           \n"
        "str r4, [r0, r3]           \n"
        "adds r3, r3, #4            \n"
        "2:                         \n"
        "adds r4, r0, r3            \n"
        "cmp r4, r1                 \n"
        "bcc 1b                     \n"
        "ldr r2, =_sbss             \n"
        "ldr r4, =_ebss             \n"
        "movs r3, #0                \n"
        "b 4f                       \n"
        "3:                         \n"
        "str r3, [r2]               \n"
        "adds r2, r2, #4            \n"
        "4:                         \n"
        "cmp r2, r4                 \n"
        "bcc 3b                     \n"
        "bl __libc_init_array       \n"
        "bl main                    \n"
        "bx lr                      \n"
    );
}

// Override STM32duino's weak init() so VTOR and PC10 power latch are configured
// strictly BEFORE hw_config_init() / HAL_Init() enables SysTick interrupts.
void init(void) {
    _dso152_latch_and_vtor();
    hw_config_init();
}

#if !defined(DSO152_CUSTOM_CLOCK_CONFIG)
// Override STM32duino's weak SystemClock_Config() so that any selected STM32F1
// variant (Generic F103RCTx, BluePill F103C8, etc.) runs at full 72 MHz from
// the DSO-152's 8 MHz HSE crystal.
void SystemClock_Config(void) {
    _dso152_latch_and_vtor();

    RCC_OscInitTypeDef RCC_OscInitStruct = {};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {};
    RCC_PeriphCLKInitTypeDef PeriphClkInit = {};

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        // Fallback to 64 MHz HSI if HSE fails to start
        RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
        RCC_OscInitStruct.HSIState = RCC_HSI_ON;
        RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
        RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
        RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI_DIV2;
        RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL16;
        HAL_RCC_OscConfig(&RCC_OscInitStruct);
    }

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
    HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2);

    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC | RCC_PERIPHCLK_USB;
    PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV6;
    PeriphClkInit.UsbClockSelection = RCC_USBCLKSOURCE_PLL_DIV1_5;
    HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit);
}
#endif /* !DSO152_CUSTOM_CLOCK_CONFIG */
}
#endif /* ARDUINO_ARCH_STM32 */

/* ============================================================================
 * Built-in 8x8 ASCII Font for Fast Direct String Rendering
 * ============================================================================ */
static const uint8_t dso_font8x8[96][8] = {
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // ' '
    {0x18, 0x3C, 0x3C, 0x18, 0x18, 0x00, 0x18, 0x00}, // '!'
    {0x66, 0x66, 0x24, 0x00, 0x00, 0x00, 0x00, 0x00}, // '"'
    {0x6C, 0x6C, 0xFE, 0x6C, 0xFE, 0x6C, 0x6C, 0x00}, // '#'
    {0x18, 0x3E, 0x60, 0x3C, 0x06, 0x7C, 0x18, 0x00}, // '$'
    {0x00, 0x63, 0x66, 0x0C, 0x18, 0x33, 0x63, 0x00}, // '%'
    {0x38, 0x6C, 0x38, 0x76, 0xDC, 0xCC, 0x76, 0x00}, // '&'
    {0x30, 0x30, 0x10, 0x20, 0x00, 0x00, 0x00, 0x00}, // '''
    {0x0C, 0x18, 0x30, 0x30, 0x30, 0x18, 0x0C, 0x00}, // '('
    {0x30, 0x18, 0x0C, 0x0C, 0x0C, 0x18, 0x30, 0x00}, // ')'
    {0x00, 0x66, 0x3C, 0xFF, 0x3C, 0x66, 0x00, 0x00}, // '*'
    {0x00, 0x18, 0x18, 0x7E, 0x18, 0x18, 0x00, 0x00}, // '+'
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x30}, // ','
    {0x00, 0x00, 0x00, 0x7E, 0x00, 0x00, 0x00, 0x00}, // '-'
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00}, // '.'
    {0x06, 0x0C, 0x18, 0x30, 0x60, 0xC0, 0x80, 0x00}, // '/'
    {0x3C, 0x66, 0x6E, 0x76, 0x66, 0x66, 0x3C, 0x00}, // '0'
    {0x18, 0x38, 0x18, 0x18, 0x18, 0x18, 0x7E, 0x00}, // '1'
    {0x3C, 0x66, 0x06, 0x0C, 0x18, 0x30, 0x7E, 0x00}, // '2'
    {0x3C, 0x66, 0x06, 0x1C, 0x06, 0x66, 0x3C, 0x00}, // '3'
    {0x0C, 0x1C, 0x34, 0x64, 0x7E, 0x04, 0x04, 0x00}, // '4'
    {0x7E, 0x60, 0x7C, 0x06, 0x06, 0x66, 0x3C, 0x00}, // '5'
    {0x1C, 0x30, 0x60, 0x7C, 0x66, 0x66, 0x3C, 0x00}, // '6'
    {0x7E, 0x06, 0x0C, 0x18, 0x30, 0x30, 0x30, 0x00}, // '7'
    {0x3C, 0x66, 0x66, 0x3C, 0x66, 0x66, 0x3C, 0x00}, // '8'
    {0x3C, 0x66, 0x66, 0x3E, 0x06, 0x0C, 0x38, 0x00}, // '9'
    {0x00, 0x18, 0x18, 0x00, 0x18, 0x18, 0x00, 0x00}, // ':'
    {0x00, 0x18, 0x18, 0x00, 0x18, 0x18, 0x30, 0x00}, // ';'
    {0x06, 0x0C, 0x18, 0x30, 0x18, 0x0C, 0x06, 0x00}, // '<'
    {0x00, 0x00, 0x7E, 0x00, 0x7E, 0x00, 0x00, 0x00}, // '='
    {0x60, 0x30, 0x18, 0x0C, 0x18, 0x30, 0x60, 0x00}, // '>'
    {0x3C, 0x66, 0x06, 0x0C, 0x18, 0x00, 0x18, 0x00}, // '?'
    {0x3C, 0x66, 0x6E, 0x6E, 0x60, 0x62, 0x3C, 0x00}, // '@'
    {0x18, 0x3C, 0x66, 0x66, 0x7E, 0x66, 0x66, 0x00}, // 'A'
    {0x7C, 0x66, 0x66, 0x7C, 0x66, 0x66, 0x7C, 0x00}, // 'B'
    {0x3C, 0x66, 0x60, 0x60, 0x60, 0x66, 0x3C, 0x00}, // 'C'
    {0x78, 0x6C, 0x66, 0x66, 0x66, 0x6C, 0x78, 0x00}, // 'D'
    {0x7E, 0x60, 0x60, 0x7C, 0x60, 0x60, 0x7E, 0x00}, // 'E'
    {0x7E, 0x60, 0x60, 0x7C, 0x60, 0x60, 0x60, 0x00}, // 'F'
    {0x3C, 0x66, 0x60, 0x6E, 0x66, 0x66, 0x3C, 0x00}, // 'G'
    {0x66, 0x66, 0x66, 0x7E, 0x66, 0x66, 0x66, 0x00}, // 'H'
    {0x3C, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3C, 0x00}, // 'I'
    {0x06, 0x06, 0x06, 0x06, 0x06, 0x66, 0x3C, 0x00}, // 'J'
    {0x66, 0x6C, 0x78, 0x70, 0x78, 0x6C, 0x66, 0x00}, // 'K'
    {0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x7E, 0x00}, // 'L'
    {0x63, 0x77, 0x7F, 0x6B, 0x63, 0x63, 0x63, 0x00}, // 'M'
    {0x66, 0x76, 0x7E, 0x7E, 0x6E, 0x66, 0x66, 0x00}, // 'N'
    {0x3C, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x00}, // 'O'
    {0x7C, 0x66, 0x66, 0x7C, 0x60, 0x60, 0x60, 0x00}, // 'P'
    {0x3C, 0x66, 0x66, 0x66, 0x6A, 0x6C, 0x36, 0x00}, // 'Q'
    {0x7C, 0x66, 0x66, 0x7C, 0x6C, 0x66, 0x66, 0x00}, // 'R'
    {0x3C, 0x66, 0x60, 0x3C, 0x06, 0x66, 0x3C, 0x00}, // 'S'
    {0x7E, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00}, // 'T'
    {0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x00}, // 'U'
    {0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x18, 0x00}, // 'V'
    {0x63, 0x63, 0x63, 0x6B, 0x7F, 0x77, 0x63, 0x00}, // 'W'
    {0x66, 0x66, 0x3C, 0x18, 0x3C, 0x66, 0x66, 0x00}, // 'X'
    {0x66, 0x66, 0x66, 0x3C, 0x18, 0x18, 0x18, 0x00}, // 'Y'
    {0x7E, 0x06, 0x0C, 0x18, 0x30, 0x60, 0x7E, 0x00}, // 'Z'
    {0x3C, 0x30, 0x30, 0x30, 0x30, 0x30, 0x3C, 0x00}, // '['
    {0x60, 0x30, 0x18, 0x0C, 0x06, 0x03, 0x01, 0x00}, // '\'
    {0x3C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x3C, 0x00}, // ']'
    {0x18, 0x3C, 0x66, 0x00, 0x00, 0x00, 0x00, 0x00}, // '^'
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF}, // '_'
    {0x30, 0x18, 0x0C, 0x00, 0x00, 0x00, 0x00, 0x00}, // '`'
    {0x00, 0x00, 0x3C, 0x06, 0x3E, 0x66, 0x3E, 0x00}, // 'a'
    {0x60, 0x60, 0x7C, 0x66, 0x66, 0x66, 0x7C, 0x00}, // 'b'
    {0x00, 0x00, 0x3C, 0x66, 0x60, 0x66, 0x3C, 0x00}, // 'c'
    {0x06, 0x06, 0x3E, 0x66, 0x66, 0x66, 0x3E, 0x00}, // 'd'
    {0x00, 0x00, 0x3C, 0x66, 0x7E, 0x60, 0x3C, 0x00}, // 'e'
    {0x1C, 0x30, 0x78, 0x30, 0x30, 0x30, 0x30, 0x00}, // 'f'
    {0x00, 0x00, 0x3E, 0x66, 0x66, 0x3E, 0x06, 0x3C}, // 'g'
    {0x60, 0x60, 0x7C, 0x66, 0x66, 0x66, 0x66, 0x00}, // 'h'
    {0x18, 0x00, 0x38, 0x18, 0x18, 0x18, 0x3C, 0x00}, // 'i'
    {0x06, 0x00, 0x0E, 0x06, 0x06, 0x66, 0x3C, 0x00}, // 'j'
    {0x60, 0x60, 0x66, 0x6C, 0x78, 0x6C, 0x66, 0x00}, // 'k'
    {0x38, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3C, 0x00}, // 'l'
    {0x00, 0x00, 0x66, 0x7F, 0x7F, 0x6B, 0x63, 0x00}, // 'm'
    {0x00, 0x00, 0x7C, 0x66, 0x66, 0x66, 0x66, 0x00}, // 'n'
    {0x00, 0x00, 0x3C, 0x66, 0x66, 0x66, 0x3C, 0x00}, // 'o'
    {0x00, 0x00, 0x7C, 0x66, 0x66, 0x7C, 0x60, 0x60}, // 'p'
    {0x00, 0x00, 0x3E, 0x66, 0x66, 0x3E, 0x06, 0x06}, // 'q'
    {0x00, 0x00, 0x7C, 0x66, 0x60, 0x60, 0x60, 0x00}, // 'r'
    {0x00, 0x00, 0x3E, 0x60, 0x3C, 0x06, 0x7C, 0x00}, // 's'
    {0x18, 0x18, 0x7E, 0x18, 0x18, 0x18, 0x0E, 0x00}, // 't'
    {0x00, 0x00, 0x66, 0x66, 0x66, 0x66, 0x3E, 0x00}, // 'u'
    {0x00, 0x00, 0x66, 0x66, 0x66, 0x3C, 0x18, 0x00}, // 'v'
    {0x00, 0x00, 0x63, 0x6B, 0x7F, 0x3E, 0x36, 0x00}, // 'w'
    {0x00, 0x00, 0x66, 0x3C, 0x18, 0x3C, 0x66, 0x00}, // 'x'
    {0x00, 0x00, 0x66, 0x66, 0x66, 0x3E, 0x06, 0x7C}, // 'y'
    {0x00, 0x00, 0x7E, 0x0C, 0x18, 0x30, 0x7E, 0x00}, // 'z'
    {0x0E, 0x18, 0x18, 0x70, 0x18, 0x18, 0x0E, 0x00}, // '{'
    {0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00}, // '|'
    {0x70, 0x18, 0x18, 0x0E, 0x18, 0x18, 0x70, 0x00}, // '}'
    {0x76, 0xDC, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // '~'
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}  // 127
};

static const char* const range_names[11] = {
    "10mV", "20mV", "50mV", "100mV", "200mV",
    "500mV", "1V", "2V", "5V", "10V", "20V"
};

/* ============================================================================
 * Low-Level 16-Bit 8080-II Parallel Bus Helpers (Port B)
 * ============================================================================ */
static inline void lcd_write_cmd(uint16_t cmd) {
    DSO_GPIOC_BRR = (1UL << PIN_LCD_RS);  // RS = 0 (Command)
    DSO_GPIOB_ODR = cmd;
    __asm__ volatile("nop");
    DSO_GPIOC_BRR = (1UL << PIN_LCD_WR);  // WR = 0
    __asm__ volatile("nop; nop; nop; nop;");
    DSO_GPIOC_BSRR = (1UL << PIN_LCD_WR); // WR = 1 (Latch on rising edge)
    __asm__ volatile("nop; nop;");
}

static inline void lcd_write_data(uint16_t data) {
    DSO_GPIOC_BSRR = (1UL << PIN_LCD_RS); // RS = 1 (Data)
    DSO_GPIOB_ODR = data;
    __asm__ volatile("nop");
    DSO_GPIOC_BRR = (1UL << PIN_LCD_WR);  // WR = 0
    __asm__ volatile("nop; nop; nop; nop;");
    DSO_GPIOC_BSRR = (1UL << PIN_LCD_WR); // WR = 1
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

/* ============================================================================
 * FNIRSI_DSO152 Class Implementation
 * ============================================================================ */
#ifdef DSO152_HAS_ADAFRUIT_GFX
FNIRSI_DSO152::FNIRSI_DSO152() : Adafruit_GFX(320, 240)
#else
FNIRSI_DSO152::FNIRSI_DSO152()
    : _width(320), _height(240), _cursor_x(0), _cursor_y(0),
      _text_fg(COLOR_WHITE), _text_bg(COLOR_BLACK), _text_size(1)
#endif
{
    _current_range = AFE_RANGE_1V;
    _ac_coupling = false;
    _pwr_press_start = 0;
    _ok_press_start = 0;
    _swd_bit_cycles = 0;
}

void FNIRSI_DSO152::begin(uint8_t default_rotation) {
    _initClocksAndGPIO();
    _initLCD(default_rotation);
    _initAFE();
}

void FNIRSI_DSO152::_initClocksAndGPIO() {
    // Enable AFIO, GPIOA, GPIOB, GPIOC, ADC1 clocks
    DSO_RCC_APB2ENR |= (1UL << 0) | (1UL << 2) | (1UL << 3) | (1UL << 4) | (1UL << 9);

    // 1. Latch Battery Power ON (PC10 = HIGH)
    DSO_GPIOC_CRH = (DSO_GPIOC_CRH & ~(0xFUL << ((PIN_PWR_LATCH - 8) * 4))) | (0x3UL << ((PIN_PWR_LATCH - 8) * 4));
    DSO_GPIOC_BSRR = (1UL << PIN_PWR_LATCH);

    // 2. Backlight Pin (PC9, Active-Low)
    DSO_GPIOC_CRH = (DSO_GPIOC_CRH & ~(0xFUL << ((PIN_LCD_BL - 8) * 4))) | (0x3UL << ((PIN_LCD_BL - 8) * 4));
    DSO_GPIOC_BRR = (1UL << PIN_LCD_BL); // Backlight ON

    // 3. Calibration Output (PA0): Push-Pull 50MHz
    DSO_GPIOA_CRL = (DSO_GPIOA_CRL & ~(0xFUL << (PIN_CAL_OUT * 4))) | (0x3UL << (PIN_CAL_OUT * 4));
    DSO_GPIOA_BRR = (1UL << PIN_CAL_OUT);

    // 4. Buttons: Input with Pull-Up (mode 0x8, ODR = 1)
    // PA9 (Down), PA10 (Run)
    DSO_GPIOA_CRH = (DSO_GPIOA_CRH & ~(0xFUL << ((PIN_BTN_DOWN - 8) * 4))) | (0x8UL << ((PIN_BTN_DOWN - 8) * 4));
    DSO_GPIOA_CRH = (DSO_GPIOA_CRH & ~(0xFUL << ((PIN_BTN_RUN - 8) * 4)))  | (0x8UL << ((PIN_BTN_RUN - 8) * 4));
    DSO_GPIOA_BSRR = (1UL << PIN_BTN_DOWN) | (1UL << PIN_BTN_RUN);

    // PC3..PC7 (Left, OK, Right, Up, Mode)
    for (int p = 3; p <= 7; p++) {
        DSO_GPIOC_CRL = (DSO_GPIOC_CRL & ~(0xFUL << (p * 4))) | (0x8UL << (p * 4));
        DSO_GPIOC_BSRR = (1UL << p);
    }
    // PC8 (Auto), PC11 (Power)
    DSO_GPIOC_CRH = (DSO_GPIOC_CRH & ~(0xFUL << ((PIN_BTN_AUTO - 8) * 4))) | (0x8UL << ((PIN_BTN_AUTO - 8) * 4));
    DSO_GPIOC_CRH = (DSO_GPIOC_CRH & ~(0xFUL << ((PIN_BTN_PWR - 8) * 4)))  | (0x8UL << ((PIN_BTN_PWR - 8) * 4));
    DSO_GPIOC_BSRR = (1UL << PIN_BTN_AUTO) | (1UL << PIN_BTN_PWR);
}

void FNIRSI_DSO152::_initLCD(uint8_t rotation) {
    // Disable JTAG-DP while keeping SW-DP enabled so PA15, PB3, PB4 work as GPIO
    DSO_AFIO_MAPR = (DSO_AFIO_MAPR & ~(7UL << 24)) | (2UL << 24);

    // Port B (PB0..PB15): 16-bit Push-Pull Outputs 50MHz
    DSO_GPIOB_CRL = 0x33333333UL;
    DSO_GPIOB_CRH = 0x33333333UL;
    DSO_GPIOB_ODR = 0x0000;

    // PA8 (RST), PA15 (CS)
    DSO_GPIOA_CRH = (DSO_GPIOA_CRH & ~(0xFUL << ((PIN_LCD_RST - 8) * 4))) | (0x3UL << ((PIN_LCD_RST - 8) * 4));
    DSO_GPIOA_CRH = (DSO_GPIOA_CRH & ~(0xFUL << ((PIN_LCD_CS - 8) * 4)))  | (0x3UL << ((PIN_LCD_CS - 8) * 4));

    // PC13 (RS), PC14 (WR), PC15 (RD)
    DSO_GPIOC_CRH = (DSO_GPIOC_CRH & ~(0xFUL << ((PIN_LCD_RS - 8) * 4))) | (0x3UL << ((PIN_LCD_RS - 8) * 4));
    DSO_GPIOC_CRH = (DSO_GPIOC_CRH & ~(0xFUL << ((PIN_LCD_WR - 8) * 4))) | (0x3UL << ((PIN_LCD_WR - 8) * 4));
    DSO_GPIOC_CRH = (DSO_GPIOC_CRH & ~(0xFUL << ((PIN_LCD_RD - 8) * 4))) | (0x3UL << ((PIN_LCD_RD - 8) * 4));

    // Idle bus state: CS = 0, RD = 1, WR = 1
    DSO_GPIOA_BRR = (1UL << PIN_LCD_CS);
    DSO_GPIOC_BSRR = (1UL << PIN_LCD_RD) | (1UL << PIN_LCD_WR);

    // Hardware Reset Pulse
    DSO_GPIOA_BSRR = (1UL << PIN_LCD_RST);
    delay(10);
    DSO_GPIOA_BRR = (1UL << PIN_LCD_RST);
    delay(20);
    DSO_GPIOA_BSRR = (1UL << PIN_LCD_RST);
    delay(50);

    // ST7789V Initialization Sequence
    lcd_write_cmd(0x11); // Sleep Out
    delay(120);

    setRotation(rotation);

    lcd_write_cmd(0x3A); lcd_write_data(0x55); // 16-bit RGB565

    lcd_write_cmd(0xB2); // Porch control
    lcd_write_data(0x0C); lcd_write_data(0x0C); lcd_write_data(0x00);
    lcd_write_data(0x33); lcd_write_data(0x33);

    lcd_write_cmd(0xB7); lcd_write_data(0x35); // Gate control
    lcd_write_cmd(0xBB); lcd_write_data(0x28); // VCOM
    lcd_write_cmd(0xC0); lcd_write_data(0x2C); // LCM
    lcd_write_cmd(0xC2); lcd_write_data(0x01); // VDV/VRH enable
    lcd_write_cmd(0xC3); lcd_write_data(0x0B); // VRH
    lcd_write_cmd(0xC4); lcd_write_data(0x20); // VDV
    lcd_write_cmd(0xC6); lcd_write_data(0x0F); // 60Hz Frame Rate
    lcd_write_cmd(0xD0); lcd_write_data(0xA4); lcd_write_data(0xA1);

    // Positive Gamma
    lcd_write_cmd(0xE0);
    lcd_write_data(0xD0); lcd_write_data(0x01); lcd_write_data(0x08);
    lcd_write_data(0x0F); lcd_write_data(0x11); lcd_write_data(0x2A);
    lcd_write_data(0x36); lcd_write_data(0x55); lcd_write_data(0x44);
    lcd_write_data(0x3A); lcd_write_data(0x0B); lcd_write_data(0x06);
    lcd_write_data(0x11); lcd_write_data(0x20);

    // Negative Gamma
    lcd_write_cmd(0xE1);
    lcd_write_data(0xD0); lcd_write_data(0x02); lcd_write_data(0x07);
    lcd_write_data(0x0A); lcd_write_data(0x0B); lcd_write_data(0x18);
    lcd_write_data(0x34); lcd_write_data(0x43); lcd_write_data(0x4A);
    lcd_write_data(0x2B); lcd_write_data(0x1B); lcd_write_data(0x1C);
    lcd_write_data(0x22); lcd_write_data(0x1F);

    lcd_write_cmd(0x21); // IPS Inversion ON
    lcd_write_cmd(0x13); // Normal Display Mode
    delay(10);
    lcd_write_cmd(0x29); // Display ON
    delay(30);

    fillScreen(COLOR_BLACK);
    setBacklight(true);
}

void FNIRSI_DSO152::_initAFE() {
    // ADC Clock Prescaler: PCLK2 / 6 = 12MHz
    DSO_RCC_CFGR = (DSO_RCC_CFGR & ~(3UL << 14)) | (2UL << 14);

    // PA1: Analog Input (0x0)
    DSO_GPIOA_CRL &= ~(0xFUL << (PIN_ADC_IN * 4));

    // PA2..PA6: Push-Pull 50MHz Outputs (Relay, AC/DC, MUX A0..A2)
    for (int p = 2; p <= 6; p++) {
        DSO_GPIOA_CRL = (DSO_GPIOA_CRL & ~(0xFUL << (p * 4))) | (0x3UL << (p * 4));
    }

    setRange(AFE_RANGE_1V);
    setCoupling(false);

    // ADC1 Power On & Calibration
    DSO_ADC1_CR1 = 0;
    DSO_ADC1_CR2 = 1; // ADON = 1
    delay(2);

    DSO_ADC1_CR2 |= (1UL << 3); // RSTCAL
    uint32_t to = 50000;
    while ((DSO_ADC1_CR2 & (1UL << 3)) && --to);

    DSO_ADC1_CR2 |= (1UL << 2); // CAL
    to = 50000;
    while ((DSO_ADC1_CR2 & (1UL << 2)) && --to);

    DSO_ADC1_SMPR2 = (2UL << 3); // 13.5 cycles sample time on Ch1
    DSO_ADC1_SQR1  = 0;          // 1 conversion
    DSO_ADC1_SQR3  = 1;          // Channel 1 (PA1)
}

/* ============================================================================
 * Power, Backlight & Bootloader Control
 * ============================================================================ */
void FNIRSI_DSO152::setPowerLatch(bool on) {
    if (on) {
        DSO_GPIOC_BSRR = (1UL << PIN_PWR_LATCH);
    } else {
        DSO_GPIOC_BRR  = (1UL << PIN_PWR_LATCH);
    }
}

void FNIRSI_DSO152::powerOff() {
    setBacklight(false);
    setPowerLatch(false);
    while (1);
}

void FNIRSI_DSO152::setBacklight(bool on) {
    if (on) {
        DSO_GPIOC_BRR  = (1UL << PIN_LCD_BL); // Active-Low ON
    } else {
        DSO_GPIOC_BSRR = (1UL << PIN_LCD_BL); // OFF
    }
}

void FNIRSI_DSO152::rebootToBootloader() {
    DSO_BOOTKEY_RAM = 0x12345678UL;
    DSO_SCB_AIRCR   = 0x05FA0004UL;
    while (1);
}

void FNIRSI_DSO152::updatePowerButton(bool allow_bootloader_shortcut) {
    DSO152_Buttons b = readButtons();
    uint32_t now = millis();

    // Check Power Button (PC11) held > 800ms
    if (b.pwr) {
        if (_pwr_press_start == 0) {
            _pwr_press_start = now;
        } else if ((now - _pwr_press_start) > 800) {
            fillRect(40, 90, 240, 50, COLOR_RED);
            drawRect(40, 90, 240, 50, COLOR_WHITE);
            drawString(72, 111, "SHUTTING DOWN...", COLOR_WHITE, COLOR_RED, 1);
            delay(300);
            powerOff();
        }
    } else {
        _pwr_press_start = 0;
    }

    // Optional: Hold OK (PC4) > 2000ms to enter USB Bootloader
    if (allow_bootloader_shortcut) {
        if (b.ok) {
            if (_ok_press_start == 0) {
                _ok_press_start = now;
            } else if ((now - _ok_press_start) > 2000) {
                fillRect(30, 90, 260, 50, COLOR_BLUE);
                drawRect(30, 90, 260, 50, COLOR_WHITE);
                drawString(52, 111, "REBOOTING TO DSO BOOT...", COLOR_WHITE, COLOR_BLUE, 1);
                delay(400);
                rebootToBootloader();
            }
        } else {
            _ok_press_start = 0;
        }
    }
}

/* ============================================================================
 * Buttons & Analog Front-End
 * ============================================================================ */
DSO152_Buttons FNIRSI_DSO152::readButtons() {
    DSO152_Buttons b;
    uint32_t ida = DSO_GPIOA_IDR;
    uint32_t idc = DSO_GPIOC_IDR;

    b.down     = !(ida & (1UL << PIN_BTN_DOWN));
    b.run      = !(ida & (1UL << PIN_BTN_RUN));
    b.left     = !(idc & (1UL << PIN_BTN_LEFT));
    b.ok       = !(idc & (1UL << PIN_BTN_OK));
    b.right    = !(idc & (1UL << PIN_BTN_RIGHT));
    b.up       = !(idc & (1UL << PIN_BTN_UP));
    b.mode     = !(idc & (1UL << PIN_BTN_MODE));
    b.auto_btn = !(idc & (1UL << PIN_BTN_AUTO));
    b.pwr      = !(idc & (1UL << PIN_BTN_PWR));
    return b;
}

void FNIRSI_DSO152::setRange(dso152_range_t range) {
    if (range > AFE_RANGE_20V) range = AFE_RANGE_20V;
    _current_range = range;

    bool pa2 = (range >= 5);
    bool pa4 = (range == 1 || range == 3 || range == 6 || range == 8 || range == 10);
    bool pa5 = (range == 2 || range == 3 || range == 7 || range == 8);
    bool pa6 = (range == 4 || range == 9 || range == 10);

    if (pa2) DSO_GPIOA_BSRR = (1UL << PIN_AFE_RELAY); else DSO_GPIOA_BRR = (1UL << PIN_AFE_RELAY);
    if (pa4) DSO_GPIOA_BSRR = (1UL << PIN_AFE_A0);    else DSO_GPIOA_BRR = (1UL << PIN_AFE_A0);
    if (pa5) DSO_GPIOA_BSRR = (1UL << PIN_AFE_A1);    else DSO_GPIOA_BRR = (1UL << PIN_AFE_A1);
    if (pa6) DSO_GPIOA_BSRR = (1UL << PIN_AFE_A2);    else DSO_GPIOA_BRR = (1UL << PIN_AFE_A2);
}

const char* FNIRSI_DSO152::getRangeName(dso152_range_t range) const {
    if (range > AFE_RANGE_20V) range = AFE_RANGE_20V;
    return range_names[range];
}

void FNIRSI_DSO152::setCoupling(bool ac_coupling) {
    _ac_coupling = ac_coupling;
    if (ac_coupling) {
        DSO_GPIOA_BSRR = (1UL << PIN_AFE_ACDC);
    } else {
        DSO_GPIOA_BRR  = (1UL << PIN_AFE_ACDC);
    }
}

uint16_t FNIRSI_DSO152::readADC() {
    DSO_ADC1_SQR3 = 1;
    DSO_ADC1_CR2 |= 1UL;
    uint32_t to = 20000;
    while (!(DSO_ADC1_SR & (1UL << 1)) && --to);
    return (uint16_t)(DSO_ADC1_DR & 0xFFFU);
}

uint32_t FNIRSI_DSO152::readMillivolts() {
    return ((uint32_t)readADC() * 3300UL) / 4095UL;
}

void FNIRSI_DSO152::captureWaveform(uint16_t *buffer, uint16_t length, uint16_t delay_loops, uint16_t trig_level) {
    // Rising-edge trigger search
    uint32_t trig_to = 1500;
    uint16_t prev = readADC();
    while (--trig_to) {
        uint16_t cur = readADC();
        if (prev < trig_level && cur >= trig_level) break;
        prev = cur;
    }

    for (uint16_t i = 0; i < length; i++) {
        buffer[i] = readADC();
        for (volatile uint16_t d = 0; d < delay_loops; d++) {
            __asm__ volatile("nop");
        }
    }
}

void FNIRSI_DSO152::setCalPin(bool high) {
    if (high) {
        DSO_GPIOA_BSRR = (1UL << PIN_CAL_OUT);
    } else {
        DSO_GPIOA_BRR  = (1UL << PIN_CAL_OUT);
    }
}

void FNIRSI_DSO152::toggleCalPin() {
    if (DSO_GPIOA_ODR & (1UL << PIN_CAL_OUT)) {
        DSO_GPIOA_BRR  = (1UL << PIN_CAL_OUT);
    } else {
        DSO_GPIOA_BSRR = (1UL << PIN_CAL_OUT);
    }
}

/* ============================================================================
 * SWD Header UART Bridge (PA13/DIO = DSO TX, PA14/CLK = DSO RX)
 * Connects the 4-pin SWD header directly to an ESP32-C3 SuperMini (RX=20, TX=21)
 * ============================================================================ */
#define DSO_DEMCR          (*(volatile uint32_t*)0xE000EDFCUL)
#define DSO_DWT_CTRL       (*(volatile uint32_t*)0xE0001000UL)
#define DSO_DWT_CYCCNT     (*(volatile uint32_t*)0xE0001004UL)
#define PIN_SWD_TX         13  // PA13 (SWDIO pad -> DSO TX -> ESP32-C3 RX/GPIO20)
#define PIN_SWD_RX         14  // PA14 (SWCLK pad -> DSO RX <- ESP32-C3 TX/GPIO21)

static inline void _swd_uart_acquire_pins(void) {
    DSO_RCC_APB2ENR |= (1UL << 0) | (1UL << 2); // AFIOEN | GPIOAEN
    // PA13 (DIO pad) = Push-Pull Output 50MHz, Idle HIGH (UART TX)
    DSO_GPIOA_BSRR = (1UL << PIN_SWD_TX);
    DSO_GPIOA_CRH  = (DSO_GPIOA_CRH & ~(0xFUL << ((PIN_SWD_TX - 8) * 4))) | (0x3UL << ((PIN_SWD_TX - 8) * 4));
    // PA14 (CLK pad) = Input with Pull-Up (mode 0x8, ODR=1) (UART RX)
    DSO_GPIOA_CRH  = (DSO_GPIOA_CRH & ~(0xFUL << ((PIN_SWD_RX - 8) * 4))) | (0x8UL << ((PIN_SWD_RX - 8) * 4));
    DSO_GPIOA_BSRR = (1UL << PIN_SWD_RX);
    // Switch SWJ_CFG = 4 (disable JTAG-DP and SW-DP temporarily so PA13/PA14 act as GPIO)
    DSO_AFIO_MAPR  = (DSO_AFIO_MAPR & ~(7UL << 24)) | (4UL << 24);
}

static inline void _swd_uart_release_pins(void) {
    // Return PA13 & PA14 to Input with Pull-Up (Idle HIGH for UART, safe for SWD)
    DSO_GPIOA_BSRR = (1UL << PIN_SWD_TX) | (1UL << PIN_SWD_RX);
    DSO_GPIOA_CRH  = (DSO_GPIOA_CRH & ~((0xFUL << ((PIN_SWD_TX - 8) * 4)) | (0xFUL << ((PIN_SWD_RX - 8) * 4))))
                   | (0x8UL << ((PIN_SWD_TX - 8) * 4)) | (0x8UL << ((PIN_SWD_RX - 8) * 4));
    // Restore SWJ_CFG = 2 (JTAG-DP disabled, SW-DP enabled so ST-Link can always connect!)
    DSO_AFIO_MAPR  = (DSO_AFIO_MAPR & ~(7UL << 24)) | (2UL << 24);
}

void FNIRSI_DSO152::swdSerialBegin(uint32_t baud) {
    if (baud == 0) baud = 57600;
    _swd_bit_cycles = 72000000UL / baud;

    // Enable ARM Cortex-M3 DWT 72 MHz hardware cycle counter
    DSO_DEMCR    |= 0x01000000UL; // TRCENA
    DSO_DWT_CTRL |= 1UL;          // CYCCNTENA

    // Keep SW-DP enabled during idle; pins are acquired on-demand during transactions
    _swd_uart_release_pins();
}

void FNIRSI_DSO152::swdSerialWrite(uint8_t b) {
    if (_swd_bit_cycles == 0) swdSerialBegin(57600);
    uint32_t bit_cyc = _swd_bit_cycles;

    // Ensure PA13 is driving TX HIGH before start bit
    if ((DSO_AFIO_MAPR & (7UL << 24)) != (4UL << 24)) {
        _swd_uart_acquire_pins();
    }

    // Start bit (LOW)
    DSO_GPIOA_BRR = (1UL << PIN_SWD_TX);
    uint32_t t0 = DSO_DWT_CYCCNT;
    uint32_t next_edge = bit_cyc;

    // 8 data bits (LSB first)
    for (uint8_t i = 0; i < 8; i++) {
        while ((uint32_t)(DSO_DWT_CYCCNT - t0) < next_edge);
        if (b & (1U << i)) {
            DSO_GPIOA_BSRR = (1UL << PIN_SWD_TX);
        } else {
            DSO_GPIOA_BRR  = (1UL << PIN_SWD_TX);
        }
        next_edge += bit_cyc;
    }

    // Stop bit (HIGH)
    while ((uint32_t)(DSO_DWT_CYCCNT - t0) < next_edge);
    DSO_GPIOA_BSRR = (1UL << PIN_SWD_TX);
    next_edge += bit_cyc;
    while ((uint32_t)(DSO_DWT_CYCCNT - t0) < next_edge);
}

void FNIRSI_DSO152::swdSerialPrint(const char *str) {
    if (!str) return;
    _swd_uart_acquire_pins();
    while (*str) {
        swdSerialWrite((uint8_t)*str++);
    }
}

void FNIRSI_DSO152::swdSerialPrintln(const char *str) {
    _swd_uart_acquire_pins();
    swdSerialPrint(str);
    swdSerialWrite('\n');
}

int FNIRSI_DSO152::swdSerialReadByte(uint32_t timeout_us) {
    if (_swd_bit_cycles == 0) swdSerialBegin(57600);
    uint32_t bit_cyc = _swd_bit_cycles;
    uint32_t wait_cycles = timeout_us * 72UL;
    uint32_t wait_start  = DSO_DWT_CYCCNT;

    // Wait for line to be idle HIGH briefly if mid-character, then falling edge of start bit
    while (DSO_GPIOA_IDR & (1UL << PIN_SWD_RX)) {
        if ((uint32_t)(DSO_DWT_CYCCNT - wait_start) >= wait_cycles) {
            return -1; // Timeout
        }
    }

    // Start bit falling edge detected!
    uint32_t t0 = DSO_DWT_CYCCNT;
    uint32_t sample_point = bit_cyc + (bit_cyc >> 1); // 1.5 bit times to center of bit 0
    uint8_t val = 0;

    for (uint8_t i = 0; i < 8; i++) {
        while ((uint32_t)(DSO_DWT_CYCCNT - t0) < sample_point);
        if (DSO_GPIOA_IDR & (1UL << PIN_SWD_RX)) {
            val |= (uint8_t)(1U << i);
        }
        sample_point += bit_cyc;
    }

    // Wait into stop bit so we are ready for the next start bit immediately
    while ((uint32_t)(DSO_DWT_CYCCNT - t0) < sample_point);
    return (int)val;
}

uint16_t FNIRSI_DSO152::swdSerialReadLine(char *buf, uint16_t max_len, uint32_t timeout_ms) {
    if (!buf || max_len == 0) return 0;
    uint16_t idx = 0;

    // Wait up to timeout_ms for the first byte of the reply
    int c = swdSerialReadByte(timeout_ms * 1000UL);
    while (c >= 0) {
        if (c == '\n') {
            break;
        }
        if (c != '\r' && idx < (max_len - 1)) {
            buf[idx++] = (char)c;
        }
        // Subsequent bytes in the same burst arrive back-to-back (< 500 us apart)
        c = swdSerialReadByte(1500);
    }
    buf[idx] = '\0';
    return idx;
}

uint16_t FNIRSI_DSO152::swdSerialTransact(const char *tx_line, char *rx_buf, uint16_t rx_max_len, uint32_t timeout_ms) {
    _swd_uart_acquire_pins();
    swdSerialPrintln(tx_line);
    uint16_t len = swdSerialReadLine(rx_buf, rx_max_len, timeout_ms);
    _swd_uart_release_pins();
    return len;
}

/* ============================================================================
 * Display Driver & Graphics Primitives
 * ============================================================================ */
void FNIRSI_DSO152::setRotation(uint8_t r) {
    uint8_t rot = r & 3;
#ifdef DSO152_HAS_ADAFRUIT_GFX
    rotation = rot;
#endif
    lcd_write_cmd(0x36);
    if (rot == 0) {
        _width = 240; _height = 320;
        lcd_write_data(0x08); // Portrait
    } else if (rot == 1) {
        _width = 320; _height = 240;
        lcd_write_data(0x70); // Landscape
    } else if (rot == 2) {
        _width = 240; _height = 320;
        lcd_write_data(0xC8); // Portrait 180
    } else {
        _width = 320; _height = 240;
        lcd_write_data(0xA0); // Landscape 180
    }
}

void FNIRSI_DSO152::invertDisplay(bool i) {
    lcd_write_cmd(i ? 0x20 : 0x21); // ST7789 IPS panel uses 0x21 as normal
}

void FNIRSI_DSO152::drawPixel(int16_t x, int16_t y, uint16_t color) {
    if (x < 0 || x >= _width || y < 0 || y >= _height) return;
    lcd_set_window(x, y, x, y);
    lcd_write_data(color);
}

void FNIRSI_DSO152::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    if (x >= _width || y >= _height || w <= 0 || h <= 0) return;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > _width)  w = _width - x;
    if (y + h > _height) h = _height - y;
    if (w <= 0 || h <= 0) return;

    lcd_set_window(x, y, x + w - 1, y + h - 1);
    DSO_GPIOC_BSRR = (1UL << PIN_LCD_RS); // RS = 1 (Data)
    DSO_GPIOB_ODR  = color;
    uint32_t total = (uint32_t)w * (uint32_t)h;
    while (total--) {
        DSO_GPIOC_BRR  = (1UL << PIN_LCD_WR);
        __asm__ volatile("nop; nop;");
        DSO_GPIOC_BSRR = (1UL << PIN_LCD_WR);
    }
}

void FNIRSI_DSO152::drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) {
    fillRect(x, y, 1, h, color);
}

void FNIRSI_DSO152::drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) {
    fillRect(x, y, w, 1, color);
}

void FNIRSI_DSO152::fillScreen(uint16_t color) {
    fillRect(0, 0, _width, _height, color);
}

void FNIRSI_DSO152::drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    drawFastHLine(x, y, w, color);
    drawFastHLine(x, y + h - 1, w, color);
    drawFastVLine(x, y, h, color);
    drawFastVLine(x + w - 1, y, h, color);
}

void FNIRSI_DSO152::drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) {
    if (x0 == x1) {
        if (y0 > y1) { int16_t t = y0; y0 = y1; y1 = t; }
        drawFastVLine(x0, y0, y1 - y0 + 1, color);
        return;
    }
    if (y0 == y1) {
        if (x0 > x1) { int16_t t = x0; x0 = x1; x1 = t; }
        drawFastHLine(x0, y0, x1 - x0 + 1, color);
        return;
    }
    int16_t dx = (x1 > x0) ? (x1 - x0) : (x0 - x1);
    int16_t dy = (y1 > y0) ? (y1 - y0) : (y0 - y1);
    int16_t sx = (x0 < x1) ? 1 : -1;
    int16_t sy = (y0 < y1) ? 1 : -1;
    int16_t err = dx - dy;

    while (1) {
        drawPixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int16_t e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 < dx)  { err += dx; y0 += sy; }
    }
}

void FNIRSI_DSO152::drawCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color) {
    int16_t f = 1 - r;
    int16_t ddF_x = 1;
    int16_t ddF_y = -2 * r;
    int16_t x = 0;
    int16_t y = r;

    drawPixel(x0, y0 + r, color);
    drawPixel(x0, y0 - r, color);
    drawPixel(x0 + r, y0, color);
    drawPixel(x0 - r, y0, color);

    while (x < y) {
        if (f >= 0) { y--; ddF_y += 2; f += ddF_y; }
        x++; ddF_x += 2; f += ddF_x;
        drawPixel(x0 + x, y0 + y, color);
        drawPixel(x0 - x, y0 + y, color);
        drawPixel(x0 + x, y0 - y, color);
        drawPixel(x0 - x, y0 - y, color);
        drawPixel(x0 + y, y0 + x, color);
        drawPixel(x0 - y, y0 + x, color);
        drawPixel(x0 + y, y0 - x, color);
        drawPixel(x0 - y, y0 - x, color);
    }
}

void FNIRSI_DSO152::fillCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color) {
    drawFastVLine(x0, y0 - r, 2 * r + 1, color);
    int16_t f = 1 - r;
    int16_t ddF_x = 1;
    int16_t ddF_y = -2 * r;
    int16_t x = 0;
    int16_t y = r;

    while (x < y) {
        if (f >= 0) { y--; ddF_y += 2; f += ddF_y; }
        x++; ddF_x += 2; f += ddF_x;
        drawFastVLine(x0 + x, y0 - y, 2 * y + 1, color);
        drawFastVLine(x0 - x, y0 - y, 2 * y + 1, color);
        drawFastVLine(x0 + y, y0 - x, 2 * x + 1, color);
        drawFastVLine(x0 - y, y0 - x, 2 * x + 1, color);
    }
}

void FNIRSI_DSO152::drawCharFast(int16_t x, int16_t y, char c, uint16_t fg, uint16_t bg, uint8_t size) {
    if (c < 32 || c > 126) c = ' ';
    uint8_t idx = (uint8_t)(c - 32);

    if (size == 1 && fg != bg && x >= 0 && y >= 0 && (x + 8) <= _width && (y + 8) <= _height) {
        // Ultra-fast hardware windowed 8x8 character push (64 pixels in one transaction)
        lcd_set_window(x, y, x + 7, y + 7);
        DSO_GPIOC_BSRR = (1UL << PIN_LCD_RS);
        for (uint8_t row = 0; row < 8; row++) {
            uint8_t line = dso_font8x8[idx][row];
            for (uint8_t col = 0; col < 8; col++) {
                DSO_GPIOB_ODR = (line & (0x80U >> col)) ? fg : bg;
                DSO_GPIOC_BRR  = (1UL << PIN_LCD_WR);
                __asm__ volatile("nop");
                DSO_GPIOC_BSRR = (1UL << PIN_LCD_WR);
            }
        }
        return;
    }

    for (int8_t row = 0; row < 8; row++) {
        uint8_t line = dso_font8x8[idx][row];
        for (int8_t col = 0; col < 8; col++) {
            if (line & (0x80U >> col)) {
                if (size == 1) drawPixel(x + col, y + row, fg);
                else fillRect(x + col * size, y + row * size, size, size, fg);
            } else if (bg != fg) {
                if (size == 1) drawPixel(x + col, y + row, bg);
                else fillRect(x + col * size, y + row * size, size, size, bg);
            }
        }
    }
}

void FNIRSI_DSO152::drawString(int16_t x, int16_t y, const char *str, uint16_t fg, uint16_t bg, uint8_t size) {
    while (*str) {
        drawCharFast(x, y, *str++, fg, bg, size);
        x += 8 * size;
        if (x + 8 * size > _width) break;
    }
}

#ifndef DSO152_HAS_ADAFRUIT_GFX
size_t FNIRSI_DSO152::write(uint8_t c) {
    if (c == '\n') {
        _cursor_x = 0;
        _cursor_y += 8 * _text_size;
    } else if (c != '\r') {
        drawCharFast(_cursor_x, _cursor_y, (char)c, _text_fg, _text_bg, _text_size);
        _cursor_x += 8 * _text_size;
        if (_cursor_x + 8 * _text_size > _width) {
            _cursor_x = 0;
            _cursor_y += 8 * _text_size;
        }
    }
    return 1;
}
#endif

/* Global Pre-Instantiated Object */
FNIRSI_DSO152 dso;

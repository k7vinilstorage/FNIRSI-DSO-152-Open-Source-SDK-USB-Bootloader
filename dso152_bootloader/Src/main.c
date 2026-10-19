/*************************************************************************************
# Released under MIT License

Copyright (c) 2020 SF Yip (yipxxx@gmail.com)

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
**************************************************************************************/

/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2020 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under Ultimate Liberty license
  * SLA0044, the "License"; You may not use this file except in compliance with
  * the License. You may obtain a copy of the License at:
  *                             www.st.com/SLA0044
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "usb_device.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdint.h>
#include <stdbool.h>
#include "btldr_config.h"
#include "crypt.h"
#include "ihex_parser.h"
#include "crc.h"
#include "fat32.h"
#include "st7789_boot.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
#if (BTLDR_ACT_NoAppExist > 0u)
bool is_appcode_exist(void)
{
  uint32_t sp = *(volatile uint32_t*)APP_ADDR;
  uint32_t reset_vec = *(volatile uint32_t*)(APP_ADDR + 4u);

  // Stack pointer must be in SRAM (0x20000000..0x20005000)
  if ((sp & 0x2FFE0000) != 0x20000000) {
    return false;
  }
  // Reset vector must be Thumb (odd) and within app flash
  if ((reset_vec & 1) == 0 || reset_vec < APP_ADDR || reset_vec >= (APP_ADDR + APP_SIZE)) {
    return false;
  }
  return true;
}
#endif

#if (BTLDR_ACT_ButtonPress > 0u)
bool is_button_down(void)
{
    // PC4 (OK/Menu button, active-low)
    return !(GPIOC->IDR & (1 << 4));
}
#endif

#if (BTLDR_ACT_BootkeyDet > 0u)

#define btldr_act_req_key (*(volatile uint32_t*)0x20004FFC)

bool bootkey_detected(void){
    return(btldr_act_req_key == BOOTKEY);
}
#endif

extern PCD_HandleTypeDef hpcd_USB_FS;

void SystemReset(void){
    HAL_Delay(500);

    HAL_PCD_Stop(&hpcd_USB_FS);

    /* Pull USB D+ PIN to GND so USB Host detects device disconnect */
    LL_GPIO_SetPinMode(GPIOA,LL_GPIO_PIN_12, LL_GPIO_MODE_OUTPUT);
    LL_GPIO_SetPinSpeed(GPIOA, LL_GPIO_PIN_12, LL_GPIO_SPEED_FREQ_LOW);
    LL_GPIO_SetPinOutputType(GPIOA,LL_GPIO_PIN_12, LL_GPIO_OUTPUT_PUSHPULL);
    LL_GPIO_ResetOutputPin(GPIOA, LL_GPIO_PIN_12);

    HAL_Delay(1000);

    NVIC_SystemReset();
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */
  static uint32_t jump_addr;

  /* 1. CRITICAL: Latch Power ON immediately */
  RCC->APB2ENR |= RCC_APB2ENR_IOPCEN | RCC_APB2ENR_IOPAEN;
  // PC10 (Power Latch): Output push-pull 50MHz
  GPIOC->CRH &= ~(0xFu << ((10 - 8) * 4));
  GPIOC->CRH |= (0x3u << ((10 - 8) * 4));
  GPIOC->BSRR = (1 << 10); // PC10 = 1 (LATCH POWER ON)

  // PC4 (OK/Menu button): Input with pull-up
  GPIOC->CRL &= ~(0xFu << (4 * 4));
  GPIOC->CRL |= (0x8u << (4 * 4));
  GPIOC->BSRR = (1 << 4);

  // PC11 (Power button): Input with pull-up
  GPIOC->CRH &= ~(0xFu << ((11 - 8) * 4));
  GPIOC->CRH |= (0x8u << ((11 - 8) * 4));
  GPIOC->BSRR = (1 << 11);

  // Small delay for pins to settle
  for (volatile int d = 0; d < 5000; d++) __asm__ volatile("nop");

  /* Check whether to enter bootloader */
  bool need_bootloader = false;

#if (BTLDR_ACT_NoAppExist > 0u)
  if (!is_appcode_exist()) need_bootloader = true;
#endif
#if (BTLDR_ACT_ButtonPress > 0u)
  if (is_button_down()) need_bootloader = true;
#endif
#if (BTLDR_ACT_BootkeyDet > 0u)
  if (bootkey_detected()) need_bootloader = true;
#endif

  if (!need_bootloader)
  {
    /* Fast jump to application */
    jump_addr = *((__IO uint32_t*)(APP_ADDR + 4u));
    SCB->VTOR = APP_ADDR;
    __set_MSP((*(__IO uint32_t*)APP_ADDR));
    ((void (*)(void))jump_addr)();
    while (1);
  }

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* Configure the system clock */
  SystemClock_Config();

  /* Force USB host to see disconnect/reconnect */
  GPIOA->CRH &= ~(0xFu << ((12 - 8) * 4));
  GPIOA->CRH |= (0x3u << ((12 - 8) * 4));
  GPIOA->BRR = (1 << 12);
  HAL_Delay(50);
  GPIOA->CRH &= ~(0xFu << ((12 - 8) * 4));
  GPIOA->CRH |= (0x4u << ((12 - 8) * 4));

  /* Initialize ST7789 display and show bootloader screen */
  st7789_boot_init();

  /* Start USB Mass Storage Device */
  MX_USB_DEVICE_Init();

  uint32_t pwr_press_start = 0;

  while (1)
  {
    if (fat32_is_flashed())
    {
#if (BTLDR_ACT_BootkeyDet > 0u)
      btldr_act_req_key = 0;
#endif
      st7789_boot_show_success();
      SystemReset();
    }

    // Power button check (PC11)
    if (!(GPIOC->IDR & (1 << 11)))
    {
      if (pwr_press_start == 0)
      {
        pwr_press_start = HAL_GetTick();
      }
      else if (HAL_GetTick() - pwr_press_start > 800)
      {
        GPIOC->BRR = (1 << 10); // Power off device!
        while (1);
      }
    }
    else
    {
      pwr_press_start = 0;
    }
  }

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  LL_FLASH_SetLatency(LL_FLASH_LATENCY_2);
  while(LL_FLASH_GetLatency()!= LL_FLASH_LATENCY_2)
  {
  }
  LL_RCC_HSE_Enable();

   /* Wait till HSE is ready */
  while(LL_RCC_HSE_IsReady() != 1)
  {

  }
  LL_RCC_PLL_ConfigDomain_SYS(LL_RCC_PLLSOURCE_HSE_DIV_1, LL_RCC_PLL_MUL_9);
  LL_RCC_PLL_Enable();

   /* Wait till PLL is ready */
  while(LL_RCC_PLL_IsReady() != 1)
  {

  }
  LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_1);
  LL_RCC_SetAPB1Prescaler(LL_RCC_APB1_DIV_2);
  LL_RCC_SetAPB2Prescaler(LL_RCC_APB2_DIV_2);
  LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_PLL);

   /* Wait till System clock is ready */
  while(LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_PLL)
  {

  }
  LL_SetSystemCoreClock(72000000);

   /* Update the time base */
  if (HAL_InitTick (TICK_INT_PRIORITY) != HAL_OK)
  {
    Error_Handler();
  }
  LL_RCC_SetUSBClockSource(LL_RCC_USB_CLKSOURCE_PLL_DIV_1_5);
}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  LL_GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_GPIOA);

  /**/
  GPIO_InitStruct.Pin = BTLDR_EN_Pin;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_UP;
  LL_GPIO_Init(BTLDR_EN_GPIO_Port, &GPIO_InitStruct);

}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */

  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     tex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

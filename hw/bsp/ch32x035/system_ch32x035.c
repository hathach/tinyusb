/********************************** (C) COPYRIGHT *******************************
 * File Name          : system_ch32x035.c
 * Author             : WCH
 * Version            : V1.0.0
 * Date               : 2023/04/06
 * Description        : CH32X035 Device Peripheral Access Layer System Source File.
 *********************************************************************************
 * Copyright (c) 2021 Nanjing Qinheng Microelectronics Co., Ltd.
 * Attention: This software (modified or not) and binary are used for
 * microcontroller manufactured by Nanjing Qinheng Microelectronics.
 *******************************************************************************/
#include "ch32x035.h"

#if defined SYSCLK_FREQ_8MHz_HSI
uint32_t SystemCoreClock = SYSCLK_FREQ_8MHz_HSI;
#elif defined SYSCLK_FREQ_12MHz_HSI
uint32_t SystemCoreClock = SYSCLK_FREQ_12MHz_HSI;
#elif defined SYSCLK_FREQ_16MHz_HSI
uint32_t SystemCoreClock = SYSCLK_FREQ_16MHz_HSI;
#elif defined SYSCLK_FREQ_24MHz_HSI
uint32_t SystemCoreClock = SYSCLK_FREQ_24MHz_HSI;
#else
uint32_t SystemCoreClock = HSI_VALUE;
#endif

__I uint8_t AHBPrescTable[16] = {1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8};

static void SetSysClock(void);

void SystemInit(void) {
  RCC->CTLR |= (uint32_t)0x00000001;
  RCC->CFGR0 |= (uint32_t)0x00000050;
  RCC->CFGR0 &= (uint32_t)0xF8FFFF5F;
  SetSysClock();
}

void SystemCoreClockUpdate(void) {
  uint32_t tmp;

  SystemCoreClock = HSI_VALUE;
  tmp             = AHBPrescTable[((RCC->CFGR0 & RCC_HPRE) >> 4)];

  if (((RCC->CFGR0 & RCC_HPRE) >> 4) < 8) {
    SystemCoreClock /= tmp;
  } else {
    SystemCoreClock >>= tmp;
  }
}

static void SetSysClock(void) {
  GPIO_IPD_Unused();

  FLASH->ACTLR &= (uint32_t)((uint32_t)~FLASH_ACTLR_LATENCY);
  FLASH->ACTLR |= (uint32_t)FLASH_ACTLR_LATENCY_2;

  RCC->CFGR0 &= (uint32_t)0xFFFFFF0F;

#if defined SYSCLK_FREQ_8MHz_HSI
  RCC->CFGR0 |= (uint32_t)RCC_HPRE_DIV6;
  FLASH->ACTLR &= (uint32_t)((uint32_t)~FLASH_ACTLR_LATENCY);
  FLASH->ACTLR |= (uint32_t)FLASH_ACTLR_LATENCY_0;
#elif defined SYSCLK_FREQ_12MHz_HSI
  RCC->CFGR0 |= (uint32_t)RCC_HPRE_DIV4;
  FLASH->ACTLR &= (uint32_t)((uint32_t)~FLASH_ACTLR_LATENCY);
  FLASH->ACTLR |= (uint32_t)FLASH_ACTLR_LATENCY_0;
#elif defined SYSCLK_FREQ_16MHz_HSI
  RCC->CFGR0 |= (uint32_t)RCC_HPRE_DIV3;
  FLASH->ACTLR &= (uint32_t)((uint32_t)~FLASH_ACTLR_LATENCY);
  FLASH->ACTLR |= (uint32_t)FLASH_ACTLR_LATENCY_1;
#elif defined SYSCLK_FREQ_24MHz_HSI
  RCC->CFGR0 |= (uint32_t)RCC_HPRE_DIV2;
  FLASH->ACTLR = (uint32_t)FLASH_ACTLR_LATENCY_1;
#else
  RCC->CFGR0 |= (uint32_t)RCC_HPRE_DIV1;
#endif
}

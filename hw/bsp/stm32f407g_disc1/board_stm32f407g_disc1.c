/**************************************************************************/
/*!
 @file    board_metro_m4_express.c
 @author  hathach (tinyusb.org)

 @section LICENSE

 Software License Agreement (BSD License)

 Copyright (c) 2018, hathach (tinyusb.org)
 All rights reserved.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:
 1. Redistributions of source code must retain the above copyright
 notice, this list of conditions and the following disclaimer.
 2. Redistributions in binary form must reproduce the above copyright
 notice, this list of conditions and the following disclaimer in the
 documentation and/or other materials provided with the distribution.
 3. Neither the name of the copyright holders nor the
 names of its contributors may be used to endorse or promote products
 derived from this software without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ''AS IS'' AND ANY
 EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE FOR ANY
 DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

 This file is part of the tinyusb stack.
*/
/**************************************************************************/

#include "bsp/board.h"

#include "stm32f4xx.h"

#include "tusb_option.h"

//--------------------------------------------------------------------+
// MACRO TYPEDEF CONSTANT ENUM DECLARATION
//--------------------------------------------------------------------+
#define LED_STATE_ON  1

void board_init(void)
{
  // Init the LED on PD14
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIODEN;
  GPIOD->MODER |= GPIO_MODER_MODE14_0;

  // USB Clock init
  // PLL input- 8 MHz (External oscillator clock; HSI clock tolerance isn't
  // tight enough- 1%, need 0.25%)
  // VCO input- 1 to 2 MHz (2 MHz, M = 4)
  // VCO output- 100 to 432 MHz (144 MHz, N = 72)
  // Main PLL out- <= 180 MHz (18 MHz, P = 3- divides by 8)
  // USB PLL out- 48 MHz (Q = 3)
  RCC->PLLCFGR = RCC_PLLCFGR_PLLSRC_HSE | (3 << RCC_PLLCFGR_PLLQ_Pos) | \
    (3 << RCC_PLLCFGR_PLLP_Pos) | (72 << RCC_PLLCFGR_PLLN_Pos) | \
    (4 << RCC_PLLCFGR_PLLM_Pos);

  // Wait for external clock to become ready
  RCC->CR |= RCC_CR_HSEON;
  while(!(RCC->CR & RCC_CR_HSERDY_Msk));

  // Wait for PLL to become ready
  RCC->CR |= RCC_CR_PLLON;
  while(!(RCC->CR & RCC_CR_PLLRDY_Msk));

  // Switch clocks!
  RCC->CFGR |= RCC_CFGR_SW_1;

  // Notify runtime of frequency change.
  SystemCoreClockUpdate();
  // Systick init
  #if CFG_TUSB_OS  == OPT_OS_NONE
    SysTick_Config(SystemCoreClock / 1000);
  #endif

  RCC->AHB2ENR |= RCC_AHB2ENR_OTGFSEN;

  // USB Pin Init
  // PA9- VUSB, PA10- ID, PA11- DM, PA12- DP
  // PC0- Power on
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
  GPIOA->MODER |= GPIO_MODER_MODE9_1 | GPIO_MODER_MODE10_1 | \
    GPIO_MODER_MODE11_1 | GPIO_MODER_MODE12_1;
  GPIOA->AFR[1] |= (10 << GPIO_AFRH_AFSEL9_Pos) | \
    (10 << GPIO_AFRH_AFSEL10_Pos) | (10 << GPIO_AFRH_AFSEL11_Pos) | \
    (10 << GPIO_AFRH_AFSEL12_Pos);

  // Pullup required on ID, despite the manual claiming there's an
  // internal pullup already (page 1245, Rev 17)
  GPIOA->PUPDR |= GPIO_PUPDR_PUPD10_0;
}


void board_led_control(bool state)
{
  if (!state) {
    GPIOD->BSRR = GPIO_BSRR_BR14;
  } else {
    GPIOD->BSRR = GPIO_BSRR_BS14;
  }
}


/*------------------------------------------------------------------*/
/* TUSB HAL MILLISECOND
 *------------------------------------------------------------------*/
#if CFG_TUSB_OS  == OPT_OS_NONE
volatile uint32_t system_ticks = 0;

void SysTick_Handler (void)
{
  system_ticks++;
}

uint32_t tusb_hal_millis(void)
{
  return board_tick2ms(system_ticks);
}
#endif

void HardFault_Handler (void)
{
  asm("bkpt");
}

// Required by __libc_init_array in startup code if we are compiling using
// -nostdlib/-nostartfiles.
void _init(void)
{

}

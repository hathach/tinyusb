/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2022 Greg Davill
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * This file is part of the TinyUSB stack.
 */

#include "stdio.h"
#include "debug_uart.h"
#include "ch32v30x.h"

#include "bsp/board_api.h"
#include "board.h"

//--------------------------------------------------------------------+
// Forward USB interrupt events to TinyUSB IRQ Handler
//--------------------------------------------------------------------+

void USBHS_IRQHandler (void) __attribute__((naked));
void USBHS_IRQHandler (void)
{
  __asm volatile ("call USBHS_IRQHandler_impl; mret");
}

__attribute__ ((used)) void USBHS_IRQHandler_impl (void)
{
  tud_int_handler(0);
}

//--------------------------------------------------------------------+
// MACRO TYPEDEF CONSTANT ENUM
//--------------------------------------------------------------------+

uint32_t SysTick_Config(uint32_t ticks)
{
  NVIC_EnableIRQ(SysTicK_IRQn);
  SysTick->CTLR=0;
  SysTick->SR=0;
  SysTick->CNT=0;
  SysTick->CMP=ticks-1;
  SysTick->CTLR=0xF;
  return 0;
}

void board_init(void) {

  /* Disable interrupts during init */
  __disable_irq();

#if CFG_TUSB_OS == OPT_OS_NONE
  SysTick_Config(SystemCoreClock / 1000);
#endif

	usart_printf_init(115200);

  RCC_USBCLK48MConfig(RCC_USBCLK48MCLKSource_USBPHY);
  RCC_USBHSPLLCLKConfig(RCC_HSBHSPLLCLKSource_HSE);
  RCC_USBHSConfig(RCC_USBPLL_Div2);
  RCC_USBHSPLLCKREFCLKConfig(RCC_USBHSPLLCKREFCLK_4M);
  RCC_USBHSPHYPLLALIVEcmd(ENABLE);
  RCC_AHBPeriphClockCmd(RCC_AHBPeriph_USBHS, ENABLE);

  GPIO_InitTypeDef GPIO_InitStructure = {0};

  // LED
  LED_CLOCK_EN();
  GPIO_InitStructure.GPIO_Pin = LED_PIN;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_Init(LED_PORT, &GPIO_InitStructure);

  // Button
  BUTTON_CLOCK_EN();
  GPIO_InitStructure.GPIO_Pin = BUTTON_PIN;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_Init(BUTTON_PORT, &GPIO_InitStructure);

  /* Enable interrupts globally */
  __enable_irq();

  board_delay(2);
}

#if CFG_TUSB_OS == OPT_OS_NONE

volatile uint32_t system_ticks = 0;

/* Small workaround to support HW stack save/restore */
void SysTick_Handler (void) __attribute__((naked));
void SysTick_Handler (void)
{
  __asm volatile ("call SysTick_Handler_impl; mret");
}

__attribute__((used)) void SysTick_Handler_impl (void)
{
  SysTick->SR = 0;
  system_ticks++;
}

uint32_t board_millis (void)
{
  return system_ticks;
}

#endif

//--------------------------------------------------------------------+
// Board porting API
//--------------------------------------------------------------------+

void board_led_write (bool state)
{
  GPIO_WriteBit(LED_PORT, LED_PIN, state);
}

uint32_t board_button_read (void)
{
  return BUTTON_STATE_ACTIVE == GPIO_ReadInputDataBit(BUTTON_PORT, BUTTON_PIN);
}

int board_uart_read (uint8_t *buf, int len)
{
  (void) buf;
  (void) len;
  return 0;
}

int board_uart_write (void const *buf, int len)
{
  int txsize = len;
  while ( txsize-- )
  {
    uart_write(*(uint8_t const*) buf);
    buf++;
  }
  return len;
}



#ifdef USE_FULL_ASSERT
/**
 * @brief  Reports the name of the source file and the source line number
 *         where the assert_param error has occurred.
 * @param  file: pointer to the source file name
 * @param  line: assert_param error line source number
 * @retval None
 */
void assert_failed(char* file, uint32_t line) {
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line
     number,
     tex: printf("Wrong parameters value: file %s on line %d\r\n", file, line)
   */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

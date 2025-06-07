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

/* metadata:
   manufacturer: WCH
*/

#include "stdio.h"

// https://github.com/openwch/ch32x035/pull/90  // TODO: Update this link if necessary
// https://github.com/openwch/ch32v20x/pull/12 // TODO: Update this link if necessary, or remove if not relevant

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-prototypes"
#endif

#include "debug_uart.h" // Assuming this is still relevant for ch32x035
#include "ch32x035.h"   // Replaced ch32v30x.h

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#include "bsp/board_api.h"
#include "board.h"

//--------------------------------------------------------------------+
// Forward USB interrupt events to TinyUSB IRQ Handler
//--------------------------------------------------------------------+

// TODO maybe having FS as port0, HS as port1 // This comment is kept as it might still be relevant

__attribute__((interrupt)) void USBHS_IRQHandler(void) { // Name kept, specific to WCH USB IP
  #if CFG_TUD_WCH_USBIP_USBHS
  tud_int_handler(0);
  #endif
}

__attribute__((interrupt)) void OTG_FS_IRQHandler(void) { // Name kept, specific to WCH USB IP
  #if CFG_TUD_WCH_USBIP_USBFS
  tud_int_handler(0);
  #endif
}

//--------------------------------------------------------------------+
// MACRO TYPEDEF CONSTANT ENUM
//--------------------------------------------------------------------+

uint32_t SysTick_Config(uint32_t ticks) {
  NVIC_EnableIRQ(SysTicK_IRQn); // Generic CMSIS/RISC-V define
  SysTick->CTLR = 0;
  SysTick->SR = 0;
  SysTick->CNT = 0;
  SysTick->CMP = ticks - 1;
  SysTick->CTLR = 0xF;
  return 0;
}

void board_init(void) {

  /* Disable interrupts during init */
  __disable_irq();

#if CFG_TUSB_OS == OPT_OS_NONE
  SysTick_Config(SystemCoreClock / 1000);
#endif

  usart_printf_init(CFG_BOARD_UART_BAUDRATE);

#ifdef CH32X035_D8C // Replaced CH32V30x_D8C - Check if this define is appropriate for ch32x035
  // ch32x035: Highspeed USB (If applicable, verify specifics for ch32x035)
  // The following RCC configurations are specific to CH32V30x's USBHS.
  // These will likely need to be adapted or removed for CH32X035 based on its datasheet.
  // For now, they are commented out or left as placeholders.
  // RCC_USBCLK48MConfig(RCC_USBCLK48MCLKSource_USBPHY);
  // RCC_USBHSPLLCLKConfig(RCC_HSBHSPLLCLKSource_HSE);
  // RCC_USBHSConfig(RCC_USBPLL_Div2);
  // RCC_USBHSPLLCKREFCLKConfig(RCC_USBHSPLLCKREFCLK_4M);
  // RCC_USBHSPHYPLLALIVEcmd(ENABLE);
  // RCC_AHBPeriphClockCmd(RCC_AHBPeriph_USBHS, ENABLE);
  #warning "USB Highspeed peripheral configuration for CH32X035 needs verification."
#endif

  // Fullspeed USB
  // Verify SystemCoreClock and corresponding RCC_OTGFSCLKSource divisors for CH32X035
  uint8_t otg_div;
  switch (SystemCoreClock) {
    case 48000000:  otg_div = RCC_OTGFSCLKSource_PLLCLK_Div1; break;
    case 96000000:  otg_div = RCC_OTGFSCLKSource_PLLCLK_Div2; break;
    case 144000000: otg_div = RCC_OTGFSCLKSource_PLLCLK_Div3; break; // Check max clock for ch32x035
    default: TU_ASSERT(0,); break; // Ensure SystemCoreClock is valid for ch32x035
  }
  RCC_OTGFSCLKConfig(otg_div); // Verify this function and its parameters for ch32x035
  RCC_AHBPeriphClockCmd(RCC_AHBPeriph_OTG_FS, ENABLE); // Verify this for ch32x035

  GPIO_InitTypeDef GPIO_InitStructure = {0};

  // LED
  // Ensure LED_CLOCK_EN, LED_PIN, LED_PORT are correctly defined for the ch32x035 board
  LED_CLOCK_EN();
  GPIO_InitStructure.GPIO_Pin = LED_PIN;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_Init(LED_PORT, &GPIO_InitStructure);

  // Button
  // Ensure BUTTON_CLOCK_EN, BUTTON_PIN, BUTTON_PORT are correctly defined for the ch32x035 board
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

__attribute__((interrupt)) void SysTick_Handler(void) { // Generic SysTick Handler
  SysTick->SR = 0;
  system_ticks++;
}

uint32_t board_millis(void) {
  return system_ticks;
}

#endif

//--------------------------------------------------------------------+
// Board porting API
//--------------------------------------------------------------------+

void board_led_write(bool state) {
  // Ensure LED_PORT, LED_PIN are correct for ch32x035
  GPIO_WriteBit(LED_PORT, LED_PIN, state);
}

uint32_t board_button_read(void) {
  // Ensure BUTTON_PORT, BUTTON_PIN, BUTTON_STATE_ACTIVE are correct for ch32x035
  return BUTTON_STATE_ACTIVE == GPIO_ReadInputDataBit(BUTTON_PORT, BUTTON_PIN);
}

int board_uart_read(uint8_t* buf, int len) {
  (void) buf;
  (void) len;
  // TODO: Implement UART read for ch32x035 if necessary
  return 0;
}

int board_uart_write(void const* buf, int len) {
  int txsize = len;
  const char* bufc = (const char*) buf;
  while (txsize--) {
    uart_write(*bufc++); // Ensure uart_write is compatible with ch32x035
  }
  uart_sync(); // Ensure uart_sync is compatible with ch32x035
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

/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2024 TinyUSB contributors
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

// WCH SDK's DEBUG macro enables a _write() that conflicts with TinyUSB's.
// TinyUSB uses UART1 for printf via debug_uart.c, so DEBUG is not needed.
// If you need a different UART or want to keep SDK's DEBUG, modify
// debug_uart.c (TinyUSB side) or CH58x_sys.c (SDK side) to remove one _write().
// If done, remove this #error to continue.
#ifdef DEBUG
  #error "Remove the DEBUG macro from preprocessor defines to avoid "  \
         "duplicate _write() between WCH SDK and TinyUSB. "            \
         "TinyUSB uses UART1 by default (see debug_uart.c)."
#endif

#include "debug_uart.h"
#include "CH58x_common.h"
#include "ch58x_it.h"

#include "bsp/board_api.h"
#include "board.h"

//--------------------------------------------------------------------+
// Forward USB interrupt events to TinyUSB IRQ Handler
//--------------------------------------------------------------------+

__INTERRUPT __HIGH_CODE void USB_IRQHandler(void) {
  tusb_int_handler(0, true);
}

__INTERRUPT __HIGH_CODE void USB2_IRQHandler(void) {
  tusb_int_handler(1, true);
}

//--------------------------------------------------------------------+
// SysTick
//--------------------------------------------------------------------+

#if CFG_TUSB_OS == OPT_OS_NONE
volatile uint32_t system_ticks = 0;

__INTERRUPT __HIGH_CODE void SysTick_Handler(void) {
  SysTick->SR = 0;
  system_ticks++;
}

uint32_t board_millis(void) {
  return system_ticks;
}
#endif

//--------------------------------------------------------------------+
// Board Init
//--------------------------------------------------------------------+

void board_init(void) {
  // Disable interrupts during init
  PFIC_DisableAllIRQ();

  // Set system clock to PLL 60MHz (default for CH582)
  SetSysClock(CLK_SOURCE_PLL_60MHz);

#if CFG_TUSB_OS == OPT_OS_NONE
  SysTick_Config(GetSysClock() / 1000);
#endif

  // UART1 init for debug output
#ifdef CFG_BOARD_UART_BAUDRATE
  usart_printf_init(CFG_BOARD_UART_BAUDRATE);
#endif

  // LED
#ifdef LED_PORT_IS_A
  GPIOA_ModeCfg(LED_PIN, GPIO_ModeOut_PP_5mA);
#else
  GPIOB_ModeCfg(LED_PIN, GPIO_ModeOut_PP_5mA);
#endif

  // Button
#ifdef BUTTON_PIN
  #ifdef BUTTON_PORT_IS_A
    GPIOA_ModeCfg(BUTTON_PIN, GPIO_ModeIN_PU);
  #else
    GPIOB_ModeCfg(BUTTON_PIN, GPIO_ModeIN_PU);
  #endif
#endif

  // USB pin enable: enable analog function for USB1 and USB2 D+/D-
  R16_PIN_ANALOG_IE |= RB_PIN_USB_IE | RB_PIN_USB2_IE;

  // D+ pull-up is only needed for the device-role port
#if CFG_TUD_ENABLED
  #if BOARD_TUD_RHPORT == 0
    R16_PIN_ANALOG_IE |= RB_PIN_USB_DP_PU;
  #else
    R16_PIN_ANALOG_IE |= RB_PIN_USB2_DP_PU;
  #endif
#endif

  // Keep USB clock active during sleep
  R8_SLP_CLK_OFF1 &= ~RB_SLP_CLK_USB;

  // Enable interrupts globally
  PFIC_EnableAllIRQ();

  board_delay(2);
}

//--------------------------------------------------------------------+
// Board porting API
//--------------------------------------------------------------------+

void board_led_write(bool state) {
#ifdef LED_PORT_IS_A
  if (state ^ LED_STATE_ON) {
    GPIOA_ResetBits(LED_PIN);
  } else {
    GPIOA_SetBits(LED_PIN);
  }
#else
  if (state ^ LED_STATE_ON) {
    GPIOB_ResetBits(LED_PIN);
  } else {
    GPIOB_SetBits(LED_PIN);
  }
#endif
}

uint32_t board_button_read(void) {
#ifdef BUTTON_PIN
  #ifdef BUTTON_PORT_IS_A
    return BUTTON_STATE_ACTIVE == (GPIOA_ReadPortPin(BUTTON_PIN) ? 1 : 0);
  #else
    return BUTTON_STATE_ACTIVE == (GPIOB_ReadPortPin(BUTTON_PIN) ? 1 : 0);
  #endif
#else
  return 0;
#endif
}

int board_uart_read(uint8_t* buf, int len) {
  (void) buf;
  (void) len;
  return 0;
}

int board_uart_write(void const* buf, int len) {
  int txsize = len;
  const char* bufc = (const char*) buf;
  while (txsize--) {
    uart_write(*bufc++);
  }
  uart_sync();
  return len;
}

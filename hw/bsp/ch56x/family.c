/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2026 Ha Thach (tinyusb.org)
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

#include "debug_uart.h"
#include "CH56x_common.h"

#include "bsp/board_api.h"
#include "board.h"

//--------------------------------------------------------------------+
// Forward USB interrupt events to TinyUSB IRQ Handler
//--------------------------------------------------------------------+

// Prototypes for the (weak) vector-table handlers overridden here
void USBSS_IRQHandler(void);
void LINK_IRQHandler(void);
void USBHS_IRQHandler(void);
void SysTick_Handler(void);

// Single rhport 0: SPEED=super compiles the USB3 dcd (USBSS + LINK vectors); SPEED=high the
// USB2 dcd (USBHS vector); the active dcd's dcd_int_handler dispatches on its own flag
// registers, so all three vectors share ONE forwarder. Aliases (not three identical
// functions) are required: gcc's identical-code-folding would otherwise rewrite one interrupt
// handler as a plain call into another, whose mret then skips the caller's epilogue and leaks
// its stack frame on every interrupt. Use the standard riscv interrupt attribute - WCH's
// "WCH-Interrupt-fast" is only understood by WCH's own gcc fork.
__attribute__((interrupt)) void USBSS_IRQHandler(void) {
  tusb_int_handler(0, true);
}

void LINK_IRQHandler(void) __attribute__((alias("USBSS_IRQHandler")));
void USBHS_IRQHandler(void) __attribute__((alias("USBSS_IRQHandler")));

#if CFG_TUD_WCH_USB30_FALLBACK
// the USB3 dcd uses TMR0 as the SuperSpeed-training timeout for the USB2 fallback
void TMR0_IRQHandler(void);
void TMR0_IRQHandler(void) __attribute__((alias("USBSS_IRQHandler")));
#endif

//--------------------------------------------------------------------+
// SysTick
//--------------------------------------------------------------------+

#if CFG_TUSB_OS == OPT_OS_NONE
volatile uint32_t system_ticks = 0;

__attribute__((interrupt)) void SysTick_Handler(void) {
  SysTick->CNTFG = 0; // clear count-reached flag
  system_ticks++;
}

uint32_t tusb_time_millis_api(void) {
  return system_ticks;
}
#endif

//--------------------------------------------------------------------+
// Board Init
//--------------------------------------------------------------------+

void board_init(void) {
  // 120 MHz system clock: required for USB3 operation (160 MHz does not work with USB3).
  // SystemInit() takes the frequency in Hz (FREQ_SYS is set by the build)
  SystemInit(FREQ_SYS);

#if CFG_TUSB_OS == OPT_OS_NONE
  SysTick_Config(GetSysClock() / 1000);
#endif

  // UART1 (TXD1 = PA8, RXD1 = PA7) for debug output
#ifdef CFG_BOARD_UART_BAUDRATE
  usart_printf_init(CFG_BOARD_UART_BAUDRATE);
#endif

  // LED
  GPIOB_ResetBits(LED_PIN);
  GPIOB_ModeCfg(LED_PIN, GPIO_Highspeed_PP_8mA);

  // Button: internal pull to the inactive level for a defined idle state
#ifdef BUTTON_PIN
  GPIOB_ModeCfg(BUTTON_PIN, BUTTON_STATE_ACTIVE ? GPIO_ModeIN_PD_NSMT : GPIO_ModeIN_PU_NSMT);
#endif
}

//--------------------------------------------------------------------+
// Board porting API
//--------------------------------------------------------------------+

void board_led_write(bool state) {
  if (state ^ LED_STATE_ON) {
    GPIOB_ResetBits(LED_PIN);
  } else {
    GPIOB_SetBits(LED_PIN);
  }
}

uint32_t board_button_read(void) {
#ifdef BUTTON_PIN
  return BUTTON_STATE_ACTIVE == (GPIOB_ReadPortPin(BUTTON_PIN) ? 1 : 0);
#else
  return 0;
#endif
}

size_t board_get_unique_id(uint8_t id[], size_t max_len) {
  // 64-bit factory unique ID (+ checksum) in the read-only info flash (CH569 datasheet 2.2.3)
  TU_ATTR_ALIGNED(4) uint8_t uid[8];
  FLASH_ROMA_READ(0x77FE4, (puint32_t)(uintptr_t)uid, 8);
  const size_t len = TU_MIN(max_len, (size_t)8);
  memcpy(id, uid, len);
  return len;
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

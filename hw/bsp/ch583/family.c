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
#include "ch583_it.h"

#include "bsp/board_api.h"
#include "board.h"

//--------------------------------------------------------------------+
// Forward USB interrupt events to TinyUSB IRQ Handler
//--------------------------------------------------------------------+

// Device only: the shared dcd_ch32_usbfs.c drives USB0 (rhport 0). CH58x's second controller
// (USB2 / rhport 1) was driven by the now-removed ch58x host driver; its handler is kept
// commented out below to ease re-adding host support.
__INTERRUPT __HIGH_CODE void USB_IRQHandler(void) {
  tusb_int_handler(0, true);
}

// __INTERRUPT __HIGH_CODE void USB2_IRQHandler(void) {
//   tusb_int_handler(1, true);
// }

//--------------------------------------------------------------------+
// SysTick
//--------------------------------------------------------------------+

#if CFG_TUSB_OS == OPT_OS_NONE
volatile uint32_t system_ticks = 0;

__INTERRUPT __HIGH_CODE void SysTick_Handler(void) {
  SysTick->SR = 0;
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

  // Device only on USB0 (rhport 0): enable analog function for USB1 D+/D- and assert its D+
  // pull-up. The shared dcd_ch32_usbfs.c drives USB0; CH58x host / USB2 (rhport 1) is unsupported
  // here — to re-add host, also OR in RB_PIN_USB2_IE below.
  R16_PIN_ANALOG_IE |= RB_PIN_USB_IE; // | RB_PIN_USB2_IE  (re-add for host on USB2)
#if CFG_TUD_ENABLED
  R16_PIN_ANALOG_IE |= RB_PIN_USB_DP_PU;
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

// CH58x has no memory-mapped unique-ID register (unlike ch32v10x/v20x at 0x1FFFF7E8), but the
// factory programs a unique 6-byte MAC address into FlashROM (it is a BLE part). GetMACAddress()
// reads it via the ISP ROM command FLASH_EEPROM_CMD, which is provided by libISP583.a.
size_t board_get_unique_id(uint8_t id[], size_t max_len) {
  // FLASH_EEPROM_CMD writes word-granular and requires a 4-byte-aligned buffer (CH58x_flash.c);
  // size 8 matches the SDK's GET_UNIQUE_ID buffer (6 MAC bytes + 2 it pads), so the read can't
  // run past the end whether it returns 6 or a full 8.
  TU_ATTR_ALIGNED(4) uint8_t mac[8];
  GetMACAddress(mac);
  size_t len = TU_MIN(max_len, (size_t) 6); // the 6-byte MAC is the unique part
  memcpy(id, mac, len);
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

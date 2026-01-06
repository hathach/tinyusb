/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2018, hathach (tinyusb.org)
 * Copyright (c) 2025, Gabriel Koppenstein
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
   manufacturer: NXP
*/

#include "bsp/board_api.h"
#include "fsl_device_registers.h"
#include "fsl_gpio.h"
#include "board.h"
#include "fsl_usart.h"
#include "fsl_clock.h"

#include "pin_mux.h"
#include "clock_config.h"

//--------------------------------------------------------------------+
// Forward USB interrupt events to TinyUSB IRQ Handler
//--------------------------------------------------------------------+

void USB_IRQHandler(void) {
  tusb_int_handler(1, true);
}

void board_init(void) {

  // Init button pin, LED pins, SWD pins & UART pins
  BOARD_InitBootPins();

  // Init Clocks
  BOARD_InitBootClocks();

#if CFG_TUSB_OS == OPT_OS_NONE
  // 1ms tick timer
  SysTick_Config(SystemCoreClock / 1000);
#elif CFG_TUSB_OS == OPT_OS_FREERTOS
  // Explicitly disable systick to prevent its ISR from running before scheduler start
  SysTick->CTRL &= ~1U;
  // If freeRTOS is used, IRQ priority is limit by max syscall ( smaller is higher )
  NVIC_SetPriority(USB_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);
#endif

#ifdef NEOPIXEL_PIN
  // No neo pixel support yet
#endif

#ifdef UART_DEV
  // Enable UART when debug log is on
  board_uart_init_clock();
  usart_config_t uart_config;
  USART_GetDefaultConfig(&uart_config);
  uart_config.baudRate_Bps = CFG_BOARD_UART_BAUDRATE;
  uart_config.enableRx = true;
  uart_config.enableTx = true;
  USART_Init(UART_DEV, &uart_config, CLOCK_GetFlexCommClkFreq(LP_FLEXCOMM_INST));
#endif

  // USB Initialization
  // Reset USB
  RESET_PeripheralReset(kUSB_RST_SHIFT_RSTn);

  // Enable USB Clock
  CLOCK_EnableClock(kCLOCK_Usb);

  // Enable USB PHY
  CLOCK_EnableUsbhsPhyClock();
}

//--------------------------------------------------------------------+
// Board porting API
//--------------------------------------------------------------------+

void board_led_write(bool state) {
  GPIO_PinWrite(LED_GPIO, LED_PORT, LED_PIN, state ? LED_STATE_ON : (1 - LED_STATE_ON));
}

uint32_t board_button_read(void) {
#ifdef BUTTON_GPIO
  return BUTTON_STATE_ACTIVE == GPIO_PinRead(BUTTON_GPIO, BOARD_INITPINS_WAKEUP_BTN_PORT, BUTTON_PIN);
#endif
}

int board_uart_read(uint8_t* buf, int len) {
  (void) buf;
  (void) len;
  return 0;
}

int board_uart_write(void const* buf, int len) {
#ifdef UART_DEV
  USART_WriteBlocking(UART_DEV, (uint8_t const*) buf, len);
  return len;
#else
  (void) buf; (void) len;
  return 0;
#endif
}

#if CFG_TUSB_OS == OPT_OS_NONE
volatile uint32_t system_ticks = 0;

void SysTick_Handler(void) {
  system_ticks++;
}

uint32_t board_millis(void) {
  return system_ticks;
}

#endif

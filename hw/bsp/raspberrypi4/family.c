/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
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

#include "bsp/board.h"
#include "board.h"

#include "broadcom/interrupts.h"
#include "broadcom/io.h"
#include "broadcom/mmu.h"
#include "broadcom/caches.h"
#include "broadcom/vcmailbox.h"

// LED
#define LED_PIN               18
#define LED_STATE_ON          1

// Button
#define BUTTON_PIN            16
#define BUTTON_STATE_ACTIVE   0

//--------------------------------------------------------------------+
// Forward USB interrupt events to TinyUSB IRQ Handler
//--------------------------------------------------------------------+
void USB_IRQHandler(void)
{
  tud_int_handler(0);
}

//--------------------------------------------------------------------+
// MACRO TYPEDEF CONSTANT ENUM
//--------------------------------------------------------------------+

//--------------------------------------------------------------------+
// Board porting API
//--------------------------------------------------------------------+
void board_init(void)
{
  setup_mmu_flat_map();
  init_caches();

  // LED
  gpio_initOutputPinWithPullNone(LED_PIN);
  board_led_write(true);

  // Button
  // TODO

  // Uart
  uart_init();

  // Turn on USB peripheral.
  vcmailbox_set_power_state(VCMAILBOX_DEVICE_USB_HCD, true);

  // Timer 1/1024 second tick
  SYSTMR->CS_b.M1 = 1;
  SYSTMR->C1 = SYSTMR->CLO + 977;
  BP_EnableIRQ(TIMER_1_IRQn);

  BP_SetPriority(USB_IRQn, 0x00);
  BP_ClearPendingIRQ(USB_IRQn);
  BP_EnableIRQ(USB_IRQn);
  BP_EnableIRQs();
}

void board_led_write(bool state)
{
  gpio_setPinOutputBool(LED_PIN, state ? LED_STATE_ON : (1-LED_STATE_ON));
}

uint32_t board_button_read(void)
{
  return 0;
}

int board_uart_read(uint8_t* buf, int len)
{
  (void) buf; (void) len;
  return 0;
}

int board_uart_write(void const * buf, int len)
{
  for (int i = 0; i < len; i++) {
    const char* cbuf = buf;
    while (!UART1->STAT_b.TX_READY) {}
    if (cbuf[i] == '\n') {
      UART1->IO = '\r';
      while (!UART1->STAT_b.TX_READY) {}
    }
    UART1->IO = cbuf[i];
  }
  return len;
}

#if CFG_TUSB_OS  == OPT_OS_NONE
volatile uint32_t system_ticks = 0;

void TIMER_1_IRQHandler(void)
{
  system_ticks++;
  SYSTMR->C1 += 977;
  SYSTMR->CS_b.M1 = 1;
}

uint32_t board_millis(void)
{
  return system_ticks;
}
#endif

void HardFault_Handler (void)
{
  // asm("bkpt");
}

// Required by __libc_init_array in startup code if we are compiling using
// -nostdlib/-nostartfiles.
void _init(void)
{

}

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
  gpio_initOutputPinWithPullNone(18);
  gpio_initOutputPinWithPullNone(19);
  gpio_initOutputPinWithPullNone(20);
  gpio_initOutputPinWithPullNone(21);
  gpio_setPinOutputBool(18, true);
  gpio_initOutputPinWithPullNone(42);
  setup_mmu_flat_map();
  init_caches();

  // gpio_initOutputPinWithPullNone(23);
  // gpio_initOutputPinWithPullNone(24);
  // gpio_initOutputPinWithPullNone(25);
  gpio_setPinOutputBool(18, false);
  uart_init();
  gpio_setPinOutputBool(18, true);
  gpio_setPinOutputBool(18, false);
  for (size_t i = 0; i < 5; i++) {
  // while (true) {
    for (size_t j = 0; j < 1000000; j++) {
      __asm__("nop");
    }
    gpio_setPinOutputBool(42, true);
    gpio_setPinOutputBool(18, true);
    gpio_setPinOutputBool(19, true);
    gpio_setPinOutputBool(20, true);
    gpio_setPinOutputBool(21, true);
    for (size_t j = 0; j < 1000000; j++) {
      __asm__("nop");
    }
    gpio_setPinOutputBool(42, false);
    gpio_setPinOutputBool(18, false);
    gpio_setPinOutputBool(19, false);
    gpio_setPinOutputBool(20, false);
    gpio_setPinOutputBool(21, false);
  }
  // uart_writeText("hello from io\n");
  // gpio_setPinOutputBool(24, true);
  // gpio_setPinOutputBool(24, false);
  // gpio_setPinOutputBool(25, true);
  // print();
  // gpio_setPinOutputBool(25, false);
  // while (true) {
  // // for (size_t i = 0; i < 5; i++) {
  //   for (size_t j = 0; j < 10000000000; j++) {
  //     __asm__("nop");
  //   }
  //   gpio_setPinOutputBool(42, true);
  //   for (size_t j = 0; j < 10000000000; j++) {
  //     __asm__("nop");
  //   }
  //   gpio_setPinOutputBool(42, false);
  // }
  // while (1) uart_update();

  // Turn on USB peripheral.
  vcmailbox_set_power_state(VCMAILBOX_DEVICE_USB_HCD, true);

  BP_SetPriority(USB_IRQn, 0x00);
  BP_ClearPendingIRQ(USB_IRQn);
  BP_EnableIRQ(USB_IRQn);
  BP_EnableIRQs();
}

void board_led_write(bool state)
{
  gpio_setPinOutputBool(42, state);
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
void SysTick_Handler (void)
{
  system_ticks++;
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

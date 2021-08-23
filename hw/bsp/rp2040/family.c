/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 * Copyright (c) 2021, Ha Thach (tinyusb.org)
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

#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "hardware/gpio.h"
#include "hardware/sync.h"
#include "hardware/structs/ioqspi.h"
#include "hardware/structs/sio.h"

#include "bsp/board.h"
#include "board.h"

#ifdef BUTTON_BOOTSEL
// This example blinks the Picoboard LED when the BOOTSEL button is pressed.
//
// Picoboard has a button attached to the flash CS pin, which the bootrom
// checks, and jumps straight to the USB bootcode if the button is pressed
// (pulling flash CS low). We can check this pin in by jumping to some code in
// SRAM (so that the XIP interface is not required), floating the flash CS
// pin, and observing whether it is pulled low.
//
// This doesn't work if others are trying to access flash at the same time,
// e.g. XIP streamer, or the other core.
bool __no_inline_not_in_flash_func(get_bootsel_button)() {
    const uint CS_PIN_INDEX = 1;

    // Must disable interrupts, as interrupt handlers may be in flash, and we
    // are about to temporarily disable flash access!
    uint32_t flags = save_and_disable_interrupts();

    // Set chip select to Hi-Z
    hw_write_masked(&ioqspi_hw->io[CS_PIN_INDEX].ctrl,
                    GPIO_OVERRIDE_LOW << IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_LSB,
                    IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_BITS);

    // Note we can't call into any sleep functions in flash right now
    for (volatile int i = 0; i < 1000; ++i);

    // The HI GPIO registers in SIO can observe and control the 6 QSPI pins.
    // Note the button pulls the pin *low* when pressed.
    bool button_state = (sio_hw->gpio_hi_in & (1u << CS_PIN_INDEX));

    // Need to restore the state of chip select, else we are going to have a
    // bad time when we return to code in flash!
    hw_write_masked(&ioqspi_hw->io[CS_PIN_INDEX].ctrl,
                    GPIO_OVERRIDE_NORMAL << IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_LSB,
                    IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_BITS);

    restore_interrupts(flags);

    return button_state;
}
#endif

//------------- Segger RTT retarget -------------//
#if defined(LOGGER_RTT)

// Logging with RTT
// - If RTT Control Block is not found by 'Auto Detection` try to use 'Search Range` with '0x20000000 0x10000'
// - SWD speed is rather slow around 1000Khz

#include "pico/stdio/driver.h"
#include "SEGGER_RTT.h"

static void stdio_rtt_write (const char *buf, int length)
{
  SEGGER_RTT_Write(0, buf, length);
}

static int stdio_rtt_read (char *buf, int len)
{
  return SEGGER_RTT_Read(0, buf, len);
}

static stdio_driver_t stdio_rtt =
{
  .out_chars = stdio_rtt_write,
  .out_flush = NULL,
  .in_chars = stdio_rtt_read
};

void stdio_rtt_init(void)
{
  stdio_set_driver_enabled(&stdio_rtt, true);
}

#endif

#ifdef UART_DEV
static uart_inst_t *uart_inst;
#endif

void board_init(void)
{
#ifdef LED_PIN
  bi_decl(bi_1pin_with_name(LED_PIN, "LED"));
  gpio_init(LED_PIN);
  gpio_set_dir(LED_PIN, GPIO_OUT);
#endif

  // Button
#ifndef BUTTON_BOOTSEL
#endif

#if defined(UART_DEV) && defined(LIB_PICO_STDIO_UART)
  bi_decl(bi_2pins_with_func(UART_TX_PIN, UART_TX_PIN, GPIO_FUNC_UART));
  uart_inst = uart_get_instance(UART_DEV);
  stdio_uart_init_full(uart_inst, CFG_BOARD_UART_BAUDRATE, UART_TX_PIN, UART_RX_PIN);
#endif

#if defined(LOGGER_RTT)
  stdio_rtt_init();
#endif

  // todo probably set up device mode?
#if TUSB_OPT_DEVICE_ENABLED

#endif

#if TUSB_OPT_HOST_ENABLED
  // set portfunc to host !!!
#endif
}

//--------------------------------------------------------------------+
// Board porting API
//--------------------------------------------------------------------+

void board_led_write(bool state)
{
#ifdef LED_PIN
  gpio_put(LED_PIN, state ? LED_STATE_ON : (1-LED_STATE_ON));
#endif
}

uint32_t board_button_read(void)
{
#ifdef BUTTON_BOOTSEL
  return BUTTON_STATE_ACTIVE == get_bootsel_button();
#else
  return 0;
#endif
}

int board_uart_read(uint8_t* buf, int len)
{
#ifdef UART_DEV
  for(int i=0;i<len;i++) {
    buf[i] = uart_getc(uart_inst);
  }
  return len;
#else
  return 0;
#endif
}

int board_uart_write(void const * buf, int len)
{
#ifdef UART_DEV
  char const* bufch = (char const*) buf;
  for(int i=0;i<len;i++) {
    uart_putc(uart_inst, bufch[i]);
  }
  return len;
#else
  return 0;
#endif
}

//--------------------------------------------------------------------+
// USB Interrupt Handler
// rp2040 implementation will install approriate handler when initializing
// tinyusb. There is no need to forward IRQ from application
//--------------------------------------------------------------------+

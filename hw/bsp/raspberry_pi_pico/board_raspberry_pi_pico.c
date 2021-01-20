/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
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
#include "../board.h"

#ifndef LED_PIN
#define LED_PIN PICO_DEFAULT_LED_PIN
#endif

void board_init(void)
{
    setup_default_uart();
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, 1);

    // Button

    // todo probably set up device mode?

#if TUSB_OPT_HOST_ENABLED
    // set portfunc to host !!!
#endif
}

////--------------------------------------------------------------------+
//// USB Interrupt Handler
////--------------------------------------------------------------------+
//void USB_IRQHandler(void)
//{
//#if CFG_TUSB_RHPORT0_MODE & OPT_MODE_HOST
//    tuh_isr(0);
//#endif
//
//#if CFG_TUSB_RHPORT0_MODE & OPT_MODE_DEVICE
//    tud_int_handler(0);
//#endif
//}

//--------------------------------------------------------------------+
// Board porting API
//--------------------------------------------------------------------+

void board_led_write(bool state)
{
    gpio_put(LED_PIN, state);
}

uint32_t board_button_read(void)
{
    return 0;
}

int board_uart_read(uint8_t* buf, int len)
{
    for(int i=0;i<len;i++) {
        buf[i] = uart_getc(uart_default);
    }
    return 0;
}

int board_uart_write(void const * buf, int len)
{
//  UART_Send(BOARD_UART_PORT, &c, 1, BLOCKING);
    for(int i=0;i<len;i++) {
        uart_putc(uart_default, ((char *)buf)[i]);
    }
    return 0;
}

#if CFG_TUSB_OS == OPT_OS_NONE
uint32_t board_millis(void)
{
    return to_ms_since_boot(get_absolute_time());
}
#endif

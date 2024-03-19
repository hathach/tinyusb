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
#include "board.h"
#include "bsp/board_api.h"

void board_init(void)
{
    PLATFORM_INIT;
    CLOCK_INIT;

#if (MCU_CORE_B91 || MCU_CORE_B92)
    // LED
    gpio_function_en(LED_PIN);
    gpio_output_en(LED_PIN);
    gpio_input_dis(LED_PIN);

    // BUTTON
    gpio_function_en(BUTTON_PIN);
    gpio_output_dis(BUTTON_PIN);
    gpio_input_en(BUTTON_PIN);
#elif (MCU_CORE_B80 || MCU_CORE_B85 || MCU_CORE_B87)
    // TODO
#endif
    board_led_write(false);
}

//--------------------------------------------------------------------+
// Board porting API
//--------------------------------------------------------------------+

void board_led_write(bool state)
{
#if (MCU_CORE_B91 || MCU_CORE_B92)
    if (state == LED_STATE_ON)
    {
        gpio_set_high_level(LED_PIN);
    }
    else
    {
        gpio_set_low_level(LED_PIN);
    }
#elif (MCU_CORE_B80 || MCU_CORE_B85 || MCU_CORE_B87)
    // TODO
#endif
}

uint32_t board_button_read(void)
{
#if (MCU_CORE_B91 || MCU_CORE_B92)
    return gpio_get_level(BUTTON_PIN);
#elif (MCU_CORE_B80 || MCU_CORE_B85 || MCU_CORE_B87)
    // TODO
    return true;
#endif
}

int board_uart_read(uint8_t *buf, int len)
{
    (void)buf;
    (void)len;

    return len;
}

int board_uart_write(void const *buf, int len)
{
    (void)buf;
    (void)len;

    return len;
}

uint32_t board_millis(void)
{
    return reg_system_tick / SYSTEM_TIMER_TICK_1MS;
}

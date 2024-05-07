/*
 * The MIT License (MIT)
 *
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

#ifndef BOARD_H_
#define BOARD_H_

#ifdef __cplusplus
 extern "C" {
#endif

#define LED_PORT              1
#define LED_PIN               18
#define LED_STATE_ON          1

// JOYSTICK_DOWN if using LPCXpresso Base Board
#define BUTTON_PORT           0
#define BUTTON_PIN            15
#define BUTTON_STATE_ACTIVE   0

#define BOARD_UART_PORT   LPC_UART3

/* System oscillator rate and RTC oscillator rate */
const uint32_t OscRateIn = 10000000;
const uint32_t RTCOscRateIn = 32768;

// Pin muxing configuration
static const PINMUX_GRP_T pinmuxing[] = {
    {LED_PORT,  LED_PIN,  IOCON_MODE_INACT | IOCON_FUNC0},
    {BUTTON_PORT, BUTTON_PIN, IOCON_FUNC0 | IOCON_MODE_PULLUP},
};

static const PINMUX_GRP_T pin_usb_mux[] = {
    {0, 29, IOCON_MODE_INACT | IOCON_FUNC1}, // D+
    {0, 30, IOCON_MODE_INACT | IOCON_FUNC1}, // D-
    {2, 9,  IOCON_MODE_INACT | IOCON_FUNC1}, // Soft Connect

    {1, 19, IOCON_MODE_INACT | IOCON_FUNC2}, // USB_PPWR (Host mode)
    {1, 22, IOCON_MODE_INACT | IOCON_FUNC2}, // USB_PWRD

    // VBUS is not connected on this board, so leave the pin at default setting.
    // Chip_IOCON_PinMux(LPC_IOCON, 1, 30, IOCON_MODE_INACT, IOCON_FUNC2);  // USB VBUS
};

#ifdef __cplusplus
 }
#endif

#endif

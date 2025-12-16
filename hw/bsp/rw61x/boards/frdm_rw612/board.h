/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2021, Ha Thach (tinyusb.org)
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
   name: FRDM-RW612
   url: https://www.nxp.com/design/design-center/development-boards-and-designs/FRDM-RW612
*/

#ifndef BOARD_H_
#define BOARD_H_

#ifdef __cplusplus
 extern "C" {
#endif


// LED - Green channel of RGB LED
#define LED_GPIO              BOARD_INITLEDPINS_LED_GREEN_PERIPHERAL
#define LED_CLK               kCLOCK_HsGpio0
#define LED_PIN               BOARD_INITLEDPINS_LED_GREEN_PIN
#define LED_PORT              BOARD_INITLEDPINS_LED_GREEN_PORT
#define LED_STATE_ON          0

// WAKE button (Dummy, use unused pin
#define BUTTON_GPIO           BOARD_INITPINS_WAKEUP_BTN_PERIPHERAL
#define BUTTON_CLK            kCLOCK_HsGpio0
#define BUTTON_PIN            BOARD_INITPINS_WAKEUP_BTN_PIN
#define BUTTON_PORT           BOARD_INITPINS_WAKEUP_BTN_PORT
#define BUTTON_STATE_ACTIVE   0

// UART
#define UART_DEV                   USART3
#define LP_FLEXCOMM_INST           3U

#define BOARD_DEBUG_UART_FRG_CLK \
    (&(const clock_frg_clk_config_t){3, kCLOCK_FrgPllDiv, 255, 0}) /*!< Select FRG3 mux as frg_pll */
#define BOARD_DEBUG_UART_CLK_ATTACH kFRG_to_FLEXCOMM3


static inline void board_uart_init_clock(void) {
  /* attach FRG0 clock to FLEXCOMM3/USART3 */
  CLOCK_SetFRGClock(BOARD_DEBUG_UART_FRG_CLK);
  CLOCK_AttachClk(BOARD_DEBUG_UART_CLK_ATTACH);
}

#ifdef __cplusplus
 }
#endif

#endif

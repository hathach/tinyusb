/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2018, hathach (tinyusb.org)
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

/** \ingroup group_demo
 * \defgroup group_board Boards Abstraction Layer
 *  @{ */

#ifndef _BSP_BOARD_H_
#define _BSP_BOARD_H_

#ifdef __cplusplus
 extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

#include "ansi_escape.h"

// NXP LPC
#if defined BOARD_LPCXPRESSO11U14
  #include "lpcxpresso11u14/board_lpcxpresso11u14.h"
#elif defined BOARD_LPCXPRESSO1347
  #include "lpcxpresso1347/board_lpcxpresso1347.h"
#elif defined BOARD_LPCXPRESSO11U68
  #include "lpcxpresso11u68/board_lpcxpresso11u68.h"
#elif defined BOARD_LPCXPRESSO1769
  #include "lpcxpresso1769/board_lpcxpresso1769.h"
#elif defined BOARD_MCB1800
  #include "mcb1800/board_mcb1800.h"
#elif defined BOARD_EA4088QS
  #include "ea4088qs/board_ea4088qs.h"
#elif defined BOARD_EA4357
  #include "ea4357/board_ea4357.h"

// Nordic nRF
#elif defined BOARD_PCA10056
  #include "pca10056/board_pca10056.h"

// Atmel SAM
#elif defined BOARD_METRO_M4_EXPRESS
  #include "metro_m4_express/board_metro_m4_express.h"
#elif defined BOARD_METRO_M0_EXPRESS
  #include "metro_m0_express/board_metro_m0_express.h"
#elif defined BOARD_STM32F407G_DISC1
  #include "stm32f407g_disc1/board_stm32f407g_disc1.h"
#else
  #error BOARD is not defined or supported yet
#endif

#define CFG_UART_BAUDRATE    115200
#define BOARD_TICKS_HZ       1000
#define board_tick2ms(tck)   ( ( ((uint64_t)(tck)) * 1000) / BOARD_TICKS_HZ )


/// Initialize on-board peripherals
void board_init(void);

//--------------------------------------------------------------------+
// LED
// Board layer use only 1 LED for indicator
//--------------------------------------------------------------------+
void board_led_control(bool state);

static inline void board_led_on(void)
{
  board_led_control(true);
}

static inline void board_led_off(void)
{
  board_led_control(false);
}

//--------------------------------------------------------------------+
// Buttons
// TODO refractor later
//--------------------------------------------------------------------+
/** Get the current state of the buttons on the board
 * \return Bitmask where a '1' means active (pressed), a '0' means inactive.
 */
uint32_t board_buttons(void);

/** Get a character input from UART
 * \return ASCII code of the input character or zero if none.
 */
uint8_t  board_uart_getchar(void);

/** Send a character to UART
 * \param[in]  c the character to be sent
 */
void board_uart_putchar(uint8_t c);


#ifdef __cplusplus
 }
#endif

#endif /* _BSP_BOARD_H_ */

/** @} */

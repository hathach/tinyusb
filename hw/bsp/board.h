/**************************************************************************/
/*!
    @file     board.h
    @author   hathach (tinyusb.org)

    @section LICENSE

    Software License Agreement (BSD License)

    Copyright (c) 2013, hathach (tinyusb.org)
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:
    1. Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
    3. Neither the name of the copyright holders nor the
    names of its contributors may be used to endorse or promote products
    derived from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ''AS IS'' AND ANY
    EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    INCLUDING NEGLIGENCE OR OTHERWISE ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

    This file is part of the tinyusb stack.
*/
/**************************************************************************/

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

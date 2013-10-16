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

/** \file
 *  \brief TBD
 *
 *  \note TBD
 */

/**
 *  \defgroup Group_Board Boards
 *  \brief TBD
 *
 *  @{
 */

#ifndef _TUSB_BOARD_H_
#define _TUSB_BOARD_H_

#ifdef __cplusplus
 extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

#include "ansi_escape.h"
#include "tusb.h"

//--------------------------------------------------------------------+
// BOARD DEFINE
//--------------------------------------------------------------------+
#define BOARD_RF1GHZNODE            1
#define BOARD_LPCXPRESSO1347        2

#define BOARD_NGX4330               3
#define BOARD_EA4357                4
#define BOARD_MCB4300               5
#define BOARD_HITEX4350             6
#define BOARD_LPCXPRESSO1769        7

#define BOARD_LPC4357USB            8

//--------------------------------------------------------------------+
// PRINTF TARGET DEFINE
//--------------------------------------------------------------------+
#define PRINTF_TARGET_SEMIHOST      1
#define PRINTF_TARGET_UART          2
#define PRINTF_TARGET_SWO           3 // aka SWV, ITM
#define PRINTF_TARGET_NONE          4

#define PRINTF(...) printf(__VA_ARGS__)

#if BOARD == 0
  #error BOARD is not defined or supported yet
#elif BOARD == BOARD_NGX4330
  #include "ngx/board_ngx4330.h"
#elif BOARD == BOARD_LPCXPRESSO1347
  #include "lpcxpresso/board_lpcxpresso1347.h"
#elif BOARD == BOARD_RF1GHZNODE
  #include "microbuilder/board_rf1ghznode.h"
#elif BOARD == BOARD_EA4357
  #include "embedded_artists/board_ea4357.h"
#elif BOARD == BOARD_MCB4300
  #include "keil/board_mcb4300.h"
#elif BOARD == BOARD_HITEX4350
  #include "hitex/board_hitex4350.h"
#elif BOARD == BOARD_LPCXPRESSO1769
  #include "lpcxpresso/board_lpcxpresso1769.h"
#elif BOARD == BOARD_LPC4357USB
  #include "microbuilder/board_lpc4357usb.h"
#else
  #error BOARD is not defined or supported yet
#endif

//--------------------------------------------------------------------+
// Common Configuration
//--------------------------------------------------------------------+
#define CFG_TICKS_PER_SECOND 1000

#if CFG_PRINTF_TARGET == PRINTF_TARGET_UART
  #define CFG_UART_ENABLE      1
  #define CFG_UART_BAUDRATE    115200
#endif

//--------------------------------------------------------------------+
// Board Common API
//--------------------------------------------------------------------+
// Init board peripherals : Clock, UART, LEDs, Buttons
void board_init(void);
void board_leds(uint32_t on_mask, uint32_t off_mask);
uint32_t board_uart_send(uint8_t *buffer, uint32_t length);
uint32_t board_uart_recv(uint8_t *buffer, uint32_t length);
uint8_t  board_uart_getchar(void);

extern volatile uint32_t system_ticks;

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_BOARD_H_ */

/** @} */

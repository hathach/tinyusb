/*
 * board.h
 *
 *  Created on: Dec 4, 2012
 *      Author: hathach
 */

/*
 * Software License Agreement (BSD License)
 * Copyright (c) 2013, hathach (tinyusb.net)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 * This file is part of the tiny usb stack.
 */

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
#include "common/binary.h" // This file is too good to not use

#define BOARD_AT86RF2XX             1
#define BOARD_LPCXPRESSO1347        2
#define BOARD_NGX4330               3
#define BOARD_EA4357                4

#define PRINTF_TARGET_DEBUG_CONSOLE 1 // IDE semihosting console
#define PRINTF_TARGET_UART          2
#define PRINTF_TARGET_SWO           3 // aka SWV, ITM

#if BOARD == 0
  #error BOARD is not defined or supported yet
#elif BOARD == BOARD_NGX4330
  #include "board_ngx4330.h"
#elif BOARD == BOARD_LPCXPRESSO1347
  #include "board_lpcxpresso1347.h"
#elif BOARD == BOARD_AT86RF2XX
  #include "board_at86rf2xx.h"
#elif BOARD == BOARD_EA4357
  #include "board_ea4357.h"
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
void board_leds(uint32_t mask, uint32_t state);
uint32_t board_uart_send(uint8_t *buffer, uint32_t length);
uint32_t board_uart_recv(uint8_t *buffer, uint32_t length);

extern volatile uint32_t system_ticks;

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_BOARD_H_ */

/** @} */

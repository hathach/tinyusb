/*
 * board.h
 *
 *  Created on: Dec 4, 2012
 *      Author: hathach
 */

/*
 * Software License Agreement (BSD License)
 * Copyright (c) 2012, hathach (tinyusb.net)
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

#define TICKS_PER_SECOND 1000

/// n-th Bit
#ifndef BIT_
#define BIT_(n) (1 << (n))
#endif

#define BOARD_NGX43XX 1
#define BOARD_LPCXPRESSO1347  2
#define BOARD_AT86RF2XX 3

#if BOARD == BOARD_NGX43XX

#elif BOARD == BOARD_LPCXPRESSO1347

#elif BOARD == BOARD_AT86RF2XX

#else
  #error BOARD is not defined or supported yet
#endif

/// Init board peripherals : Clock, UART, LEDs, Buttons
void board_init(void);
void board_leds(uint32_t mask, uint32_t state);

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_BOARD_H_ */

/** @} */

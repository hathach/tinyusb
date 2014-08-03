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
/** \defgroup group_supported_board Supported Boards
 *  @{ */
#define BOARD_LPCXPRESSO11U14       1114 ///< LPCXpresso 11u14, some APIs requires the base board
#define BOARD_LPCXPRESSO11U68       1168 ///< LPC11U37 from microbuilder http://www.microbuilder.eu/Blog/13-03-14/LPC1xxx_1GHZ_Wireless_Board_Preview.aspx
#define BOARD_LPCXPRESSO1347        1347 ///< LPCXpresso 1347, some APIs requires the base board
#define BOARD_LPCXPRESSO1769        1769 ///< LPCXpresso 1769, some APIs requires the base board

#define BOARD_NGX4330               4330 ///< NGX 4330 Xplorer
#define BOARD_EA4357                4357 ///< Embedded Artists LPC4357 developer kit
#define BOARD_MCB4300               4300 ///< Keil MCB4300
#define BOARD_HITEX4350             4350 ///< Hitex 4350

#define BOARD_LPC4357USB            4304 ///< microbuilder.eu

#define BOARD_LPCLINK2              4370 ///< LPClink2 uses as LPC4370 development board
/** @} */

//--------------------------------------------------------------------+
// PRINTF TARGET DEFINE
//--------------------------------------------------------------------+
/** \defgroup group_printf Printf Retarget
 * \brief Retarget the standard stdio printf/getchar to other IOs
 *  @{ */
#define PRINTF_TARGET_SEMIHOST      1 ///< Using the semihost support from toolchain, requires no hardware but is the slowest
#define PRINTF_TARGET_UART          2 ///< Using UART as stdio, this is the default for most of the board
#define PRINTF_TARGET_SWO           3 ///< Using non-instructive serial wire output (SWO), is the best option since it does not slow down MCU but requires supported from debugger and IDE
#define PRINTF_TARGET_NONE          4 ///< Using none at all.
/** @} */

#define PRINTF(...) printf(__VA_ARGS__)

//--------------------------------------------------------------------+
// BOARD INCLUDE
//--------------------------------------------------------------------+
#if BOARD == BOARD_LPCXPRESSO11U14
  #include "lpcxpresso/board_lpcxpresso11u14.h"
#elif BOARD == BOARD_LPCXPRESSO11U68
  #include "lpcxpresso/board_lpcxpresso11u68.h"
#elif BOARD == BOARD_LPCXPRESSO1347
  #include "lpcxpresso/board_lpcxpresso1347.h"
#elif BOARD == BOARD_LPCXPRESSO1769
  #include "lpcxpresso/board_lpcxpresso1769.h"
#elif BOARD == BOARD_NGX4330
  #include "ngx/board_ngx4330.h"
#elif BOARD == BOARD_EA4357
  #include "embedded_artists/ea4357/board_ea4357.h"
#elif BOARD == BOARD_MCB4300
  #include "keil/board_mcb4300.h"
#elif BOARD == BOARD_HITEX4350
  #include "hitex/board_hitex4350.h"
#elif BOARD == BOARD_LPC4357USB
  #include "microbuilder/board_lpc4357usb.h"
#elif BOARD == BOARD_LPCLINK2
 #include "lpcxpresso/board_lpclink2.h"
#else
  #error BOARD is not defined or supported yet
#endif

//--------------------------------------------------------------------+
// Common Configuration
//--------------------------------------------------------------------+
#define CFG_UART_BAUDRATE    115200 ///< Baudrate for UART

//--------------------------------------------------------------------+
// Board Common API
//--------------------------------------------------------------------+
/** \defgroup group_board_api Board API
 * \brief All the board must support these APIs.
 *  @{ */

/// Initialize all required peripherals on board including uart, led, buttons etc ...
void board_init(void);

/** \brief Turns on and off leds on the board
 * \param[in]  on_mask  Bitmask for LED's numbers is turning ON
 * \param[out] off_mask Bitmask for LED's numbers is turning OFF
 * \note the \a on_mask is more priority then \a off_mask, if an led's number is present on both.
 * It will be turned ON.
 */
void board_leds(uint32_t on_mask, uint32_t off_mask);

/** \brief Get the current state of the buttons on the board
 * \return Bitmask where a '1' means active (pressed), a '0' means inactive.
 */
uint32_t board_buttons(void);

/** \brief Get a character input from UART
 * \return ASCII code of the input character or zero if none.
 */
uint8_t  board_uart_getchar(void);

/** \brief Send a character to UART
 * \param[in]  c the character to be sent
 */
void board_uart_putchar(uint8_t c);

/** @} */

//------------- Board Application  -------------//
OSAL_TASK_FUNCTION( led_blinking_task , p_task_para);

/// Initialize the LED blinking task application. The initial blinking rate is 1 Hert (1 per second)
void led_blinking_init(void);

/** \brief Change the blinking rate.
 * \param[in]  ms The interval between on and off.
 */
void led_blinking_set_interval(uint32_t ms);

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_BOARD_H_ */

/** @} */

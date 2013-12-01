/**************************************************************************/
/*!
    @file     board_ea4357.h
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

/** \ingroup TBD
 *  \defgroup TBD
 *  \brief TBD
 *
 *  @{
 */

#ifndef _TUSB_BOARD_EA4357_H_
#define _TUSB_BOARD_EA4357_H_

#ifdef __cplusplus
 extern "C" {
#endif

#include "LPC43xx.h"
#include "lpc43xx_scu.h"
#include "lpc43xx_cgu.h"
#include "lpc43xx_gpio.h"
#include "lpc43xx_uart.h"
#include "lpc43xx_i2c.h"


#include "../oem_base_board/pca9532.h" // LEDs
//#include "../oem_board/nand.h"a


//#define CFG_PRINTF_TARGET       PRINTF_TARGET_SWO
#define CFG_PRINTF_TARGET       PRINTF_TARGET_UART // FIXME keil's cmsis rtx does not work with UART (work with SWO)

// TODO clean up
/*=========================================================================
    HARDWARE MAC ADDRESS
    -----------------------------------------------------------------------*/
	#define BOARD_MAC_ADDR0 0x00
	#define BOARD_MAC_ADDR1 0x10
	#define BOARD_MAC_ADDR2 0x20
	#define BOARD_MAC_ADDR3 0x30
	#define BOARD_MAC_ADDR4 0x40
	#define BOARD_MAC_ADDR5 0x50
/*=========================================================================*/

/*=========================================================================
    EMAC CONFIGURATION
    -----------------------------------------------------------------------*/
	/*  The PHY address connected the to MII/RMII */
	#define LPC_PHYDEF_PHYADDR 1    /**< The PHY address on the PHY device. */

	/*  Enable autonegotiation mode.
	 *  If this is enabled, the PHY will attempt to auto-negotiate the
	 *  best link mode if the PHY supports it. If this is not enabled,
	 *  the PHY_USE_FULL_DUPLEX and PHY_USE_100MBS defines will be
	 *  used to select the link mode. Note that auto-negotiation may
	 *  take a few seconds to complete.
	 */
	#define PHY_USE_AUTONEG 1 /**< Enables auto-negotiation mode. */

	/*  Sets up the PHY interface to either full duplex operation or
	 *  half duplex operation if PHY_USE_AUTONEG is not enabled.
	 */
	#define PHY_USE_FULL_DUPLEX 1 /**< Sets duplex mode to full. */

	/*  Sets up the PHY interface to either 100MBS operation or 10MBS
	 *  operation if PHY_USE_AUTONEG is not enabled.
	 */
	#define PHY_USE_100MBS 1 /**< Sets data rate to 100Mbps. */

	/*  Selects RMII or MII connection type in the EMAC peripheral */
	#define LPC_EMAC_RMII 1   /**< Use the RMII or MII driver variant */

	/*  Defines the number of descriptors used for RX */
	#define LPC_NUM_BUFF_RXDESCS 20

	/*  Defines the number of descriptors used for TX */
	#define LPC_NUM_BUFF_TXDESCS 20

	/*  Enables slow speed memory buffering
	 *  Enable this define if you expect to transfer packets directly
	 *  from SPI FLASH or any slower memory. This will add a check
	 *  before queueing up the transfer pbuf to make sure the packet
	 *  is not in slow memoey (defined by the LPC_SLOWMEM_ARRAY). If
	 *  the packet does exists in slow memory, a pbuf will be created
	 *  in the PBUF_RAM pool, copied to it, and sent from there.
	 */
	#define LPC_CHECK_SLOWMEM 0

	/*  Array of slow memory addresses for LPC_CHECK_SLOWMEM
	 *  Define the array - start and ending address - for the slow
	 *  memory regions in the system that need pbuf copies.
	 *
	 *  Not defined since LPC_CHECK_SLOWMEM = 0.
	 */
	#define LPC_SLOWMEM_ARRAY
/*=========================================================================*/

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_BOARD_EA4357_H_ */

/** @} */

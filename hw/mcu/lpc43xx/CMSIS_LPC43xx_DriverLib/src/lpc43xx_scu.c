/**********************************************************************
* $Id$		lpc43xx_scu.c		2011-06-02
*//**
* @file		lpc43xx_scu.c
* @brief	Contains all functions support for SCU firmware library on lpc43xx
* @version	1.0
* @date		02. June. 2011
* @author	NXP MCU SW Application Team
*
* Copyright(C) 2011, NXP Semiconductor
* All rights reserved.
*
***********************************************************************
* Software that is described herein is for illustrative purposes only
* which provides customers with programming information regarding the
* products. This software is supplied "AS IS" without any warranties.
* NXP Semiconductors assumes no responsibility or liability for the
* use of the software, conveys no license or title under any patent,
* copyright, or mask work right to the product. NXP Semiconductors
* reserves the right to make changes in the software without
* notification. NXP Semiconductors also make no representation or
* warranty that such application will be suitable for the specified
* use without further testing or modification.
* Permission to use, copy, modify, and distribute this software and its
* documentation is hereby granted, under NXP Semiconductors�
* relevant copyright in the software, without fee, provided that it
* is used in conjunction with NXP Semiconductors microcontrollers.  This
* copyright, permission, and disclaimer notice must appear in all copies of
* this code.
**********************************************************************/

/* Peripheral group ----------------------------------------------------------- */
/** @addtogroup SCU
 * @{
 */

/* Includes ------------------------------------------------------------------- */
#include "LPC43xx.h"                    /* lpc43xx definitions                */
#include "lpc_types.h"
#include "lpc43xx_scu.h"

/* Pin modes
*  =========
*  The EPUN and EPD bits in the SFS registers allow the selection of weak on-chip
*  pull-up or pull-down resistors with a typical value of 50 kOhm for each pin or the
*  selection of the repeater mode.
*  The possible on-chip resistor configurations are pull-up enabled, pull-down enabled, or no
*  pull-up/pull-down. The default value is pull-up enabled.
*
*  The repeater mode enables the pull-up resistor if the pin is at a logic HIGH and enables
*  the pull-down resistor if the pin is at a logic LOW. This causes the pin to retain its last
*  known state if it is configured as an input and is not driven externally. Repeater mode may
*  typically be used to prevent a pin from floating (and potentially using significant power if it
*  floats to an indeterminate state) if it is temporarily not driven.
*  Repeater mode is enabled when both pull-up and pull-down are enabled.
*
*  To be able to receive a digital signal, the input buffer must be enabled through bit EZI in
*  the pin configuration registers. By default, the input buffer is disabled.
*  For pads that support both a digital and an analog function, the input buffer must be
*  disabled before enabling the analog function.
*
*  All digital pins support a programmable glitch filter (bit ZIF), which can be switched on or
*  off. By default, the glitch filter is on. The glitch filter should be disabled for
*  clocking signals with frequencies higher than 30 MHz.
*
*  Normal-drive and high-speed pins support a programmable slew rate (bit EHS) to select
*  between lower noise and low speed or higher noise and high speed . The typical
*  frequencies supported are 50 MHz/80 MHz for normal-drive pins and 75 MHz/180 MHz for
*  high-speed pins.
*/

/*********************************************************************//**
 * @brief 		Configure pin function
 * @param[in]	port	Port number, should be: 0..15
 * @param[in]	pin	Pin number, should be: 0..31
 * @param[in]	mode	Pin mode, should be:
 * 					- MD_PUP	:Pull-up enabled
 * 					- MD_BUK	:Plain input
 * 					- MD_PLN	:Repeater mode
 * 					- MD_PDN	:Pull-down enabled
 * 					- MD_EHS        :Slew rate
 * 					- MD_EZI        :Input buffer enable
 * 					- MD_ZI         :Glitch filter enabled
 * 					- MD_EHD0       :High drive  8 mA
 * 					- MD_EHD1       :High drive 14 mA
 * 					- MD_EHD2       :High drive 20 mA
 * @param[in]	func 	Function mode, should be:
 * 					- FUNC0		:Function 0
 * 					- FUNC1		:Function 1
 * 					- FUNC2		:Function 2
 * 					- FUNC3		:Function 3
 * 					- FUNC4		:Function 4
 * 					- FUNC5		:Function 5
 * 					- FUNC6		:Function 6
 * 					- FUNC7		:Function 7
 * @return		None
 **********************************************************************/
void scu_pinmux(uint8_t port, uint8_t pin, uint8_t mode, uint8_t func)
{
  uint32_t * scu_base=(uint32_t*)(LPC_SCU_BASE);
  scu_base[(PORT_OFFSET*port+PIN_OFFSET*pin)/4]=mode+func;
} /* scu_pinmux */

/**
 * @}
 */

/*
 * @brief LPC17xx/40xx IOCON driver
 *
 * Copyright(C) NXP Semiconductors, 2014
 * All rights reserved.
 *
 * Software that is described herein is for illustrative purposes only
 * which provides customers with programming information regarding the
 * LPC products.  This software is supplied "AS IS" without any warranties of
 * any kind, and NXP Semiconductors and its licensor disclaim any and
 * all warranties, express or implied, including all implied warranties of
 * merchantability, fitness for a particular purpose and non-infringement of
 * intellectual property rights.  NXP Semiconductors assumes no responsibility
 * or liability for the use of the software, conveys no license or rights under any
 * patent, copyright, mask work right, or any other intellectual property rights in
 * or to any products. NXP Semiconductors reserves the right to make changes
 * in the software without notification. NXP Semiconductors also makes no
 * representation or warranty that such application will be suitable for the
 * specified use without further testing or modification.
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation is hereby granted, under NXP Semiconductors' and its
 * licensor's relevant copyrights in the software, without fee, provided that it
 * is used in conjunction with NXP Semiconductors microcontrollers.  This
 * copyright, permission, and disclaimer notice must appear in all copies of
 * this code.
 */

#include "chip.h"

/*****************************************************************************
 * Private types/enumerations/variables
 ****************************************************************************/

/*****************************************************************************
 * Public types/enumerations/variables
 ****************************************************************************/

/*****************************************************************************
 * Private functions
 ****************************************************************************/

/*****************************************************************************
 * Public functions
 ****************************************************************************/

#if defined(CHIP_LPC175X_6X)
/* Sets I/O Control pin mux */
void Chip_IOCON_PinMuxSet(LPC_IOCON_T *pIOCON, uint8_t port, uint8_t pin, uint32_t modefunc)
{
	Chip_IOCON_PinMux(pIOCON, port, pin, 
					  /* mode is in bits 3:2 */
					  modefunc >> 2, 
					  /* func is in bits 1:0 */
					  modefunc & 3 );
}

/* Setup pin modes and function */
void Chip_IOCON_PinMux(LPC_IOCON_T *pIOCON, uint8_t port, uint8_t pin, uint32_t mode, uint8_t func)
{
	uint8_t reg, bitPos;
	uint32_t temp;

	bitPos =  IOCON_BIT_INDEX(pin);
	reg = IOCON_REG_INDEX(port,pin);
	
	temp = pIOCON->PINSEL[reg] & ~(0x03UL << bitPos);
	pIOCON->PINSEL[reg] = temp | (func << bitPos);

	temp = pIOCON->PINMODE[reg] & ~(0x03UL << bitPos);
	pIOCON->PINMODE[reg] = temp | (mode << bitPos);
}
#endif /* defined(CHIP_LPC175X_6X) */

/* Set all I/O Control pin muxing */
void Chip_IOCON_SetPinMuxing(LPC_IOCON_T *pIOCON, const PINMUX_GRP_T* pinArray, uint32_t arrayLength)
{
	uint32_t ix;

	for (ix = 0; ix < arrayLength; ix++ ) {
		Chip_IOCON_PinMuxSet(pIOCON, pinArray[ix].pingrp, pinArray[ix].pinnum, pinArray[ix].modefunc);
	}
}

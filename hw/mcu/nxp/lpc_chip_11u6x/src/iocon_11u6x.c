/*
 * @brief LPC11u6x IOCON driver
 *
 * @note
 * Copyright(C) NXP Semiconductors, 2013
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

/* Sets I/O Control pin mux */
void Chip_IOCON_PinMuxSet(LPC_IOCON_T *pIOCON, uint8_t port, uint8_t pin, uint32_t modefunc)
{
	switch (port) {
	case 0:
		pIOCON->PIO0[pin] = modefunc;
		break;

	case 1:
		pIOCON->PIO1[pin] = modefunc;
		break;

	case 2:
		if (pin >= 2) {
			pIOCON->PIO2B[pin - 2] = modefunc;
		}
		else {
			pIOCON->PIO2A[pin] = modefunc;
		}
		break;

	default:
		break;
	}
}

/*Set all I/O Control pin muxing*/
void Chip_IOCON_SetPinMuxing(LPC_IOCON_T *pIOCON, const PINMUX_GRP_T* pinArray, uint32_t arrayLength)
{
	uint32_t ix;

	for (ix = 0; ix < arrayLength; ix++ ) {
		Chip_IOCON_PinMuxSet(pIOCON, pinArray[ix].port, pinArray[ix].pin, pinArray[ix].modefunc);
	}
}

/* Return value of I/O Control pin mux */
uint32_t Chip_IOCON_GetPinMux(LPC_IOCON_T *pIOCON, uint8_t port, uint8_t pin)
{
	uint32_t iocon_value = 0;
	switch (port) {
	case 0:
		iocon_value = pIOCON->PIO0[pin];
		break;

	case 1:
		iocon_value = pIOCON->PIO1[pin];
		break;

	case 2:
		if (pin >= 2) {
			iocon_value = pIOCON->PIO2B[pin - 2];
		}
		else {
			iocon_value = pIOCON->PIO2A[pin];
		}
		break;

	default:
		break;
	}
	return iocon_value;
}

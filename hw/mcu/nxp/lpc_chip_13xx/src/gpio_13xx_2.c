/*
 * @brief LPC13xx GPIO chip driver for LPC13(11/13/42/43) families
 *
 * @note
 * Copyright(C) NXP Semiconductors, 2012
 * All rights reserved.
 *
 * @par
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
 * @par
 * Permission to use, copy, modify, and distribute this software and its
 * documentation is hereby granted, under NXP Semiconductors' and its
 * licensor's relevant copyrights in the software, without fee, provided that it
 * is used in conjunction with NXP Semiconductors microcontrollers.  This
 * copyright, permission, and disclaimer notice must appear in all copies of
 * this code.
 */

#include "chip.h"

#if defined(CHIP_LPC1343)

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

/* Initialize GPIO block */
void Chip_GPIO_Init(LPC_GPIO_T *pGPIO)
{
	Chip_Clock_EnablePeriphClock(SYSCTL_CLOCK_GPIO);
}

/* De-Initialize GPIO block */
void Chip_GPIO_DeInit(LPC_GPIO_T *pGPIO)
{
	Chip_Clock_DisablePeriphClock(SYSCTL_CLOCK_GPIO);
}

/* Set GPIO direction */
void Chip_GPIO_WriteDirBit(LPC_GPIO_T *pGPIO, uint32_t port, uint8_t bit, bool setting)
{
	if (setting) {
		pGPIO[port].DIR |= 1UL << bit;
	}
	else {
		pGPIO[port].DIR &= ~(1UL << bit);
	}
}

/* Set Direction for a GPIO port */
void Chip_GPIO_SetDir(LPC_GPIO_T *pGPIO, uint8_t port, uint32_t bit, uint8_t out)
{
	if (out) {
		pGPIO[port].DIR |= bit;
	}
	else {
		pGPIO[port].DIR &= ~bit;
	}
}

/* Set GPIO direction for a single GPIO pin */
void Chip_GPIO_SetPinDIR(LPC_GPIO_T *pGPIO, uint8_t port, uint8_t pin, bool output)
{
	if (output) {
		Chip_GPIO_SetPinDIROutput(pGPIO, port, pin);
	}
	else {
		Chip_GPIO_SetPinDIRInput(pGPIO, port, pin);
	}
}

/* Set GPIO direction for a all selected GPIO pins to an input or output */
void Chip_GPIO_SetPortDIR(LPC_GPIO_T *pGPIO, uint8_t port, uint32_t pinMask, bool outSet)
{
	if (outSet) {
		Chip_GPIO_SetPortDIROutput(pGPIO, port, pinMask);
	}
	else {
		Chip_GPIO_SetPortDIRInput(pGPIO, port, pinMask);
	}
}

/* Composite function for setting up a full interrupt configuration for a single pin */
void Chip_GPIO_SetupPinInt(LPC_GPIO_T *pGPIO, uint8_t port, uint8_t pin, GPIO_INT_MODE_T mode)
{
	uint32_t pinMask = (1 << pin);

	/* Edge mode selected? */
	if ((uint32_t) mode & 0x2) {
		Chip_GPIO_SetPinModeEdge(pGPIO, port, pinMask);

		/* Interrupt on both edges selected? */
		if ((uint32_t) mode & 0x4) {
			Chip_GPIO_SetEdgeModeBoth(pGPIO, port, pinMask);
		}
		else {
			Chip_GPIO_SetEdgeModeSingle(pGPIO, port, pinMask);
		}
	}
	else {
		/* Level mode */
		Chip_GPIO_SetPinModeLevel(pGPIO, port, pinMask);
	}

	/* Level selections will not alter 'dual edge' mode */
	if ((uint32_t) mode & 0x1) {
		/* High edge or level mode selected */
		Chip_GPIO_SetModeHigh(pGPIO, port, pinMask);
	}
	else {
		Chip_GPIO_SetModeLow(pGPIO, port, pinMask);
	}
}

#endif /* if defined(CHIP_LPC1343) */

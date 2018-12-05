/*
 * @brief	LPC17xx/40xx System and Control driver
 *
 * @note
 * Copyright(C) NXP Semiconductors, 2014
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

/*****************************************************************************
 * Private types/enumerations/variables
 ****************************************************************************/

/*****************************************************************************
 * Public types/enumerations/variables
 ****************************************************************************/

/*****************************************************************************
 * Private functions
 ****************************************************************************/
/* Returns and clears the current sleep mode entry flags */
uint32_t Chip_SYSCTL_GetClrSleepFlags(uint32_t flags) {
	uint32_t savedFlags = LPC_SYSCTL->PCON;

	LPC_SYSCTL->PCON = flags;

	return savedFlags & (SYSCTL_PD_SMFLAG | SYSCTL_PD_DSFLAG |
						 SYSCTL_PD_PDFLAG | SYSCTL_PD_DPDFLAG);
}

#if !defined(CHIP_LPC175X_6X)
/* Resets a peripheral */
void Chip_SYSCTL_PeriphReset(CHIP_SYSCTL_RESET_T periph)
{
	uint32_t bitIndex, regIndex = (uint32_t) periph;

	/* Get register array index and clock index into the register */
	bitIndex = (regIndex % 32);
	regIndex = regIndex / 32;

	/* Reset peripheral */
	LPC_SYSCTL->RSTCON[regIndex] = (1 << bitIndex);
	LPC_SYSCTL->RSTCON[regIndex] &= ~(1 << bitIndex);
}

#endif /*!defined(CHIP_LPC175X_6X)*/

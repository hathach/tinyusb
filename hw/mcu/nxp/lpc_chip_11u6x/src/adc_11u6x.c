/*
 * @brief LPC11u6x ADC driver
 *
 * @note
 * Copyright(C) NXP Semiconductors, 2013
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

/* Set ADC interrupt bits (safe) */
void Chip_ADC_SetIntBits(LPC_ADC_T *pADC, uint32_t intMask)
{
	uint32_t temp;

	/* Read and write values may not be the same, write 0 to
	   undefined bits */
	temp = pADC->INTEN & 0x07FFFFFF;

	pADC->INTEN = temp | intMask;
}

/* Clear ADC interrupt bits (safe) */
void Chip_ADC_ClearIntBits(LPC_ADC_T *pADC, uint32_t intMask)
{
	uint32_t temp;

	/* Read and write values may not be the same, write 0 to
	   undefined bits */
	temp = pADC->INTEN & 0x07FFFFFF;

	pADC->INTEN = temp & ~intMask;
}

/* Set ADC threshold selection bits (safe) */
void Chip_ADC_SetTHRSELBits(LPC_ADC_T *pADC, uint32_t mask)
{
	uint32_t temp;

	/* Read and write values may not be the same, write 0 to
	   undefined bits */
	temp = pADC->CHAN_THRSEL & 0x00000FFF;

	pADC->CHAN_THRSEL = temp | mask;
}

/* Clear ADC threshold selection bits (safe) */
void Chip_ADC_ClearTHRSELBits(LPC_ADC_T *pADC, uint32_t mask)
{
	uint32_t temp;

	/* Read and write values may not be the same, write 0 to
	   undefined bits */
	temp = pADC->CHAN_THRSEL & 0x00000FFF;

	pADC->CHAN_THRSEL = temp & ~mask;
}

/*****************************************************************************
 * Public functions
 ****************************************************************************/

/* Initialize the ADC peripheral */
void Chip_ADC_Init(LPC_ADC_T *pADC, uint32_t flags)
{
	/* Power up ADC and enable ADC base clock */
	Chip_SYSCTL_PowerUp(SYSCTL_POWERDOWN_ADC_PD);
	Chip_Clock_EnablePeriphClock(SYSCTL_CLOCK_ADC);

	/* Disable ADC interrupts */
	pADC->INTEN = 0;

	/* Set ADC control options */
	pADC->CTRL = flags;
}

/* Start ADC calibration */
void Chip_ADC_StartCalibration(LPC_ADC_T *pADC)
{
	uint32_t save_ADC_CTRL;
	
	save_ADC_CTRL = pADC->CTRL;
	Chip_ADC_SetClockRate(LPC_ADC, ADC_CALIBRATE_RATE);
	
	pADC->CTRL = (pADC->CTRL | ADC_CR_CALMODEBIT) & ~ADC_CR_LPWRMODEBIT;
}

/**
 * @brief	Start ADC calibration
 * @param	pADC	: The base of ADC peripheral on the chip
 * @return	TRUE if calibration is complete, otherwise FALSE.
 */
bool Chip_ADC_IsCalibrationDone(LPC_ADC_T *pADC)
{
	return (bool) ((pADC->CTRL & ADC_CR_CALMODEBIT) == 0);
}


/* Shutdown ADC */
void Chip_ADC_DeInit(LPC_ADC_T *pADC)
{
	pADC->INTEN = 0;
	pADC->CTRL = 0;

	/* Stop ADC clock and then power down ADC */
	Chip_Clock_DisablePeriphClock(SYSCTL_CLOCK_ADC);
	Chip_SYSCTL_PowerDown(SYSCTL_POWERDOWN_ADC_PD);
}

/* Set ADC clock rate */
void Chip_ADC_SetClockRate(LPC_ADC_T *pADC, uint32_t rate)
{
	Chip_ADC_SetDivider(pADC, ((Chip_Clock_GetSystemClockRate() + (rate>>2)) / rate) -1);
}

/* Helper function for safely setting ADC sequencer register bits */
void Chip_ADC_SetSequencerBits(LPC_ADC_T *pADC, ADC_SEQ_IDX_T seqIndex, uint32_t bits)
{
	uint32_t temp;

	/* Read sequencer register and mask off bits 20..25 */
	temp = pADC->SEQ_CTRL[seqIndex] & ~(0x3F << 20);

	/* OR in passed bits */
	pADC->SEQ_CTRL[seqIndex] = temp | bits;
}

/* Helper function for safely clearing ADC sequencer register bits */
void Chip_ADC_ClearSequencerBits(LPC_ADC_T *pADC, ADC_SEQ_IDX_T seqIndex, uint32_t bits)
{
	uint32_t temp;

	/* Read sequencer register and mask off bits 20..25 */
	temp = pADC->SEQ_CTRL[seqIndex] & ~(0x3F << 20);

	/* OR in passed bits */
	pADC->SEQ_CTRL[seqIndex] = temp & ~bits;
}

/* Enable interrupts in ADC (sequencers A/B and overrun) */
void Chip_ADC_EnableInt(LPC_ADC_T *pADC, uint32_t intMask)
{
	Chip_ADC_SetIntBits(pADC, intMask);
}

/* Disable interrupts in ADC (sequencers A/B and overrun) */
void Chip_ADC_DisableInt(LPC_ADC_T *pADC, uint32_t intMask)
{
	Chip_ADC_ClearIntBits(pADC, intMask);
}

/* Enable a threshold event interrupt in ADC */
void Chip_ADC_SetThresholdInt(LPC_ADC_T *pADC, uint8_t ch, ADC_INTEN_THCMP_T thInt)
{
	int shiftIndex = 3 + (ch * 2);

	/* Clear current bits first */
	Chip_ADC_ClearIntBits(pADC, (ADC_INTEN_CMP_MASK << shiftIndex));

	/* Set new threshold interrupt type */
	Chip_ADC_SetIntBits(pADC, ((uint32_t) thInt << shiftIndex));
}

/* Select threshold 0 values for comparison for selected channels */
void Chip_ADC_SelectTH0Channels(LPC_ADC_T *pADC, uint32_t channels)
{
	Chip_ADC_ClearTHRSELBits(pADC, channels);
}

/* Select threshold 1 value for comparison for selected channels */
void Chip_ADC_SelectTH1Channels(LPC_ADC_T *pADC, uint32_t channels)
{
	Chip_ADC_SetTHRSELBits(pADC, channels);
}

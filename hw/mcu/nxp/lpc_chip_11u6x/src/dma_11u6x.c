/*
 * @brief LPC11u6x DMA chip driver
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

/* DMA SRAM table - this can be optionally used with the Chip_DMA_SetSRAMBase()
   function if a DMA SRAM table is needed. This table is correctly aligned for
     the DMA controller. */
#if defined(__CC_ARM)
/* Keil alignement to 256 bytes */
__align(256) DMA_CHDESC_T Chip_DMA_Table[MAX_DMA_CHANNEL];
#endif /* defined (__CC_ARM) */

/* IAR support */
#if defined(__ICCARM__)
/* IAR EWARM alignement to 256 bytes */
#pragma data_alignment=256
DMA_CHDESC_T Chip_DMA_Table[MAX_DMA_CHANNEL];
#endif /* defined (__ICCARM__) */

#if defined( __GNUC__ )
/* GNU alignement to 256 bytes */
DMA_CHDESC_T Chip_DMA_Table[MAX_DMA_CHANNEL] __attribute__ ((aligned(256)));
#endif /* defined (__GNUC__) */

/*****************************************************************************
 * Private functions
 ****************************************************************************/

/*****************************************************************************
 * Public functions
 ****************************************************************************/

/* Set DMA transfer register interrupt bits (safe) */
void Chip_DMA_SetTranBits(LPC_DMA_T *pDMA, DMA_CHID_T ch, uint32_t mask)
{
	uint32_t temp;

	/* Read and write values may not be the same, write 0 to
	   undefined bits */
	temp = pDMA->DMACH[ch].XFERCFG & ~0xFC000CC0;

	pDMA->DMACH[ch].XFERCFG = temp | mask;
}

/* Clear DMA transfer register interrupt bits (safe) */
void Chip_DMA_ClearTranBits(LPC_DMA_T *pDMA, DMA_CHID_T ch, uint32_t mask)
{
	uint32_t temp;

	/* Read and write values may not be the same, write 0 to
	   undefined bits */
	temp = pDMA->DMACH[ch].XFERCFG & ~0xFC000CC0;

	pDMA->DMACH[ch].XFERCFG = temp & ~mask;
}

/* Update the transfer size in an existing DMA channel transfer configuration */
void Chip_DMA_SetupChannelTransferSize(LPC_DMA_T *pDMA, DMA_CHID_T ch, uint32_t trans)
{
	Chip_DMA_ClearTranBits(pDMA, ch, (0x3FF << 16));
	Chip_DMA_SetTranBits(pDMA, ch, DMA_XFERCFG_XFERCOUNT(trans));
}

/* Sets up a DMA channel with the passed DMA transfer descriptor */
bool Chip_DMA_SetupTranChannel(LPC_DMA_T *pDMA, DMA_CHID_T ch, DMA_CHDESC_T *desc)
{
	bool good = false;
	DMA_CHDESC_T *pDesc = (DMA_CHDESC_T *) pDMA->SRAMBASE;

	if ((Chip_DMA_GetActiveChannels(pDMA) & (1 << ch)) == 0) {
		/* Channel is not active, so update the descriptor */
	   pDesc[ch] = *desc;

	   good = true;
   }

   return good;
}

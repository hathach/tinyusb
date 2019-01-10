/*
 * @brief	SD Card Interface registers and control functions
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

#if defined(CHIP_LPC177X_8X) || defined(CHIP_LPC40XX)

/*****************************************************************************
 * Private types/enumerations/variables
 ****************************************************************************/

static void writeDelay(void)
{
	volatile uint8_t i;
	for ( i = 0; i < 0x10; i++ ) {	/* delay 3MCLK + 2PCLK  */
	}
}

/*****************************************************************************
 * Public types/enumerations/variables
 ****************************************************************************/

/*****************************************************************************
 * Private functions
 ****************************************************************************/

/*****************************************************************************
 * Public functions
 ****************************************************************************/

/* Set power state of SDC peripheral */
void Chip_SDC_PowerControl(LPC_SDC_T *pSDC, SDC_PWR_CTRL_T pwrMode, uint32_t flag)
{
	pSDC->POWER = SDC_PWR_CTRL(pwrMode) | flag;
	writeDelay();
}

/* Set clock divider for SDC peripheral */
void Chip_SDC_SetClockDiv(LPC_SDC_T *pSDC, uint8_t div)
{
	uint32_t temp;
	temp = (pSDC->CLOCK & (~SDC_CLOCK_CLKDIV_BITMASK));
	pSDC->CLOCK = temp | (SDC_CLOCK_CLKDIV(div));
	writeDelay();
}

/* Clock control for SDC peripheral*/
void Chip_SDC_ClockControl(LPC_SDC_T *pSDC, SDC_CLOCK_CTRL_T ctrlType,
						   FunctionalState NewState)
{
	if (NewState) {
		pSDC->CLOCK |= (1 << ctrlType);
	}
	else {
		pSDC->CLOCK &= (~(1 << ctrlType));
	}
	writeDelay();
}

/* Initialize SDC peripheral */
static void SDC_Init(LPC_SDC_T *pSDC)
{
	/* Disable SD_CLK */
	Chip_SDC_ClockControl(pSDC, SDC_CLOCK_ENABLE, DISABLE);

	/* Power-off */
	Chip_SDC_PowerControl(pSDC, SDC_POWER_OFF, 0);
	writeDelay();

	/* Disable all interrupts */
	pSDC->MASK0 = 0;

	/*Setting for timeout problem */
	pSDC->DATATIMER = 0x1FFFFFFF;

	pSDC->COMMAND = 0;
	writeDelay();

	pSDC->DATACTRL = 0;
	writeDelay();

	/* clear all pending interrupts */
	pSDC->CLEAR = SDC_CLEAR_ALL;
}

/* Initializes the SDC card controller */
void Chip_SDC_Init(LPC_SDC_T *pSDC)
{
	uint32_t i = 0;
	Chip_Clock_EnablePeriphClock(SYSCTL_CLOCK_SDC);
	Chip_SYSCTL_PeriphReset(SYSCTL_RESET_PCSDC);

	/* Initialize GPDMA controller */
	Chip_GPDMA_Init(LPC_GPDMA);

	/* Initialize SDC peripheral */
	SDC_Init(pSDC);

	/* Power-up SDC Peripheral */
	Chip_SDC_PowerControl(pSDC, SDC_POWER_UP, 0);

	/* delays for the supply output is stable*/
	for ( i = 0; i < 0x80000; i++ ) {}

	Chip_SDC_SetClock(pSDC, SDC_IDENT_CLOCK_RATE);
	Chip_SDC_ClockControl(pSDC, SDC_CLOCK_ENABLE, ENABLE);

	/* Power-on SDC Interface */
	Chip_SDC_PowerControl(pSDC, SDC_POWER_ON, 0);

}

/* Set Command Info */
void Chip_SDC_SetCommand(LPC_SDC_T *pSDC, uint32_t Cmd, uint32_t Arg)
{
	/* Clear status register */
	pSDC->CLEAR = SDC_CLEAR_ALL;

	/* Set the argument first, finally command */
	pSDC->ARGUMENT = Arg;

	/* Write command value, enable the command */
	pSDC->COMMAND = Cmd | SDC_COMMAND_ENABLE;

	writeDelay();
}

/* Reset Command Info */
void Chip_SDC_ResetCommand(LPC_SDC_T *pSDC)
{
	pSDC->CLEAR = SDC_CLEAR_ALL;

	pSDC->ARGUMENT = 0xFFFFFFFF;

	pSDC->COMMAND = 0;

	writeDelay();
}

/* Get Command response */
void Chip_SDC_GetResp(LPC_SDC_T *pSDC, SDC_RESP_T *pResp)
{
	uint8_t i;
	pResp->CmdIndex = SDC_RESPCOMMAND_VAL(pSDC->RESPCMD);
	for (i = 0; i < SDC_CARDSTATUS_BYTENUM; i++) {
		pResp->Data[i] = pSDC->RESPONSE[i];
	}
}

/* Setup Data Transfer Information */
void Chip_SDC_SetDataTransfer(LPC_SDC_T *pSDC, SDC_DATA_TRANSFER_T *pTransfer)
{
	uint32_t DataCtrl = 0;
	pSDC->DATATIMER = pTransfer->Timeout;
	pSDC->DATALENGTH = pTransfer->BlockNum * SDC_DATACTRL_BLOCKSIZE_VAL(pTransfer->BlockSize);
	DataCtrl = SDC_DATACTRL_ENABLE;
	DataCtrl |= ((uint32_t) pTransfer->Dir) | ((uint32_t) pTransfer->Mode) | SDC_DATACTRL_BLOCKSIZE(
		pTransfer->BlockSize);
	if (pTransfer->DMAUsed) {
		DataCtrl |= SDC_DATACTRL_DMA_ENABLE;
	}
	pSDC->DATACTRL = DataCtrl;
	writeDelay();
}

/* Write data to FIFO */
void Chip_SDC_WriteFIFO(LPC_SDC_T *pSDC, uint32_t *pSrc, bool bFirstHalf)
{
	uint8_t start = 0, end = 7;
	if (!bFirstHalf) {
		start += 8;
		end += 8;
	}
	for (; start <= end; start++) {
		pSDC->FIFO[start] = *pSrc;
		pSrc++;
	}
}

/* Read data from FIFO */
void Chip_SDC_ReadFIFO(LPC_SDC_T *pSDC, uint32_t *pDst, bool bFirstHalf)
{
	uint8_t start = 0, end = 7;

	if (!bFirstHalf) {
		start += 8;
		end += 8;
	}
	for (; start <= end; start++) {
		*pDst = pSDC->FIFO[start];
		pDst++;
	}
}

/* Set SD_CLK Clock */
void Chip_SDC_SetClock(LPC_SDC_T *pSDC, uint32_t freq)
{
	uint32_t PClk;
	uint32_t ClkValue = 0;

	PClk = Chip_Clock_GetPeripheralClockRate();

	ClkValue = (PClk + 2 * freq - 1) / (2 * freq);
	if (ClkValue > 0) {
		ClkValue -= 1;
	}
	Chip_SDC_SetClockDiv(pSDC, ClkValue);
}

/* Shutdown the SDC card controller */
void Chip_SDC_DeInit(LPC_SDC_T *pSDC)
{
	/* Power-off */
	Chip_SDC_PowerControl(pSDC, SDC_POWER_OFF, 0);

	Chip_Clock_DisablePeriphClock(SYSCTL_CLOCK_SDC);
}

#endif /* defined(CHIP_LPC177X_8X) || defined(CHIP_LPC4XX) */

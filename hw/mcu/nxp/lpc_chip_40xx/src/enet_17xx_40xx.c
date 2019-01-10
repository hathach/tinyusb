/*
 * @brief LPC17xx/40xx ethernet driver
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

/* Saved address for PHY and clock divider */
STATIC uint32_t phyAddr;

/* Divider index values for the MII PHY clock */
STATIC const uint8_t EnetClkDiv[] = {4, 6, 8, 10, 14, 20, 28, 36, 40, 44,
									 48, 52, 56, 60, 64};

/*****************************************************************************
 * Public types/enumerations/variables
 ****************************************************************************/

/*****************************************************************************
 * Private functions
 ****************************************************************************/

STATIC INLINE void resetENET(LPC_ENET_T *pENET)
{
	volatile uint32_t i;

#if defined(CHIP_LPC177X_8X) || defined(CHIP_LPC40XX)
	Chip_SYSCTL_PeriphReset(SYSCTL_RESET_ENET);
#endif

	/* Reset ethernet peripheral */
	Chip_ENET_Reset(pENET);
	for (i = 0; i < 100; i++) {}
}

/*****************************************************************************
 * Public functions
 ****************************************************************************/

/* Basic Ethernet interface initialization */
void Chip_ENET_Init(LPC_ENET_T *pENET, bool useRMII)
{
	Chip_Clock_EnablePeriphClock(SYSCTL_CLOCK_ENET);
	resetENET(pENET);

	/* Initial MAC configuration for  full duplex,
	   100Mbps, inter-frame gap use default values */
	pENET->MAC.MAC1 = ENET_MAC1_PARF;
	pENET->MAC.MAC2 = ENET_MAC2_FULLDUPLEX | ENET_MAC2_CRCEN | ENET_MAC2_PADCRCEN;

	if (useRMII) {
		pENET->CONTROL.COMMAND = ENET_COMMAND_FULLDUPLEX | ENET_COMMAND_PASSRUNTFRAME | ENET_COMMAND_RMII;
	}
	else {
		pENET->CONTROL.COMMAND = ENET_COMMAND_FULLDUPLEX | ENET_COMMAND_PASSRUNTFRAME;
	}

	pENET->MAC.IPGT = ENET_IPGT_FULLDUPLEX;
	pENET->MAC.IPGR = ENET_IPGR_P2_DEF;
	pENET->MAC.SUPP = ENET_SUPP_100Mbps_SPEED;
	pENET->MAC.MAXF = ENET_ETH_MAX_FLEN;
	pENET->MAC.CLRT = ENET_CLRT_DEF;

	/* Setup default filter */
	pENET->CONTROL.COMMAND |= ENET_COMMAND_PASSRXFILTER;

	/* Clear all MAC interrupts */
	pENET->MODULE_CONTROL.INTCLEAR = 0xFFFF;

	/* Disable MAC interrupts */
	pENET->MODULE_CONTROL.INTENABLE = 0;
}

/* Ethernet interface shutdown */
void Chip_ENET_DeInit(LPC_ENET_T *pENET)
{
	/* Disable packet reception */
	pENET->MAC.MAC1 &= ~ENET_MAC1_RXENABLE;
	pENET->CONTROL.COMMAND = 0;

	/* Clear all MAC interrupts */
	pENET->MODULE_CONTROL.INTCLEAR = 0xFFFF;

	/* Disable MAC interrupts */
	pENET->MODULE_CONTROL.INTENABLE = 0;

	Chip_Clock_DisablePeriphClock(SYSCTL_CLOCK_ENET);
}

/* Sets up the PHY link clock divider and PHY address */
void Chip_ENET_SetupMII(LPC_ENET_T *pENET, uint32_t div, uint8_t addr)
{
	/* Save clock divider and PHY address in MII address register */
	phyAddr = ENET_MADR_PHYADDR(addr);

	/*  Write to MII configuration register and reset */
	pENET->MAC.MCFG = ENET_MCFG_CLOCKSEL(div) | ENET_MCFG_RES_MII;

	/* release reset */
	pENET->MAC.MCFG &= ~(ENET_MCFG_RES_MII);
}

/* Find the divider index for a desired MII clock rate */
uint32_t Chip_ENET_FindMIIDiv(LPC_ENET_T *pENET, uint32_t clockRate)
{
	uint32_t tmp, divIdx = 0;

	/* Find desired divider value */
	tmp = Chip_Clock_GetENETClockRate() / clockRate;

	/* Determine divider index from desired divider */
	for (divIdx = 0; divIdx < (sizeof(EnetClkDiv) / sizeof(EnetClkDiv[0])); divIdx++) {
		/* Closest index, but not higher than desired rate */
		if (EnetClkDiv[divIdx] >= tmp) {
			return divIdx;
		}
	}

	/* Use maximum divider index */
	return (sizeof(EnetClkDiv) / sizeof(EnetClkDiv[0])) - 1;
}

/* Starts a PHY write via the MII */
void Chip_ENET_StartMIIWrite(LPC_ENET_T *pENET, uint8_t reg, uint16_t data)
{
	/* Write value at PHY address and register */
	pENET->MAC.MCMD = 0;
	pENET->MAC.MADR = phyAddr | ENET_MADR_REGADDR(reg);
	pENET->MAC.MWTD = data;
}

/*Starts a PHY read via the MII */
void Chip_ENET_StartMIIRead(LPC_ENET_T *pENET, uint8_t reg)
{
	/* Read value at PHY address and register */
	pENET->MAC.MADR = phyAddr | ENET_MADR_REGADDR(reg);
	pENET->MAC.MCMD = ENET_MCMD_READ;
}

/* Read MII data */
uint16_t Chip_ENET_ReadMIIData(LPC_ENET_T *pENET)
{
	pENET->MAC.MCMD = 0;
	return pENET->MAC.MRDD;
}

/* Sets full duplex for the ENET interface */
void Chip_ENET_SetFullDuplex(LPC_ENET_T *pENET)
{
	pENET->MAC.MAC2 |= ENET_MAC2_FULLDUPLEX;
	pENET->CONTROL.COMMAND |= ENET_COMMAND_FULLDUPLEX;
	pENET->MAC.IPGT = ENET_IPGT_FULLDUPLEX;
}

/* Sets half duplex for the ENET interface */
void Chip_ENET_SetHalfDuplex(LPC_ENET_T *pENET)
{
	pENET->MAC.MAC2 &= ~ENET_MAC2_FULLDUPLEX;
	pENET->CONTROL.COMMAND &= ~ENET_COMMAND_FULLDUPLEX;
	pENET->MAC.IPGT = ENET_IPGT_HALFDUPLEX;
}

/* Configures the initial ethernet transmit descriptors */
void Chip_ENET_InitTxDescriptors(LPC_ENET_T *pENET,
								 ENET_TXDESC_T *pDescs,
								 ENET_TXSTAT_T *pStatus,
								 uint32_t descNum)
{
	/* Setup descriptor list base addresses */
	pENET->CONTROL.TX.DESCRIPTOR = (uint32_t) pDescs;
	pENET->CONTROL.TX.DESCRIPTORNUMBER = descNum - 1;
	pENET->CONTROL.TX.STATUS = (uint32_t) pStatus;
	pENET->CONTROL.TX.PRODUCEINDEX = 0;
}

/* Configures the initial ethernet receive descriptors */
void Chip_ENET_InitRxDescriptors(LPC_ENET_T *pENET,
								 ENET_RXDESC_T *pDescs,
								 ENET_RXSTAT_T *pStatus,
								 uint32_t descNum)
{
	/* Setup descriptor list base addresses */
	pENET->CONTROL.RX.DESCRIPTOR = (uint32_t) pDescs;
	pENET->CONTROL.RX.DESCRIPTORNUMBER = descNum - 1;
	pENET->CONTROL.RX.STATUS = (uint32_t) pStatus;
	pENET->CONTROL.RX.CONSUMEINDEX = 0;
}

/* Get status for the descriptor list */
ENET_BUFF_STATUS_T Chip_ENET_GetBufferStatus(LPC_ENET_T *pENET,
											 uint16_t produceIndex,
											 uint16_t consumeIndex,
											 uint16_t buffSize)
{
	/* Empty descriptor list */
	if (consumeIndex == produceIndex) {
		return ENET_BUFF_EMPTY;
	}

	/* Full descriptor list */
	if ((consumeIndex == 0) &&
		(produceIndex == (buffSize - 1))) {
		return ENET_BUFF_FULL;
	}

	/* Wrap-around, full descriptor list */
	if (consumeIndex == (produceIndex + 1)) {
		return ENET_BUFF_FULL;
	}

	return ENET_BUFF_PARTIAL_FULL;
}

/* Get the number of descriptor filled */
uint32_t Chip_ENET_GetFillDescNum(LPC_ENET_T *pENET, uint16_t produceIndex, uint16_t consumeIndex, uint16_t buffSize)
{
	/* Empty descriptor list */
	if (consumeIndex == produceIndex) {
		return 0;
	}

	if (consumeIndex > produceIndex) {
		return (buffSize - consumeIndex) + produceIndex;
	}

	return produceIndex - consumeIndex;
}

/* Increase the current Tx Produce Descriptor Index */
uint16_t Chip_ENET_IncTXProduceIndex(LPC_ENET_T *pENET)
{
	/* Get current TX produce index */
	uint32_t idx = pENET->CONTROL.TX.PRODUCEINDEX;

	/* Start frame transmission by incrementing descriptor */
	idx++;
	if (idx > pENET->CONTROL.TX.DESCRIPTORNUMBER) {
		idx = 0;
	}
	pENET->CONTROL.TX.PRODUCEINDEX = idx;

	return idx;
}

/* Increase the current Rx Consume Descriptor Index */
uint16_t Chip_ENET_IncRXConsumeIndex(LPC_ENET_T *pENET)
{
	/* Get current RX consume index */
	uint32_t idx = pENET->CONTROL.RX.CONSUMEINDEX;

	/* Consume descriptor */
	idx++;
	if (idx > pENET->CONTROL.RX.DESCRIPTORNUMBER) {
		idx = 0;
	}
	pENET->CONTROL.RX.CONSUMEINDEX = idx;

	return idx;
}

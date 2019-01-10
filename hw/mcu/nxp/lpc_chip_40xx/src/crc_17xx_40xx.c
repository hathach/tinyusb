/*
 * @brief LPC17xx/40xx Cyclic Redundancy Check (CRC) Engine driver
 *
 * @note
 * Copyright(C) NXP Semiconductors, 2014
 * All rights reserved.
 *
 * @par
 * Software that is described herein is for illustrative purposes only
 * which provides customers with programming information regarding the
 * LPC products.  This software is supplied "AS IS" without any warranties of
 * any kind, and NXP Semiconductors and its licenser disclaim any and
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

/*****************************************************************************
 * Public types/enumerations/variables
 ****************************************************************************/

/*****************************************************************************
 * Private functions
 ****************************************************************************/

/*****************************************************************************
 * Public functions
 ****************************************************************************/

/* Sets up the CRC engine with defaults based on the polynomial to be used */
void Chip_CRC_UseDefaultConfig(CRC_POLY_T poly)
{
	switch (poly) {
	case CRC_POLY_CRC16:
		LPC_CRC->MODE = MODE_CFG_CRC16;
		LPC_CRC->SEED = CRC_SEED_CRC16;
		break;

	case CRC_POLY_CRC32:
		LPC_CRC->MODE = MODE_CFG_CRC32;
		LPC_CRC->SEED = CRC_SEED_CRC32;
		break;

	case CRC_POLY_CCITT:
	default:
		LPC_CRC->MODE = MODE_CFG_CCITT;
		LPC_CRC->SEED = CRC_SEED_CCITT;
		break;
	}
}

/* configure CRC engine and compute CCITT checksum from 8-bit data */
uint32_t Chip_CRC_CRC8(const uint8_t *data, uint32_t bytes)
{
	Chip_CRC_UseDefaultConfig(CRC_POLY_CCITT);
	while (bytes > 0) {
		Chip_CRC_Write8(*data);
		data++;
		bytes--;
	}
	return Chip_CRC_Sum();
}

/* Convenience function for computing a standard CRC16 checksum from 16-bit data block */
uint32_t Chip_CRC_CRC16(const uint16_t *data, uint32_t hwords)
{
	Chip_CRC_UseDefaultConfig(CRC_POLY_CRC16);
	while (hwords > 0) {
		Chip_CRC_Write16(*data);
		data++;
		hwords--;
	}
	return Chip_CRC_Sum();
}

/* Convenience function for computing a standard CRC32 checksum from 32-bit data block */
uint32_t Chip_CRC_CRC32(const uint32_t *data, uint32_t words)
{
	Chip_CRC_UseDefaultConfig(CRC_POLY_CRC32);
	while (words > 0) {
		Chip_CRC_Write32(*data);
		data++;
		words--;
	}
	return Chip_CRC_Sum();
}

#endif /* defined(CHIP_LPC177X_8X) || defined(CHIP_LPC40XX) */

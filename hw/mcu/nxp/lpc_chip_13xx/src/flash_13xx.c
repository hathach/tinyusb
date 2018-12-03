/*
 * @brief LPC13xx Flash/EEPROM programming driver
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

/*Read Part Identification number*/
void Chip_FLASH_ReadPartID(FLASH_READ_PART_ID_OUTPUT_T *pOutput)
{
	FLASH_READ_PART_ID_COMMAND_T command;
	command.cmd = FLASH_READ_PART_ID;
	Chip_FLASH_Execute((FLASH_COMMAND_T *) &command, (FLASH_OUTPUT_T *) pOutput);
}

/*Read Boot code version number */
void Chip_FLASH_ReadBootCodeVersion(FLASH_READ_BOOTCODE_VER_OUTPUT_T *pOutput)
{
	FLASH_READ_BOOTCODE_VER_COMMAND_T command;
	command.cmd = FLASH_READ_BOOT_VER;
	Chip_FLASH_Execute((FLASH_COMMAND_T *) &command, (FLASH_OUTPUT_T *) pOutput);
}

/* Reinvoke ISP */
void Chip_FLASH_ReInvokeISP(void)
{
	FLASH_REINVOKE_ISP_COMMAND_T command;
	FLASH_REINVOKE_ISP_OUTPUT_T output;

	command.cmd = FLASH_REINVOKE_ISP;
	Chip_FLASH_Execute((FLASH_COMMAND_T *) &command, (FLASH_OUTPUT_T *) &output);
}

/* Read UID */
void Chip_FLASH_ReadUID(FLASH_READ_UID_OUTPUT_T *pOutput)
{
	FLASH_READ_UID_COMMAND_T command;
	command.cmd = FLASH_READ_UID;
	Chip_FLASH_Execute((FLASH_COMMAND_T *) &command, (FLASH_OUTPUT_T *) pOutput);
}

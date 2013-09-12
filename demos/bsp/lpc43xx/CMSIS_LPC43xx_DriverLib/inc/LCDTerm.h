/**********************************************************************
* $Id$		LCDTerm.h.c			2011-12-06
*//**
* @file		LCDTerm.h.c
* @brief	This is a library that can be used to display text on the LCD of Hitex 1800 board
* @version	1.0
* @date		06. Dec. 2011
* @author	NXP MCU SW Application Team
*
* Copyright(C) 2011, NXP Semiconductor
* All rights reserved.
*
***********************************************************************
* Software that is described herein is for illustrative purposes only
* which provides customers with programming information regarding the
* products. This software is supplied "AS IS" without any warranties.
* NXP Semiconductors assumes no responsibility or liability for the
* use of the software, conveys no license or title under any patent,
* copyright, or mask work right to the product. NXP Semiconductors
* reserves the right to make changes in the software without
* notification. NXP Semiconductors also make no representation or
* warranty that such application will be suitable for the specified
* use without further testing or modification.
* Permission to use, copy, modify, and distribute this software and its
* documentation is hereby granted, under NXP Semiconductors’
* relevant copyright in the software, without fee, provided that it
* is used in conjunction with NXP Semiconductors microcontrollers.  This
* copyright, permission, and disclaimer notice must appear in all copies of
* this code.
**********************************************************************/

#include "lpc43xx_ssp.h"
#ifndef HITEX_LCD_TERM
#define HITEX_LCD_TERM					2
#endif
#define Highlight 	1
#define NoHighlight 0
SSP_DATA_SETUP_Type *InitLCDTerm(void);
void WriteChar(char ch, SSP_DATA_SETUP_Type *xferConfig, uint8_t nHighlight);

/**********************************************************************
* $Id$		lpc43xx_sct.c		2011-06-02
*//**
* @file		lpc43xx_sct.c
* @brief	Contains all functions support for SCT firmware library on lpc43xx
* @version	1.0
* @date		02. June. 2011
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

/* Peripheral group ----------------------------------------------------------- */
/** @addtogroup SCT
 * @{
 */

/* Includes ------------------------------------------------------------------- */
#include "lpc43xx_sct.h"

/* If this source file built with example, the lpc43xx FW library configuration
 * file in each example directory ("lpc43xx_libcfg.h") must be included,
 * otherwise the default FW library configuration file must be included instead
 */
#ifdef __BUILD_WITH_EXAMPLE__
#include "lpc43xx_libcfg.h"
#else
#include "lpc43xx_libcfg_default.h"
#endif /* __BUILD_WITH_EXAMPLE__ */


#ifdef _SCT

/* Public Functions ----------------------------------------------------------- */
/** @addtogroup SCT_Public_Functions
 * @{
 */

/*********************************************************************//**
 * @brief 		Select 16/32 bit SCT counter
 * @param[in]	value 	configuration value for SCT
 * 					- SCT_CONFIG_16BIT_COUNTER	:16-bit counter
 * 					- SCT_CONFIG_32BIT_COUNTER	:32-bit counter
 * @return 		None
 **********************************************************************/
void SCT_Config(uint32_t value)
{
	CHECK_PARAM(PARAM_SCT_CONFIG_COUNTER_TYPE(value));

	LPC_SCT->CONFIG = value;
}

/*********************************************************************//**
* @brief 		Setting SCT control
* @param[in]	value	setting value
* @param[in]	ena 	Enable/disable status
* 					- ENABLE
* 					- DISABLE
* @return 		None
**********************************************************************/
void SCT_ControlSet(uint32_t value, FunctionalState ena)
{
	uint32_t tem;

	CHECK_PARAM(PARAM_FUNCTIONALSTATE(ena));

	tem = LPC_SCT->CTRL_U;

	if(ena == ENABLE)
	{
		tem |= value;
	}
	else
	{
		tem &= (~value);
	}

	LPC_SCT->CTRL_U = tem;

}

/*********************************************************************//**
* @brief 		Set start mode for ADC
* @param[in]	outnum 	number of SCT output, should be: 0..15
* @param[in]	value 	solution value, should be
*  					- SCT_RES_NOCHANGE			:No change
*					- SCT_RES_SET_OUTPUT		:Set output
*					- SCT_RES_CLEAR_OUTPUT		:Clear output
*					- SCT_RES_TOGGLE_OUTPUT		:Toggle output
* @return 		None
*********************************************************************/
void SCT_ConflictResolutionSet(uint8_t outnum, uint8_t value)
{
	uint32_t tem;

	CHECK_PARAM(PARAM_SCT_OUTPUT_NUM(outnum));
	CHECK_PARAM(PARAM_SCT_RES(value));

	tem = LPC_SCT->RES;
	tem &= ~(0x03 << (2*outnum));
	tem |= (value << (2*outnum));
	LPC_SCT->RES = tem;
}

/*********************************************************************//**
* @brief 		Clear SCT event generating interrupt request
* @param[in]	even_num 	SCT event number, should be: 0..15
* @return 		None
*********************************************************************/
void SCT_EventFlagClear(uint8_t even_num)
{
	CHECK_PARAM(PARAM_SCT_EVENT(even_num));

	LPC_SCT->EVFLAG = (1 << (even_num));
}
/**
 * @}
 */

#endif /* _SCT */

/**
 * @}
 */

/* --------------------------------- End Of File ------------------------------ */

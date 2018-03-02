/**********************************************************************
* $Id$		lpc43xx_gpio.c		2011-06-02
*//**
* @file		lpc43xx_gpio.c
* @brief	Contains all functions support for GPIO firmware library
* 			on lpc43xx
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
/** @addtogroup GPIO
 * @{
 */

/* Includes ------------------------------------------------------------------- */
#include "lpc43xx_gpio.h"
#include "lpc_types.h"

/* If this source file built with example, the lpc43xx FW library configuration
 * file in each example directory ("lpc43xx_libcfg.h") must be included,
 * otherwise the default FW library configuration file must be included instead
 */
#ifdef __BUILD_WITH_EXAMPLE__
#include "lpc43xx_libcfg.h"
#else
#include "lpc43xx_libcfg_default.h"
#endif /* __BUILD_WITH_EXAMPLE__ */


#ifdef _GPIO

/* Private Functions ---------------------------------------------------------- */

/* End of Private Functions --------------------------------------------------- */


/* Public Functions ----------------------------------------------------------- */
/** @addtogroup GPIO_Public_Functions
 * @{
 */


/* GPIO ------------------------------------------------------------------------------ */

/*********************************************************************//**
 * @brief		Set Direction for GPIO port.
 * @param[in]	portNum	Port Number value, should be in range from 0 to 4
 * @param[in]	bitValue	Value that contains all bits to set direction,
 * 				in range from 0 to 0xFFFFFFFF.
 * 				example: value 0x5 to set direction for bit 0 and bit 1.
 * @param[in]	dir	Direction value, should be:
 * 					- 0: Input.
 * 					- 1: Output.
 * @return		None
 *
 * Note:
 * All remaining bits that are not activated in bitValue (value '0')
 * will not be effected by this function.
 **********************************************************************/
void GPIO_SetDir(uint8_t portNum, uint32_t bitValue, uint8_t dir)
{
	if (dir)
	{
		LPC_GPIO_PORT->DIR[portNum] |= bitValue;
	} else
		{
		LPC_GPIO_PORT->DIR[portNum] &= ~bitValue;
	}
}


/*********************************************************************//**
 * @brief		Set Value for bits that have output direction on GPIO port.
 * @param[in]	portNum	Port number value, should be in range from 0 to 4
 * @param[in]	bitValue Value that contains all bits on GPIO to set, should
 * 				be in range from 0 to 0xFFFFFFFF.
 * 				example: value 0x5 to set bit 0 and bit 1.
 * @return		None
 *
 * Note:
 * - For all bits that has been set as input direction, this function will
 * not effect.
 * - For all remaining bits that are not activated in bitValue (value '0')
 * will not be effected by this function.
 **********************************************************************/
void GPIO_SetValue(uint8_t portNum, uint32_t bitValue)
{
	LPC_GPIO_PORT->SET[portNum] = bitValue;
}


/*********************************************************************//**
 * @brief		Clear Value for bits that have output direction on GPIO port.
 * @param[in]	portNum	Port number value, should be in range from 0 to 4
 * @param[in]	bitValue Value that contains all bits on GPIO to clear, should
 * 				be in range from 0 to 0xFFFFFFFF.
 * 				example: value 0x5 to clear bit 0 and bit 1.
 * @return		None
 *
 * Note:
 * - For all bits that has been set as input direction, this function will
 * not effect.
 * - For all remaining bits that are not activated in bitValue (value '0')
 * will not be effected by this function.
 **********************************************************************/
void GPIO_ClearValue(uint8_t portNum, uint32_t bitValue)
{
	LPC_GPIO_PORT->CLR[portNum] = bitValue;
}


/*********************************************************************//**
 * @brief		Read Current state on port pin that have input direction of GPIO
 * @param[in]	portNum	Port number to read value, in range from 0 to 4
 * @return		Current value of GPIO port.
 *
 * Note: Return value contain state of each port pin (bit) on that GPIO regardless
 * its direction is input or output.
 **********************************************************************/
uint32_t GPIO_ReadValue(uint8_t portNum)
{
	return LPC_GPIO_PORT->PIN[portNum];
}


#ifdef GPIO_INT
/*********************************************************************//**
 * @brief		Enable GPIO interrupt (just used for P0.0-P0.30, P2.0-P2.13)
 * @param[in]	portNum		Port number to read value, should be: 0 or 2
 * @param[in]	bitValue	Value that contains all bits on GPIO to enable,
 * 				should be in range from 0 to 0xFFFFFFFF.
 * @param[in]	edgeState	state of edge, should be:
 * 					- 0: Rising edge
 * 					- 1: Falling edge
 * @return		None
 **********************************************************************/
void GPIO_IntCmd(uint8_t portNum, uint32_t bitValue, uint8_t edgeState)
{
	if((portNum == 0)&&(edgeState == 0))
		LPC_GPIOINT->IO0IntEnR = bitValue;
	else if ((portNum == 2)&&(edgeState == 0))
		LPC_GPIOINT->IO2IntEnR = bitValue;
	else if ((portNum == 0)&&(edgeState == 1))
		LPC_GPIOINT->IO0IntEnF = bitValue;
	else if ((portNum == 2)&&(edgeState == 1))
		LPC_GPIOINT->IO2IntEnF = bitValue;
	else
		//Error
		while(1);
}


/*********************************************************************//**
 * @brief		Get GPIO Interrupt Status (just used for P0.0-P0.30, P2.0-P2.13)
 * @param[in]	portNum	Port number to read value, should be: 0 or 2
 * @param[in]	pinNum	Pin number, should be: 0..30(with port 0) and 0..13
 * 				(with port 2)
 * @param[in]	edgeState	state of edge, should be:
 * 					- 0	:Rising edge
 * 					- 1	:Falling edge
 * @return		Function status,	could be:
 * 					- ENABLE	:Interrupt has been generated due to a rising edge on P0.0
 * 					- DISABLE	:A rising edge has not been detected on P0.0
 **********************************************************************/
FunctionalState GPIO_GetIntStatus(uint8_t portNum, uint32_t pinNum, uint8_t edgeState)
{
	if((portNum == 0) && (edgeState == 0))//Rising Edge
		return (((LPC_GPIOINT->IO0IntStatR)>>pinNum)& 0x1);
	else if ((portNum == 2) && (edgeState == 0))
		return (((LPC_GPIOINT->IO2IntStatR)>>pinNum)& 0x1);
	else if ((portNum == 0) && (edgeState == 1))//Falling Edge
		return (((LPC_GPIOINT->IO0IntStatF)>>pinNum)& 0x1);
	else if ((portNum == 2) && (edgeState == 1))
		return (((LPC_GPIOINT->IO2IntStatF)>>pinNum)& 0x1);
	else
		//Error
		while(1);
}


/*********************************************************************//**
 * @brief		Clear GPIO interrupt (just used for P0.0-P0.30, P2.0-P2.13)
 * @param[in]	portNum	Port number to read value, should be: 0 or 2
 * @param[in]	bitValue Value that contains all bits on GPIO to enable,
 * 				should be in range from 0 to 0xFFFFFFFF.
 * @return		None
 **********************************************************************/
void GPIO_ClearInt(uint8_t portNum, uint32_t bitValue)
{
	if(portNum == 0)
		LPC_GPIOINT->IO0IntClr = bitValue;
	else if (portNum == 2)
		LPC_GPIOINT->IO2IntClr = bitValue;
	else
		//Invalid portNum
		while(1);
}
#endif


/* FIO word accessible ----------------------------------------------------------------- */
/* Stub function for FIO (word-accessible) style */

/**
 * @brief The same with GPIO_SetDir()
 */
void FIO_SetDir(uint8_t portNum, uint32_t bitValue, uint8_t dir)
{
	GPIO_SetDir(portNum, bitValue, dir);
}

/**
 * @brief The same with GPIO_SetValue()
 */
void FIO_SetValue(uint8_t portNum, uint32_t bitValue)
{
	GPIO_SetValue(portNum, bitValue);
}

/**
 * @brief The same with GPIO_ClearValue()
 */
void FIO_ClearValue(uint8_t portNum, uint32_t bitValue)
{
	GPIO_ClearValue(portNum, bitValue);
}

/**
 * @brief The same with GPIO_ReadValue()
 */
uint32_t FIO_ReadValue(uint8_t portNum)
{
	return (GPIO_ReadValue(portNum));
}


#ifdef GPIO_INT
/**
 * @brief The same with GPIO_IntCmd()
 */
void FIO_IntCmd(uint8_t portNum, uint32_t bitValue, uint8_t edgeState)
{
	GPIO_IntCmd(portNum, bitValue, edgeState);
}

/**
 * @brief The same with GPIO_GetIntStatus()
 */
FunctionalState FIO_GetIntStatus(uint8_t portNum, uint32_t pinNum, uint8_t edgeState)
{
	return (GPIO_GetIntStatus(portNum, pinNum, edgeState));
}

/**
 * @brief The same with GPIO_ClearInt()
 */
void FIO_ClearInt(uint8_t portNum, uint32_t bitValue)
{
	GPIO_ClearInt(portNum, bitValue);
}
#endif


/*********************************************************************//**
 * @brief		Set mask value for bits in FIO port
 * @param[in]	portNum	Port number, in range from 0 to 4
 * @param[in]	bitValue Value that contains all bits in to set, should be
 * 				in range from 0 to 0xFFFFFFFF.
 * @param[in]	maskValue	Mask value contains state value for each bit:
 * 					- 0	:not mask.
 * 					- 1	:mask.
 * @return		None
 *
 * Note:
 * - All remaining bits that are not activated in bitValue (value '0')
 * will not be effected by this function.
 * - After executing this function, in mask register, value '0' on each bit
 * enables an access to the corresponding physical pin via a read or write access,
 * while value '1' on bit (masked) that corresponding pin will not be changed
 * with write access and if read, will not be reflected in the updated pin.
 **********************************************************************/
void FIO_SetMask(uint8_t portNum, uint32_t bitValue, uint8_t maskValue)
{
		if (maskValue)
		{
		LPC_GPIO_PORT->MASK[portNum] |= bitValue;
	} else
		{
		LPC_GPIO_PORT->MASK[portNum] &= ~bitValue;
	}
}

/**
 * @}
 */

#endif /* _GPIO */

/**
 * @}
 */


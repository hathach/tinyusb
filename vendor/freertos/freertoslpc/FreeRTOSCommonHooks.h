/*
 * @brief Common FreeRTOS functions shared among platforms
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

#ifndef __FREERTOSCOMMONHOOKS_H_
#define __FREERTOSCOMMONHOOKS_H_

/** @ingroup FreeRTOS_COMMON
 * @{
 */

/**
 * @brief	Delay for the specified number of milliSeconds
 * @param	ms	: Delay in milliSeconds
 * @return	Nothing
 * @note	Delays the specified number of milliSeoconds using a task delay
 */
void FreeRTOSDelay(uint32_t ms);

/**
 * @brief	FreeRTOS malloc fail hook
 * @return	Nothing
 * @note	This function is alled when a malloc fails to allocate data.
 */
void vApplicationMallocFailedHook(void);

/**
 * @brief	FreeRTOS application idle hook
 * @return	Nothing
 * @note	Calls ARM Wait for Interrupt function to idle core
 */
void vApplicationIdleHook(void);

/**
 * @brief	FreeRTOS stack overflow hook
 * @param	pxTask		: Task handle that overflowed stack
 * @param	pcTaskName	: Task name that overflowed stack
 * @return	Nothing
 * @note	This function is alled when a stack overflow occurs.
 */
void vApplicationStackOverflowHook(xTaskHandle pxTask, signed char *pcTaskName);

/**
 * @brief	FreeRTOS application tick hook
 * @return	Nothing
 * @note	This just returns to the caller.
 */
void vApplicationTickHook(void);

/**
 * @}
 */

#endif /* __FREERTOSCOMMONHOOKS_H_ */

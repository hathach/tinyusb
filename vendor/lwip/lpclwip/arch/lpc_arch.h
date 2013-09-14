/*
 * @brief Architecture specific functions used with the LWIP examples
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

#ifndef __LPC_ARCH_H_
#define __LPC_ARCH_H_

#include "lwip/opt.h"

#ifdef __cplusplus
extern "C"
{
#endif

/** @defgroup NET_LWIP_ARCH Architecture specific functions used with the LWIP examples
 * @ingroup NET_LWIP
 * @{
 */

#if NO_SYS == 1
/**
 * @brief	Enable LWIP tick and interrupt
 * @param	period	: Period of the systick clock
 * @return	Nothing
 * @note	This enables the systick interrupt and sets up the systick rate. This
 * function is only used in standalone systems.
 */
void SysTick_Enable(uint32_t period);

/**
 * @brief	Disable LWIP tick
 * @return	Nothing
 * This disables the systick interrupt. This function is only used in
 * standalone systems.
 */
void SysTick_Disable(void);

/**
 * @brief	Get the current systick time in milliSeconds
 * @return	current systick time in milliSeconds
 * @note	Returns the current systick time in milliSeconds. This function is only
 * used in standalone systems.
 */
uint32_t SysTick_GetMS(void);

#endif

/**
 * @brief	Delay for the specified number of milliSeconds
 * @param	ms	: Time in milliSeconds to delay
 * @return	Nothing
 * @note	For standalone systems. This function will block for the specified
 * number of milliSconds. For RTOS based systems, this function will delay
 * the task by the specified number of milliSeconds.
 */
void msDelay(uint32_t ms);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* __LPC_ARCH_H_ */

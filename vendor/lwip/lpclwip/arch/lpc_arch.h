/**********************************************************************
* $Id$		lpc_arch.h			2011-11-20
*//**
* @file		lpc_arch.h
* @brief	Architecture specific functions used with the LWIP examples
* @version	1.0
* @date		20. Nov. 2011
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
**********************************************************************/

#ifndef __LPC_ARCH_H
#define __LPC_ARCH_H

#include "lwip/opt.h"

#ifdef __cplusplus
extern "C"
{
#endif

/** @ingroup lpc_arch
 */

#if NO_SYS == 1
/** \brief  Enable systick rate and interrupt
 *
 *  This enables the systick interrupt and sets up the systick rate. This
 *  function is only used in standalone systems.
 *
 *  \param[in]   period   Period of the systick clock
 */
void SysTick_Enable(uint32_t period);

/** \brief  Disable systick
 *
 *  This disables the systick interrupt. This function is only used in
 *  standalone systems.
 */
void SysTick_Disable(void);

/** \brief  Get the current systick time in milliSeconds
 *
 *  Returns the current systick time in milliSeconds. This function is only
 *  used in standalone systems.
 *
 *  /returns current systick time in milliSeconds
 */
uint32_t SysTick_GetMS(void);
#endif

/** \brief  Delay for the specified number of milliSeconds
 *
 *  For standalone systems. This function will block for the specified
 *  number of milliSconds. For RTOS based systems, this function will delay
 *  the task by the specified number of milliSeconds.
 *
 *  \param[in]  ms Time in milliSeconds to delay
 */
void msDelay(uint32_t ms);

/**		  
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* __LPC_ARCH_H */

/* --------------------------------- End Of File ------------------------------ */

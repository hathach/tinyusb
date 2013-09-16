/**********************************************************************
* $Id$		lpc18xx_43xx_systick_arch.c			2011-11-20
*//**
* @file		lpc18xx_43xx_systick_arch.c
* @brief	Setups up the system tick to generate a reference timebase
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

#include "lwip/opt.h"

#if NO_SYS == 1

#ifdef LPC43XX
#include "lpc43xx_cgu.h"
#else
#ifdef LPC43XX
#include "lpc18xx_cgu.h"
#else
#error LPC18XX or LPC43XX for target system not defined!
#endif
#endif
#include "lpc_arch.h"

/** @defgroup LPC18xx_43xx_systick	LPC18xx_43xx LWIP (standalone) timer base
 * @ingroup LPC18xx_43xx
 * @{
 */

/* Saved reference period */
static uint32_t saved_period;
 
/* Saved total time in mS since timer was enabled */
static volatile u32_t systick_timems;

/* Enable systick rate and interrupt */
void SysTick_Enable(uint32_t period)
{
	saved_period = period;
	SysTick_Config(CGU_GetPCLKFrequency(CGU_PERIPHERAL_M4CORE) / (1000 / period));
}

/* Disable systick */
void SysTick_Disable(void)
{
  SysTick->CTRL &= ~SysTick_CTRL_ENABLE_Msk;
}

/** \brief  SysTick IRQ handler and timebase management
 *
 *  This function keeps a timebase for the sysTick that can be
 *  used for other functions.
 */
void SysTick_Handler(void)
{
	/* Increment tick count */
	systick_timems += saved_period;
}

/* Get the current systick time in milliSeconds */
uint32_t SysTick_GetMS(void)
{
	return systick_timems;
}

/* Delay for the specified number of milliSeconds */
void msDelay(uint32_t ms)
{
	uint32_t to = ms + systick_timems;

	while (to > systick_timems);
}
#endif

/**
 * @}
 */

 /* --------------------------------- End Of File ------------------------------ */

/**********************************************************************
 * @brief	Setups up the LWIP timebase (tick)
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

#include "lwip/opt.h"

#if NO_SYS == 1

#include "chip.h"
#include "lpc_arch.h"

/** @ingroup NET_LWIP_ARCH
 * @{
 */

/*****************************************************************************
 * Private types/enumerations/variables
 ****************************************************************************/

/* Saved reference period foe standalone mode */
static uint32_t saved_period;

#if (defined(CHIP_LPC43XX) && defined(CORE_M0))

#define RITIMER_IRQn_PRI  (255)

/* RITimer Reload value */
static uint32_t reload_val;
#endif

/* Saved total time in mS since timer was enabled */
static volatile u32_t systick_timems;

/*****************************************************************************
 * Public types/enumerations/variables
 ****************************************************************************/

/* Current system clock rate, mainly used for sysTick */
extern uint32_t SystemCoreClock;

/*****************************************************************************
 * Private functions
 ****************************************************************************/

/*****************************************************************************
 * Public functions
 ****************************************************************************/

#if (defined(CHIP_LPC43XX) && defined(CORE_M0))

/* Enable LWIP tick and interrupt */
void SysTick_Enable(uint32_t period)
{
	saved_period = period;

	/* Clear any pending interrupt */
	Chip_RIT_ClearInt(LPC_RITIMER);

	/* Calculate reload value */
	reload_val = ( SystemCoreClock / ( 1000 / period ) );
	Chip_RIT_SetCOMPVAL(LPC_RITIMER, Chip_RIT_GetCounter(LPC_RITIMER) + reload_val);/* Let it tick */

	/* Set the priority and enable the interrupt */
	NVIC_SetPriority((IRQn_Type) RITIMER_IRQn, RITIMER_IRQn_PRI);
	NVIC_EnableIRQ((IRQn_Type) RITIMER_IRQn);
}

/* Disable LWIP tick */
void SysTick_Disable(void)
{
	Chip_RIT_Disable(LPC_RITIMER);
}

/**
 * @brief	RITimer IRQ handler and timebase management
 * @return	Nothing
 * @note	This function keeps a timebase for LWIP that can be
 * used for other functions.
 */
void RIT_IRQHandler(void)
{
	/* Clear RITimer Interrupt, Reload counter value */
	Chip_RIT_ClearInt(LPC_RITIMER);
	Chip_RIT_SetCOMPVAL(LPC_RITIMER, Chip_RIT_GetCounter(LPC_RITIMER) + reload_val);/* Reload value */

	/* Increment tick count */
	systick_timems += saved_period;
}

#else

/* Enable LWIP tick and interrupt */
void SysTick_Enable(uint32_t period)
{
	saved_period = period;
	SysTick_Config((SystemCoreClock * period) / 1000);
}

/* Disable LWIP tick */
void SysTick_Disable(void)
{
	SysTick->CTRL &= ~SysTick_CTRL_ENABLE_Msk;
}

/**
 * @brief	SysTick IRQ handler and timebase management
 * @return	Nothing
 * @note	This function keeps a timebase for LWIP that can be
 * used for other functions.
 */
void SysTick_Handler(void)
{
	/* Increment tick count */
	systick_timems += saved_period;
}

#endif

/* Get the current systick time in milliSeconds */
uint32_t SysTick_GetMS(void)
{
	return systick_timems;
}

/* Delay for the specified number of milliSeconds */
void msDelay(uint32_t ms)
{
	uint32_t to = ms + systick_timems;

	while (to > systick_timems) {}
}

/**
 * @brief	LWIP standalone mode time support
 * @return	Returns the current time in mS
 * @note	Returns the current time in mS. This is needed for the LWIP timers
 */
u32_t sys_now(void)
{
	return (u32_t) SysTick_GetMS();
}

/**
 * @}
 */

#endif /* NO_SYS == 1 */

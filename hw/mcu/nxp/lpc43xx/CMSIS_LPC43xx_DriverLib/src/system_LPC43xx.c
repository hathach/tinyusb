/**********************************************************************
* $Id$		system_lpc43xx.c			2012-05-21
*//**
* @file		system_lpc43xx.c
* @brief	Cortex-M3 Device System Source File for NXP lpc43xx Series.
* @version	1.0
* @date		21. May. 2011
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
* documentation is hereby granted, under NXP Semiconductors'
* relevant copyright in the software, without fee, provided that it
* is used in conjunction with NXP Semiconductors microcontrollers.  This
* copyright, permission, and disclaimer notice must appear in all copies of
* this code.
**********************************************************************/

#include "LPC43xx.h"
#if !defined(__CODE_RED)
#include "fpu_enable.h"
#endif

// CodeRed - call clock init code by default
#ifdef __CODE_RED
#include "lpc43xx_cgu.h"
#endif

/*----------------------------------------------------------------------------
  Define clocks
 *----------------------------------------------------------------------------*/
#define __IRC            (12000000UL)    /* IRC Oscillator frequency          */

/*----------------------------------------------------------------------------
  Clock Variable definitions
 *----------------------------------------------------------------------------*/
uint32_t SystemCoreClock = __IRC;		/*!< System Clock Frequency (Core Clock)*/

extern uint32_t getPC(void);

/**
 * Initialize the system
 *
 * @param  none
 * @return none
 *
 * @brief  Setup the microcontroller system.
 *         Initialize the System.
 */
void SystemInit (void)
{
    uint32_t org;

#if !defined(__CODE_RED)
#if defined(CORE_M4) && defined(USE_FPU)
        fpuEnable();
#endif
#endif

#if !defined(CORE_M0)
// Set up Cortex_M3 or M4 VTOR register to point to vector table
// This code uses a toolchain defined symbol to locate the vector table
// If this is not completed, interrupts are likely to cause an exception.
	unsigned int * pSCB_VTOR = (unsigned int *) 0xE000ED08;
#if defined(__IAR_SYSTEMS_ICC__)
	extern void *__vector_table;

	org = *pSCB_VTOR = (unsigned int)&__vector_table;
#elif defined(__CODE_RED)
	extern void *g_pfnVectors;

	// CodeRed - correct to assign address of variable not contents
	// org = *pSCB_VTOR = (unsigned int)g_pfnVectors;
	org = *pSCB_VTOR = (unsigned int)&g_pfnVectors;
#elif defined(__ARMCC_VERSION)
	extern void *__Vectors;

	org = *pSCB_VTOR = (unsigned int)&__Vectors;
#else
#error Unknown compiler
#endif
#else
// Cortex M0?
	#error Cannot configure VTOR on Cortex_M0
#endif

// LPC18xx/LPC43xx ROM sets the PLL to run from IRC and drive the part
// at 96 MHz out of reset
    SystemCoreClock = 96000000;

// In case we are running from external flash, (booted by boot rom)
// We enable the EMC buffer to improve performance.
    if(org == 0x1C000000)
    {
    /*Enable Buffer for External Flash*/
    LPC_EMC->STATICCONFIG0 |= 1<<19;
}

// CodeRed - call clock init code by default
#ifdef __CODE_RED
    // Call clock initialisation code
    CGU_Init();
#endif

// In case we are running from internal flash, we configure the flash
// accelerator. This is a conservative value that should work up to 204
// MHz on the LPC43xx or 180 MHz on the LPC18xx. This value may change
// as the chips are characterized and should also change based on
// core clock speed.
#define FLASH_ACCELERATOR_SPEED 6
#ifdef INTERNAL_FLASH
	{
		uint32_t *MAM,t;

		// Set up flash controller for both banks
		// Bank A
		MAM = (uint32_t *)(LPC_CREG_BASE + 0x120);
		t=*MAM;
		t &= ~(0xF<<12);
		*MAM = t | (FLASH_ACCELERATOR_SPEED<<12);
		// Bank B
		MAM = (uint32_t *)(LPC_CREG_BASE + 0x124);
		t=*MAM;
		t &= ~(0xF<<12);
		*MAM = t | (FLASH_ACCELERATOR_SPEED<<12);
	}
#endif
}

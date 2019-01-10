/*
 * @brief LPC17xx/40xx Chip specific SystemInit
 *
 * @note
 * Copyright(C) NXP Semiconductors, 2014
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
#if defined(CHIP_LPC175X_6X)
void Chip_SetupIrcClocking(void)
{
	/* Disconnect the Main PLL if it is connected already */
	if (Chip_Clock_IsMainPLLConnected()) {
		Chip_Clock_DisablePLL(SYSCTL_MAIN_PLL, SYSCTL_PLL_CONNECT);
	}

	/* Disable the PLL if it is enabled */
	if (Chip_Clock_IsMainPLLEnabled()) {
		Chip_Clock_DisablePLL(SYSCTL_MAIN_PLL, SYSCTL_PLL_ENABLE);
	}

	Chip_Clock_SetCPUClockDiv(0);
	Chip_Clock_SetMainPLLSource(SYSCTL_PLLCLKSRC_IRC);

	/* FCCO = ((44+1) * 2 * 4MHz) / (0+1) = 360MHz */
	Chip_Clock_SetupPLL(SYSCTL_MAIN_PLL, 44, 0);

	Chip_Clock_EnablePLL(SYSCTL_MAIN_PLL, SYSCTL_PLL_ENABLE);

	Chip_Clock_SetCPUClockDiv(2);
	while (!Chip_Clock_IsMainPLLLocked()) {} /* Wait for the PLL to Lock */

	Chip_Clock_EnablePLL(SYSCTL_MAIN_PLL, SYSCTL_PLL_CONNECT);
}

void Chip_SetupXtalClocking(void)
{
	/* Disconnect the Main PLL if it is connected already */
	if (Chip_Clock_IsMainPLLConnected()) {
		Chip_Clock_DisablePLL(SYSCTL_MAIN_PLL, SYSCTL_PLL_CONNECT);
	}

	/* Disable the PLL if it is enabled */
	if (Chip_Clock_IsMainPLLEnabled()) {
		Chip_Clock_DisablePLL(SYSCTL_MAIN_PLL, SYSCTL_PLL_ENABLE);
	}

	/* Enable the crystal */
	if (!Chip_Clock_IsCrystalEnabled())
		Chip_Clock_EnableCrystal();
	while(!Chip_Clock_IsCrystalEnabled()) {}

	/* Set PLL0 Source to Crystal Oscillator */
	Chip_Clock_SetCPUClockDiv(0);
	Chip_Clock_SetMainPLLSource(SYSCTL_PLLCLKSRC_MAINOSC);

	/* FCCO = ((15+1) * 2 * 12MHz) / (0+1) = 384MHz */
	Chip_Clock_SetupPLL(SYSCTL_MAIN_PLL, 15, 0);

	Chip_Clock_EnablePLL(SYSCTL_MAIN_PLL, SYSCTL_PLL_ENABLE);

	/* 384MHz / (3+1) = 96MHz */
	Chip_Clock_SetCPUClockDiv(3);
	while (!Chip_Clock_IsMainPLLLocked()) {} /* Wait for the PLL to Lock */

	Chip_Clock_EnablePLL(SYSCTL_MAIN_PLL, SYSCTL_PLL_CONNECT);
}
#endif

#if (defined(CHIP_LPC177X_8X) | defined(CHIP_LPC40XX))
/* Clock and PLL initialization based on the internal oscillator */
void Chip_SetupIrcClocking(void)
{
	/* Clock the CPU from SYSCLK, in case if it is clocked by PLL0 */
	Chip_Clock_SetCPUClockSource(SYSCTL_CCLKSRC_SYSCLK);

	/* Disable the PLL if it is enabled */
	if (Chip_Clock_IsMainPLLEnabled()) {
		Chip_Clock_DisablePLL(SYSCTL_MAIN_PLL, SYSCTL_PLL_ENABLE);
	}

	/* It is safe to switch the PLL Source to IRC */
	Chip_Clock_SetMainPLLSource(SYSCTL_PLLCLKSRC_IRC);

	/* FCCO = 12MHz * (9+1) * 2 * (0+1) = 240MHz */
	/* Fout = FCCO / ((0+1) * 2) = 120MHz */
	Chip_Clock_SetupPLL(SYSCTL_MAIN_PLL, 9, 0);

	Chip_Clock_EnablePLL(SYSCTL_MAIN_PLL, SYSCTL_PLL_ENABLE);
	Chip_Clock_SetCPUClockDiv(1);
	while (!Chip_Clock_IsMainPLLLocked()) {} /* Wait for the PLL to Lock */
	Chip_Clock_SetCPUClockSource(SYSCTL_CCLKSRC_MAINPLL);

	/* Peripheral clocking will be derived from PLL0 with a divider of 2 (60MHz) */
	Chip_Clock_SetPCLKDiv(2);
}

/* Clock and PLL initialization based on the external oscillator */
void Chip_SetupXtalClocking(void)
{
	/* Enable the crystal */
	if (!Chip_Clock_IsCrystalEnabled())
		Chip_Clock_EnableCrystal();

	while(!Chip_Clock_IsCrystalEnabled()) {}

	/* Clock the CPU from SYSCLK, in case if it is clocked by PLL0 */
	Chip_Clock_SetCPUClockSource(SYSCTL_CCLKSRC_SYSCLK);

	/* Disable the PLL if it is enabled */
	if (Chip_Clock_IsMainPLLEnabled()) {
		Chip_Clock_DisablePLL(SYSCTL_MAIN_PLL, SYSCTL_PLL_ENABLE);
	}

	/* It is safe to switch the PLL Source to Crystal Oscillator */
	Chip_Clock_SetMainPLLSource(SYSCTL_PLLCLKSRC_MAINOSC);

	/* FCCO = 12MHz * (9+1) * 2 * (0+1) = 240MHz */
	/* Fout = FCCO / ((0+1) * 2) = 120MHz */
	Chip_Clock_SetupPLL(SYSCTL_MAIN_PLL, 9, 0);

	Chip_Clock_EnablePLL(SYSCTL_MAIN_PLL, SYSCTL_PLL_ENABLE);
	Chip_Clock_SetCPUClockDiv(1);

	while (!Chip_Clock_IsMainPLLLocked()) {} /* Wait for the PLL to Lock */
	Chip_Clock_SetCPUClockSource(SYSCTL_CCLKSRC_MAINPLL);

	/* Peripheral clocking will be derived from PLL0 with a divider of 2 (60MHz) */
	Chip_Clock_SetPCLKDiv(2);
}
#endif

/* Set up and initialize hardware prior to call to main */
void Chip_SystemInit(void)
{
	/* Setup Chip clocking */
	Chip_SetupIrcClocking();
}

/*
 * @brief LPC17xx/40xx Miscellaneous chip specific functions
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

/* System Clock Frequency (Core Clock) */
uint32_t SystemCoreClock;

/*****************************************************************************
 * Private functions
 ****************************************************************************/

/*****************************************************************************
 * Public functions
 ****************************************************************************/

/* Update system core clock rate, should be called if the system has
   a clock rate change */
void SystemCoreClockUpdate(void)
{
	/* CPU core speed */
	SystemCoreClock = Chip_Clock_GetSystemClockRate();
}

/* Sets up USB PLL, all needed clocks and enables USB PHY on the chip. USB pins which are
	muxed to different pads are not initialized here. This routine assumes that the XTAL 
	OSC is enabled and running prior to this call. */
void Chip_USB_Init(void)
{

#if defined(CHIP_LPC175X_6X)
	/* Setup USB PLL1 for a 48MHz clock
	   Input clock rate (FIN) is main oscillator = 12MHz
	   PLL1 Output = USBCLK = 48MHz = FIN * MSEL, so MSEL = 4.
	   FCCO = USBCLK = USBCLK * 2 * P. It must be between 156 MHz to 320 MHz.
	   so P = 2 and FCCO = 48MHz * 2 * 2 = 192MHz */
	Chip_Clock_SetupPLL(SYSCTL_USB_PLL, 3, 1);	/* Multiply by 4, Divide by 2 */

	/* Use PLL1 output as USB Clock Source */
	/* Enable PLL1 */
	Chip_Clock_EnablePLL(SYSCTL_USB_PLL, SYSCTL_PLL_ENABLE);

	/* Wait for PLL1 to lock */
	while (!Chip_Clock_IsUSBPLLLocked()) {}

	/* Connect PLL1 */
	Chip_Clock_EnablePLL(SYSCTL_USB_PLL, SYSCTL_PLL_ENABLE | SYSCTL_PLL_CONNECT);

	/* Wait for PLL1 to be connected */
	while (!Chip_Clock_IsUSBPLLConnected()) {}

#else

	/* Select XTAL as clock source for USB block and divider as 1 */
	LPC_SYSCTL->USBCLKSEL = 0x1;
	/* Setup USB PLL1 for a 48MHz clock
	   Input clock rate (FIN) is main oscillator = 12MHz
	   PLL output = 48MHz = FIN * MSEL, so MSEL = 4
	   FCCO must be between 156 MHz to 320 MHz, where FCCO = PLL output * 2 * P,
	   so P = 2 and FCCO = 48MHz * 2 * 2 = 192MHz */
	Chip_Clock_SetupPLL(SYSCTL_USB_PLL, 3, 1);  

	/* Wait for USB PLL to lock */
	while ((Chip_Clock_GetPLLStatus(SYSCTL_USB_PLL) & SYSCTL_PLLSTS_LOCKED) == 0) {}

	/* Select PLL1/USBPLL as clock source for USB block and divider as 1 */
	LPC_SYSCTL->USBCLKSEL = (SYSCTL_USBCLKSRC_USBPLL << 8) | 0x01;

#endif /* defined(CHIP_LPC175X_6X) */

	/* Enable AHB clock to the USB block and USB RAM. */
	Chip_Clock_EnablePeriphClock(SYSCTL_CLOCK_USB);

}

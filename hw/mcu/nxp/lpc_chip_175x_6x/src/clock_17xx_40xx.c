/*
 * @brief	LPC17xx/40xx System and Control driver
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

/* Enables or connects a PLL */
void Chip_Clock_EnablePLL(CHIP_SYSCTL_PLL_T PLLNum, uint32_t flags) {
	uint32_t temp;

	temp = LPC_SYSCTL->PLL[PLLNum].PLLCON;
	temp |= flags;
	LPC_SYSCTL->PLL[PLLNum].PLLCON = temp;
	Chip_Clock_FeedPLL(PLLNum);
}

/* Disables or disconnects a PLL */
void Chip_Clock_DisablePLL(CHIP_SYSCTL_PLL_T PLLNum, uint32_t flags) {
	uint32_t temp;

	temp = LPC_SYSCTL->PLL[PLLNum].PLLCON;
	temp &= ~flags;
	LPC_SYSCTL->PLL[PLLNum].PLLCON = temp;
	Chip_Clock_FeedPLL(PLLNum);
}

/* Sets up a PLL */
void Chip_Clock_SetupPLL(CHIP_SYSCTL_PLL_T PLLNum, uint32_t msel, uint32_t psel) {
	uint32_t PLLcfg;

#if defined(CHIP_LPC175X_6X)
	/* PLL0 and PLL1 are slightly different */
	if (PLLNum == SYSCTL_MAIN_PLL) {
		PLLcfg = (msel) | (psel << 16);
	}
	else {
		PLLcfg = (msel) | (psel << 5);
	}

#else
	PLLcfg = (msel) | (psel << 5);
#endif

	LPC_SYSCTL->PLL[PLLNum].PLLCFG = PLLcfg;
	LPC_SYSCTL->PLL[PLLNum].PLLCON = 0x1;
	Chip_Clock_FeedPLL(PLLNum);
}

/* Enables power and clocking for a peripheral */
void Chip_Clock_EnablePeriphClock(CHIP_SYSCTL_CLOCK_T clk) {
	uint32_t bs = (uint32_t) clk;

#if defined(CHIP_LPC40XX)
	if (bs >= 32) {
		LPC_SYSCTL->PCONP1 |= (1 << (bs - 32));
	}
	else {
		LPC_SYSCTL->PCONP |= (1 << bs);
	}
#else
	LPC_SYSCTL->PCONP |= (1 << bs);
#endif
}

/* Disables power and clocking for a peripheral */
void Chip_Clock_DisablePeriphClock(CHIP_SYSCTL_CLOCK_T clk) {
	uint32_t bs = (uint32_t) clk;

#if defined(CHIP_LPC40XX)
	if (bs >= 32) {
		LPC_SYSCTL->PCONP1 &= ~(1 << (bs - 32));
	}
	else {
		LPC_SYSCTL->PCONP |= ~(1 << bs);
	}
#else
	LPC_SYSCTL->PCONP |= ~(1 << bs);
#endif
}

/* Returns power enables state for a peripheral */
bool Chip_Clock_IsPeripheralClockEnabled(CHIP_SYSCTL_CLOCK_T clk)
{
	uint32_t bs = (uint32_t) clk;

#if defined(CHIP_LPC40XX)
	if (bs >= 32) {
		bs = LPC_SYSCTL->PCONP1 & (1 << (bs - 32));
	}
	else {
		bs = LPC_SYSCTL->PCONP & (1 << bs);
	}
#else
	bs = LPC_SYSCTL->PCONP & (1 << bs);
#endif

	return (bool) (bs != 0);
}

/* Sets the current CPU clock source */
void Chip_Clock_SetCPUClockSource(CHIP_SYSCTL_CCLKSRC_T src)
{
#if defined(CHIP_LPC175X_6X)
	/* LPC175x/6x CPU clock source is based on PLL connect status */
	if (src == SYSCTL_CCLKSRC_MAINPLL) {
		/* Connect PLL0 */
		Chip_Clock_EnablePLL(SYSCTL_MAIN_PLL, SYSCTL_PLL_CONNECT);
	}
	else {
		Chip_Clock_DisablePLL(SYSCTL_MAIN_PLL, SYSCTL_PLL_CONNECT);
	}
#else
	/* LPC177x/8x and 407x/8x CPU clock source is based on CCLKSEL */
	if (src == SYSCTL_CCLKSRC_MAINPLL) {
		/* Connect PLL0 */
		LPC_SYSCTL->CCLKSEL |= (1 << 8);
	}
	else {
		LPC_SYSCTL->CCLKSEL &= ~(1 << 8);
	}
#endif
}

/* Returns the current CPU clock source */
CHIP_SYSCTL_CCLKSRC_T Chip_Clock_GetCPUClockSource(void)
{
	CHIP_SYSCTL_CCLKSRC_T src;
#if defined(CHIP_LPC175X_6X)
	/* LPC175x/6x CPU clock source is based on PLL connect status */
	if (Chip_Clock_IsMainPLLConnected()) {
		src = SYSCTL_CCLKSRC_MAINPLL;
	}
	else {
		src = SYSCTL_CCLKSRC_SYSCLK;
	}
#else
	/* LPC177x/8x and 407x/8x CPU clock source is based on CCLKSEL */
	if (LPC_SYSCTL->CCLKSEL & (1 << 8)) {
		src = SYSCTL_CCLKSRC_MAINPLL;
	}
	else {
		src = SYSCTL_CCLKSRC_SYSCLK;
	}
#endif

	return src;
}

/* Selects the CPU clock divider */
void Chip_Clock_SetCPUClockDiv(uint32_t div)
{
#if defined(CHIP_LPC175X_6X)
	LPC_SYSCTL->CCLKSEL = div;
#else
	uint32_t temp;

	/* Save state of CPU clock source bit */
	temp = LPC_SYSCTL->CCLKSEL & (1 << 8);
	LPC_SYSCTL->CCLKSEL = temp | div;
#endif
}

/* Gets the CPU clock divider */
uint32_t Chip_Clock_GetCPUClockDiv(void)
{
#if defined(CHIP_LPC175X_6X)
	return (LPC_SYSCTL->CCLKSEL & 0xFF) + 1;
#else
	return LPC_SYSCTL->CCLKSEL & 0x1F;
#endif
}

#if !defined(CHIP_LPC175X_6X)
/* Selects the USB clock divider source */
void Chip_Clock_SetUSBClockSource(CHIP_SYSCTL_USBCLKSRC_T src)
{
	uint32_t temp;

	/* Mask out current source, but keep divider */
	temp = LPC_SYSCTL->USBCLKSEL & ~(0x3 << 8);
	LPC_SYSCTL->USBCLKSEL = temp | (((uint32_t) src) << 8);
}

#endif

/* Sets the USB clock divider */
void Chip_Clock_SetUSBClockDiv(uint32_t div)
{
	uint32_t temp;

	/* Mask out current divider */
#if defined(CHIP_LPC175X_6X)
	temp = LPC_SYSCTL->USBCLKSEL & ~(0xF);
#else
	temp = LPC_SYSCTL->USBCLKSEL & ~(0x1F);
#endif
	LPC_SYSCTL->USBCLKSEL = temp | div;
}

/* Gets the USB clock divider */
uint32_t Chip_Clock_GetUSBClockDiv(void)
{
#if defined(CHIP_LPC175X_6X)
	return (LPC_SYSCTL->USBCLKSEL & 0xF) + 1;
#else
	return (LPC_SYSCTL->USBCLKSEL & 0x1F) + 1;
#endif
}

#if defined(CHIP_LPC175X_6X)
/* Selects a clock divider for a peripheral */
void Chip_Clock_SetPCLKDiv(CHIP_SYSCTL_PCLK_T clk, CHIP_SYSCTL_CLKDIV_T div)
{
	uint32_t temp, bitIndex, regIndex = (uint32_t) clk;

	/* Get register array index and clock index into the register */
	bitIndex = ((regIndex % 16) * 2);
	regIndex = regIndex / 16;

	/* Mask and update register */
	temp = LPC_SYSCTL->PCLKSEL[regIndex] & ~(0x3 << bitIndex);
	temp |= (((uint32_t) div) << bitIndex);
	LPC_SYSCTL->PCLKSEL[regIndex] = temp;
}

/* Gets a clock divider for a peripheral */
uint32_t Chip_Clock_GetPCLKDiv(CHIP_SYSCTL_PCLK_T clk)
{
	uint32_t div = 1, bitIndex, regIndex = ((uint32_t) clk) * 2;

	/* Get register array index and clock index into the register */
	bitIndex = regIndex % 32;
	regIndex = regIndex / 32;

	/* Mask and update register */
	div = LPC_SYSCTL->PCLKSEL[regIndex];
	div = (div >> bitIndex) & 0x3;
	if (div == SYSCTL_CLKDIV_4) {
		div = 4;
	}
	else if (div == SYSCTL_CLKDIV_1) {
		div = 1;
	}
	else if (div == SYSCTL_CLKDIV_2) {
		div = 2;
	}
	else {
		/* Special case for CAN clock divider */
		if ((clk == SYSCTL_PCLK_CAN1) || (clk == SYSCTL_PCLK_CAN2) || (clk == SYSCTL_PCLK_ACF)) {
			div = 6;
		}
		else {
			div = 8;
		}
	}

	return div;
}

#endif

/* Selects a source clock and divider rate for the CLKOUT pin */
void Chip_Clock_SetCLKOUTSource(CHIP_SYSCTL_CLKOUTSRC_T src,
								uint32_t div)
{
	uint32_t temp;

	temp = LPC_SYSCTL->CLKOUTCFG & ~0x1FF;
	temp |= ((uint32_t) src) | ((div - 1) << 4);
	LPC_SYSCTL->CLKOUTCFG = temp;
}

/* Returns the current SYSCLK clock rate */
uint32_t Chip_Clock_GetSYSCLKRate(void)
{
	/* Determine clock input rate to SYSCLK based on input selection */
	switch (Chip_Clock_GetMainPLLSource()) {
	case (uint32_t) SYSCTL_PLLCLKSRC_IRC:
		return Chip_Clock_GetIntOscRate();

	case (uint32_t) SYSCTL_PLLCLKSRC_MAINOSC:
		return Chip_Clock_GetMainOscRate();

#if defined(CHIP_LPC175X_6X)
	case (uint32_t) SYSCTL_PLLCLKSRC_RTC:
		return Chip_Clock_GetRTCOscRate();
#endif
	}
	return 0;
}

/* Returns the main PLL output clock rate */
uint32_t Chip_Clock_GetMainPLLOutClockRate(void)
{
	uint32_t clkhr = 0;

#if defined(CHIP_LPC175X_6X)
	/* Only valid if enabled */
	if (Chip_Clock_IsMainPLLEnabled()) {
		uint32_t msel, nsel;

		/* PLL0 rate is (FIN * 2 * MSEL) / NSEL, get MSEL and NSEL */
		msel = 1 + (LPC_SYSCTL->PLL[SYSCTL_MAIN_PLL].PLLCFG & 0x7FFF);
		nsel = 1 + ((LPC_SYSCTL->PLL[SYSCTL_MAIN_PLL].PLLCFG >> 16) & 0xFF);
		clkhr = (Chip_Clock_GetMainPLLInClockRate() * 2 * msel) / nsel;
	}
#else
	if (Chip_Clock_IsMainPLLEnabled()) {
		uint32_t msel;

		/* PLL0 rate is (FIN * MSEL) */
		msel = 1 + (LPC_SYSCTL->PLL[SYSCTL_MAIN_PLL].PLLCFG & 0x1F);
		clkhr = (Chip_Clock_GetMainPLLInClockRate() * msel);
	}
#endif

	return (uint32_t) clkhr;
}

/* Get USB output clock rate */
uint32_t Chip_Clock_GetUSBPLLOutClockRate(void)
{
	uint32_t clkhr = 0;

	/* Only valid if enabled */
	if (Chip_Clock_IsUSBPLLEnabled()) {
		uint32_t msel;

		/* PLL1 input clock (FIN) is always main oscillator */
		/* PLL1 rate is (FIN * MSEL) */
		msel = 1 + (LPC_SYSCTL->PLL[SYSCTL_USB_PLL].PLLCFG & 0x1F);
		clkhr = (Chip_Clock_GetUSBPLLInClockRate() * msel);
	}

	return (uint32_t) clkhr;
}

/* Get the main clock rate */
/* On 175x/6x devices, this is the input clock to the CPU divider.
   Additionally, on 177x/8x and 407x/8x devices, this is also the
   input clock to the peripheral divider. */
uint32_t Chip_Clock_GetMainClockRate(void)
{
	switch (Chip_Clock_GetCPUClockSource()) {
	case SYSCTL_CCLKSRC_MAINPLL:
		return Chip_Clock_GetMainPLLOutClockRate();

	case SYSCTL_CCLKSRC_SYSCLK:
		return Chip_Clock_GetSYSCLKRate();

	default:
		return 0;
	}
}

/* Get CCLK rate */
uint32_t Chip_Clock_GetSystemClockRate(void)
{
	return Chip_Clock_GetMainClockRate() / Chip_Clock_GetCPUClockDiv();
}

/* Returns the USB clock (USB_CLK) rate */
uint32_t Chip_Clock_GetUSBClockRate(void)
{
	uint32_t div, clkrate;
#if defined(CHIP_LPC175X_6X)
	/* The USB clock rate is derived from PLL1 or PLL0 */
	if (Chip_Clock_IsUSBPLLConnected()) {
		/* Use PLL1 clock for USB source with divider of 1 */
		clkrate = Chip_Clock_GetUSBPLLOutClockRate();
		div = 1;
	}
	else {
		clkrate = Chip_Clock_GetMainClockRate();
		div = Chip_Clock_GetUSBClockDiv();
	}

#else
	/* Get clock from source drving USB */
	switch (Chip_Clock_GetUSBClockSource()) {
	case SYSCTL_USBCLKSRC_SYSCLK:
	default:
		clkrate = Chip_Clock_GetSYSCLKRate();
		break;

	case SYSCTL_USBCLKSRC_MAINPLL:
		clkrate = Chip_Clock_GetMainPLLOutClockRate();
		break;

	case SYSCTL_USBCLKSRC_USBPLL:
		clkrate = Chip_Clock_GetUSBPLLOutClockRate();
		break;
	}

	div = Chip_Clock_GetUSBClockDiv();
#endif

	return clkrate / div;
}

#if !defined(CHIP_LPC175X_6X)
/* Selects the SPIFI clock divider source */
void Chip_Clock_SetSPIFIClockSource(CHIP_SYSCTL_SPIFICLKSRC_T src)
{
	uint32_t temp;

	/* Mask out current source, but keep divider */
	temp = LPC_SYSCTL->SPIFICLKSEL & ~(0x3 << 8);
	LPC_SYSCTL->SPIFICLKSEL = temp | (((uint32_t) src) << 8);
}

/* Sets the SPIFI clock divider */
void Chip_Clock_SetSPIFIClockDiv(uint32_t div)
{
	uint32_t temp;

	/* Mask out current divider */
	temp = LPC_SYSCTL->SPIFICLKSEL & ~(0x1F);
	LPC_SYSCTL->SPIFICLKSEL = temp | div;
}

/* Returns the SPIFI clock rate */
uint32_t Chip_Clock_GetSPIFIClockRate(void)
{
	uint32_t div, clkrate;

	/* Get clock from source drving USB */
	switch (Chip_Clock_GetSPIFIClockSource()) {
	case SYSCTL_SPIFICLKSRC_SYSCLK:
	default:
		clkrate = Chip_Clock_GetSYSCLKRate();
		break;

	case SYSCTL_SPIFICLKSRC_MAINPLL:
		clkrate = Chip_Clock_GetMainPLLOutClockRate();
		break;

	case SYSCTL_SPIFICLKSRC_USBPLL:
		clkrate = Chip_Clock_GetUSBPLLOutClockRate();
		break;
	}

	div = Chip_Clock_GetSPIFIClockDiv();

	return clkrate / div;
}

#endif

#if defined(CHIP_LPC175X_6X)
/* Returns the clock rate for a peripheral */
uint32_t Chip_Clock_GetPeripheralClockRate(CHIP_SYSCTL_PCLK_T clk) {
	/* 175x/6x clock is derived from CPU clock with CPU divider */
	return Chip_Clock_GetSystemClockRate() / Chip_Clock_GetPCLKDiv(clk);
}

#else
/* Returns the clock rate for all peripherals */
uint32_t Chip_Clock_GetPeripheralClockRate(void)
{
	uint32_t clkrate = 0, div;

	/* Get divider, a divider of 0 means the clock is disabled */
	div = Chip_Clock_GetPCLKDiv();
	if (div != 0) {
		/* Derived from periperhal clock input and peripheral clock divider */
		clkrate = Chip_Clock_GetMainClockRate() / div;
	}

	return clkrate;
}

#endif

/*
 * @brief LPC11U6X Clock control functions
 *
 * @note
 * Copyright(C) NXP Semiconductors, 2013
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

#ifndef __CLOCK_11U6X_H_
#define __CLOCK_11U6X_H_

#ifdef __cplusplus
extern "C" {
#endif

/** @defgroup CLOCK_11U6X CHIP: LPC11u6x Clock Control block driver
 * @ingroup CHIP_11U6X_Drivers
 * @{
 */

/** Internal oscillator frequency */
#define SYSCTL_IRC_FREQ (12000000)

/**
 * @brief	Set System PLL divider values
 * @param	msel    : PLL feedback divider value. M = msel + 1.
 * @param	psel    : PLL post divider value. P =  (1<<psel).
 * @return	Nothing
 * @note	See the user manual for how to setup the PLL.
 */
STATIC INLINE void Chip_Clock_SetupSystemPLL(uint8_t msel, uint8_t psel)
{
	LPC_SYSCTL->SYSPLLCTRL = (msel & 0x1F) | ((psel & 0x3) << 5);
}

/**
 * @brief	Read System PLL lock status
 * @return	true of the PLL is locked. false if not locked
 */
STATIC INLINE bool Chip_Clock_IsSystemPLLLocked(void)
{
	return (bool) ((LPC_SYSCTL->SYSPLLSTAT & 1) != 0);
}

/**
 * Clock sources for system PLL
 */
typedef enum CHIP_SYSCTL_PLLCLKSRC {
	SYSCTL_PLLCLKSRC_IRC = 0,		/*!< Internal oscillator in */
	SYSCTL_PLLCLKSRC_MAINOSC,		/*!< Crystal (main) oscillator in */
	SYSCTL_PLLCLKSRC_SYSOSC = SYSCTL_PLLCLKSRC_MAINOSC,
	SYSCTL_PLLCLKSRC_RESERVED1,		/*!< Reserved */
	SYSCTL_PLLCLKSRC_RTC32K,		/*!< RTC 32KHz clock */
} CHIP_SYSCTL_PLLCLKSRC_T;

/**
 * @brief	Set System PLL clock source
 * @param	src	: Clock source for system PLL
 * @return	Nothing
 * @note	This function will also toggle the clock source update register
 * to update the clock source.
 */
void Chip_Clock_SetSystemPLLSource(CHIP_SYSCTL_PLLCLKSRC_T src);

/**
 * @brief	Set USB PLL divider values
 * @param	msel    : PLL feedback divider value. M = msel + 1.
 * @param	psel    : PLL post divider value. P = (1<<psel).
 * @return	Nothing
 * @note	See the user manual for how to setup the PLL.
 */
STATIC INLINE void Chip_Clock_SetupUSBPLL(uint8_t msel, uint8_t psel)
{
	LPC_SYSCTL->USBPLLCTRL = (msel & 0x1F) | ((psel & 0x3) << 5);
}

/**
 * @brief	Read USB PLL lock status
 * @return	true of the PLL is locked. false if not locked
 */
STATIC INLINE bool Chip_Clock_IsUSBPLLLocked(void)
{
	return (bool) ((LPC_SYSCTL->USBPLLSTAT & 1) != 0);
}

/**
 * Clock sources for USB PLL
 */
typedef enum CHIP_SYSCTL_USBPLLCLKSRC {
	SYSCTL_USBPLLCLKSRC_IRC = 0,		/*!< Internal oscillator in */
	SYSCTL_USBPLLCLKSRC_MAINOSC,		/*!< Crystal (main) oscillator in */
	SYSCTL_USBPLLCLKSRC_SYSOSC = SYSCTL_USBPLLCLKSRC_MAINOSC,
	SYSCTL_USBPLLCLKSRC_RESERVED1,		/*!< Reserved */
	SYSCTL_USBPLLCLKSRC_RESERVED2,		/*!< Reserved */
} CHIP_SYSCTL_USBPLLCLKSRC_T;

/**
 * @brief	Set USB PLL clock source
 * @param	src	: Clock source for USB PLL
 * @return	Nothing
 * @note	This function will also toggle the clock source update register
 * to update the clock source.
 */
void Chip_Clock_SetUSBPLLSource(CHIP_SYSCTL_USBPLLCLKSRC_T src);

/**
 * @brief	Bypass System Oscillator and set oscillator frequency range
 * @param	bypass	: Flag to bypass oscillator
 * @param	highfr	: Flag to set oscillator range from 15-25 MHz
 * @return	Nothing
 * @note	Sets the PLL input to bypass the oscillator. This would be
 * used if an external clock that is not an oscillator is attached
 * to the XTALIN pin.
 */
void Chip_Clock_SetPLLBypass(bool bypass, bool highfr);

/**
 * @brief	Enable the RTC 32KHz output
 * @return	Nothing
 * @note	This clock can be used for the main clock directly, but
 *			do not use this clock with the system PLL.
 */
STATIC INLINE void Chip_Clock_EnableRTCOsc(void)
{
	LPC_SYSCTL->RTCOSCCTRL  = 1;
}

/**
 * @brief	Disable the RTC 32KHz output
 * @return	Nothing
 */
STATIC INLINE void Chip_Clock_DisableRTCOsc(void)
{
	LPC_SYSCTL->RTCOSCCTRL  = 0;
}

/**
 * Watchdog and low frequency oscillator frequencies plus or minus 40%
 */
typedef enum CHIP_WDTLFO_OSC {
	WDTLFO_OSC_ILLEGAL,
	WDTLFO_OSC_0_60,	/*!< 0.6 MHz watchdog/LFO rate */
	WDTLFO_OSC_1_05,	/*!< 1.05 MHz watchdog/LFO rate */
	WDTLFO_OSC_1_40,	/*!< 1.4 MHz watchdog/LFO rate */
	WDTLFO_OSC_1_75,	/*!< 1.75 MHz watchdog/LFO rate */
	WDTLFO_OSC_2_10,	/*!< 2.1 MHz watchdog/LFO rate */
	WDTLFO_OSC_2_40,	/*!< 2.4 MHz watchdog/LFO rate */
	WDTLFO_OSC_2_70,	/*!< 2.7 MHz watchdog/LFO rate */
	WDTLFO_OSC_3_00,	/*!< 3.0 MHz watchdog/LFO rate */
	WDTLFO_OSC_3_25,	/*!< 3.25 MHz watchdog/LFO rate */
	WDTLFO_OSC_3_50,	/*!< 3.5 MHz watchdog/LFO rate */
	WDTLFO_OSC_3_75,	/*!< 3.75 MHz watchdog/LFO rate */
	WDTLFO_OSC_4_00,	/*!< 4.0 MHz watchdog/LFO rate */
	WDTLFO_OSC_4_20,	/*!< 4.2 MHz watchdog/LFO rate */
	WDTLFO_OSC_4_40,	/*!< 4.4 MHz watchdog/LFO rate */
	WDTLFO_OSC_4_60		/*!< 4.6 MHz watchdog/LFO rate */
} CHIP_WDTLFO_OSC_T;

/**
 * @brief	Setup Watchdog oscillator rate and divider
 * @param	wdtclk	: Selected watchdog clock rate
 * @param	div		: Watchdog divider value, even value between 2 and 64
 * @return	Nothing
 * @note	Watchdog rate = selected rate divided by divider rate
 */
STATIC INLINE void Chip_Clock_SetWDTOSC(CHIP_WDTLFO_OSC_T wdtclk, uint8_t div)
{
	LPC_SYSCTL->WDTOSCCTRL  = (((uint32_t) wdtclk) << 5) | ((div >> 1) - 1);
}

/**
 * Clock sources for main system clock
 */
typedef enum CHIP_SYSCTL_MAINCLKSRC {
	SYSCTL_MAINCLKSRC_IRC = 0,		/*!< Internal oscillator */
	SYSCTL_MAINCLKSRC_PLLIN,		/*!< System PLL input */
	SYSCTL_MAINCLKSRC_WDTOSC,		/*!< Watchdog oscillator rate */
	SYSCTL_MAINCLKSRC_PLLOUT,		/*!< System PLL output */
} CHIP_SYSCTL_MAINCLKSRC_T;

/**
 * @brief	Set main system clock source
 * @param	src	: Clock source for main system
 * @return	Nothing
 * @note	This function will also toggle the clock source update register
 * to update the clock source.
 */
void Chip_Clock_SetMainClockSource(CHIP_SYSCTL_MAINCLKSRC_T src);

/**
 * @brief   Returns the main clock source
 * @return	Which clock is used for the core clock source?
 */
STATIC INLINE CHIP_SYSCTL_MAINCLKSRC_T Chip_Clock_GetMainClockSource(void)
{
	return (CHIP_SYSCTL_MAINCLKSRC_T) (LPC_SYSCTL->MAINCLKSEL);
}

/**
 * @brief	Set system clock divider
 * @param	div	: divider for system clock
 * @return	Nothing
 * @note	Use 0 to disable, or a divider value of 1 to 255. The system clock
 * rate is the main system clock divided by this value.
 */
STATIC INLINE void Chip_Clock_SetSysClockDiv(uint32_t div)
{
	LPC_SYSCTL->SYSAHBCLKDIV  = div;
}

/**
 * System and peripheral clocks
 */
typedef enum CHIP_SYSCTL_CLOCK {
	SYSCTL_CLOCK_SYS = 0,				/*!< 0: System clock */
	SYSCTL_CLOCK_ROM,					/*!< 1:  ROM clock */
	SYSCTL_CLOCK_RAM0,					/*!< 2: RAM0 clock */
	SYSCTL_CLOCK_FLASHREG,				/*!< 3: FLASH register interface clock */
	SYSCTL_CLOCK_FLASHARRAY,			/*!< 4: FLASH array access clock */
	SYSCTL_CLOCK_I2C0,					/*!< 5: I2C0 clock */
	SYSCTL_CLOCK_GPIO,					/*!< 6: GPIO clock */
	SYSCTL_CLOCK_CT16B0,				/*!< 7: 16-bit Counter/timer 0 clock */
	SYSCTL_CLOCK_CT16B1,				/*!< 8: 16-bit Counter/timer 1 clock */
	SYSCTL_CLOCK_CT32B0,				/*!< 9: 32-bit Counter/timer 0 clock */
	SYSCTL_CLOCK_CT32B1,				/*!< 10: 32-bit Counter/timer 1 clock */
	SYSCTL_CLOCK_SSP0,					/*!< 11: SSP0 clock */
	SYSCTL_CLOCK_UART0,					/*!< 12: UART0 clock */
	SYSCTL_CLOCK_ADC,					/*!< 13: ADC clock */
	SYSCTL_CLOCK_USB,					/*!< 14: USB clock */
	SYSCTL_CLOCK_WDT,					/*!< 15: Watchdog timer clock */
	SYSCTL_CLOCK_IOCON,					/*!< 16: IOCON block clock */
	SYSCTL_CLOCK_RESERVED17,			/*!< 17: Reserved */
	SYSCTL_CLOCK_SSP1,					/*!< 18: SSP1 clock */
	SYSCTL_CLOCK_PINT,					/*!< 19: GPIO Pin int register interface clock */
	SYSCTL_CLOCK_USART1,				/*!< 20: USART1 clock */
	SYSCTL_CLOCK_USART2,				/*!< 21: USART2 clock */
	SYSCTL_CLOCK_USART3_4,				/*!< 22: USART3_4 clock */
	SYSCTL_CLOCK_P0INT,					/*!< 23: GPIO GROUP1 interrupt register clock */
	SYSCTL_CLOCK_GROUP0INT = SYSCTL_CLOCK_P0INT,/*!< 23: GPIO GROUP0 interrupt register interface clock */
	SYSCTL_CLOCK_P1INT,					/*!< 24: GPIO GROUP1 interrupt register clock */
	SYSCTL_CLOCK_GROUP1INT = SYSCTL_CLOCK_P1INT,/*!< 24: GPIO GROUP1 interrupt register interface clock */
	SYSCTL_CLOCK_I2C1,					/*!< 25: I2C1 clock */
	SYSCTL_CLOCK_RAM1,					/*!< 26: SRAM block clock */
	SYSCTL_CLOCK_USBRAM,				/*!< 27: USB SRAM block clock */
	SYSCTL_CLOCK_CRC,					/*!< 25: CRC clock */
	SYSCTL_CLOCK_DMA,					/*!< 25: DMA clock */
	SYSCTL_CLOCK_RTC,					/*!< 25: RTC clock */
	SYSCTL_CLOCK_SCT0_1,				/*!< 25: SCT 0/1 clock */
} CHIP_SYSCTL_CLOCK_T;

/**
 * @brief	Enable a system or peripheral clock
 * @param	clk	: Clock to enable
 * @return	Nothing
 */
STATIC INLINE void Chip_Clock_EnablePeriphClock(CHIP_SYSCTL_CLOCK_T clk)
{
	LPC_SYSCTL->SYSAHBCLKCTRL |= (1 << clk);
}

/**
 * @brief	Disable a system or peripheral clock
 * @param	clk	: Clock to disable
 * @return	Nothing
 */
STATIC INLINE void Chip_Clock_DisablePeriphClock(CHIP_SYSCTL_CLOCK_T clk)
{
	LPC_SYSCTL->SYSAHBCLKCTRL &= ~(1 << clk);
}

/**
 * @brief	Set SSP0 divider
 * @param	div	: divider for SSP0 clock
 * @return	Nothing
 * @note	Use 0 to disable, or a divider value of 1 to 255. The SSP0 clock
 * rate is the main system clock divided by this value.
 */
STATIC INLINE void Chip_Clock_SetSSP0ClockDiv(uint32_t div)
{
	LPC_SYSCTL->SSP0CLKDIV  = div;
}

/**
 * @brief	Return SSP0 divider
 * @return	divider for SSP0 clock
 * @note	A value of 0 means the clock is disabled.
 */
STATIC INLINE uint32_t Chip_Clock_GetSSP0ClockDiv(void)
{
	return LPC_SYSCTL->SSP0CLKDIV;
}

/**
 * @brief	Set USART0 divider clock
 * @param	div	: divider for UART clock
 * @return	Nothing
 * @note	Use 0 to disable, or a divider value of 1 to 255. The UART clock
 * rate is the main system clock divided by this value.
 */
STATIC INLINE void Chip_Clock_SetUSART0ClockDiv(uint32_t div)
{
	LPC_SYSCTL->USART0CLKDIV  = div;
}

/**
 * @brief	Return USART0 divider
 * @return	divider for UART clock
 * @note	A value of 0 means the clock is disabled.
 */
STATIC INLINE uint32_t Chip_Clock_GetUASRT0ClockDiv(void)
{
	return LPC_SYSCTL->USART0CLKDIV;
}

/**
 * @brief	Set SSP1 divider clock
 * @param	div	: divider for SSP1 clock
 * @return	Nothing
 * @note	Use 0 to disable, or a divider value of 1 to 255. The SSP1 clock
 * rate is the main system clock divided by this value.
 */
STATIC INLINE void Chip_Clock_SetSSP1ClockDiv(uint32_t div)
{
	LPC_SYSCTL->SSP1CLKDIV  = div;
}

/**
 * @brief	Return SSP1 divider
 * @return	divider for SSP1 clock
 * @note	A value of 0 means the clock is disabled.
 */
STATIC INLINE uint32_t Chip_Clock_GetSSP1ClockDiv(void)
{
	return LPC_SYSCTL->SSP1CLKDIV;
}

/**
 * @brief	Set USART 1/2/3/4 UART base rate (up to main clock rate)
 * @param	rate	: Desired rate for fractional divider/multipler output
 * @param	fEnable	: true to use fractional clocking, false for integer clocking
 * @return	Actual rate generated
 * @note	USARTs 1 - 4 use the same base clock for their baud rate
 *			basis. This function is used to generate that clock, while the
 *			UART driver's SetBaud functions will attempt to get the closest
 *			baud rate from this base clock without altering it. This needs
 *			to be setup prior to individual UART setup.<br>
 *			UARTs need a base clock 16x faster than the baud rate, so if you
 *			need a 115.2Kbps baud rate, you will need a clock rate of at
 *			least (115.2K * 16). The UART base clock is generated from the
 *			main system clock, so fractional clocking may be the only
 *			possible choice when using a low main system clock frequency.
 *			Do not alter the FRGCLKDIV register after this call.
 */
uint32_t Chip_Clock_SetUSARTNBaseClockRate(uint32_t rate, bool fEnable);

/**
 * @brief	Get USART 1/2/3/4 UART base rate
 * @return	USART 1/2/3/4 UART base rate
 */
uint32_t Chip_Clock_GetUSARTNBaseClockRate(void);

/**
 * @brief	Set USART 1/2/3/4 fractional baud rate divider clock
 * @param	div	: divider for USART 1/2/3/4 fractional baud rate clock
 * @return	Nothing
 * @note	Use 0 to disable, or a divider value of 1 to 255.
 * This does not affect USART0.
 */
STATIC INLINE void Chip_Clock_SetUSARTNBaseClockDiv(uint8_t div)
{
	LPC_SYSCTL->FRGCLKDIV = (uint32_t) div;
}

/**
 * @brief	Return USART 1/2/3/4 fractional baud rate divider
 * @return	divider for USART 1/2/3/4 fractional baud rate clock
 * @note	A value of 0 means the clock is disabled.
 * This does not affect USART0.
 */
STATIC INLINE uint32_t Chip_Clock_GetUSARTNBaseClockDiv(void)
{
	return LPC_SYSCTL->FRGCLKDIV;
}

/**
 * @brief	Set The USART Fractional Generator Divider
 * @param   div  :  Fractional Generator Divider value, should be 0xFF
 * @return	Nothing
 */
STATIC INLINE void Chip_Clock_SetUSARTNFRGDivider(uint8_t div)
{
	LPC_SYSCTL->UARTFRGDIV = (uint32_t) div;
}

/**
 * @brief	Set The USART Fractional Generator Divider
 * @return	Value of USART Fractional Generator Divider
 */
STATIC INLINE uint32_t Chip_Clock_GetUSARTNFRGDivider(void)
{
	return LPC_SYSCTL->UARTFRGDIV;
}

/**
 * @brief	Set The USART Fractional Generator Multiplier
 * @param   mult  :  An 8-bit value (0-255) U_PCLK = UARTCLKDIV/(1 + MULT/256)
 * @return	Nothing
 */
STATIC INLINE void Chip_Clock_SetUSARTNFRGMultiplier(uint8_t mult)
{
	LPC_SYSCTL->UARTFRGMULT = (uint32_t) mult;
}

/**
 * @brief	Get The USART Fractional Generator Multiplier
 * @return	Value of USART Fractional Generator Multiplier
 */
STATIC INLINE uint32_t Chip_Clock_GetUSARTNFRGMultiplier(void)
{
	return LPC_SYSCTL->UARTFRGMULT;
}

/**
 * Clock sources for USB
 */
typedef enum CHIP_SYSCTL_USBCLKSRC {
	SYSCTL_USBCLKSRC_PLLOUT = 0,	/*!< USB PLL out */
	SYSCTL_USBCLKSRC_MAINSYSCLK,	/*!< Main system clock */
} CHIP_SYSCTL_USBCLKSRC_T;

/**
 * @brief	Set USB clock source and divider
 * @param	src	: Clock source for USB
 * @param	div	: divider for USB clock
 * @return	Nothing
 * @note	Use 0 to disable, or a divider value of 1 to 255. The USB clock
 * rate is either the main system clock or USB PLL output clock divided
 * by this value. This function will also toggle the clock source
 * update register to update the clock source.
 */
void Chip_Clock_SetUSBClockSource(CHIP_SYSCTL_USBCLKSRC_T src, uint32_t div);

/**
 * Clock sources for CLKOUT
 */
typedef enum CHIP_SYSCTL_CLKOUTSRC {
	SYSCTL_CLKOUTSRC_IRC = 0,		/*!< Internal oscillator for CLKOUT */
	SYSCTL_CLKOUTSRC_MAINOSC,		/*!< Main oscillator for CLKOUT */
	SYSCTL_CLKOUTSRC_SYSOSC = SYSCTL_CLKOUTSRC_MAINOSC,
	SYSCTL_CLKOUTSRC_WDTOSC,		/*!< Watchdog oscillator for CLKOUT */
	SYSCTL_CLKOUTSRC_MAINSYSCLK,	/*!< Main system clock for CLKOUT */
} CHIP_SYSCTL_CLKOUTSRC_T;

/**
 * @brief	Set CLKOUT clock source and divider
 * @param	src	: Clock source for CLKOUT
 * @param	div	: divider for CLKOUT clock
 * @return	Nothing
 * @note	Use 0 to disable, or a divider value of 1 to 255. The CLKOUT clock
 * rate is the clock source divided by the divider. This function will
 * also toggle the clock source update register to update the clock
 * source.
 */
void Chip_Clock_SetCLKOUTSource(CHIP_SYSCTL_CLKOUTSRC_T src, uint32_t div);

/**
 * @brief	Set IOCON glitch filter clock divider value
 * @param	index	: IOCON divider index (0 - 6) to set
 * @param	div		: value for IOCON filter divider value
 * @return	Nothing
 * @note	Use 0 to disable, or a divider value of 1 to 255.
 */
STATIC INLINE void Chip_Clock_SetIOCONFiltClockDiv(int index, uint32_t div)
{
	LPC_SYSCTL->IOCONCLKDIV[6 - index]  = div;
}

/**
 * @brief	Return IOCON glitch filter clock divider value
 * @param	index	: IOCON divider index (0 - 6) to get
 * @return	IOCON glitch filter clock divider value
 */
STATIC INLINE uint32_t Chip_Clock_GetIOCONFiltClockDiv(int index)
{
	return LPC_SYSCTL->IOCONCLKDIV[6 - index];
}

/**
 * @brief	Returns the main oscillator clock rate
 * @return	main oscillator clock rate in Hz
 */
STATIC INLINE uint32_t Chip_Clock_GetMainOscRate(void)
{
	return OscRateIn;
}

/**
 * @brief	Returns the internal oscillator (IRC) clock rate
 * @return	internal oscillator (IRC) clock rate in Hz
 */
STATIC INLINE uint32_t Chip_Clock_GetIntOscRate(void)
{
	return SYSCTL_IRC_FREQ;
}

/**
 * @brief	Returns the RTC clock rate
 * @return	RTC oscillator clock rate in Hz
 */
STATIC INLINE uint32_t Chip_Clock_GetRTCOscRate(void)
{
	return RTCOscRateIn;
}

/**
 * @brief	Return estimated watchdog oscillator rate
 * @return	Estimated watchdog oscillator rate
 * @note	This rate is accurate to plus or minus 40%.
 */
uint32_t Chip_Clock_GetWDTOSCRate(void);

/**
 * @brief	Return System PLL input clock rate
 * @return	System PLL input clock rate
 */
uint32_t Chip_Clock_GetSystemPLLInClockRate(void);

/**
 * @brief	Return System PLL output clock rate
 * @return	System PLL output clock rate
 */
uint32_t Chip_Clock_GetSystemPLLOutClockRate(void);

/**
 * @brief	Return USB PLL input clock rate
 * @return	USB PLL input clock rate
 */
uint32_t Chip_Clock_GetUSBPLLInClockRate(void);

/**
 * @brief	Return USB PLL output clock rate
 * @return	USB PLL output clock rate
 */
uint32_t Chip_Clock_GetUSBPLLOutClockRate(void);

/**
 * @brief	Return main clock rate
 * @return	main clock rate
 */
uint32_t Chip_Clock_GetMainClockRate(void);

/**
 * @brief	Return system clock rate
 * @return	system clock rate
 */
uint32_t Chip_Clock_GetSystemClockRate(void);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* __CLOCK_11U6X_H_ */

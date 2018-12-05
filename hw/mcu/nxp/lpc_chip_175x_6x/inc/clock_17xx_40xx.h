/*
 * @brief LPC17XX/40XX Clock control functions
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

#ifndef __CLOCK_17XX_40XX_H_
#define __CLOCK_17XX_40XX_H_

#ifdef __cplusplus
extern "C" {
#endif

/** @defgroup CLOCK_17XX_40XX CHIP: LPC17xx/40xx Clock Driver
 * @ingroup CHIP_17XX_40XX_Drivers
 * @{
 */

#define SYSCTL_OSCRANGE_15_25 (1 << 4)	/*!< SCS register - main oscillator range 15 to 25MHz */
#define SYSCTL_OSCEC          (1 << 5)	/*!< SCS register - main oscillator enable */
#define SYSCTL_OSCSTAT        (1 << 6)	/*!< SCS register - main oscillator is ready status */

/*!< Internal oscillator frequency */
#if defined(CHIP_LPC175X_6X)
#define SYSCTL_IRC_FREQ (4000000)
#else
#define SYSCTL_IRC_FREQ (12000000)
#endif

#define SYSCTL_PLL_ENABLE   (1 << 0)/*!< PLL enable flag */
#if defined(CHIP_LPC175X_6X)
#define SYSCTL_PLL_CONNECT (1 << 1)	/*!< PLL connect flag only applies to 175x/6x */
#endif

/**
 * @brief	Enables or connects a PLL
 * @param	PLLNum:	PLL number
 * @param	flags:	SYSCTL_PLL_ENABLE or SYSCTL_PLL_CONNECT
 * @return	Nothing
 * @note	This will also perform a PLL feed sequence. Connect only applies to the
 * LPC175x/6x devices.
 */
void Chip_Clock_EnablePLL(CHIP_SYSCTL_PLL_T PLLNum, uint32_t flags);

/**
 * @brief	Disables or disconnects a PLL
 * @param	PLLNum:	PLL number
 * @param	flags:	SYSCTL_PLL_ENABLE or SYSCTL_PLL_CONNECT
 * @return	Nothing
 * @note	This will also perform a PLL feed sequence. Connect only applies to the
 * LPC175x/6x devices.
 */
void Chip_Clock_DisablePLL(CHIP_SYSCTL_PLL_T PLLNum, uint32_t flags);

/**
 * @brief	Sets up a PLL
 * @param	PLLNum:		PLL number
 * @param	msel:	PLL Multiplier value (Must be pre-decremented)
 * @param	psel:	PLL Divider value (Must be pre-decremented)
 * @note		See the User Manual for limitations on these values for stable PLL
 * operation. Be careful with these values - they must be safe values for the
 * msl, nsel, and psel registers so must be already decremented by 1 or the
 * the correct value for psel (0 = div by 1, 1 = div by 2, etc.).
 * @return	Nothing
 */
void Chip_Clock_SetupPLL(CHIP_SYSCTL_PLL_T PLLNum, uint32_t msel, uint32_t psel);

#if defined(CHIP_LPC175X_6X)
#define SYSCTL_PLL0STS_ENABLED   (1 << 24)	/*!< PLL0 enable flag */
#define SYSCTL_PLL0STS_CONNECTED (1 << 25)	/*!< PLL0 connect flag */
#define SYSCTL_PLL0STS_LOCKED    (1 << 26)	/*!< PLL0 connect flag */
#define SYSCTL_PLL1STS_ENABLED   (1 << 8)	/*!< PLL1 enable flag */
#define SYSCTL_PLL1STS_CONNECTED (1 << 9)	/*!< PLL1 connect flag */
#define SYSCTL_PLL1STS_LOCKED    (1 << 10)	/*!< PLL1 connect flag */
#else
#define SYSCTL_PLLSTS_ENABLED   (1 << 8)	/*!< PLL enable flag */
#define SYSCTL_PLLSTS_LOCKED    (1 << 10)	/*!< PLL connect flag */
#endif

/**
 * @brief	Returns PLL status
 * @param	PLLNum:		PLL number
 * @return	Current enabled flags, Or'ed SYSCTL_PLLSTS_* states
 * @note	Note flag positions for PLL0 and PLL1 differ on the LPC175x/6x devices.
 */
STATIC INLINE uint32_t Chip_Clock_GetPLLStatus(CHIP_SYSCTL_PLL_T PLLNum)
{
	return LPC_SYSCTL->PLL[PLLNum].PLLSTAT;
}

/**
 * @brief	Read PLL0 enable status
 * @return	true of the PLL0 is enabled. false if not enabled
 */
STATIC INLINE bool Chip_Clock_IsMainPLLEnabled(void)
{
#if defined(CHIP_LPC175X_6X)
	return (bool) ((LPC_SYSCTL->PLL[0].PLLSTAT & SYSCTL_PLL0STS_ENABLED) != 0);
#else
	return (bool) ((LPC_SYSCTL->PLL[0].PLLSTAT & SYSCTL_PLLSTS_ENABLED) != 0);
#endif
}

/**
 * @brief	Read PLL1 enable status
 * @return	true of the PLL1 is enabled. false if not enabled
 */
STATIC INLINE bool Chip_Clock_IsUSBPLLEnabled(void)
{
#if defined(CHIP_LPC175X_6X)
	return (bool) ((LPC_SYSCTL->PLL[1].PLLSTAT & SYSCTL_PLL1STS_ENABLED) != 0);
#else
	return (bool) ((LPC_SYSCTL->PLL[1].PLLSTAT & SYSCTL_PLLSTS_ENABLED) != 0);
#endif
}

/**
 * @brief	Read PLL0 lock status
 * @return	true of the PLL0 is locked. false if not locked
 */
STATIC INLINE bool Chip_Clock_IsMainPLLLocked(void)
{
#if defined(CHIP_LPC175X_6X)
	return (bool) ((LPC_SYSCTL->PLL[0].PLLSTAT & SYSCTL_PLL0STS_LOCKED) != 0);
#else
	return (bool) ((LPC_SYSCTL->PLL[0].PLLSTAT & SYSCTL_PLLSTS_LOCKED) != 0);
#endif
}

/**
 * @brief	Read PLL1 lock status
 * @return	true of the PLL1 is locked. false if not locked
 */
STATIC INLINE bool Chip_Clock_IsUSBPLLLocked(void)
{
#if defined(CHIP_LPC175X_6X)
	return (bool) ((LPC_SYSCTL->PLL[1].PLLSTAT & SYSCTL_PLL1STS_LOCKED) != 0);
#else
	return (bool) ((LPC_SYSCTL->PLL[1].PLLSTAT & SYSCTL_PLLSTS_LOCKED) != 0);
#endif
}

#if defined(CHIP_LPC175X_6X)
/**
 * @brief	Read PLL0 connect status
 * @return	true of the PLL0 is connected. false if not connected
 */
STATIC INLINE bool Chip_Clock_IsMainPLLConnected(void)
{
	return (bool) ((LPC_SYSCTL->PLL[0].PLLSTAT & SYSCTL_PLL0STS_CONNECTED) != 0);
}

/**
 * @brief	Read PLL1 lock status
 * @return	true of the PLL1 is connected. false if not connected
 */
STATIC INLINE bool Chip_Clock_IsUSBPLLConnected(void)
{
	return (bool) ((LPC_SYSCTL->PLL[1].PLLSTAT & SYSCTL_PLL1STS_CONNECTED) != 0);
}
#endif

/**
 * @brief	Enables the external Crystal oscillator
 * @return	Nothing
 */
STATIC INLINE void Chip_Clock_EnableCrystal(void)
{
	LPC_SYSCTL->SCS |= SYSCTL_OSCEC;
}

/**
 * @brief	Checks if the external Crystal oscillator is enabled
 * @return	true if enabled, false otherwise
 */
STATIC INLINE bool Chip_Clock_IsCrystalEnabled(void)
{
	return (LPC_SYSCTL->SCS & SYSCTL_OSCSTAT) != 0;
}

/**
 * @brief Sets the external crystal oscillator range to 15Mhz - 25MHz
 * @return	Nothing
 */
STATIC INLINE void Chip_Clock_SetCrystalRangeHi(void)
{
	LPC_SYSCTL->SCS |= SYSCTL_OSCRANGE_15_25;
}

/**
 * @brief Sets the external crystal oscillator range to 1Mhz - 20MHz
 * @return	Nothing
 */
STATIC INLINE void Chip_Clock_SetCrystalRangeLo(void)
{
	LPC_SYSCTL->SCS &= ~SYSCTL_OSCRANGE_15_25;
}

/**
 * @brief	Feeds a PLL
 * @param	PLLNum:	PLL number
 * @return	Nothing
 */
STATIC INLINE void Chip_Clock_FeedPLL(CHIP_SYSCTL_PLL_T PLLNum)
{
	LPC_SYSCTL->PLL[PLLNum].PLLFEED = 0xAA;
	LPC_SYSCTL->PLL[PLLNum].PLLFEED = 0x55;
}

/**
 * Power control for peripherals
 */
typedef enum CHIP_SYSCTL_CLOCK {
#if defined(CHIP_LPC175X_6X)
	SYSCTL_CLOCK_RSVD0,
#else
	SYSCTL_CLOCK_LCD,					/*!< LCD clock */
#endif
	SYSCTL_CLOCK_TIMER0,			/*!< Timer 0 clock */
	SYSCTL_CLOCK_TIMER1,			/*!< Timer 1 clock */
	SYSCTL_CLOCK_UART0,				/*!< UART 0 clock */
	SYSCTL_CLOCK_UART1,				/*!< UART 1 clock */
#if defined(CHIP_LPC175X_6X)
	SYSCTL_CLOCK_RSVD5,
#else
	SYSCTL_CLOCK_PWM0,				/*!< PWM0 clock */
#endif
	SYSCTL_CLOCK_PWM1,				/*!< PWM1 clock */
	SYSCTL_CLOCK_I2C0,				/*!< I2C0 clock */
#if defined(CHIP_LPC175X_6X)
	SYSCTL_CLOCK_SPI,					/*!< SPI clock */
#else
	SYSCTL_CLOCK_UART4,				/*!< UART 4 clock */
#endif
	SYSCTL_CLOCK_RTC,					/*!< RTC clock */
	SYSCTL_CLOCK_SSP1,				/*!< SSP1 clock */
#if defined(CHIP_LPC175X_6X)
	SYSCTL_CLOCK_RSVD11,
#else
	SYSCTL_CLOCK_EMC,					/*!< EMC clock */
#endif
	SYSCTL_CLOCK_ADC,					/*!< ADC clock */
	SYSCTL_CLOCK_CAN1,				/*!< CAN1 clock */
	SYSCTL_CLOCK_CAN2,				/*!< CAN2 clock */
	SYSCTL_CLOCK_GPIO,				/*!< GPIO clock */
#if defined(CHIP_LPC175X_6X)
	SYSCTL_CLOCK_RIT,				/*!< RIT clock */
#else
	SYSCTL_CLOCK_SPIFI,				/*!< SPIFI clock */
#endif
	SYSCTL_CLOCK_MCPWM,				/*!< MCPWM clock */
	SYSCTL_CLOCK_QEI,					/*!< QEI clock */
	SYSCTL_CLOCK_I2C1,				/*!< I2C1 clock */
#if defined(CHIP_LPC175X_6X)
	SYSCTL_CLOCK_RSVD20,
#else
	SYSCTL_CLOCK_SSP2,				/*!< SSP2 clock */
#endif
	SYSCTL_CLOCK_SSP0,				/*!< SSP0 clock */
	SYSCTL_CLOCK_TIMER2,			/*!< Timer 2 clock */
	SYSCTL_CLOCK_TIMER3,			/*!< Timer 3 clock */
	SYSCTL_CLOCK_UART2,				/*!< UART 2 clock */
	SYSCTL_CLOCK_UART3,				/*!< UART 3 clock */
	SYSCTL_CLOCK_I2C2,				/*!< I2C2 clock */
	SYSCTL_CLOCK_I2S,					/*!< I2S clock */
#if defined(CHIP_LPC175X_6X)
	SYSCTL_CLOCK_RSVD28,
#else
	SYSCTL_CLOCK_SDC,				/*!< SD Card interface clock */
#endif
	SYSCTL_CLOCK_GPDMA,				/*!< GP DMA clock */
	SYSCTL_CLOCK_ENET,				/*!< EMAC/Ethernet clock */
	SYSCTL_CLOCK_USB,					/*!< USB clock */
	SYSCTL_CLOCK_RSVD32,
	SYSCTL_CLOCK_RSVD33,
	SYSCTL_CLOCK_RSVD34,
#if defined(CHIP_LPC40XX)
	SYSCTL_CLOCK_CMP,				/*!< Comparator clock (PCONP1) */
#else
	SYSCTL_CLOCK_RSVD35,
#endif
} CHIP_SYSCTL_CLOCK_T;

/**
 * @brief	Enables power and clocking for a peripheral
 * @param	clk:	Clock to enable
 * @return	Nothing
 * @note	Only peripheral clocks that are defined in the PCONP registers of the clock
 * and power controller can be enabled and disabled with this function.
 * Some clocks need to be enabled elsewhere (ie, USB) and will return
 * false to indicate it can't be enabled with this function.
 */
void Chip_Clock_EnablePeriphClock(CHIP_SYSCTL_CLOCK_T clk);

/**
 * @brief	Disables power and clocking for a peripheral
 * @param	clk:	Clock to disable
 * @return	Nothing
 * @note	Only peripheral clocks that are defined in the PCONP registers of the clock
 * and power controller can be enabled and disabled with this function.
 * Some clocks need to be disabled elsewhere (ie, USB) and will return
 * false to indicate it can't be disabled with this function.
 */
void Chip_Clock_DisablePeriphClock(CHIP_SYSCTL_CLOCK_T clk);

/**
 * @brief	Returns power enables state for a peripheral
 * @param	clk:	Clock to check
 * @return	true if the clock is enabled, false if disabled
 */
bool Chip_Clock_IsPeripheralClockEnabled(CHIP_SYSCTL_CLOCK_T clk);

#if !defined(CHIP_LPC175X_6X)
/**
 * EMC clock divider values
 */
typedef enum CHIP_SYSCTL_EMC_DIV {
	SYSCTL_EMC_DIV1 = 0,
	SYSCTL_EMC_DIV2 = 1,
} CHIP_SYSCTL_EMC_DIV_T;

/**
 * @brief	Selects a EMC divider rate
 * @param	emcDiv:	Source clock for PLL
 * @return	Nothing
 * @note	This function controls division of the clock before it is used by the EMC.
 * The EMC uses the same base clock as the CPU and the APB peripherals. The
 * EMC clock can tun at half or the same as the CPU clock. This is intended to
 * be used primarily when the CPU is running faster than the external bus can
 * support.
 */
STATIC INLINE void Chip_Clock_SetEMCClockDiv(CHIP_SYSCTL_EMC_DIV_T emcDiv)
{
	LPC_SYSCTL->EMCCLKSEL = (uint32_t) emcDiv;
}

/**
 * @brief	Get EMC divider rate
 * @return	divider value
 */
STATIC INLINE uint32_t Chip_Clock_GetEMCClockDiv(void)
{
	return ((uint32_t) LPC_SYSCTL->EMCCLKSEL) + 1;
}

#endif

/**
 * Selectable CPU clock sources
 */
typedef enum CHIP_SYSCTL_CCLKSRC {
	SYSCTL_CCLKSRC_SYSCLK,		/*!< Select Sysclk as the input to the CPU clock divider. */
	SYSCTL_CCLKSRC_MAINPLL,		/*!< Select the output of the Main PLL as the input to the CPU clock divider. */
} CHIP_SYSCTL_CCLKSRC_T;

/**
 * @brief	Sets the current CPU clock source
 * @param   src     Source selected
 * @return	Nothing
 * @note	When setting the clock source to the PLL, it should
 * be enabled and locked.
 */
void Chip_Clock_SetCPUClockSource(CHIP_SYSCTL_CCLKSRC_T src);

/**
 * @brief	Returns the current CPU clock source
 * @return	CPU clock source
 * @note	On 177x/8x and 407x/8x devices, this is also the peripheral
 * clock source.
 */
CHIP_SYSCTL_CCLKSRC_T Chip_Clock_GetCPUClockSource(void);

/**
 * @brief	Sets the CPU clock divider
 * @param	div:	CPU clock divider, between 1 and divider max
 * @return	Nothing
 * @note	The maximum divider for the 175x/6x is 256. The maximum divider for
 * the 177x/8x and 407x/8x is 32. Note on 175x/6x devices, the divided CPU
 * clock rate is used as the input to the peripheral clock dividers,
 * while 177x/8x and 407x/8x devices use the undivided CPU clock rate.
 */
void Chip_Clock_SetCPUClockDiv(uint32_t div);

/**
 * @brief	Gets the CPU clock divider
 * @return	CPU clock divider, between 1 and divider max
 * @note	The maximum divider for the 175x/6x is 256. The maximum divider for
 * the 177x/8x and 407x/8x is 32. Note on 175x/6x devices, the divided CPU
 * clock rate is used as the input to the peripheral clock dividers,
 * while 177x/8x and 407x/8x devices use the undivided CPU clock rate.
 */
uint32_t Chip_Clock_GetCPUClockDiv(void);

#if !defined(CHIP_LPC175X_6X)
/**
 * Clock sources for the USB divider. On 175x/6x devices, only the USB
 * PLL1 can be used as an input for the USB divider
 */
typedef enum CHIP_SYSCTL_USBCLKSRC {
	SYSCTL_USBCLKSRC_SYSCLK,		/*!< SYSCLK clock as USB divider source */
	SYSCTL_USBCLKSRC_MAINPLL,		/*!< PLL0 clock as USB divider source */
	SYSCTL_USBCLKSRC_USBPLL,		/*!< PLL1 clock as USB divider source */
	SYSCTL_USBCLKSRC_RESERVED
} CHIP_SYSCTL_USBCLKSRC_T;

/**
 * @brief	Sets the USB clock divider source
 * @param	src:	USB clock divider source clock
 * @return	Nothing
 * @note	This function doesn't apply for LPC175x/6x devices. The divider must be
 * be selected with the selected source to give a valid USB clock with a
 * rate of 48MHz.
 */
void Chip_Clock_SetUSBClockSource(CHIP_SYSCTL_USBCLKSRC_T src);

/**
 * @brief	Gets the USB clock divider source
 * @return	USB clock divider source clock
 */
STATIC INLINE CHIP_SYSCTL_USBCLKSRC_T Chip_Clock_GetUSBClockSource(void)
{
	return (CHIP_SYSCTL_USBCLKSRC_T) ((LPC_SYSCTL->USBCLKSEL >> 8) & 0x3);
}

#endif /* !defined(CHIP_LPC175X_6X)*/

/**
 * @brief	Sets the USB clock divider
 * @param	div:	USB clock divider to generate 48MHz from USB source clock
 * @return	Nothing
 * @note	Divider values are between 1 and 32 (16 max for 175x/6x)
 */
void Chip_Clock_SetUSBClockDiv(uint32_t div);

/**
 * @brief	Gets the USB clock divider
 * @return	USB clock divider
 * @note	Divider values are between 1 and 32 (16 max for 175x/6x)
 */
uint32_t Chip_Clock_GetUSBClockDiv(void);

/**
 * PLL source clocks
 */
typedef enum CHIP_SYSCTL_PLLCLKSRC {
	SYSCTL_PLLCLKSRC_IRC,			/*!< PLL is sourced from the internal oscillator (IRC) */
	SYSCTL_PLLCLKSRC_MAINOSC,		/*!< PLL is sourced from the main oscillator */
#if defined(CHIP_LPC175X_6X)
	SYSCTL_PLLCLKSRC_RTC,			/*!< PLL is sourced from the RTC oscillator */
#else
	SYSCTL_PLLCLKSRC_RESERVED1,
#endif
	SYSCTL_PLLCLKSRC_RESERVED2
} CHIP_SYSCTL_PLLCLKSRC_T;

/**
 * @brief	Selects a input clock source for SYSCLK
 * @param	src:	input clock source for SYSCLK
 * @return	Nothing
 * @note	SYSCLK is used for sourcing PLL0, SPIFI FLASH, the USB clock
 * divider, and the CPU clock divider.
 */
STATIC INLINE void Chip_Clock_SetMainPLLSource(CHIP_SYSCTL_PLLCLKSRC_T src)
{
	LPC_SYSCTL->CLKSRCSEL = src;
}

/**
 * @brief	Returns the input clock source for SYSCLK
 * @return	input clock source for SYSCLK
 */
STATIC INLINE CHIP_SYSCTL_PLLCLKSRC_T Chip_Clock_GetMainPLLSource(void)
{
	return (CHIP_SYSCTL_PLLCLKSRC_T) (LPC_SYSCTL->CLKSRCSEL & 0x3);
}

#if defined(CHIP_LPC175X_6X)
/**
 * Clock and power peripheral clock divider rates used with the
 * Clock_CLKDIVSEL_T clock types (devices only)
 */
typedef enum {
	SYSCTL_CLKDIV_4,			/*!< Divider by 4 */
	SYSCTL_CLKDIV_1,			/*!< Divider by 1 */
	SYSCTL_CLKDIV_2,			/*!< Divider by 2 */
	SYSCTL_CLKDIV_8,			/*!< Divider by 8, not for use with CAN */
	SYSCTL_CLKDIV_6_CCAN = SYSCTL_CLKDIV_8	/*!< Divider by 6, CAN only */
} CHIP_SYSCTL_CLKDIV_T;

/**
 * Peripheral clock selection for LPC175x/6x
 * This is a list of clocks that can be divided on the 175x/6x
 */
typedef enum {
	SYSCTL_PCLK_WDT,		/*!< Watchdog divider */
	SYSCTL_PCLK_TIMER0,	/*!< Timer 0 divider */
	SYSCTL_PCLK_TIMER1,	/*!< Timer 1 divider */
	SYSCTL_PCLK_UART0,	/*!< UART 0 divider */
	SYSCTL_PCLK_UART1,	/*!< UART 1 divider */
	SYSCTL_PCLK_RSVD5,
	SYSCTL_PCLK_PWM1,		/*!< PWM 1 divider */
	SYSCTL_PCLK_I2C0,		/*!< I2C 0 divider */
	SYSCTL_PCLK_SPI,		/*!< SPI divider */
	SYSCTL_PCLK_RSVD9,
	SYSCTL_PCLK_SSP1,		/*!< SSP 1 divider */
	SYSCTL_PCLK_DAC,		/*!< DAC divider */
	SYSCTL_PCLK_ADC,		/*!< ADC divider */
	SYSCTL_PCLK_CAN1,		/*!< CAN 1 divider */
	SYSCTL_PCLK_CAN2,		/*!< CAN 2 divider */
	SYSCTL_PCLK_ACF,		/*!< ACF divider */
	SYSCTL_PCLK_QEI,		/*!< QEI divider */
	SYSCTL_PCLK_GPIOINT,	/*!< GPIOINT divider */
	SYSCTL_PCLK_PCB,		/*!< PCB divider */
	SYSCTL_PCLK_I2C1,		/*!< I2C 1 divider */
	SYSCTL_PCLK_RSVD20,
	SYSCTL_PCLK_SSP0,		/*!< SSP 0 divider */
	SYSCTL_PCLK_TIMER2,	/*!< Timer 2 divider */
	SYSCTL_PCLK_TIMER3,	/*!< Timer 3 divider */
	SYSCTL_PCLK_UART2,	/*!< UART 2 divider */
	SYSCTL_PCLK_UART3,	/*!< UART 3 divider */
	SYSCTL_PCLK_I2C2,		/*!< I2C 2 divider */
	SYSCTL_PCLK_I2S,		/*!< I2S divider */
	SYSCTL_PCLK_RSVD28,
	SYSCTL_PCLK_RIT,		/*!< Repetitive timer divider */
	SYSCTL_PCLK_SYSCON,	/*!< SYSCON divider */
	SYSCTL_PCLK_MCPWM		/*!< Motor control PWM divider */
} CHIP_SYSCTL_PCLK_T;

/**
 * @brief	Selects a clock divider for a peripheral
 * @param	clk:		Clock to set divider for
 * @param	div:	Divider for the clock
 * @return	Nothing
 * @note	Selects the divider for a peripheral. A peripheral clock is generated
 * from the CPU clock divided by its peripheral clock divider.
 * Only peripheral clocks that are defined in the PCLKSEL registers of
 * the clock and power controller can used this function.
 * (LPC175X/6X only)
 */
void Chip_Clock_SetPCLKDiv(CHIP_SYSCTL_PCLK_T clk, CHIP_SYSCTL_CLKDIV_T div);

/**
 * @brief	Gets a clock divider for a peripheral
 * @param	clk:		Clock to set divider for
 * @return	The divider for the clock
 * @note	Selects the divider for a peripheral. A peripheral clock is generated
 * from the CPU clock divided by its peripheral clock divider.
 * Only peripheral clocks that are defined in the PCLKSEL registers of
 * the clock and power controller can used this function.
 * (LPC175X/6X only)
 */
uint32_t Chip_Clock_GetPCLKDiv(CHIP_SYSCTL_PCLK_T clk);

#else
/**
 * @brief	Sets a clock divider for all peripherals
 * @param	div:	Divider for all peripherals, 0 = disable
 * @return	Nothing
 * @note	All the peripherals in the device use the same clock divider. The
 * divider is based on the CPU's clock rate. Use 0 to disable all
 * peripheral clocks or a divider of 1 to 15. (LPC177X/8X and 407X/8X)
 */
STATIC INLINE void Chip_Clock_SetPCLKDiv(uint32_t div)
{
	LPC_SYSCTL->PCLKSEL = div;
}

/**
 * @brief	Gets the clock divider for all peripherals
 * @return	Divider for all peripherals, 0 = disabled
 * @note	All the peripherals in the device use the same clock divider. The
 * divider is based on the CPU's clock rate. (LPC177X/8X and 407X/8X)
 */
STATIC INLINE uint32_t Chip_Clock_GetPCLKDiv(void)
{
	return LPC_SYSCTL->PCLKSEL & 0x1F;
}

#endif

#if !defined(CHIP_LPC175X_6X)
/**
 * Clock sources for the SPIFI clock divider
 */
typedef enum CHIP_SYSCTL_SPIFICLKSRC {
	SYSCTL_SPIFICLKSRC_SYSCLK,		/*!< SYSCLK clock as SPIFI divider source */
	SYSCTL_SPIFICLKSRC_MAINPLL,		/*!< PLL0 clock as SPIFI divider source */
	SYSCTL_SPIFICLKSRC_USBPLL,		/*!< PLL1 clock as SPIFI divider source */
	SYSCTL_SPIFICLKSRC_RESERVED
} CHIP_SYSCTL_SPIFICLKSRC_T;

/**
 * @brief	Sets the SPIFI clock divider source
 * @param	src:	SPIFI clock divider source clock
 * @return	Nothing
 */
void Chip_Clock_SetSPIFIClockSource(CHIP_SYSCTL_SPIFICLKSRC_T src);

/**
 * @brief	Gets the SPIFI clock divider source
 * @return	SPIFI clock divider source clock
 */
STATIC INLINE CHIP_SYSCTL_SPIFICLKSRC_T Chip_Clock_GetSPIFIClockSource(void)
{
	return (CHIP_SYSCTL_SPIFICLKSRC_T) ((LPC_SYSCTL->SPIFICLKSEL >> 8) & 0x3);
}

/**
 * @brief	Sets the SPIFI clock divider
 * @param	div:	SPIFI clock divider, 0 to disable
 * @return	Nothing
 * @note	Divider values are between 1 and 31
 */
void Chip_Clock_SetSPIFIClockDiv(uint32_t div);

/**
 * @brief	Gets the SPIFI clock divider
 * @return	SPIFI clock divider
 * @note	Divider values are between 1 and 31, 0 is disabled
 */
STATIC INLINE uint32_t Chip_Clock_GetSPIFIClockDiv(void)
{
	return LPC_SYSCTL->SPIFICLKSEL & 0x1F;
}

/**
 * @brief	Set the LCD clock prescaler
 * @param	div:	Divider value, minimum of 1
 * @return	Nothing
 */
STATIC INLINE void Chip_Clock_SetLCDClockDiv(uint32_t div)
{
	LPC_SYSCTL->LCD_CFG = (div - 1);
}

/**
 * @brief	Get the LCD clock prescaler
 * @return	Current divider value
 */
STATIC INLINE uint32_t Chip_Clock_GetLCDClockDiv(void)
{
	return (LPC_SYSCTL->LCD_CFG & 0x1F) + 1;
}

#endif

/**
 * Clock sources for the CLKOUT pin
 */
typedef enum {
	SYSCTL_CLKOUTSRC_CPU,			/*!< CPU clock as CLKOUT source */
	SYSCTL_CLKOUTSRC_MAINOSC,		/*!< Main oscillator clock as CLKOUT source */
	SYSCTL_CLKOUTSRC_IRC,			/*!< IRC oscillator clock as CLKOUT source */
	SYSCTL_CLKOUTSRC_USB,			/*!< USB clock as CLKOUT source */
	SYSCTL_CLKOUTSRC_RTC,			/*!< RTC clock as CLKOUT source */
#if defined(CHIP_LPC175X_6X)
	SYSCTL_CLKOUTSRC_RESERVED1,
	SYSCTL_CLKOUTSRC_RESERVED2,
#else
	SYSCTL_CLKOUTSRC_SPIFI,		/*!< SPIFI clock as CLKOUT source */
	SYSCTL_CLKOUTSRC_WATCHDOGOSC,	/*!< Watchdog oscillator as CLKOUT source */
#endif
	SYSCTL_CLKOUTSRC_RESERVED3
} CHIP_SYSCTL_CLKOUTSRC_T;

/**
 * @brief	Selects a source clock and divider rate for the CLKOUT pin
 * @param	src:	source selected
 * @param	div:	Divider for the clock source on CLKOUT, 1 to 16
 * @return	Nothing
 * @note	This function will disable the CLKOUT signal if its enabled. Use
 * Chip_Clock_EnableCLKOUT to re-enable CLKOUT after a call to this
 * function.
 */
void Chip_Clock_SetCLKOUTSource(CHIP_SYSCTL_CLKOUTSRC_T src, uint32_t div);

/**
 * @brief	Enables the clock on the CLKOUT pin
 * @return	Nothing
 */
STATIC INLINE void Chip_Clock_EnableCLKOUT(void)
{
	LPC_SYSCTL->CLKOUTCFG |= (1 << 8);
}

/**
 * @brief	Disables the clock on the CLKOUT pin
 * @return	Nothing
 */
STATIC INLINE void Chip_Clock_DisableCLKOUT(void)
{
	LPC_SYSCTL->CLKOUTCFG &= ~(1 << 8);
}

/**
 * @brief	Returns the CLKOUT activity indication status
 * @return	true if CLKOUT is enabled, false if disabled and stopped
 * @note	CLKOUT activity indication. Reads as true when CLKOUT is
 * enabled. Read as false when CLKOUT has been disabled via
 * the CLKOUT_EN bit and the clock has completed being stopped.
 */
STATIC INLINE bool Chip_Clock_IsCLKOUTEnabled(void)
{
	return (bool) ((LPC_SYSCTL->CLKOUTCFG & (1 << 9)) != 0);
}

/**
 * @brief	Returns the main oscillator clock rate
 * @return	main oscillator clock rate
 */
STATIC INLINE uint32_t Chip_Clock_GetMainOscRate(void)
{
	return OscRateIn;
}

/**
 * @brief	Returns the internal oscillator (IRC) clock rate
 * @return	internal oscillator (IRC) clock rate
 */
STATIC INLINE uint32_t Chip_Clock_GetIntOscRate(void)
{
	return SYSCTL_IRC_FREQ;
}

/**
 * @brief	Returns the RTC oscillator clock rate
 * @return	RTC oscillator clock rate
 */
STATIC INLINE uint32_t Chip_Clock_GetRTCOscRate(void)
{
	return RTCOscRateIn;
}

/**
 * @brief	Returns the current SYSCLK clock rate
 * @return	SYSCLK clock rate
 * @note	SYSCLK is used for sourcing PLL0, SPIFI FLASH, the USB clock
 * divider, and the CPU clock divider.
 */
uint32_t Chip_Clock_GetSYSCLKRate(void);

/**
 * @brief	Return Main PLL (PLL0) input clock rate
 * @return	PLL0 input clock rate
 */
STATIC INLINE uint32_t Chip_Clock_GetMainPLLInClockRate(void)
{
	return Chip_Clock_GetSYSCLKRate();
}

/**
 * @brief	Return PLL0 (Main PLL) output clock rate
 * @return	PLL0 output clock rate
 */
uint32_t Chip_Clock_GetMainPLLOutClockRate(void);

/**
 * @brief	Return USB PLL input clock rate
 * @return	USB PLL input clock rate
 */
STATIC INLINE uint32_t Chip_Clock_GetUSBPLLInClockRate(void)
{
	return Chip_Clock_GetMainOscRate();
}

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
 * @brief	Gets the USB clock (USB_CLK) rate
 * @return	USB clock (USB_CLK) clock rate
 * @note	The clock source and divider are used to generate the USB clock rate.
 */
uint32_t Chip_Clock_GetUSBClockRate(void);

#if !defined(CHIP_LPC175X_6X)

/**
 * @brief	Returns the SPIFI clock rate
 * @return	SPIFI clock clock rate
 */
uint32_t Chip_Clock_GetSPIFIClockRate(void);

/**
 * @brief	Returns clock rate for EMC
 * @return	Clock rate for the peripheral
 */
STATIC INLINE uint32_t Chip_Clock_GetEMCClockRate(void)
{
	return Chip_Clock_GetSystemClockRate() / Chip_Clock_GetEMCClockDiv();
}

#endif

#if defined(CHIP_LPC177X_8X) || defined(CHIP_LPC40XX)
/**
 * @brief	Returns clock rate for a peripheral (from peripheral clock)
 * @param	clk:	Clock to get rate of
 * @return	Clock rate for the peripheral
 * @note	This covers most common peripheral clocks, but not every clock
 * in the system. LPC177x/8x and LPC407x/8x devices use the same
 * clock for all periphreals, while the LPC175x/6x have unique
 * dividers (except to RTC ) that may alter the peripheral clock rate.
 */
uint32_t Chip_Clock_GetPeripheralClockRate(void);

#else
/**
 * @brief	Returns clock rate for a peripheral (from peripheral clock)
 * @param	clk:	Clock to get rate of
 * @return	Clock rate for the peripheral
 * @note	This covers most common peripheral clocks, but not every clock
 * in the system. LPC177x/8x and LPC407x/8x devices use the same
 * clock for all periphreals, while the LPC175x/6x have unique
 * dividers (except to RTC ) that may alter the peripheral clock rate.
 */
uint32_t Chip_Clock_GetPeripheralClockRate(CHIP_SYSCTL_PCLK_T clk);

/**
 * @brief	Returns clock rate for RTC
 * @return	Clock rate for the peripheral
 */
STATIC INLINE uint32_t Chip_Clock_GetRTCClockRate(void)
{
	return Chip_Clock_GetSystemClockRate() / 8;
}

#endif

/**
 * @brief	Returns clock rate for Ethernet
 * @return	Clock rate for the peripheral
 */
STATIC INLINE uint32_t Chip_Clock_GetENETClockRate(void)
{
	return Chip_Clock_GetSystemClockRate();
}

/**
 * @brief	Returns clock rate for GPDMA
 * @return	Clock rate for the peripheral
 */
STATIC INLINE uint32_t Chip_Clock_GetGPDMAClockRate(void)
{
	return Chip_Clock_GetSystemClockRate();
}

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* __CLOCK_17XX_40XX_H_ */

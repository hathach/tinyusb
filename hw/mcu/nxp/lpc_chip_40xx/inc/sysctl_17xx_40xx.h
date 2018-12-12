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

#ifndef _SYSCTL_17XX_40XX_H_
#define _SYSCTL_17XX_40XX_H_

#ifdef __cplusplus
extern "C" {
#endif

/** @defgroup SYSCTL_17XX_40XX CHIP: LPC17xx/40xx System Control block driver
 * @ingroup CHIP_17XX_40XX_Drivers
 * @{
 */

/**
 * @brief LPC17XX/40XX Clock and Power PLL register block structure
 */
typedef struct {
	__IO uint32_t PLLCON;					/*!< (R/W)  PLL Control Register */
	__IO uint32_t PLLCFG;					/*!< (R/W)  PLL Configuration Register */
	__I  uint32_t PLLSTAT;					/*!< (R/ )  PLL Status Register */
	__O  uint32_t PLLFEED;					/*!< ( /W)  PLL Feed Register */
	uint32_t RESERVED1[4];
} SYSCTL_PLL_REGS_T;

/**
 * Selectable PLLs
 */
typedef enum {
	SYSCTL_MAIN_PLL,			/*!< Main PLL (PLL0) */
	SYSCTL_USB_PLL,				/*!< USB PLL (PLL1) */
} CHIP_SYSCTL_PLL_T;

/**
 * @brief LPC17XX/40XX Clock and Power register block structure
 */
typedef struct {
	__IO uint32_t FLASHCFG;					/*!< Offset: 0x000 (R/W)  Flash Accelerator Configuration Register */
	uint32_t RESERVED0[15];
	__IO uint32_t MEMMAP;					/*!< Offset: 0x000 (R/W)  Flash Accelerator Configuration Register */
	uint32_t RESERVED1[15];
	SYSCTL_PLL_REGS_T PLL[SYSCTL_USB_PLL + 1];		/*!< Offset: 0x080: PLL0 and PLL1 */
	__IO uint32_t PCON;						/*!< Offset: 0x0C0 (R/W)  Power Control Register */
	__IO uint32_t PCONP;					/*!< Offset: 0x0C4 (R/W)  Power Control for Peripherals Register */
#if defined(CHIP_LPC175X_6X)
	uint32_t RESERVED2[15];
#elif defined(CHIP_LPC177X_8X)
	uint32_t RESERVED2[14];
	__IO uint32_t EMCCLKSEL;				/*!< Offset: 0x100 (R/W)  External Memory Controller Clock Selection Register */
#else
	__IO uint32_t PCONP1;					/*!< Offset: 0x0C8 (R/W)  Power Control 1 for Peripherals Register */
	uint32_t RESERVED2[13];
	__IO uint32_t EMCCLKSEL;				/*!< Offset: 0x100 (R/W)  External Memory Controller Clock Selection Register */
#endif
	__IO uint32_t CCLKSEL;					/*!< Offset: 0x104 (R/W)  CPU Clock Selection Register */
	__IO uint32_t USBCLKSEL;				/*!< Offset: 0x108 (R/W)  USB Clock Selection Register */
	__IO uint32_t CLKSRCSEL;				/*!< Offset: 0x10C (R/W)  Clock Source Select Register */
	__IO uint32_t CANSLEEPCLR;				/*!< Offset: 0x110 (R/W)  CAN Sleep Clear Register */
	__IO uint32_t CANWAKEFLAGS;				/*!< Offset: 0x114 (R/W)  CAN Wake-up Flags Register */
	uint32_t RESERVED3[10];
	__IO uint32_t EXTINT;					/*!< Offset: 0x140 (R/W)  External Interrupt Flag Register */
	uint32_t RESERVED4;
	__IO uint32_t EXTMODE;					/*!< Offset: 0x148 (R/W)  External Interrupt Mode Register */
	__IO uint32_t EXTPOLAR;					/*!< Offset: 0x14C (R/W)  External Interrupt Polarity Register */
	uint32_t RESERVED5[12];
	__IO uint32_t RSID;						/*!< Offset: 0x180 (R/W)  Reset Source Identification Register */
#if defined(CHIP_LPC175X_6X) || defined(CHIP_LPC40XX)
	uint32_t RESERVED6[7];
#elif defined(CHIP_LPC177X_8X)
	uint32_t RESERVED6;
	uint32_t MATRIXARB;
	uint32_t RESERVED6A[5];
#endif
	__IO uint32_t SCS;						/*!< Offset: 0x1A0 (R/W)  System Controls and Status Register */
	__IO uint32_t RESERVED7;
#if defined(CHIP_LPC175X_6X)
	__IO uint32_t PCLKSEL[2];				/*!< Offset: 0x1A8 (R/W)  Peripheral Clock Selection Register */
	uint32_t RESERVED8[4];
#else
	__IO uint32_t PCLKSEL;				/*!< Offset: 0x1A8 (R/W)  Peripheral Clock Selection Register */
	uint32_t RESERVED9;
	__IO uint32_t PBOOST;					/*!< Offset: 0x1B0 (R/W)  Power Boost control register */
	__IO uint32_t SPIFICLKSEL;
	__IO uint32_t LCD_CFG;					/*!< Offset: 0x1B8 (R/W)  LCD Configuration and clocking control Register */
	uint32_t RESERVED10;
#endif
	__IO uint32_t USBIntSt;					/*!< Offset: 0x1C0 (R/W)  USB Interrupt Status Register */
	__IO uint32_t DMAREQSEL;				/*!< Offset: 0x1C4 (R/W)  DMA Request Select Register */
	__IO uint32_t CLKOUTCFG;				/*!< Offset: 0x1C8 (R/W)  Clock Output Configuration Register */
#if defined(CHIP_LPC175X_6X)
	uint32_t RESERVED11[6];
#else
	__IO uint32_t RSTCON[2];				/*!< Offset: 0x1CC (R/W)  RESET Control0/1 Registers */
	uint32_t RESERVED11[2];
	__IO uint32_t EMCDLYCTL;				/*!< Offset: 0x1DC (R/W) SDRAM programmable delays          */
	__IO uint32_t EMCCAL;					/*!< Offset: 0x1E0 (R/W) Calibration of programmable delays */
#endif
} LPC_SYSCTL_T;

/**
 * @brief FLASH Access time definitions
 */
typedef enum {
	FLASHTIM_20MHZ_CPU = 0,		/*!< Flash accesses use 1 CPU clocks. Use for up to 20 MHz CPU clock */
	FLASHTIM_40MHZ_CPU = 1,		/*!< Flash accesses use 2 CPU clocks. Use for up to 40 MHz CPU clock */
	FLASHTIM_60MHZ_CPU = 2,		/*!< Flash accesses use 3 CPU clocks. Use for up to 60 MHz CPU clock */
	FLASHTIM_80MHZ_CPU = 3,		/*!< Flash accesses use 4 CPU clocks. Use for up to 80 MHz CPU clock */
	FLASHTIM_100MHZ_CPU = 4,	/*!< Flash accesses use 5 CPU clocks. Use for up to 100 MHz CPU clock */
#if defined(CHIP_LPC177X_8X) || defined(CHIP_LPC40XX)
	FLASHTIM_120MHZ_CPU = 3,	/*!< Flash accesses use 4 CPU clocks. Use for up to 120 MHz CPU clock with power boot on*/
#else
	FLASHTIM_120MHZ_CPU = 4,	/*!< Flash accesses use 5 CPU clocks. Use for up to 120 Mhz for LPC1759 and LPC1769 only.*/
#endif
	FLASHTIM_SAFE_SETTING = 5,	/*!< Flash accesses use 6 CPU clocks. Safe setting for any allowed conditions */
} FMC_FLASHTIM_T;

/**
 * @brief	Set FLASH memory access time in clocks
 * @param	clks	: Clock cycles for FLASH access (minus 1)
 * @return	Nothing
 * @note	See the user manual for valid settings for this register for when
 * power boot is enabled or off.
 */
STATIC INLINE void Chip_SYSCTL_SetFLASHAccess(FMC_FLASHTIM_T clks)
{
	uint32_t tmp = LPC_SYSCTL->FLASHCFG & 0xFFF;

	/* Don't alter lower bits */
	LPC_SYSCTL->FLASHCFG = tmp | (clks << 12);
}

/**
 * System memory remap modes used to remap interrupt vectors
 */
typedef enum CHIP_SYSCTL_BOOT_MODE_REMAP {
	REMAP_BOOT_LOADER_MODE,	/*!< Interrupt vectors are re-mapped to Boot ROM */
	REMAP_USER_FLASH_MODE	/*!< Interrupt vectors are not re-mapped and reside in Flash */
} CHIP_SYSCTL_BOOT_MODE_REMAP_T;

/**
 * @brief	Re-map interrupt vectors
 * @param	remap	: system memory map value
 * @return	Nothing
 */
STATIC INLINE void Chip_SYSCTL_Map(CHIP_SYSCTL_BOOT_MODE_REMAP_T remap)
{
	LPC_SYSCTL->MEMMAP = (uint32_t) remap;
}

/**
 * System reset status
 */
#define SYSCTL_RST_POR    (1 << 0)	/*!< POR reset status */
#define SYSCTL_RST_EXTRST (1 << 1)	/*!< External reset status */
#define SYSCTL_RST_WDT    (1 << 2)	/*!< Watchdog reset status */
#define SYSCTL_RST_BOD    (1 << 3)	/*!< Brown-out detect reset status */
#if defined(CHIP_LPC177X_8X) || defined(CHIP_LPC40XX)
#define SYSCTL_RST_SYSRST (1 << 4)	/*!< software system reset status */
#define SYSCTL_RST_LOCKUP (1 << 5)	/*!< "lockup" reset status */
#endif

/**
 * @brief	Get system reset status
 * @return	An Or'ed value of SYSCTL_RST_*
 * @note	This function returns the detected reset source(s).
 */
STATIC INLINE uint32_t Chip_SYSCTL_GetSystemRSTStatus(void)
{
	return LPC_SYSCTL->RSID;
}

/**
 * @brief	Clear system reset status
 * @param	reset	: An Or'ed value of SYSCTL_RST_* status to clear
 * @return	Nothing
 */
STATIC INLINE void Chip_SYSCTL_ClearSystemRSTStatus(uint32_t reset)
{
	LPC_SYSCTL->RSID = reset;
}

/**
 * @brief	Enable brown-out detection
 * @return	Nothing
 */
STATIC INLINE void Chip_SYSCTL_EnableBOD(void)
{
	LPC_SYSCTL->PCON |= (1 << 3);
}

/**
 * @brief	Disable brown-out detection
 * @return	Nothing
 */
STATIC INLINE void Chip_SYSCTL_DisableBOD(void)
{
	LPC_SYSCTL->PCON &= ~(1 << 3);
}

/**
 * @brief	Enable brown-out detection reset
 * @return	Nothing
 */
STATIC INLINE void Chip_SYSCTL_EnableBODReset(void)
{
	LPC_SYSCTL->PCON |= (1 << 4);
}

/**
 * @brief	Disable brown-out detection reset
 * @return	Nothing
 */
STATIC INLINE void Chip_SYSCTL_DisableBODReset(void)
{
	LPC_SYSCTL->PCON &= ~(1 << 4);
}

/**
 * @brief	Enable brown-out detection reduced power mode
 * @return	Nothing
 */
STATIC INLINE void Chip_SYSCTL_EnableBODRPM(void)
{
	LPC_SYSCTL->PCON |= (1 << 5);
}

/**
 * @brief	Disable brown-out detection reduced power mode
 * @return	Nothing
 */
STATIC INLINE void Chip_SYSCTL_DisableBODRPM(void)
{
	LPC_SYSCTL->PCON &= ~(1 << 5);
}

#define SYSCTL_PD_SMFLAG (1 << 8)	/*!< Sleep Mode entry flag */
#define SYSCTL_PD_DSFLAG (1 << 9)	/*!< Deep Sleep entry flag */
#define SYSCTL_PD_PDFLAG (1 << 10)	/*!< Power-down entry flag */
#define SYSCTL_PD_DPDFLAG (1 << 11)	/*!< Deep Power-down entry flag */

/**
 * @brief	Returns and clears the current sleep mode entry flags
 * @param	flags:	One or more flags to clear, SYSCTL_PD_*
 * @return	An Or'ed value of the sleep flags, SYSCTL_PD_*
 * @note	These flags indicate the successful entry of one or more
 * sleep modes.
 */
uint32_t Chip_SYSCTL_GetClrSleepFlags(uint32_t flags);

#if !defined(CHIP_LPC175X_6X)
/**
 * @brief	Enable power boost for clock operation over 100MHz
 * @return	Nothing
 */
STATIC INLINE void Chip_SYSCTL_EnableBoost(void)
{
	LPC_SYSCTL->PBOOST = 0x3;
}

/**
 * @brief	Disable power boost for clock operation under 100MHz
 * @return	Nothing
 */
STATIC INLINE void Chip_SYSCTL_DisableBoost(void)
{
	LPC_SYSCTL->PBOOST = 0x0;
}

#endif

#if !defined(CHIP_LPC175X_6X)
/**
 * Peripheral reset numbers
 * This is a list of peripherals that can be reset
 */
typedef enum {
	SYSCTL_RESET_LCD,					/*!< LCD reset */
	SYSCTL_RESET_TIMER0,			/*!< Timer 0 reset */
	SYSCTL_RESET_TIMER1,			/*!< Timer 1 reset */
	SYSCTL_RESET_UART0,				/*!< UART 0 reset */
	SYSCTL_RESET_UART1,				/*!< UART 1 reset */
	SYSCTL_RESET_PWM0,				/*!< PWM0 reset */
	SYSCTL_RESET_PWM1,				/*!< PWM1 reset */
	SYSCTL_RESET_I2C0,				/*!< I2C0 reset */
	SYSCTL_RESET_UART4,				/*!< UART 4 reset */
	SYSCTL_RESET_RTC,					/*!< RTC reset */
	SYSCTL_RESET_SSP1,				/*!< SSP1 reset */
	SYSCTL_RESET_EMC,					/*!< EMC reset */
	SYSCTL_RESET_ADC,					/*!< ADC reset */
	SYSCTL_RESET_CAN1,				/*!< CAN1 reset */
	SYSCTL_RESET_CAN2,				/*!< CAN2 reset */
	SYSCTL_RESET_GPIO,				/*!< GPIO reset */
	SYSCTL_RESET_SPIFI,				/*!< SPIFI reset */
	SYSCTL_RESET_MCPWM,				/*!< MCPWM reset */
	SYSCTL_RESET_QEI,					/*!< QEI reset */
	SYSCTL_RESET_I2C1,				/*!< I2C1 reset */
	SYSCTL_RESET_SSP2,				/*!< SSP2 reset */
	SYSCTL_RESET_SSP0,				/*!< SSP0 reset */
	SYSCTL_RESET_TIMER2,			/*!< Timer 2 reset */
	SYSCTL_RESET_TIMER3,			/*!< Timer 3 reset */
	SYSCTL_RESET_UART2,				/*!< UART 2 reset */
	SYSCTL_RESET_UART3,				/*!< UART 3 reset */
	SYSCTL_RESET_I2C2,				/*!< I2C2 reset */
	SYSCTL_RESET_I2S,					/*!< I2S reset */
	SYSCTL_RESET_PCSDC,				/*!< SD Card interface reset */
	SYSCTL_RESET_GPDMA,				/*!< GP DMA reset */
	SYSCTL_RESET_ENET,				/*!< EMAC/Ethernet reset */
	SYSCTL_RESET_USB,					/*!< USB reset */
	SYSCTL_RESET_IOCON,				/*!< IOCON reset */
	SYSCTL_RESET_DAC,					/*!< DAC reset */
	SYSCTL_RESET_CANACC,			/*!< CAN acceptance filter reset */
} CHIP_SYSCTL_RESET_T;

/**
 * @brief	Resets a peripheral
 * @param	periph:	Peripheral to reset
 * @return	Nothing
 */
void Chip_SYSCTL_PeriphReset(CHIP_SYSCTL_RESET_T periph);

#endif

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* _SYSCTL_17XX_40XX_H_ */

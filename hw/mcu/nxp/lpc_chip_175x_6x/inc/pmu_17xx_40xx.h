/*
 * @brief LPC17xx_40xx PMU chip driver
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

#ifndef __PMU_17XX_40XX_H_
#define __PMU_17XX_40XX_H_

#ifdef __cplusplus
extern "C" {
#endif

/** @defgroup PMU_17XX_40XX CHIP: LPC17xx_40xx PMU driver
 * @ingroup CHIP_17XX_40XX_Drivers
 * @{
 */

/**
 * @brief LPC17xx_40xx Power Management Unit register block structure
 */
typedef struct {
	__IO uint32_t PCON;		/*!< Offset: 0x000 Power control Register (R/W) */
} LPC_PMU_T;

/**
 * @brief LPC17xx_40xx low power mode type definitions
 */
typedef enum CHIP_PMU_MCUPOWER {
	PMU_MCU_SLEEP = 0,		/*!< Sleep mode */
	PMU_MCU_DEEP_SLEEP,		/*!< Deep Sleep mode */
	PMU_MCU_POWER_DOWN,		/*!< Power down mode */
	PMU_MCU_DEEP_PWRDOWN	/*!< Deep power down mode */
} CHIP_PMU_MCUPOWER_T;

/**
 * PMU PCON register bit fields & masks
 */
#define PMU_PCON_PM0_FLAG			(1 << 0)
#define PMU_PCON_PM1_FLAG			(1 << 1)
#define PMU_PCON_BODRPM_FLAG		(1 << 2)
#define PMU_PCON_BOGD_FLAG			(1 << 3)
#define PMU_PCON_BORD_FLAG			(1 << 4)
#define PMU_PCON_SMFLAG          	(1 << 8)	/*!< Sleep mode flag */
#define PMU_PCON_DSFLAG          	(1 << 9)	/*!< Deep Sleep mode flag */
#define PMU_PCON_PDFLAG             (1 << 10)	/*!< Power-down flag */
#define PMU_PCON_DPDFLAG            (1 << 11)	/*!< Deep power-down flag */

/**
 * @brief	Enter MCU Sleep mode
 * @param	pPMU	: Pointer to PMU register block
 * @return	None
 * @note	The sleep mode affects the ARM Cortex-M0+ core only. Peripherals
 * and memories are active.
 */
void Chip_PMU_SleepState(LPC_PMU_T *pPMU);

/**
 * @brief	Enter MCU Deep Sleep mode
 * @param	pPMU	: Pointer to PMU register block
 * @return	None
 * @note	In Deep-sleep mode, the peripherals receive no internal clocks.
 * The flash is in stand-by mode. The SRAM memory and all peripheral registers
 * as well as the processor maintain their internal states. The WWDT, WKT,
 * and BOD can remain active to wake up the system on an interrupt.
 */
void Chip_PMU_DeepSleepState(LPC_PMU_T *pPMU);

/**
 * @brief	Enter MCU Power down mode
 * @param	pPMU	: Pointer to PMU register block
 * @return	None
 * @note	In Power-down mode, the peripherals receive no internal clocks.
 * The internal SRAM memory and all peripheral registers as well as the
 * processor maintain their internal states. The flash memory is powered
 * down. The WWDT, WKT, and BOD can remain active to wake up the system
 * on an interrupt.
 */
void Chip_PMU_PowerDownState(LPC_PMU_T *pPMU);

/**
 * @brief	Enter MCU Deep Power down mode
 * @param	pPMU	: Pointer to PMU register block
 * @return	None
 * @note	For maximal power savings, the entire system is shut down
 * except for the general purpose registers in the PMU and the self
 * wake-up timer. Only the general purpose registers in the PMU maintain
 * their internal states. The part can wake up on a pulse on the WAKEUP
 * pin or when the self wake-up timer times out. On wake-up, the part
 * reboots.
 */
void Chip_PMU_DeepPowerDownState(LPC_PMU_T *pPMU);

/**
 * @brief	Place the MCU in a low power state
 * @param	pPMU		: Pointer to PMU register block
 * @param	SleepMode	: Sleep mode
 * @return	None
 */
void Chip_PMU_Sleep(LPC_PMU_T *pPMU, CHIP_PMU_MCUPOWER_T SleepMode);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* __PMU_17XX_40XX_H_ */

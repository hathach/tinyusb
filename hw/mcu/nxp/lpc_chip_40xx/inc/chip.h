/*
 * @brief LPC17xx/LPC40xx basic chip inclusion file
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

#ifndef __CHIP_H_
#define __CHIP_H_

#include "lpc_types.h"
#include "sys_config.h"
#include "cmsis.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @ingroup CHIP_17XX_40XX_DRIVER_OPTIONS
 * @{
 */

/**
 * @brief	System oscillator rate
 * This value is defined externally to the chip layer and contains
 * the value in Hz for the external oscillator for the board. If using the
 * internal oscillator, this rate can be 0.
 */
extern const uint32_t OscRateIn;

/**
 * @brief	RTC oscillator rate
 * This value is defined externally to the chip layer and contains
 * the value in Hz for the RTC oscillator for the board. This is
 * usually 32KHz (32768). If not using the RTC, this rate can be 0.
 */
extern const uint32_t RTCOscRateIn;

/**
 * @}
 */

/** @defgroup SUPPORT_17XX_40XX_FUNC CHIP: LPC17xx/40xx support functions
 * @ingroup CHIP_17XX_40XX_Drivers
 * @{
 */

/**
 * @brief	Current system clock rate, mainly used for sysTick
 */
extern uint32_t SystemCoreClock;

/**
 * @brief	Update system core clock rate, should be called if the
 *			system has a clock rate change
 * @return	None
 */
void SystemCoreClockUpdate(void);

/**
 * @brief	Set up and initialize hardware prior to call to main()
 * @return	None
 * @note	Chip_SystemInit() is called prior to the application and sets up
 * system clocking prior to the application starting.
 */
void Chip_SystemInit(void);

/**
 * @brief	USB Pin and clock initialization
 * Calling this function will initialize the USB pins and the clock
 * @return	None
 * @note	This function will assume that the chip is clocked by an
 * external crystal oscillator of frequency 12MHz and the Oscillator
 * is running.
 */
void Chip_USB_Init(void);

/**
 * @brief	Clock and PLL initialization based on the external oscillator
 * @return	None
 * @note	This function assumes an external crystal oscillator
 * frequency of 12MHz.
 */
void Chip_SetupXtalClocking(void);

/**
 * @brief	Clock and PLL initialization based on the internal oscillator
 * @return	None
 */
void Chip_SetupIrcClocking(void);

/**
 * @}
 */

#if defined(CHIP_LPC175X_6X) || defined(CHIP_LPC177X_8X)

#ifndef CORE_M3
#error CORE_M3 is not defined for the LPC17xx architecture
#error CORE_M3 should be defined as part of your compiler define list
#endif

#elif defined(CHIP_LPC40XX)

#ifndef CORE_M4
#error CORE_M4 is not defined for the LPC40xx architecture
#error CORE_M4 should be defined as part of your compiler define list
#endif

#elif defined(CHIP_LPC40XX)
#error CHIP_LPC175X_6X/CHIP_LPC177X_8X/CHIP_LPC40XX is not defined!
#endif

#if defined(CHIP_LPC175X_6X)
#include "chip_lpc175x_6x.h"

#elif defined(CHIP_LPC177X_8X)
#include "chip_lpc177x_8x.h"

#elif defined(CHIP_LPC40XX)
#include "chip_lpc407x_8x.h"
#endif

#ifdef __cplusplus
}
#endif

#endif /* __CHIP_H_ */

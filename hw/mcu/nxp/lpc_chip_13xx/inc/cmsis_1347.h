/*
 * @brief Basic CMSIS include file
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

#ifndef __CMSIS_1347_H_
#define __CMSIS_1347_H_

#include "lpc_types.h"
#include "sys_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @defgroup CMSIS_1347 CHIP: LPC1347 CMSIS include file
 * @ingroup CHIP_13XX_CMSIS_Drivers
 * @{
 */

#if defined(__ARMCC_VERSION)
// Kill warning "#pragma push with no matching #pragma pop"
  #pragma diag_suppress 2525
  #pragma push
  #pragma anon_unions
#elif defined(__CWCC__)
  #pragma push
  #pragma cpp_extensions on
#elif defined(__GNUC__)
/* anonymous unions are enabled by default */
#elif defined(__IAR_SYSTEMS_ICC__)
//  #pragma push // FIXME not usable for IAR
  #pragma language=extended
#else
  #error Not supported compiler type
#endif

/*
 * ==========================================================================
 * ---------- Interrupt Number Definition -----------------------------------
 * ==========================================================================
 */
 
#if !defined(CHIP_LPC1347)
#error Incorrect or missing device variant (CHIP_LPC1347)
#endif

/** @defgroup CMSIS_1347_IRQ CHIP_1347: LPC1315/LPC1316/LPC1317/LPC1345/LPC1346/LPC1347 peripheral interrupt numbers
 * @{
 */

typedef enum LPC1347_IRQn {
	Reset_IRQn                    = -15,/*!< 1  Reset Vector, invoked on Power up and warm reset */
	NonMaskableInt_IRQn           = -14,/*!< 2  Non maskable Interrupt, cannot be stopped or preempted */
	HardFault_IRQn                = -13,/*!< 3  Hard Fault, all classes of Fault */
	MemoryManagement_IRQn         = -12,/*!< 4  Memory Management, MPU mismatch, including Access Violation and No Match */
	BusFault_IRQn                 = -11,/*!< 5  Bus Fault, Pre-Fetch-, Memory Access Fault, other address/memory related Fault */
	UsageFault_IRQn               = -10,/*!< 6  Usage Fault, i.e. Undef Instruction, Illegal State Transition */
	SVCall_IRQn                   = -5,	/*!< 11  System Service Call via SVC instruction */
	DebugMonitor_IRQn             = -4,	/*!< 12  Debug Monitor                    */
	PendSV_IRQn                   = -2,	/*!< 14  Pendable request for system service */
	SysTick_IRQn                  = -1,	/*!< 15  System Tick Timer                */

	/******  LPC1347 Specific Interrupt Numbers *******************************************************/
	PIN_INT0_IRQn                 = 0,	/*!< 0  PIN_INT0                         */
	PIN_INT1_IRQn                 = 1,	/*!< 1  PIN_INT1                         */
	PIN_INT2_IRQn                 = 2,	/*!< 2  PIN_INT2                         */
	PIN_INT3_IRQn                 = 3,	/*!< 3  PIN_INT3                         */
	PIN_INT4_IRQn                 = 4,	/*!< 4  PIN_INT4                         */
	PIN_INT5_IRQn                 = 5,	/*!< 5  PIN_INT5                         */
	PIN_INT6_IRQn                 = 6,	/*!< 6  PIN_INT6                         */
	PIN_INT7_IRQn                 = 7,	/*!< 7  PIN_INT7                         */
	GINT0_IRQn                    = 8,	/*!< 8  GINT0                            */
	GINT1_IRQn                    = 9,	/*!< 9  GINT1                            */
	Reserved0_IRQn                = 10,	/*!< 10  Reserved Interrupt               */
	Reserved1_IRQn                = 11,	/*!< 11  Reserved Interrupt               */
	RIT_IRQn                      = 12,	/*!< 12  Repetitive Interrupt Timer       */
	Reserved2_IRQn                = 13,	/*!< 13  Reserved Interrupt               */
	SSP1_IRQn                     = 14,	/*!< 14  SSP1                             */
	I2C0_IRQn                     = 15,	/*!< 15  I2C                              */
	TIMER_16_0_IRQn               = 16,	/*!< 16-bit Timer0 Interrupt                          */
	TIMER_16_1_IRQn               = 17,	/*!< 16-bit Timer1 Interrupt                          */
	TIMER_32_0_IRQn               = 18,	/*!< 32-bit Timer0 Interrupt                          */
	TIMER_32_1_IRQn               = 19,	/*!< 32-bit Timer1 Interrupt                          */
	SSP0_IRQn                     = 20,	/*!< 20  SSP0                             */
	UART0_IRQn                    = 21,	/*!< 21  USART                            */
	USB0_IRQn                     = 22,	/*!< 22  USB_IRQ                          */
	USB0_FIQ_IRQn                 = 23,	/*!< 23  USB_FIQ                          */
	ADC_IRQn                      = 24,	/*!< 24  ADC                              */
	WDT_IRQn                      = 25,	/*!< 25  WDT                              */
	BOD_IRQn                      = 26,	/*!< 26  BOD                              */
	FMC_IRQn                      = 27,	/*!< 27  FMC                              */
	Reserved3_IRQn                = 28,	/*!< 28  Reserved Interrupt               */
	Reserved4_IRQn                = 29,	/*!< 29  Reserved Interrupt               */
	USB_WAKEUP_IRQn               = 30,	/*!< 30  USBWAKEUP                        */
	Reserved5_IRQn                = 31,	/*!< 31  Reserved Interrupt               */
} LPC1347_IRQn_Type;

/**
 * @}
 */

/*
 * ==========================================================================
 * ----------- Processor and Core Peripheral Section ------------------------
 * ==========================================================================
 */

/** @defgroup CMSIS_1347_COMMON CHIP: Common LPC1347 Cortex CMSIS definitions
 * @{
 */

/* Configuration of the Cortex-M3 Processor and Core Peripherals */
#define __CM3_REV              0x0201		/*!< Cortex-M3 Core Revision               */
#define __MPU_PRESENT             0			/*!< MPU present or not                    */
#define __NVIC_PRIO_BITS          3			/*!< Number of Bits used for Priority Levels */
#define __Vendor_SysTickConfig    0			/*!< Set to 1 if different SysTick Config is used */

/**
 * @}
 */

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* __CMSIS_1347_H_ */

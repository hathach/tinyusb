/*
 * @brief Basic CMSIS include file for LPC175x/6x
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

#ifndef __CMSIS_175X_6X_H_
#define __CMSIS_175X_6X_H_

#include "lpc_types.h"
#include "sys_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @defgroup CMSIS_175X_6X CHIP: LPC175x/6x CMSIS include file
 * @ingroup CHIP_17XX_40XX_Drivers
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
#if !defined(CHIP_LPC175X_6X)
#error Incorrect or missing device variant (CHIP_LPC175X_6X)
#endif

/** @defgroup CMSIS_175X_6X_IRQ CHIP_175X_6X: LPC175x/6x peripheral interrupt numbers
 * @{
 */

typedef enum {
	/* -------------------------  Cortex-M3 Processor Exceptions Numbers  ----------------------------- */
	Reset_IRQn                    = -15,		/*!< 1 Reset Vector, invoked on Power up and warm reset */
	NonMaskableInt_IRQn           = -14,		/*!< 2 Non maskable Interrupt, cannot be stopped or preempted */
	HardFault_IRQn                = -13,		/*!< 3 Hard Fault, all classes of Fault */
	MemoryManagement_IRQn         = -12,		/*!< 4 Memory Management, MPU mismatch, including Access Violation and No Match */
	BusFault_IRQn                 = -11,		/*!< 5 Bus Fault, Pre-Fetch-, Memory Access Fault, other address/memory related Fault */
	UsageFault_IRQn               = -10,		/*!< 6 Usage Fault, i.e. Undef Instruction, Illegal State Transition  */
	SVCall_IRQn                   = -5,			/*!< 11 System Service Call via SVC instruction   */
	DebugMonitor_IRQn             = -4,			/*!< 12 CDebug Monitor   */
	PendSV_IRQn                   = -2,			/*!< 14 Pendable request for system service */
	SysTick_IRQn                  = -1,			/*!< 15 System Tick Interrupt */

	/* ---------------------------  LPC17xx Specific Interrupt Numbers  ------------------------------- */
	WDT_IRQn                      = 0,			/*!< Watchdog Timer Interrupt                         */
	TIMER0_IRQn                   = 1,			/*!< Timer0 Interrupt                                 */
	TIMER1_IRQn                   = 2,			/*!< Timer1 Interrupt                                 */
	TIMER2_IRQn                   = 3,			/*!< Timer2 Interrupt                                 */
	TIMER3_IRQn                   = 4,			/*!< Timer3 Interrupt                                 */
	UART0_IRQn                    = 5,			/*!< UART0 Interrupt                                  */
	UART_IRQn                     = UART0_IRQn,	/*!< Alias for UART0 Interrupt                        */
	UART1_IRQn                    = 6,			/*!< UART1 Interrupt                                  */
	UART2_IRQn                    = 7,			/*!< UART2 Interrupt                                  */
	UART3_IRQn                    = 8,			/*!< UART3 Interrupt                                  */
	PWM1_IRQn                     = 9,			/*!< PWM1 Interrupt                                   */
	I2C0_IRQn                     = 10,			/*!< I2C0 Interrupt                                   */
	I2C_IRQn                      = I2C0_IRQn,	/*!< Alias for I2C0 Interrupt                         */
	I2C1_IRQn                     = 11,			/*!< I2C1 Interrupt                                   */
	I2C2_IRQn                     = 12,			/*!< I2C2 Interrupt                                   */
	SPI_IRQn                      = 13,			/*!< SPI Interrupt                                    */
	SSP0_IRQn                     = 14,			/*!< SSP0 Interrupt                                   */
	SSP_IRQn                      = SSP0_IRQn,	/*!< Alias for SSP0 Interrupt                         */
	SSP1_IRQn                     = 15,			/*!< SSP1 Interrupt                                   */
	PLL0_IRQn                     = 16,			/*!< PLL0 Lock (Main PLL) Interrupt                   */
	RTC_IRQn                      = 17,			/*!< Real Time Clock and event recorder Interrupt     */
	EINT0_IRQn                    = 18,			/*!< External Interrupt 0 Interrupt                   */
	EINT1_IRQn                    = 19,			/*!< External Interrupt 1 Interrupt                   */
	EINT2_IRQn                    = 20,			/*!< External Interrupt 2 Interrupt                   */
	EINT3_IRQn                    = 21,			/*!< External Interrupt 3 Interrupt                   */
	ADC_IRQn                      = 22,			/*!< A/D Converter Interrupt                          */
	BOD_IRQn                      = 23,			/*!< Brown-Out Detect Interrupt                       */
	USB_IRQn                      = 24,			/*!< USB Interrupt                                    */
	CAN_IRQn                      = 25,			/*!< CAN Interrupt                                    */
	DMA_IRQn                      = 26,			/*!< General Purpose DMA Interrupt                    */
	I2S_IRQn                      = 27,			/*!< I2S Interrupt                                    */
	ETHERNET_IRQn                 = 28,			/*!< Ethernet Interrupt                               */
	RITIMER_IRQn                  = 29,			/*!< Repetitive Interrupt Interrupt                   */
	MCPWM_IRQn                    = 30,			/*!< Motor Control PWM Interrupt                      */
	QEI_IRQn                      = 31,			/*!< Quadrature Encoder Interface Interrupt           */
	PLL1_IRQn                     = 32,			/*!< PLL1 Lock (USB PLL) Interrupt                    */
	USBActivity_IRQn              = 33,			/*!< USB Activity interrupt                           */
	CANActivity_IRQn              = 34,			/*!< CAN Activity interrupt                           */
} LPC175X_6X_IRQn_Type;

/**
 * @}
 */

/*
 * ==========================================================================
 * ----------- Processor and Core Peripheral Section ------------------------
 * ==========================================================================
 */

/** @defgroup CMSIS_175X_6X_COMMON CHIP: LPC175x/6x Cortex CMSIS definitions
 * @{
 */

#define __CM3_REV                 0x0200
#define __MPU_PRESENT             1			/*!< MPU present or not                    */
#define __NVIC_PRIO_BITS          5			/*!< Number of Bits used for Priority Levels */
#define __Vendor_SysTickConfig    0			/*!< Set to 1 if different SysTick Config is used */
#define __FPU_PRESENT             0			/*!< FPU present or not                    */

/**
 * @}
 */

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* __CMSIS_175X_6X_H_ */

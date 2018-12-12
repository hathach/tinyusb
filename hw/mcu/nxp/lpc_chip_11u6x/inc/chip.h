/*
 * @brief LPC11U6x basic chip inclusion file
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

#ifndef __CHIP_H_
#define __CHIP_H_

#include "lpc_types.h"
#include "sys_config.h"
#include "cmsis.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef CORE_M0PLUS
#error CORE_M0PLUS is not defined for the LPC11U6x architecture
#error CORE_M0PLUS should be defined as part of your compiler define list
#endif

#if !defined(CHIP_LPC11U6X)
#error "CHIP_LPC11U6X is not defined!"
#endif

/** @defgroup PERIPH_11U6X_BASE CHIP: LPC11u6x Peripheral addresses and register set declarations
 * @ingroup CHIP_11U6X_Drivers
 * @{
 */

#define LPC_PMU_BASE              0x40038000
#define LPC_IOCON_BASE            0x40044000
#define LPC_SYSCTL_BASE           0x40048000
#define LPC_GPIO_PORT_BASE        0xA0000000
#define LPC_GPIO_GROUP_INT0_BASE  0x4005C000
#define LPC_GPIO_GROUP_INT1_BASE  0x40060000
#define LPC_PIN_INT_BASE          0xA0004000
#define LPC_USART0_BASE           0x40008000
#define LPC_USART1_BASE           0x4006C000
#define LPC_USART2_BASE           0x40070000
#define LPC_USART3_BASE           0x40074000
#define LPC_USART4_BASE           0x4004C000
#define LPC_I2C0_BASE             0x40000000
#define LPC_I2C1_BASE             0x40020000
#define LPC_SSP0_BASE             0x40040000
#define LPC_SSP1_BASE             0x40058000
#define LPC_USB0_BASE             0x40080000
#define LPC_ADC_BASE              0x4001C000
#define LPC_SCT0_BASE             0x5000C000
#define LPC_SCT1_BASE             0x5000E000
#define LPC_TIMER16_0_BASE        0x4000C000
#define LPC_TIMER16_1_BASE        0x40010000
#define LPC_TIMER32_0_BASE        0x40014000
#define LPC_TIMER32_1_BASE        0x40018000
#define LPC_RTC_BASE              0x40024000
#define LPC_WWDT_BASE             0x40004000
#define LPC_DMA_BASE              0x50004000
#define LPC_CRC_BASE              0x50000000
#define LPC_FLASH_BASE            0x4003C000
#define LPC_DMATRIGMUX_BASE       0x40028000UL

#define LPC_PMU                   ((LPC_PMU_T              *) LPC_PMU_BASE)
#define LPC_IOCON                 ((LPC_IOCON_T            *) LPC_IOCON_BASE)
#define LPC_SYSCTL                ((LPC_SYSCTL_T           *) LPC_SYSCTL_BASE)
#define LPC_SYSCON                ((LPC_SYSCTL_T           *) LPC_SYSCTL_BASE) /* Alias for LPC_SYSCTL */
#define LPC_GPIO                  ((LPC_GPIO_T             *) LPC_GPIO_PORT_BASE)
#define LPC_GPIOGROUP             ((LPC_GPIOGROUPINT_T     *) LPC_GPIO_GROUP_INT0_BASE)
#define LPC_PININT                ((LPC_PIN_INT_T          *) LPC_PIN_INT_BASE)
#define LPC_USART0                ((LPC_USART0_T           *) LPC_USART0_BASE)
#define LPC_USART1                ((LPC_USARTN_T           *) LPC_USART1_BASE)
#define LPC_USART2                ((LPC_USARTN_T           *) LPC_USART2_BASE)
#define LPC_USART3                ((LPC_USARTN_T           *) LPC_USART3_BASE)
#define LPC_USART4                ((LPC_USARTN_T           *) LPC_USART4_BASE)
#define LPC_I2C0                  ((LPC_I2C_T              *) LPC_I2C0_BASE)
#define LPC_I2C1                  ((LPC_I2C_T              *) LPC_I2C1_BASE)
#define LPC_SSP0                  ((LPC_SSP_T              *) LPC_SSP0_BASE)
#define LPC_SSP1                  ((LPC_SSP_T              *) LPC_SSP1_BASE)
#define LPC_USB                   ((LPC_USB_T              *) LPC_USB0_BASE)
#define LPC_ADC                   ((LPC_ADC_T              *) LPC_ADC_BASE)
#define LPC_SCT0                  ((LPC_SCT_T              *) LPC_SCT0_BASE)
#define LPC_SCT1                  ((LPC_SCT_T              *) LPC_SCT1_BASE)
#define LPC_TIMER16_0             ((LPC_TIMER_T            *) LPC_TIMER16_0_BASE)
#define LPC_TIMER16_1             ((LPC_TIMER_T            *) LPC_TIMER16_1_BASE)
#define LPC_TIMER32_0             ((LPC_TIMER_T            *) LPC_TIMER32_0_BASE)
#define LPC_TIMER32_1             ((LPC_TIMER_T            *) LPC_TIMER32_1_BASE)
#define LPC_RTC                   ((LPC_RTC_T              *) LPC_RTC_BASE)
#define LPC_WWDT                  ((LPC_WWDT_T             *) LPC_WWDT_BASE)
#define LPC_DMA                   ((LPC_DMA_T              *) LPC_DMA_BASE)
#define LPC_DMATRIGMUX            ((LPC_DMATRIGMUX_T       *) LPC_DMATRIGMUX_BASE)
#define LPC_CRC                   ((LPC_CRC_T              *) LPC_CRC_BASE)
#define LPC_FMC                   ((LPC_FMC_T              *) LPC_FLASH_BASE)

/* IRQ Handler Alias list */
#define UART1_4_IRQHandler        USART1_4_IRQHandler
#define UART2_3_IRQHandler        USART2_3_IRQHandler
#define UART0_IRQHandler          USART0_IRQHandler

/**
 * @}
 */

/** @ingroup CHIP_11U6X_DRIVER_OPTIONS
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

/* Include order is important! */
#include "romapi_11u6x.h"
#include "syscon_11u6x.h"
#include "clock_11u6x.h"
#include "iocon_11u6x.h"
#include "pmu_11u6x.h"
#include "crc_11u6x.h"
#include "gpio_11u6x.h"
#include "pinint_11u6x.h"
#include "gpiogroup_11u6x.h"
#include "timer_11u6x.h"
#include "uart_0_11u6x.h"
#include "uart_n_11u6x.h"
#include "ssp_11u6x.h"
#include "adc_11u6x.h"
#include "dma_11u6x.h"
#include "i2c_11u6x.h"
#include "i2cm_11u6x.h"
#include "usbd_11u6x.h"
#include "sct_11u6x.h"
#include "rtc_11u6x.h"
#include "wwdt_11u6x.h"
#include "fmc_11u6x.h"

/** @defgroup SUPPORT_11U6X_FUNC CHIP: LPC11u6x support functions
 * @ingroup CHIP_11U6X_Drivers
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

#ifdef __cplusplus
}
#endif

#endif /* __CHIP_H_ */

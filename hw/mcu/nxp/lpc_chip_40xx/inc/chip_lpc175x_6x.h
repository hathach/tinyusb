/*
 * @brief LPC175x/6x basic chip inclusion file
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

#ifndef __CHIP_LPC175X_6X_H_
#define __CHIP_LPC175X_6X_H_

#include "lpc_types.h"
#include "sys_config.h"
#include "cmsis.h"

#ifdef __cplusplus
extern "C" {
#endif

#if !defined(CORE_M3)
#error CORE_M3 is not defined for the LPC175x/6x architecture
#error CORE_M3 should be defined as part of your compiler define list
#endif

#ifndef CHIP_LPC175X_6X
#error CHIP_LPC175X_6X is not defined!
#endif

/** @defgroup PERIPH_175X_6X_BASE CHIP: LPC175x/6x Peripheral addresses and register set declarations
 * @ingroup CHIP_17XX_40XX_Drivers
 * @{
 */

#define LPC_GPIO0_BASE            0x2009C000
#define LPC_GPIO1_BASE            0x2009C020
#define LPC_GPIO2_BASE            0x2009C040
#define LPC_GPIO3_BASE            0x2009C060
#define LPC_GPIO4_BASE            0x2009C080

/* APB0 peripheral */
#define LPC_WWDT_BASE             0x40000000
#define LPC_TIMER0_BASE           0x40004000
#define LPC_TIMER1_BASE           0x40008000
#define LPC_UART0_BASE            0x4000C000
#define LPC_UART1_BASE            0x40010000
#define LPC_PWM1_BASE             0x40018000
#define LPC_I2C0_BASE             0x4001C000
#define LPC_SPI_BASE              0x40020000
#define LPC_RTC_BASE              0x40024000
#define LPC_REGFILE_BASE          0x40024044
#define LPC_GPIOINT_BASE          0x40028080
#define LPC_IOCON_BASE            0x4002C000
#define LPC_SSP1_BASE             0x40030000
#define LPC_ADC_BASE              0x40034000
#define LPC_CANAF_RAM_BASE        0x40038000
#define LPC_CANAF_BASE            0x4003C000
#define LPC_CANCR_BASE            0x40040000
#define LPC_CAN1_BASE             0x40044000
#define LPC_CAN2_BASE             0x40048000
#define LPC_I2C1_BASE             0x4005C000

/* APB1 peripheral */
#define LPC_FMC_BASE              0x40084000
#define LPC_SSP0_BASE             0x40088000
#define LPC_DAC_BASE              0x4008C000
#define LPC_TIMER2_BASE           0x40090000
#define LPC_TIMER3_BASE           0x40094000
#define LPC_UART2_BASE            0x40098000
#define LPC_UART3_BASE            0x4009C000
#define LPC_I2C2_BASE             0x400A0000
#define LPC_I2S_BASE              0x400A8000
#define LPC_RITIMER_BASE          0x400B0000
#define LPC_MCPWM_BASE            0x400B8000
#define LPC_QEI_BASE              0x400BC000
#define LPC_SYSCTL_BASE           0x400FC000
#define LPC_PMU_BASE              0x400FC0C0

/* AHB peripheral */
#define LPC_ENET_BASE             0x50000000
#define LPC_GPDMA_BASE            0x50004000
#define LPC_USB_BASE              0x5000C000

/* Assign LPC_* names to structures mapped to addresses */
#define LPC_PMU                   ((LPC_PMU_T              *) LPC_PMU_BASE)
#define LPC_GPDMA                 ((LPC_GPDMA_T            *) LPC_GPDMA_BASE)
#define LPC_USB                   ((LPC_USB_T              *) LPC_USB_BASE)
#define LPC_ETHERNET              ((LPC_ENET_T             *) LPC_ENET_BASE)
#define LPC_GPIO                  ((LPC_GPIO_T             *) LPC_GPIO0_BASE)
#define LPC_GPIO1                 ((LPC_GPIO_T             *) LPC_GPIO1_BASE)
#define LPC_GPIO2                 ((LPC_GPIO_T             *) LPC_GPIO2_BASE)
#define LPC_GPIO3                 ((LPC_GPIO_T             *) LPC_GPIO3_BASE)
#define LPC_GPIO4                 ((LPC_GPIO_T             *) LPC_GPIO4_BASE)
#define LPC_GPIOINT               ((LPC_GPIOINT_T          *) LPC_GPIOINT_BASE)
#define LPC_RTC                   ((LPC_RTC_T              *) LPC_RTC_BASE)
#define LPC_REGFILE               ((LPC_REGFILE_T          *) LPC_REGFILE_BASE)
#define LPC_WWDT                  ((LPC_WWDT_T             *) LPC_WWDT_BASE)
#define LPC_UART0                 ((LPC_USART_T            *) LPC_UART0_BASE)
#define LPC_UART1                 ((LPC_USART_T            *) LPC_UART1_BASE)
#define LPC_UART2                 ((LPC_USART_T            *) LPC_UART2_BASE)
#define LPC_UART3                 ((LPC_USART_T            *) LPC_UART3_BASE)
#define LPC_SPI                   ((LPC_SPI_T              *) LPC_SPI_BASE)
#define LPC_SSP0                  ((LPC_SSP_T              *) LPC_SSP0_BASE)
#define LPC_SSP1                  ((LPC_SSP_T              *) LPC_SSP1_BASE)
#define LPC_TIMER0                ((LPC_TIMER_T            *) LPC_TIMER0_BASE)
#define LPC_TIMER1                ((LPC_TIMER_T            *) LPC_TIMER1_BASE)
#define LPC_TIMER2                ((LPC_TIMER_T            *) LPC_TIMER2_BASE)
#define LPC_TIMER3                ((LPC_TIMER_T            *) LPC_TIMER3_BASE)
#define LPC_MCPWM                 ((LPC_MCPWM_T            *) LPC_MCPWM_BASE)
#define LPC_I2C0                  ((LPC_I2C_T              *) LPC_I2C0_BASE)
#define LPC_I2C1                  ((LPC_I2C_T              *) LPC_I2C1_BASE)
#define LPC_I2C2                  ((LPC_I2C_T              *) LPC_I2C2_BASE)
#define LPC_I2S                   ((LPC_I2S_T              *) LPC_I2S_BASE)
#define LPC_QEI                   ((LPC_QEI_T              *) LPC_QEI_BASE)
#define LPC_DAC                   ((LPC_DAC_T              *) LPC_DAC_BASE)
#define LPC_ADC                   ((LPC_ADC_T              *) LPC_ADC_BASE)
#define LPC_IOCON                 ((LPC_IOCON_T            *) LPC_IOCON_BASE)
#define LPC_SYSCTL                ((LPC_SYSCTL_T           *) LPC_SYSCTL_BASE)
#define LPC_SYSCON                ((LPC_SYSCTL_T           *) LPC_SYSCTL_BASE) /* Alias for LPC_SYSCTL */
#define LPC_CANAF_RAM             ((LPC_CANAF_RAM_T        *) LPC_CANAF_RAM_BASE)
#define LPC_CANAF                 ((LPC_CANAF_T            *) LPC_CANAF_BASE)
#define LPC_CANCR                 ((LPC_CANCR_T            *) LPC_CANCR_BASE)
#define LPC_CAN1                  ((LPC_CAN_T              *) LPC_CAN1_BASE)
#define LPC_CAN2                  ((LPC_CAN_T              *) LPC_CAN2_BASE)
#define LPC_RITIMER               ((LPC_RITIMER_T          *) LPC_RITIMER_BASE)
#define LPC_FMC                   ((LPC_FMC_T              *) LPC_FMC_BASE)

/* IRQ Handler Alias list */
#define UART_IRQHandler           UART0_IRQHandler
#define I2C_IRQHandler            I2C0_IRQHandler
#define SSP_IRQHandler            SSP0_IRQHandler

/**
 * @}
 */

#include "sysctl_17xx_40xx.h"
#include "clock_17xx_40xx.h"
#include "iocon_17xx_40xx.h"
#include "adc_17xx_40xx.h"
#include "can_17xx_40xx.h"
#include "dac_17xx_40xx.h"
#include "enet_17xx_40xx.h"
#include "gpdma_17xx_40xx.h"
#include "gpio_17xx_40xx.h"
#include "gpioint_17xx_40xx.h"
#include "i2c_17xx_40xx.h"
#include "i2s_17xx_40xx.h"
#include "mcpwm_17xx_40xx.h"
#include "pmu_17xx_40xx.h"
#include "qei_17xx_40xx.h"
#include "ritimer_17xx_40xx.h"
#include "rtc_17xx_40xx.h"
#include "spi_17xx_40xx.h"
#include "ssp_17xx_40xx.h"
#include "timer_17xx_40xx.h"
#include "uart_17xx_40xx.h"
#include "usb_17xx_40xx.h"
#include "wwdt_17xx_40xx.h"
#include "fmc_17xx_40xx.h"
#include "romapi_17xx_40xx.h"
/* FIXME : PWM drivers */

#ifdef __cplusplus
}
#endif

#endif /* __CHIP_LPC175X_6X_H_ */

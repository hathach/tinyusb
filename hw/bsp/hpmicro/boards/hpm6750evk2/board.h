/*
 * Copyright (c) 2021-2025 HPMicro
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef _HPM_BOARD_H
#define _HPM_BOARD_H

#include <stdio.h>
#include "hpm_common.h"
#include "hpm_soc.h"
#include "hpm_soc_feature.h"
#include "pinmux.h"

#define BOARD_NAME          "hpm6750evk2"

#ifndef BOARD_RUNNING_CORE
#define BOARD_RUNNING_CORE HPM_CORE0
#endif

#define BOARD_CPU_FREQ (816000000UL)

/* console section */
#if BOARD_RUNNING_CORE == HPM_CORE0
    #define BOARD_CONSOLE_UART_BASE       HPM_UART0
    #define BOARD_CONSOLE_UART_CLK_NAME   clock_uart0
    #define BOARD_CONSOLE_UART_IRQ        IRQn_UART0
    #define BOARD_CONSOLE_UART_TX_DMA_REQ HPM_DMA_SRC_UART0_TX
    #define BOARD_CONSOLE_UART_RX_DMA_REQ HPM_DMA_SRC_UART0_RX
#else
    #define BOARD_CONSOLE_UART_BASE       HPM_UART13
    #define BOARD_CONSOLE_UART_CLK_NAME   clock_uart13
    #define BOARD_CONSOLE_UART_IRQ        IRQn_UART13
    #define BOARD_CONSOLE_UART_TX_DMA_REQ HPM_DMA_SRC_UART13_TX
    #define BOARD_CONSOLE_UART_RX_DMA_REQ HPM_DMA_SRC_UART13_RX
#endif

#define BOARD_CONSOLE_UART_BAUDRATE (115200UL)

/* sdram section */
#define BOARD_SDRAM_ADDRESS          (0x40000000UL)
#define BOARD_SDRAM_SIZE             (32 * SIZE_1MB)
#define BOARD_SDRAM_CS               FEMC_SDRAM_CS0
#define BOARD_SDRAM_PORT_SIZE        FEMC_SDRAM_PORT_SIZE_32_BITS
#define BOARD_SDRAM_COLUMN_ADDR_BITS FEMC_SDRAM_COLUMN_ADDR_9_BITS
#define BOARD_SDRAM_REFRESH_COUNT    (8192UL)
#define BOARD_SDRAM_REFRESH_IN_MS    (64UL)

#define BOARD_FLASH_BASE_ADDRESS (0x80000000UL)
#define BOARD_FLASH_SIZE         (16 << 20)

/* gpio section */
#define BOARD_R_GPIO_CTRL  HPM_GPIO0
#define BOARD_R_GPIO_INDEX GPIO_DI_GPIOB
#define BOARD_R_GPIO_PIN   11
#define BOARD_G_GPIO_CTRL  HPM_GPIO0
#define BOARD_G_GPIO_INDEX GPIO_DI_GPIOB
#define BOARD_G_GPIO_PIN   12
#define BOARD_B_GPIO_CTRL  HPM_GPIO0
#define BOARD_B_GPIO_INDEX GPIO_DI_GPIOB
#define BOARD_B_GPIO_PIN   13

#define BOARD_LED_GPIO_CTRL HPM_GPIO0

#define BOARD_LED_GPIO_INDEX GPIO_DI_GPIOB
#define BOARD_LED_GPIO_PIN   12
#define BOARD_LED_OFF_LEVEL  0
#define BOARD_LED_ON_LEVEL   1

#define BOARD_LED_TOGGLE_RGB 1

#define BOARD_APP_GPIO_INDEX GPIO_DI_GPIOZ
#define BOARD_APP_GPIO_PIN   2
#define BOARD_BUTTON_PRESSED_VALUE 0

#define USING_GPIO0_FOR_GPIOZ
#ifndef USING_GPIO0_FOR_GPIOZ
#define BOARD_APP_GPIO_CTRL HPM_BGPIO
#define BOARD_APP_GPIO_IRQ  IRQn_BGPIO
#else
#define BOARD_APP_GPIO_CTRL HPM_GPIO0
#define BOARD_APP_GPIO_IRQ  IRQn_GPIO0_Z
#endif

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */


void board_init_clock(void);
void board_init_pmp(void);
void board_init_console(void);
void board_init_gpio_pins(void);
void board_init_led_pins(void);
void board_init_usb(USB_Type *ptr);
void board_print_banner(void);
void board_print_clock_freq(void);


#if defined(__cplusplus)
}
#endif /* __cplusplus */
#endif /* _HPM_BOARD_H */

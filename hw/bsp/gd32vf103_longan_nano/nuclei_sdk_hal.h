// See LICENSE for license details.
#ifndef _NUCLEI_SDK_HAL_H
#define _NUCLEI_SDK_HAL_H

#include "nmsis_gcc.h"
#include "gd32vf103.h"
#include "gd32vf103_libopt.h"
#include "drv_usb_hw.h"
#include "drv_usb_dev.h"

#ifdef __cplusplus
extern "C" {
#endif

/* sipeed longan nano board UART com port */
#define SOC_DEBUG_UART      USART0
#define GD32_COM0 USART0
#define GD32_COM_CLK RCU_USART0
#define GD32_COM_TX_PIN GPIO_PIN_9
#define GD32_COM_RX_PIN GPIO_PIN_10
#define GD32_COM_TX_GPIO_PORT GPIOA
#define GD32_COM_RX_GPIO_PORT GPIOA
#define GD32_COM_TX_GPIO_CLK RCU_GPIOA
#define GD32_COM_RX_GPIO_CLK RCU_GPIOA

#ifdef __cplusplus
}
#endif
#endif

/**
  **************************************************************************
  * @file     at32f415_conf.h
  * @brief    at32f415 config header file
  **************************************************************************
  *                       Copyright notice & Disclaimer
  *
  * The software Board Support Package (BSP) that is made available to
  * download from Artery official website is the copyrighted work of Artery.
  * Artery authorizes customers to use, copy, and distribute the BSP
  * software and its related documentation for the purpose of design and
  * development in conjunction with Artery microcontrollers. Use of the
  * software is governed by this copyright notice and the following disclaimer.
  *
  * THIS SOFTWARE IS PROVIDED ON "AS IS" BASIS WITHOUT WARRANTIES,
  * GUARANTEES OR REPRESENTATIONS OF ANY KIND. ARTERY EXPRESSLY DISCLAIMS,
  * TO THE FULLEST EXTENT PERMITTED BY LAW, ALL EXPRESS, IMPLIED OR
  * STATUTORY OR OTHER WARRANTIES, GUARANTEES OR REPRESENTATIONS,
  * INCLUDING BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY,
  * FITNESS FOR A PARTICULAR PURPOSE, OR NON-INFRINGEMENT.
  *
  **************************************************************************
  */

/* define to prevent recursive inclusion -------------------------------------*/
#ifndef __AT32F415_CONF_H
#define __AT32F415_CONF_H

#ifdef __cplusplus
extern "C" {
#endif


/**
  * @brief in the following line adjust the value of high speed external crystal (hext)
  * used in your application
  * tip: to avoid modifying this file each time you need to use different hext, you
  *      can define the hext value in your toolchain compiler preprocessor.
  */
#if !defined HEXT_VALUE
#define HEXT_VALUE               ((uint32_t)8000000) /*!< value of the high speed external crystal in hz */
#endif

/**
  * @brief in the following line adjust the high speed external crystal (hext) startup
  * timeout value
  */
#define HEXT_STARTUP_TIMEOUT             ((uint16_t)0x3000)  /*!< time out for hext start up */
#define HICK_VALUE                       ((uint32_t)8000000) /*!< value of the high speed internal clock in hz */
#define LEXT_VALUE                       ((uint32_t)32768)   /*!< value of the low speed external clock in hz */

/* module define -------------------------------------------------------------*/
#define CRM_MODULE_ENABLED
//#define CMP_MODULE_ENABLED
//#define TMR_MODULE_ENABLED
//#define ERTC_MODULE_ENABLED
#define GPIO_MODULE_ENABLED
//#define I2C_MODULE_ENABLED
#define USART_MODULE_ENABLED
//#define PWC_MODULE_ENABLED
//#define CAN_MODULE_ENABLED
//#define ADC_MODULE_ENABLED
//#define SPI_MODULE_ENABLED
#define DMA_MODULE_ENABLED
//#define DEBUG_MODULE_ENABLED
#define FLASH_MODULE_ENABLED
//#define CRC_MODULE_ENABLED
//#define WWDT_MODULE_ENABLED
//#define WDT_MODULE_ENABLED
#define EXINT_MODULE_ENABLED
//#define SDIO_MODULE_ENABLED
#define USB_MODULE_ENABLED
#define MISC_MODULE_ENABLED

/* includes ------------------------------------------------------------------*/
#ifdef CRM_MODULE_ENABLED
#include "at32f415_crm.h"
#endif
#ifdef CMP_MODULE_ENABLED
#include "at32f415_cmp.h"
#endif
#ifdef TMR_MODULE_ENABLED
#include "at32f415_tmr.h"
#endif
#ifdef ERTC_MODULE_ENABLED
#include "at32f415_ertc.h"
#endif
#ifdef GPIO_MODULE_ENABLED
#include "at32f415_gpio.h"
#endif
#ifdef I2C_MODULE_ENABLED
#include "at32f415_i2c.h"
#endif
#ifdef USART_MODULE_ENABLED
#include "at32f415_usart.h"
#endif
#ifdef PWC_MODULE_ENABLED
#include "at32f415_pwc.h"
#endif
#ifdef CAN_MODULE_ENABLED
#include "at32f415_can.h"
#endif
#ifdef ADC_MODULE_ENABLED
#include "at32f415_adc.h"
#endif
#ifdef SPI_MODULE_ENABLED
#include "at32f415_spi.h"
#endif
#ifdef DMA_MODULE_ENABLED
#include "at32f415_dma.h"
#endif
#ifdef DEBUG_MODULE_ENABLED
#include "at32f415_debug.h"
#endif
#ifdef FLASH_MODULE_ENABLED
#include "at32f415_flash.h"
#endif
#ifdef CRC_MODULE_ENABLED
#include "at32f415_crc.h"
#endif
#ifdef WWDT_MODULE_ENABLED
#include "at32f415_wwdt.h"
#endif
#ifdef WDT_MODULE_ENABLED
#include "at32f415_wdt.h"
#endif
#ifdef EXINT_MODULE_ENABLED
#include "at32f415_exint.h"
#endif
#ifdef SDIO_MODULE_ENABLED
#include "at32f415_sdio.h"
#endif
#ifdef MISC_MODULE_ENABLED
#include "at32f415_misc.h"
#endif
#ifdef USB_MODULE_ENABLED
#include "at32f415_usb.h"
#endif

#ifdef __cplusplus
}
#endif

#endif /* __AT32F415_CONF_H */

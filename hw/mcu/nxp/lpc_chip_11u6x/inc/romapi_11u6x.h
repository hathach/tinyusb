/*
 * @brief LPC11xx ROM API declarations and functions
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

#ifndef __ROMAPI_11U6X_H_
#define __ROMAPI_11U6X_H_

#include "iap.h"
#include "error.h"
#include "rom_dma_11u6x.h"
#include "rom_i2c_11u6x.h"
#include "rom_pwr_11u6x.h"
#include "rom_uart_11u6x.h"
#include "romdiv_11u6x.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @defgroup ROMAPI_11U6X CHIP: LPC11U6X ROM API declarations and functions
 * @ingroup CHIP_11U6X_Drivers
 * @{
 */

/**
 * @brief LPC11U6X High level ROM API structure
 */
typedef struct {
	const uint32_t usbdApiBase;			/*!< USBD API function table base address */
	const uint32_t reserved0;			/*!< Reserved */
	const uint32_t reserved1;			/*!< Reserved */
	const PWRD_API_T *pPWRD;			/*!< Power API function table base address */
	const ROM_DIV_API_T *divApiBase;	/*!< Divider API function table base address */
	const I2CD_API_T *pI2CD;			/*!< I2C driver API function table base address */
	const DMAD_API_T *pDMAD;			/*!< DMA driver API function table base address */
	const uint32_t reserved2;			/*!< Reserved */
	const uint32_t reserved3;			/*!< Reserved */
	const UARTD_API_T *pUARTND;			/*!< USART 1/2/3/4 driver API function table base address */
	const uint32_t reserved4;			/*!< Reserved */
	const UARTD_API_T *pUART0D;			/*!< USART 0 driver API function table base address */
} LPC_ROM_API_T;

/* Pointer to ROM API function address */
#define LPC_ROM_API_BASE_LOC	0x1FFF1FF8
#define LPC_ROM_API		(*(LPC_ROM_API_T * *) LPC_ROM_API_BASE_LOC)

/* Pointer to @ref PWRD_API_T functions in ROM */
#define LPC_PWRD_API	((LPC_ROM_API)->pPWRD)

/* Pointer to @ref I2CD_API_T functions in ROM */
#define LPC_I2CD_API	((LPC_ROM_API)->pI2CD)

/* Pointer to @ref UARTD_API_T functions in ROM for UART 0 */
#define LPC_UART0D_API	((LPC_ROM_API)->pUART0D)

/* Pointer to @ref UARTD_API_T functions in ROM for UARTS 1-4 */
#define LPC_UARTND_API	((LPC_ROM_API)->pUARTND)

/* Pointer to @ref DMAD_API_T functions in ROM for DMA */
#define LPC_DMAD_API	((LPC_ROM_API)->pDMAD)

/* Pointer to ROM IAP entry functions */
#define IAP_ENTRY_LOCATION        0X1FFF1FF1

/**
 * @brief LPC11U6X IAP_ENTRY API function type
 */
static INLINE void iap_entry(unsigned int cmd_param[5], unsigned int status_result[4])
{
	((IAP_ENTRY_T) IAP_ENTRY_LOCATION)(cmd_param, status_result);
}

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* __ROMAPI_11U6X_H_ */

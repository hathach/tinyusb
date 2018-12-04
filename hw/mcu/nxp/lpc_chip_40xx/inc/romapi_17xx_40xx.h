/*
 * @brief LPC17xx/40xx ROM API declarations and functions
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

#ifndef __ROMAPI_17XX40XX_H_
#define __ROMAPI_17XX40XX_H_

#include "iap.h"
#include "error.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @defgroup ROMAPI_407X_8X CHIP: LPC17XX/40XX ROM API declarations and functions
 * @ingroup CHIP_17XX_40XX_Drivers
 * @{
 */

#if !defined(CHIP_LPC175X_6X)
/**
 * @brief LPC17XX/40XX High level ROM API structure
 */
typedef struct {
	const uint32_t usbdApiBase;			/*!< USBD API function table base address */
	const uint32_t reserved0;			/*!< Reserved */
	const uint32_t reserved1;			/*!< Reserved */
	const uint32_t reserved2;			/*!< Reserved */
	const uint32_t reserved3;			/*!< Reserved */
	const uint32_t reserved4;			/*!< Reserved */
	const uint32_t reserved5;			/*!< Reserved */
	const uint32_t reserved6;			/*!< Reserved */
	const uint32_t reserved7;			/*!< Reserved */
	const uint32_t reserved8;			/*!< Reserved */
	const uint32_t reserved9;			/*!< Reserved */
	const uint32_t reserved10;		/*!< Reserved */
} LPC_ROM_API_T;

/* Pointer to ROM API function address */
#define LPC_ROM_API_BASE_LOC	0x1FFF1FF8
#define LPC_ROM_API		(*(LPC_ROM_API_T * *) LPC_ROM_API_BASE_LOC)

#endif /* !defined(CHIP_LPC175X_6X) */

/* Pointer to ROM IAP entry functions */
#define IAP_ENTRY_LOCATION		0X1FFF1FF1

/**
 * @brief LPC17XX/40XX IAP_ENTRY API function type
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

#endif /* __ROMAPI_17XX40XX_H_ */

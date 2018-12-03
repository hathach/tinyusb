/*
 * @brief LPC13xx USB device register block
 *
 * @note
 * Copyright(C) NXP Semiconductors, 2012
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

#ifndef __USBD_13XX_H_
#define __USBD_13XX_H_

#ifdef __cplusplus
extern "C" {
#endif

/** @defgroup USBD_13XX CHIP: LPC13xx USB Device driver
 * @ingroup CHIP_13XX_Drivers
 * @{
 */

#if defined(CHIP_LPC1347)

/**
 * @brief USB device register block structure
 */

typedef struct {				/*!< (@ 0x40080000) USB Structure */
	__IO uint32_t DEVCMDSTAT;	/*!< (@ 0x40080000) USB Device Command/Status register */
	__IO uint32_t INFO;			/*!< (@ 0x40080004) USB Info register */
	__IO uint32_t EPLISTSTART;	/*!< (@ 0x40080008) USB EP Command/Status List start address */
	__IO uint32_t DATABUFSTART;	/*!< (@ 0x4008000C) USB Data buffer start address */
	__IO uint32_t LPM;			/*!< (@ 0x40080010) Link Power Management register */
	__IO uint32_t EPSKIP;		/*!< (@ 0x40080014) USB Endpoint skip */
	__IO uint32_t EPINUSE;		/*!< (@ 0x40080018) USB Endpoint Buffer in use */
	__IO uint32_t EPBUFCFG;		/*!< (@ 0x4008001C) USB Endpoint Buffer Configuration register */
	__IO uint32_t INTSTAT;		/*!< (@ 0x40080020) USB interrupt status register */
	__IO uint32_t INTEN;		/*!< (@ 0x40080024) USB interrupt enable register */
	__IO uint32_t INTSETSTAT;	/*!< (@ 0x40080028) USB set interrupt status register */
	__IO uint32_t INTROUTING;	/*!< (@ 0x4008002C) USB interrupt routing register */
	__I  uint32_t RESERVED0[1];
	__I  uint32_t EPTOGGLE;		/*!< (@ 0x40080034) USB Endpoint toggle register */
} LPC_USB_T;

#endif /* defined(CHIP_LPC1347) */

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* __USBD_13XX_H_ */

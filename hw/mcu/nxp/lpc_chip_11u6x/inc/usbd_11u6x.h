/*
 * @brief LPC11U6x USB device register block
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

#ifndef __USBD_11U6X_H_
#define __USBD_11U6X_H_

#ifdef __cplusplus
extern "C" {
#endif

/** @defgroup USBD_11U6X CHIP: LPC11u6x USB Device driver
 * @ingroup CHIP_11U6X_Drivers
 * @{
 */

#define USB_SETUP_RCVD      (0x1 << 8) /* SETUP token received */
#define BUF_ACTIVE          (0x1U << 31)

/**
 * @brief USB device register block structure
 */
typedef struct {				/*!< USB Structure */
	__IO uint32_t DEVCMDSTAT;	/*!< USB Device Command/Status register */
	__IO uint32_t INFO;			/*!< USB Info register */
	__IO uint32_t EPLISTSTART;	/*!< USB EP Command/Status List start address */
	__IO uint32_t DATABUFSTART;	/*!< USB Data buffer start address */
	__IO uint32_t LPM;			/*!< Link Power Management register */
	__IO uint32_t EPSKIP;		/*!< USB Endpoint skip */
	__IO uint32_t EPINUSE;		/*!< USB Endpoint Buffer in use */
	__IO uint32_t EPBUFCFG;		/*!< USB Endpoint Buffer Configuration register */
	__IO uint32_t INTSTAT;		/*!< USB interrupt status register */
	__IO uint32_t INTEN;		/*!< USB interrupt enable register */
	__IO uint32_t INTSETSTAT;	/*!< USB set interrupt status register */
	__IO uint32_t INTROUTING;	/*!< USB interrupt routing register */
	__I  uint32_t RESERVED0[1];
	__I  uint32_t EPTOGGLE;		/*!< USB Endpoint toggle register */
} LPC_USB_T;

/**
 * @}
 */

/**
 * @brief	Read Device Command Status
 * @param	pUSB	: USB peripheral selected
 * @return	Device command status register value
 */
STATIC INLINE uint32_t Chip_USB_GetDevCmdStatus(LPC_USB_T *pUSB)
{
	return pUSB->DEVCMDSTAT;
}

/**
 * @brief	Read Start of Endpoint List
 * @param	pUSB	: USB peripheral selected
 * @return	Endpoint list start address register value
 */
STATIC INLINE uint32_t Chip_USB_GetEPListStart(LPC_USB_T *pUSB)
{
	return pUSB->EPLISTSTART;
}

#ifdef __cplusplus
}
#endif

#endif /* __USBD_11U6X_H_ */

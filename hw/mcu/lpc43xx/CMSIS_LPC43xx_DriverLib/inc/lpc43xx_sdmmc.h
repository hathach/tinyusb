/**********************************************************************
* $Id$		lpc43xx_sdmmc.h		2012-Aug-15
*//**
* @file		lpc43xx_sdmmc.h
* @brief	SD/MMC card access and data driver
* @version	1.0
* @date		15. Aug. 2012
* @author	NXP MCU SW Application Team
*
* Copyright(C) 2011, NXP Semiconductor
* All rights reserved.
*
***********************************************************************
* Software that is described herein is for illustrative purposes only
* which provides customers with programming information regarding the
* products. This software is supplied "AS IS" without any warranties.
* NXP Semiconductors assumes no responsibility or liability for the
* use of the software, conveys no license or title under any patent,
* copyright, or mask work right to the product. NXP Semiconductors
* reserves the right to make changes in the software without
* notification. NXP Semiconductors also make no representation or
* warranty that such application will be suitable for the specified
* use without further testing or modification.
* Permission to use, copy, modify, and distribute this software and its
* documentation is hereby granted, under NXP Semiconductors'
* relevant copyright in the software, without fee, provided that it
* is used in conjunction with NXP Semiconductors microcontrollers.  This
* copyright, permission, and disclaimer notice must appear in all copies of
* this code.
**********************************************************************/

/* Peripheral group ----------------------------------------------------------- */
/** @defgroup SDMMC	SDMMC (SDMMC Card Interface)
 * @ingroup LPC4300CMSIS_FwLib_Drivers
 * @{
 */
#ifndef LPC43XX_SDMMC_H
#define LPC43XX_SDMMC_H

#ifdef __cplusplus
extern "C"
{
#endif

/* Private Macros ------------------------------------------------------------- */
/** @defgroup SDMMC_Private_Macros SDMMC Private Macros
 * @{
 */

/** \brief card type defines
 */
#define CARD_TYPE_SD    (1 << 0)
#define CARD_TYPE_4BIT  (1 << 1)
#define CARD_TYPE_8BIT  (1 << 2)
#define CARD_TYPE_HC    (OCR_HC_CCS) /*!< high capacity card > 2GB */

#define MMC_SECTOR_SIZE 		512

/** \brief Setup options for the SDIO driver
 */
#define US_TIMEOUT 				1000000 	/*!< give 1 atleast 1 sec for the card to respond */
#define MS_ACQUIRE_DELAY		(10) 		/*!< inter-command acquire oper condition delay in msec*/
#define INIT_OP_RETRIES   		50  		/*!< initial OP_COND retries */
#define SET_OP_RETRIES    		1000 		/*!< set OP_COND retries */
#define SDIO_BUS_WIDTH			4			/*!< Max bus width supported */
#define SD_MMC_ENUM_CLOCK       400000		/*!< Typical enumeration clock rate */
#define MMC_MAX_CLOCK           20000000	/*!< Max MMC clock rate */
#define MMC_LOW_BUS_MAX_CLOCK   26000000	/*!< Type 0 MMC card max clock rate */
#define MMC_HIGH_BUS_MAX_CLOCK  52000000	/*!< Type 1 MMC card max clock rate */
#define SD_MAX_CLOCK            25000000	/*!< Max SD clock rate */

/* Function prototype for event setup function */
typedef void (*MCI_EVSETUP_FUNC_T)(uint32_t);

/* Function prototype for wait (for IRQ) function */
typedef uint32_t (*MCI_WAIT_CB_FUNC_T)(uint32_t);

/* Function prototype for milliSecond delay function */
typedef void (*MCI_MSDELAY_FUNC_T)(uint32_t);

/**
 * @}
 */

/* Public Functions ----------------------------------------------------------- */
/** @defgroup SDMMC_Public_Functions SDMMC Public Functions
 * @{
 */

/* Attempt to enumerate an SDMMC card */
int32_t sdmmc_acquire(MCI_EVSETUP_FUNC_T evsetup_cb,
	MCI_WAIT_CB_FUNC_T waitfunc_cb, MCI_MSDELAY_FUNC_T msdelay_func,
	struct _mci_card_struct *pcardinfo);

/* Get card's current state (idle, transfer, program, etc.) */
int32_t sdmmc_get_state(void);

/* Get card's size */
int32_t sdmmc_get_device_size(void);

/* SDMMC read function - reads data from a card */
int32_t sdmmc_read_blocks(void *buffer, int32_t start_block,
    int32_t end_block);

/* SDMMC write function - writes data to a card. After calling this
   function, do not use read or write until the card state has
   left the program state. */
int32_t sdmmc_write_blocks(void *buffer, int32_t start_block,
    int32_t end_block);

/**
 * @}
 */
 
#ifdef __cplusplus
}
#endif

#endif /* end LPC43XX_SDMMC_H */
/**
 * @}
 */

/* --------------------------------- End Of File ------------------------------ */

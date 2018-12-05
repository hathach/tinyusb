/*
 * @brief LPC17xx/40xx SDMMC Card Interface driver
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

#ifndef __SDMMC_17XX_40XX_H_
#define __SDMMC_17XX_40XX_H_

#include "sdmmc.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @defgroup SDMMC_17XX_40XX CHIP: LPC17xx/40xx SDMMC card driver
 * @ingroup CHIP_17XX_40XX_Drivers
 * @{
 */

#if defined(CHIP_LPC177X_8X) || defined(CHIP_LPC40XX)

/*
 * SD/MMC Response type definitions
 */
#define CMDRESP_NONE_TYPE       (SDC_COMMAND_NO_RSP)
#define CMDRESP_R1_TYPE         (SDC_COMMAND_SHORT_RSP)
#define CMDRESP_R1b_TYPE        (SDC_COMMAND_SHORT_RSP)
#define CMDRESP_R2_TYPE         (SDC_COMMAND_LONG_RSP)
#define CMDRESP_R3_TYPE         (SDC_COMMAND_SHORT_RSP)
#define CMDRESP_R6_TYPE         (SDC_COMMAND_SHORT_RSP)
#define CMDRESP_R7_TYPE         (SDC_COMMAND_SHORT_RSP)

#ifdef SDC_DMA_ENABLE

/**
 * @brief SDC Event structure
 */
typedef struct {
	uint8_t DmaChannel;		/*!< DMA Channel used for transfer data */
} SDMMC_EVENT_T;

#else

/**
 * @brief SDC Event structure
 */
typedef struct {
	void *Buffer;		/*!< Pointer to data buffer */
	uint32_t Size;		/*!< Transfer size */
	uint32_t Index;		/*!< Current transfer index */
	uint8_t  Dir;		/*!< Transfer direction 0: transmit, 1: receive */
} SDMMC_EVENT_T;
#endif /* SDC_DMA_ENABLE */

/*
 * SD command values (Command Index, Response)
 */
#define SD_GO_IDLE_STATE           (SDC_COMMAND_INDEX(MMC_GO_IDLE_STATE) | CMDRESP_NONE_TYPE | SDC_COMMAND_INTERRUPT)	/*!< GO_IDLE_STATE(MMC) or RESET(SD) */
#define SD_CMD1_SEND_OP_COND       (SDC_COMMAND_INDEX(MMC_SEND_OP_COND) | CMDRESP_R3_TYPE | 0)					/*!< SEND_OP_COND(MMC) or ACMD41(SD) */
#define SD_CMD2_ALL_SEND_CID       (SDC_COMMAND_INDEX(MMC_ALL_SEND_CID) | CMDRESP_R2_TYPE | 0)					/*!< ALL_SEND_CID */
#define SD_CMD3_SET_RELATIVE_ADDR  (SDC_COMMAND_INDEX(MMC_SET_RELATIVE_ADDR) | CMDRESP_R1_TYPE | 0)				/*!< SET_RELATE_ADDR */
#define SD_CMD3_SEND_RELATIVE_ADDR (SDC_COMMAND_INDEX(SD_SEND_RELATIVE_ADDR) | CMDRESP_R6_TYPE | 0)				/*!< SEND_RELATE_ADDR */
#define SD_CMD7_SELECT_CARD        (SDC_COMMAND_INDEX(MMC_SELECT_CARD) | CMDRESP_R1b_TYPE | 0)					/*!< SELECT/DESELECT_CARD */
#define SD_CMD8_SEND_IF_COND       (SDC_COMMAND_INDEX(SD_CMD8) | CMDRESP_R7_TYPE | 0)							/*!< SEND_IF_COND */
#define SD_CMD9_SEND_CSD           (SDC_COMMAND_INDEX(MMC_SEND_CSD) | CMDRESP_R2_TYPE | 0)						/*!< SEND_CSD */
#define SD_CMD12_STOP_TRANSMISSION (SDC_COMMAND_INDEX(MMC_STOP_TRANSMISSION) | CMDRESP_R1_TYPE | 0)				/*!< STOP_TRANSMISSION */
#define SD_CMD13_SEND_STATUS       (SDC_COMMAND_INDEX(MMC_SEND_STATUS) | CMDRESP_R1_TYPE | 0)					/*!< SEND_STATUS */

/* Block-Oriented Read Commands (class 2) */
#define SD_CMD16_SET_BLOCKLEN      (SDC_COMMAND_INDEX(MMC_SET_BLOCKLEN) | CMDRESP_R1_TYPE | 0)					/*!< SET_BLOCK_LEN */
#define SD_CMD17_READ_SINGLE_BLOCK (SDC_COMMAND_INDEX(MMC_READ_SINGLE_BLOCK) | CMDRESP_R1_TYPE | 0)				/*!< READ_SINGLE_BLOCK */
#define SD_CMD18_READ_MULTIPLE_BLOCK (SDC_COMMAND_INDEX(MMC_READ_MULTIPLE_BLOCK) | CMDRESP_R1_TYPE | 0)			/*!< READ_MULTIPLE_BLOCK */

/* Block-Oriented Write Commands (class 4) */
#define SD_CMD24_WRITE_BLOCK       (SDC_COMMAND_INDEX(MMC_WRITE_BLOCK) | CMDRESP_R1_TYPE | 0)					/*!< WRITE_BLOCK */
#define SD_CMD25_WRITE_MULTIPLE_BLOCK (SDC_COMMAND_INDEX(MMC_WRITE_MULTIPLE_BLOCK) | CMDRESP_R1_TYPE | 0)		/*!< WRITE_MULTIPLE_BLOCK */

/* Erase Commands (class 5) */
#define SD_CMD32_ERASE_WR_BLK_START (SDC_COMMAND_INDEX(SD_ERASE_WR_BLK_START) | CMDRESP_R1_TYPE | 0)			/*!< ERASE_WR_BLK_START */
#define SD_CMD33_ERASE_WR_BLK_END   (SDC_COMMAND_INDEX(SD_ERASE_WR_BLK_END) | CMDRESP_R1_TYPE | 0)				/*!< ERASE_WR_BLK_END */
#define SD_CMD38_ERASE              (SDC_COMMAND_INDEX(SD_ERASE) | CMDRESP_R1b_TYPE | 0)						/*!< ERASE */

/* Application-Specific Commands (class 8) */
#define SD_CMD55_APP_CMD           (SDC_COMMAND_INDEX(MMC_APP_CMD) | CMDRESP_R1_TYPE | 0)						/*!< APP_CMD */
#define SD_ACMD6_SET_BUS_WIDTH     (SDC_COMMAND_INDEX(SD_APP_SET_BUS_WIDTH) | CMDRESP_R1_TYPE | 0)				/*!< SET_BUS_WIDTH */
#define SD_ACMD13_SEND_SD_STATUS   (SDC_COMMAND_INDEX(MMC_SEND_STATUS) | CMDRESP_R1_TYPE | 0)					/*!< SEND_SD_STATUS */
#define SD_ACMD41_SD_SEND_OP_COND  (SDC_COMMAND_INDEX(SD_APP_OP_COND) | CMDRESP_R3_TYPE | 0)					/*!< SD_SEND_OP_COND */

/**
 * @brief	SD card interrupt service routine
 * @param	pSDC	: Pointer to SDC peripheral base address
 * @param	txBuf	: Pointer to TX Buffer (If it is NULL, dont send data to card)
 * @param	txCnt	: Pointer to buffer storing the current transmit index
 * @param	rxBuf	: Pointer to RX Buffer (If it is NULL, dont read data from card)
 * @param	rxCnt	: Pointer to buffer storing the current receive index
 * @return	Positive value: Data transfer
 *          Negative value: Error in data transfer
 *          Zero: Data transfer completed
 */
int32_t Chip_SDMMC_IRQHandler (LPC_SDC_T *pSDC, uint8_t *txBuf, uint32_t *txCnt,
							   uint8_t *rxBuf, uint32_t *rxCnt);

/**
 * @brief	Function to enumerate the SD/MMC/SDHC/MMC+ cards
 * @param	pSDC	    : Pointer to SDC peripheral base address
 * @param	pCardInfo	: Pointer to pre-allocated card info structure
 * @return	1 if a card is acquired, otherwise 0
 */
int32_t Chip_SDMMC_Acquire(LPC_SDC_T *pSDC, SDMMC_CARD_T *pCardInfo);

/**
 * @brief	Get card's current state (idle, transfer, program, etc.)
 * @param	pSDC	    : Pointer to SDC peripheral base address
 * @param	pCardInfo	: Pointer to pre-allocated card info structure
 * @return	Current SD card  state
 */
SDMMC_STATE_T Chip_SDMMC_GetCardState(LPC_SDC_T *pSDC, SDMMC_CARD_T *pCardInfo);

/**
 * @brief	Get 'card status' of SD Memory card
 * @param	pSDC	    : Pointer to SDC peripheral base address
 * @param	pCardInfo	: Pointer to pre-allocated card info structure
 * @return	Current SD card status
 */
uint32_t Chip_SDMMC_GetCardStatus(LPC_SDC_T *pSDC, SDMMC_CARD_T *pCardInfo);

/**
 * @brief	Get 'sd status' of SD Memory card
 * @param	pSDC	    : Pointer to SDC peripheral base address
 * @param	pCardInfo	: Pointer to pre-allocated card info structure
 * @param	pStatus		: Pointer to buffer storing status (it must be 64-byte-length)
 * @return	Number of bytes read
 */
int32_t Chip_SDMMC_GetSDStatus(LPC_SDC_T *pSDC, SDMMC_CARD_T *pCardInfo, uint32_t *pStatus);

/**
 * @brief	Performs the read of data from the SD/MMC card
 * @param	pSDC		: The base of SDC peripheral on the chip
 * @param	pCardInfo	: Pointer to Card information structure
 * @param	buffer		: Pointer to data buffer to copy to
 * @param	startblock	: Start block number
 * @param	blockNum	: Number of block to read
 * @return	Bytes read, or 0 on error
 */
int32_t Chip_SDMMC_ReadBlocks(LPC_SDC_T *pSDC,
							  SDMMC_CARD_T *pCardInfo,
							  void *buffer,
							  int32_t startblock,
							  int32_t blockNum);

/**
 * @brief	Performs write of data to the SD/MMC card
 * @param	pSDC		: The base of SDC peripheral on the chip
 * @param	pCardInfo	: Pointer to Card information structure
 * @param	buffer		: Pointer to data buffer to copy to
 * @param	startblock	: Start block number
 * @param	blockNum	: Number of block to write
 * @return	Number of bytes actually written, or 0 on error
 */
int32_t Chip_SDMMC_WriteBlocks(LPC_SDC_T *pSDC,
							   SDMMC_CARD_T *pCardInfo,
							   void *buffer,
							   int32_t startblock,
							   int32_t blockNum);

#endif /* defined(CHIP_LPC177X_8X) || defined(CHIP_LPC40XX) */

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* __SDC_17XX_40XX_H_ */

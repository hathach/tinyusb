/*
 * @brief LPC17xx/40xx SDMMC card driver
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

#include "chip.h"

#if defined(CHIP_LPC177X_8X) || defined(CHIP_LPC40XX)

/*****************************************************************************
 * Private types/enumerations/variables
 ****************************************************************************/
#define CMD_TIMEOUT                  (0x10000)
#define DATA_TIMEOUT                 (0x1000000)
#define DATA_TIMER_VALUE_R           (SDC_TRAN_CLOCK_RATE / 4)		// 250ms
#define DATA_TIMER_VALUE_W           (SDC_TRAN_CLOCK_RATE)		// 1000ms
#define MS_ACQUIRE_DELAY             (100)			/*!< inter-command acquire oper condition delay in msec*/

/* SD Card HW has a data length regsiter of 16-bits.
 * This supports max of 0xFFFF bytes of data transfer at a time.
 * We will use transfers of 127 blocks of 512 bytes at a time
 */
 #define SDMMC_MAX_BLK               (127)
 #define SDMMC_MAX_TFR_SIZE          (SDMMC_MAX_BLK * MMC_SECTOR_SIZE)

/*****************************************************************************
 * Public types/enumerations/variables
 ****************************************************************************/

/*****************************************************************************
 * Private functions
 ****************************************************************************/
/* Send a command to card */
STATIC int32_t sendCmd(LPC_SDC_T *pSDC, uint32_t Command, uint32_t Arg, uint32_t timeout)
{
	int32_t ret = SDC_RET_TIMEOUT;
	uint32_t Status;

	/* Set Command Info */
	Chip_SDC_SetCommand(pSDC, Command, Arg);

	while (timeout) {

		Status = Chip_SDC_GetStatus(pSDC);

		/* check if command was sent */
		if (((Command & SDC_COMMAND_RSP_BITMASK) == SDC_COMMAND_NO_RSP) && (Status & SDC_STATUS_CMDSENT)) {
			ret =  SDC_RET_OK;
			break;
		}
		/* check if response was received */
		if (Status & SDC_STATUS_CMDRESPEND) {
			ret = SDC_RET_OK;
			break;
		}

		/* check command sending status */
		if (Status & SDC_STATUS_CMDERR) {
			if (Status & SDC_STATUS_CMDCRCFAIL) {
				if ((SDC_COMMAND_INDEX(Command) == MMC_SEND_OP_COND) ||
					(SDC_COMMAND_INDEX(Command) == SD_APP_OP_COND) ||
					(SDC_COMMAND_INDEX(Command) == MMC_STOP_TRANSMISSION)) {
					ret = SDC_RET_OK;	/* ignore CRC error if it's a resp for SEND_OP_COND  or STOP_TRANSMISSION. */
					break;
				}
			}
			ret = SDC_RET_CMD_FAILED;
			break;
		}

		timeout--;
	}

	Chip_SDC_ResetCommand(pSDC);

	return ret;
}

/* Function to send a command to card and get its response (if any)*/
STATIC int32_t executeCmd(LPC_SDC_T *pSDC, uint32_t Command, uint32_t Arg, SDC_RESP_T *pResp)
{
	int32_t Ret = SDC_RET_FAILED;

	/* Send Command to card */
	Ret = sendCmd(pSDC, Command, Arg, CMD_TIMEOUT);
	if (Ret != SDC_RET_OK) {
		return Ret;
	}

	/* Get response (if any) */
	if ((Command & SDC_COMMAND_RSP_BITMASK) != SDC_COMMAND_NO_RSP) {

		Chip_SDC_GetResp(pSDC, pResp);

		/* If the response is not R1, in the response field, the Expected Cmd data
		        won't be the same as the CMD data in SendCmd(). Below four cmds have
		        R2 or R3 response. We don't need to check if MCI_RESP_CMD is the same
		        as the Expected or not. */
		if ((SDC_COMMAND_INDEX(Command) != MMC_SEND_OP_COND) &&
			(SDC_COMMAND_INDEX(Command) != SD_APP_OP_COND) &&
			(SDC_COMMAND_INDEX(Command) != MMC_ALL_SEND_CID) &&
			(SDC_COMMAND_INDEX(Command) != MMC_SEND_CSD) &&
			(pResp->CmdIndex != SDC_COMMAND_INDEX(Command))) {
			return SDC_RET_CMD_FAILED;
		}
	}

	return SDC_RET_OK;
}

/* Check R1 response, the result is stored in pCheckResult parameter. */
/* This function return 1 to exit the command execution, 0 to retry sending command */
STATIC int32_t checkR1Response(uint32_t resp, int32_t *pCheckResult)
{
	int32_t Ret = 1;

	if (!(resp & R1_READY_FOR_DATA)) {
		*pCheckResult = SDC_RET_NOT_READY;
		Ret = 0;
	}
	else if (R1_STATUS(resp)) {
		*pCheckResult =  SDC_RET_FAILED;
	}
	else {
		*pCheckResult =  SDC_RET_OK;
	}
	return Ret;
}

/* Send APP_CMD to card*/
STATIC int32_t sendAppCmd(LPC_SDC_T *pSDC, uint16_t rca)

{
	int32_t Ret = SDC_RET_FAILED;
	SDC_RESP_T Response;
	uint32_t RetryCnt =  20;

	while (RetryCnt > 0) {
		Ret = executeCmd(pSDC, SD_CMD55_APP_CMD, CMD55_RCA(rca), &Response);
		if (Ret == SDC_RET_OK) {
			if (checkR1Response(Response.Data[0], &Ret)) {
				if (Ret != SDC_RET_OK) {
					return Ret;
				}
				if (Response.Data[0] & R1_APP_CMD) {
					return SDC_RET_OK;
				}
				else {
					Ret = SDC_RET_FAILED;
				}
			}
		}
		RetryCnt--;
	}
	return SDC_RET_FAILED;
}

/* Send Reset command to card*/
STATIC INLINE int32_t cardReset(LPC_SDC_T *pSDC)
{
	return executeCmd(pSDC, SD_GO_IDLE_STATE, 0, NULL);
}

/* Send Interface condition to card*/
STATIC int32_t sendIfCond(LPC_SDC_T *pSDC)
{
	int32_t Ret = SDC_RET_FAILED;
	SDC_RESP_T Response;
	uint32_t RetryCnt =  20;

	while (RetryCnt > 0) {
		Ret = executeCmd(pSDC, SD_CMD8_SEND_IF_COND, (CMD8_VOLTAGESUPPLIED_27_36 | CMD8_CHECKPATTERN(
														  CMD8_DEF_PATTERN)), &Response);
		if (Ret == SDC_RET_OK) {
			if ((Response.Data[0] & CMDRESP_R7_VOLTAGE_ACCEPTED) &&
				(CMDRESP_R7_CHECK_PATTERN(Response.Data[0]) == CMD8_DEF_PATTERN)) {
				return SDC_RET_OK;
			}
			return SDC_RET_BAD_PARAMETERS;
		}
		RetryCnt--;
	}
	return Ret;
}

/* Send Operation condition to card */
STATIC int32_t sendOpCond(LPC_SDC_T *pSDC, uint32_t *pOCR)
{
	int32_t Ret = SDC_RET_FAILED;
	SDC_RESP_T Response;
	uint32_t RetryCnt =  0x200;

	while (RetryCnt > 0) {
		Ret = executeCmd(pSDC, SD_CMD1_SEND_OP_COND, SDC_OCR_27_36, &Response);
		if (Ret == SDC_RET_OK) {
			*pOCR = Response.Data[0];
			if (*pOCR & SDC_OCR_IDLE) {
				if ((Response.Data[0] & SDC_OCR_27_36) != SDC_OCR_27_36) {
					return SDC_RET_BAD_PARAMETERS;
				}
				return SDC_RET_OK;
			}
		}
		RetryCnt--;
	}
	return SDC_RET_FAILED;
}

/* Send ACMD41 command to card.If *Ocr = 0, it gets OCR. Otherwise, it starts initialization card.  */
/* Open Drain bit must be cleared before calling this function */
STATIC int32_t sendAppOpCond(LPC_SDC_T *pSDC, uint16_t rca, bool hcs, uint32_t *pOcr, bool *pCCS)
{
	int32_t Ret = SDC_RET_FAILED;
	SDC_RESP_T Response;
	uint32_t Argument;
	uint32_t RetryCnt =  0x2000;	/* The host repeatedly issues ACMD41 for at least 1 second or
									       until the busy bit are set to 1 */

	Argument = ACMD41_OCR(*pOcr);
	if (hcs) {
		Argument |= ACMD41_HCS;
	}

	while (RetryCnt > 0) {
		Ret = sendAppCmd(pSDC, rca);
		if (Ret == SDC_RET_OK) {
			Ret = executeCmd(pSDC, SD_ACMD41_SD_SEND_OP_COND, Argument, &Response);
			if (Ret == SDC_RET_OK) {
				if (Response.Data[0] & CMDRESP_R3_INIT_COMPLETE) {
					if (*pOcr == 0) {
						*pOcr = CMDRESP_R3_OCR_VAL(Response.Data[0]);
						return SDC_RET_OK;
					}
					if ((CMDRESP_R3_OCR_VAL(Response.Data[0]) & *pOcr) != *pOcr) {
						return SDC_RET_BAD_PARAMETERS;
					}
					*pCCS = (Response.Data[0] & CMDRESP_R3_HC_CCS) ? true : false;
					return SDC_RET_OK;
				}
			}
		}
		else {
			return Ret;
		}
		RetryCnt--;
	}
	return SDC_RET_FAILED;
}

/* Get CID */
STATIC int32_t getCID(LPC_SDC_T *pSDC, uint32_t *pCID)
{
	int32_t Ret = SDC_RET_FAILED;
	SDC_RESP_T Response;
	uint32_t RetryCnt =  20;

	while (RetryCnt > 0) {
		Ret = executeCmd(pSDC, SD_CMD2_ALL_SEND_CID, 0, &Response);
		if (Ret == SDC_RET_OK) {
			pCID[3] = Response.Data[0];
			pCID[2] = Response.Data[1];
			pCID[1] = Response.Data[2];
			pCID[0] = Response.Data[3];
			return SDC_RET_OK;
		}
		RetryCnt--;
	}
	return Ret;
}

/* Set Addr */
STATIC int32_t setAddr(LPC_SDC_T *pSDC, uint16_t addr)
{
	int32_t Ret = SDC_RET_FAILED;
	SDC_RESP_T Response;
	uint32_t RetryCnt =  20;

	while (RetryCnt > 0) {
		Ret = executeCmd(pSDC, SD_CMD3_SET_RELATIVE_ADDR, CMD3_RCA(addr), &Response);
		if (Ret == SDC_RET_OK) {
			if (checkR1Response(Response.Data[0], &Ret)) {
				return Ret;
			}
		}
		RetryCnt--;
	}
	return Ret;
}

STATIC int32_t getAddr(LPC_SDC_T *pSDC, uint16_t *pRCA)
{
	int32_t Ret = SDC_RET_FAILED;
	SDC_RESP_T Response;
	uint32_t RetryCnt =  20;

	*pRCA = 0;
	while (RetryCnt > 0) {
		Ret = executeCmd(pSDC, SD_CMD3_SEND_RELATIVE_ADDR, 0, &Response);
		if (Ret == SDC_RET_OK) {
			if (!(CMDRESP_R6_CARD_STATUS(Response.Data[0]) & R1_READY_FOR_DATA)) {
				Ret = SDC_RET_NOT_READY;
			}
			else if (R1_CURRENT_STATE(CMDRESP_R6_CARD_STATUS(Response.Data[0])) != SDMMC_STBY_ST) {
				Ret = SDC_RET_ERR_STATE;
			}
			else {
				*pRCA = CMDRESP_R6_RCA_VAL(Response.Data[0]);
				return SDC_RET_OK;
			}
		}
		RetryCnt--;
	}
	return Ret;
}

STATIC int32_t getCSD(LPC_SDC_T *pSDC, uint16_t rca, uint32_t *pCSD)
{
	int32_t Ret = SDC_RET_FAILED;
	SDC_RESP_T Response;
	uint32_t RetryCnt =  20;

	while (RetryCnt > 0) {
		Ret = executeCmd(pSDC, SD_CMD9_SEND_CSD, CMD9_RCA(rca), &Response);
		if (Ret == SDC_RET_OK) {
			pCSD[3] = Response.Data[0];
			pCSD[2] = Response.Data[1];
			pCSD[1] = Response.Data[2];
			pCSD[0] = Response.Data[3];
			return Ret;
		}
		RetryCnt--;
	}
	return Ret;
}

/* Select card*/
STATIC int32_t selectCard(LPC_SDC_T *pSDC, uint16_t addr)
{
	int32_t Ret = SDC_RET_FAILED;
	SDC_RESP_T Response;
	uint32_t RetryCnt =  20;

	while (RetryCnt > 0) {
		Ret = executeCmd(pSDC, SD_CMD7_SELECT_CARD, CMD7_RCA(addr), &Response);
		if (Ret == SDC_RET_OK) {
			if (checkR1Response(Response.Data[0], &Ret)) {
				return Ret;
			}
		}
		RetryCnt--;
	}
	return Ret;
}

/* Get card status */
STATIC int32_t getStatus(LPC_SDC_T *pSDC, uint16_t rca, uint32_t *pStatus)
{
	int32_t Ret = SDC_RET_FAILED;
	SDC_RESP_T Response;
	uint32_t RetryCnt =  20;

	*pStatus = (uint32_t) -1;
	while (RetryCnt > 0) {
		Ret = executeCmd(pSDC, SD_CMD13_SEND_STATUS, CMD13_RCA(rca), &Response);
		if (Ret == SDC_RET_OK) {
			checkR1Response(Response.Data[0], &Ret);
			*pStatus = Response.Data[0];
			return Ret;
		}
		RetryCnt--;
	}
	return Ret;
}

/* Set bus width.  */
STATIC int32_t getAppStatus(LPC_SDC_T *pSDC, uint16_t rca, uint32_t *pStatus)
{
	int32_t Ret = SDC_RET_FAILED;
	SDC_RESP_T Response;
	uint8_t RetryCnt =  0x20;

	while (RetryCnt > 0) {
		Ret = sendAppCmd(pSDC, rca);
		if (Ret == SDC_RET_OK) {
			Ret = executeCmd(pSDC, SD_ACMD13_SEND_SD_STATUS, 0, &Response);
			if (Ret == SDC_RET_OK) {
				if (checkR1Response(Response.Data[0], &Ret)) {
					return Ret;
				}
			}
		}
		RetryCnt--;
	}
	return SDC_RET_FAILED;
}

/* Helper function to get a bit field withing multi-word  buffer. Used to get
   fields with-in CSD & EXT-CSD */
STATIC uint32_t getBits(LPC_SDC_T *pSDC, int32_t start, int32_t end, uint32_t *data)
{
	uint32_t v;
	uint32_t i = end >> 5;
	uint32_t j = start & 0x1f;

	if (i == (start >> 5)) {
		v = (data[i] >> j);
	}
	else {
		v = ((data[i] << (32 - j)) | (data[start >> 5] >> j));
	}

	return v & ((1 << (end - start + 1)) - 1);
}

/* Function to process the CSD & EXT-CSD of the card */
STATIC void processCSD(LPC_SDC_T *pSDC, SDMMC_CARD_T *pCardInfo)
{
	int32_t CSize = 0;
	int32_t CSizeMult = 0;
	int32_t Mult = 0;

	/* compute block length based on CSD response */
	pCardInfo->block_len = 1 << getBits(pSDC, 80, 83, pCardInfo->csd);

	if ((pCardInfo->card_type & CARD_TYPE_HC) && (pCardInfo->card_type & CARD_TYPE_SD)) {
		/* See section 5.3.3 CSD Register (CSD Version 2.0) of SD2.0 spec  an explanation for the calculation of these values */
		CSize = getBits(pSDC, 48, 63, (uint32_t *) pCardInfo->csd) + 1;
		pCardInfo->blocknr = CSize << 10;	/* 512 byte blocks */
	}
	else {
		/* See section 5.3 of the 4.1 revision of the MMC specs for  an explanation for the calculation of these values */
		CSize = getBits(pSDC, 62, 73, (uint32_t *) pCardInfo->csd);
		CSizeMult = getBits(pSDC, 47, 49, (uint32_t *) pCardInfo->csd);
		Mult = 1 << (CSizeMult + 2);
		pCardInfo->blocknr = (CSize + 1) * Mult;

		/* adjust blocknr to 512/block */
		if (pCardInfo->block_len > MMC_SECTOR_SIZE) {
			pCardInfo->blocknr = pCardInfo->blocknr * (pCardInfo->block_len >> 9);
		}
	}

	pCardInfo->device_size = pCardInfo->blocknr << 9;	/* blocknr * 512 */
}

/* Set bus width.  */
STATIC int32_t setBusWidth(LPC_SDC_T *pSDC, uint16_t rca, uint8_t width)
{
	int32_t Ret = SDC_RET_FAILED;
	SDC_RESP_T Response;
	uint8_t RetryCnt =  0x20;

	while (RetryCnt > 0) {
		Ret = sendAppCmd(pSDC, rca);
		if (Ret == SDC_RET_OK) {
			Ret = executeCmd(pSDC, SD_ACMD6_SET_BUS_WIDTH, ACMD6_BUS_WIDTH(width), &Response);
			if (Ret == SDC_RET_OK) {
				if (checkR1Response(Response.Data[0], &Ret)) {
					return Ret;
				}
			}
		}
		RetryCnt--;
	}
	return SDC_RET_FAILED;
}

/* Puts current selected card in trans state */
STATIC int32_t setTranState(LPC_SDC_T *pSDC, uint16_t rca)
{
	int32_t Ret = 0;
	uint32_t status = 0;
	SDMMC_STATE_T state;

	/* get current state of the card */
	Ret = getStatus(pSDC, rca, &status);
	if (Ret != SDC_RET_OK) {
		/* unable to get the card state. So return immediatly. */
		return Ret;
	}

	/* check card state in response */
	state = (SDMMC_STATE_T) R1_CURRENT_STATE(status);
	switch (state) {
	case SDMMC_STBY_ST:
		/* put card in 'Trans' state */
		Ret = selectCard(pSDC, rca);
		if (Ret != SDC_RET_OK) {
			/* unable to put the card in Trans state. So return immediatly. */
			return Ret;
		}
		getStatus(pSDC, rca, &status);
		if (((SDMMC_STATE_T) R1_CURRENT_STATE(status)) != SDMMC_TRAN_ST) {
			return SDC_RET_ERR_STATE;
		}
		break;

	case SDMMC_TRAN_ST:
		/*do nothing */
		break;

	default:
		/* card shouldn't be in other states so return */
		return SDC_RET_ERR_STATE;
	}

	return SDC_RET_OK;
}

/* Set bus width.  */
STATIC int32_t setblock_length(LPC_SDC_T *pSDC, uint32_t rca, uint32_t block_len)
{
	int32_t Ret = SDC_RET_FAILED;
	SDC_RESP_T Response;
	uint8_t RetryCnt =  0x20;

	while (RetryCnt > 0) {
		Ret = executeCmd(pSDC, SD_CMD16_SET_BLOCKLEN, block_len, &Response);
		if (Ret == SDC_RET_OK) {
			if (checkR1Response(Response.Data[0], &Ret)) {
				return Ret;
			}
		}
		RetryCnt--;
	}
	return SDC_RET_FAILED;
}

/* Sets card data width and block size */
STATIC int32_t setCardParams(LPC_SDC_T *pSDC, SDMMC_CARD_T *pCardInfo)
{
	int32_t Ret;

	Chip_SDC_SetClock(pSDC, SDC_TRAN_CLOCK_RATE);
	if (pCardInfo->card_type & CARD_TYPE_SD) {
		Chip_SDC_ClockControl(pSDC, SDC_CLOCK_WIDEBUS_MODE, ENABLE);
		Ret = setBusWidth(pSDC, pCardInfo->rca, ACMD6_BUS_WIDTH_4);
		if (Ret != SDC_RET_OK) {
			return Ret;
		}
	}
	else {
		Chip_SDC_ClockControl(pSDC, SDC_CLOCK_WIDEBUS_MODE, DISABLE);
	}

	/* set block length */
	Ret = setblock_length(pSDC, pCardInfo->rca, MMC_SECTOR_SIZE);
	return Ret;
}

STATIC int32_t readBlocks(LPC_SDC_T *pSDC, uint32_t card_type, uint32_t startBlock, uint32_t blockNum)
{
	int32_t Ret = SDC_RET_FAILED;
	SDC_RESP_T Response;
	uint32_t Command, Argument;
	uint8_t RetryCnt =  0x20;

	if (blockNum == 1) {
		Command = SD_CMD17_READ_SINGLE_BLOCK;
	}
	else {
		Command = SD_CMD18_READ_MULTIPLE_BLOCK;
	}

	/* Select single or multiple read based on number of blocks */
	/* if high capacity card use block indexing */
	if (card_type & CARD_TYPE_HC) {
		Argument = startBlock;
	}
	else {	/*fix at 512 bytes*/
		Argument = startBlock << 9;
	}

	while (RetryCnt > 0) {
		Ret = executeCmd(pSDC, Command, Argument, &Response);
		if (Ret == SDC_RET_OK) {
			if (checkR1Response(Response.Data[0], &Ret)) {
				return Ret;
			}
		}
		RetryCnt--;
	}
	return Ret;
}

STATIC int32_t writeBlocks(LPC_SDC_T *pSDC, uint32_t card_type, uint32_t startBlock, uint32_t blockNum)
{
	int32_t Ret = SDC_RET_FAILED;
	SDC_RESP_T Response;
	uint32_t Command, Argument;
	uint8_t RetryCnt =  0x20;

	if (blockNum == 1) {
		Command = SD_CMD24_WRITE_BLOCK;
	}
	else {
		Command = SD_CMD25_WRITE_MULTIPLE_BLOCK;
	}

	/* if high capacity card use block indexing */
	if (card_type & CARD_TYPE_HC) {
		Argument = startBlock;
	}
	else {	/*fix at 512 bytes*/
		Argument = startBlock << 9;

	}

	while (RetryCnt > 0) {
		Ret = executeCmd(pSDC, Command, Argument, &Response);
		if (Ret == SDC_RET_OK) {
			if (checkR1Response(Response.Data[0], &Ret)) {
				return Ret;
			}
		}
		RetryCnt--;
	}
	return Ret;
}

STATIC int32_t stopTranmission(LPC_SDC_T *pSDC, uint32_t rca)
{
	uint32_t Status;
	int32_t Ret = SDC_RET_FAILED;
	SDC_RESP_T Response;
	uint32_t RetryCnt =  20;

	Ret = getStatus(pSDC, rca, &Status);
	if (Ret != SDC_RET_OK) {
		return SDC_RET_ERR_STATE;
	}

	if (R1_CURRENT_STATE(Status) == SDMMC_TRAN_ST) {
		return SDC_RET_OK;
	}

	if ((R1_CURRENT_STATE(Status) != SDMMC_DATA_ST) &&
		(R1_CURRENT_STATE(Status) != SDMMC_RCV_ST)) {
		return SDC_RET_ERR_STATE;
	}

	while (RetryCnt > 0) {
		Ret = executeCmd(pSDC, SD_CMD12_STOP_TRANSMISSION, 0, &Response);
		if (Ret == SDC_RET_OK) {
			if (checkR1Response(Response.Data[0], &Ret)) {
				if (Ret != SDC_RET_OK) {
					return Ret;
				}
				Ret = getStatus(pSDC, rca, &Status);
				if ((R1_CURRENT_STATE(Status) == SDMMC_TRAN_ST) || (R1_CURRENT_STATE(Status) == SDMMC_PRG_ST)) {
					return SDC_RET_OK;
				}
				return SDC_RET_ERR_STATE;
			}
		}
		RetryCnt--;
	}
	return Ret;
}

STATIC int32_t Chip_SDMMC_FIFOIRQHandler(LPC_SDC_T *pSDC, uint8_t *txBuf, uint32_t *txCnt,
										 uint8_t *rxBuf, uint32_t *rxCnt)
{
	uint32_t Status;
	Status = Chip_SDC_GetStatus(pSDC);

	if (txBuf ) {
		if (Status & SDC_STATUS_TXFIFOHALFEMPTY) {
			if (*txCnt % 64) {
				Chip_SDC_WriteFIFO(pSDC, (uint32_t *) &txBuf[*txCnt], false);
			}
			else {
				Chip_SDC_WriteFIFO(pSDC, (uint32_t *) &txBuf[*txCnt], true);
			}
			*txCnt += 32;
		}
	}

	if (rxBuf ) {
		if (Status & SDC_STATUS_RXFIFOHALFFULL) {
			if (*rxCnt % 64) {
				Chip_SDC_ReadFIFO(pSDC, (uint32_t *) &rxBuf[*rxCnt], false);
			}
			else {
				Chip_SDC_ReadFIFO(pSDC, (uint32_t *) &rxBuf[*rxCnt], true);
			}
			*rxCnt += 32;
		}
	}

	Chip_SDC_ClearStatus(pSDC, SDC_STATUS_FIFO);

	return 1;
}

/*****************************************************************************
 * Public functions
 ****************************************************************************/

/* SDMMC IRQ handler function */
int32_t Chip_SDMMC_IRQHandler(LPC_SDC_T *pSDC, uint8_t *txBuf, uint32_t *txCnt,
							  uint8_t *rxBuf, uint32_t *rxCnt)
{
	uint32_t Status;

	Status = Chip_SDC_GetStatus(pSDC);

	if ( Status & SDC_STATUS_DATAERR) {
		Chip_SDC_ClearStatus(pSDC, SDC_STATUS_DATAERR);
		return -1;	/* Data transfer error */
	}

	if ( Status & SDC_STATUS_DATAEND) {
		Chip_SDC_ClearStatus(pSDC, SDC_STATUS_DATAEND);
		Chip_SDC_SetIntMask(pSDC, 0);
		return 0;
	}

	if ( Status & SDC_STATUS_DATABLOCKEND) {
		Chip_SDC_ClearStatus(pSDC, SDC_STATUS_DATABLOCKEND);
		return 1;
	}

	if (Status & SDC_STATUS_FIFO) {
		return Chip_SDMMC_FIFOIRQHandler(pSDC, txBuf, txCnt, rxBuf, rxCnt);
	}

	return 1;
}

/* Get card's current state (idle, transfer, program, etc.) */
SDMMC_STATE_T Chip_SDMMC_GetCardState(LPC_SDC_T *pSDC, SDMMC_CARD_T *pCardInfo)
{
	uint32_t Status;

	/* get current state of the card */
	getStatus(pSDC, pCardInfo->rca, &Status);

	/* check card state in response */
	return (SDMMC_STATE_T) R1_CURRENT_STATE(Status);
}

/* Get current card status */
uint32_t Chip_SDMMC_GetCardStatus(LPC_SDC_T *pSDC, SDMMC_CARD_T *pCardInfo)
{
	uint32_t Status;
	getStatus(pSDC, pCardInfo->rca, &Status);
	return Status;
}

/* Get current sd status */
int32_t Chip_SDMMC_GetSDStatus(LPC_SDC_T *pSDC, SDMMC_CARD_T *pCardInfo, uint32_t *pStatus)
{
	int32_t Ret;
	uint16_t ByteNum = 64;
	SDMMC_EVENT_T  Event;
	SDC_DATA_TRANSFER_T Transfer;

	/* Put to tran state */
	if (setTranState(pSDC, pCardInfo->rca) != SDC_RET_OK) {
		return 0;
	}

#ifdef SDC_DMA_ENABLE
	Chip_SDC_SetIntMask(pSDC, SDC_MASK0_DATA | SDC_MASK0_RXDATAERR);
	Event.DmaChannel = Chip_GPDMA_GetFreeChannel(LPC_GPDMA, GPDMA_CONN_SDC);

	/* DMA Setup */
	Chip_GPDMA_Transfer(LPC_GPDMA, Event.DmaChannel,
						GPDMA_CONN_SDC,
						(uint32_t) pStatus,
						GPDMA_TRANSFERTYPE_P2M_CONTROLLER_PERIPHERAL,
						ByteNum);
#else
	Chip_SDC_SetIntMask(pSDC, SDC_MASK0_DATA | SDC_MASK0_RXDATAERR | SDC_MASK0_RXFIFO);

	Event.Buffer = pStatus;
	Event.Size = ByteNum;
	Event.Index = 0;
	Event.Dir = 1;
#endif
	pCardInfo->evsetup_cb((void *) &Event);

	/* set transfer information */
	Transfer.BlockNum = 1;
	Transfer.BlockSize = SDC_BLOCK_SIZE_64;	/* 512 bit */
	Transfer.Dir = SDC_TRANSFER_DIR_FROMCARD;
#ifdef SDC_DMA_ENABLE
	Transfer.DMAUsed = true;
#else
	Transfer.DMAUsed = false;
#endif
	Transfer.Mode = SDC_TRANSFER_MODE_BLOCK;
	Transfer.Timeout = DATA_TIMER_VALUE_R;
	Chip_SDC_SetDataTransfer(pSDC, &Transfer);

	/* Send ACMD13 command */
	Ret = getAppStatus(pSDC, pCardInfo->rca, pStatus);
	if (Ret != SDC_RET_OK) {
		ByteNum = 0;
		goto send_end;
	}

	/* Wait for transfer Finish */
	if ((pCardInfo->waitfunc_cb()) != 0) {
		ByteNum = 0;
	}

send_end:
#ifdef SDC_DMA_ENABLE
	Chip_GPDMA_Stop(LPC_GPDMA, Event.DmaChannel);
#endif
	if (Chip_SDMMC_GetCardState(pSDC, pCardInfo) == SDMMC_DATA_ST) {
		/* Send Stop transmission command */
		stopTranmission(pSDC, pCardInfo->rca);
	}

	/*Wait for card enters to tran state*/
	while ((Chip_SDMMC_GetCardState(pSDC, pCardInfo) != SDMMC_TRAN_ST)) {}

	return ByteNum;
}

/* Function to enumerate the SD/MMC/SDHC/MMC+ cards */
int32_t Chip_SDMMC_Acquire(LPC_SDC_T *pSDC, SDMMC_CARD_T *pCardInfo)
{
	int32_t Ret;

	/* Initialize card info */
	pCardInfo->speed = SDC_TRAN_CLOCK_RATE;
	pCardInfo->card_type = 0;

	/* During identification phase, the clock should be less than
	   400Khz. Once we pass this phase, the normal clock can be set up
	   to 25Mhz on SD card and 20Mhz on MMC card. */
	Chip_SDC_SetClock(pSDC, SDC_IDENT_CLOCK_RATE);

	/* Clear Open Drain output control for SD */
	Chip_SDC_PowerControl(pSDC, SDC_POWER_ON, 0);

	/* Card Reset */
	Ret = cardReset(pSDC);
	if (Ret != 0) {
		return Ret;
	}

	pCardInfo->msdelay_func(MS_ACQUIRE_DELAY);

	/* Send interface operation condiftion */
	Ret = sendIfCond(pSDC);
	if (Ret == SDC_RET_BAD_PARAMETERS) {
		return Ret;		/* Non-compatible voltage range or check pattern is not correct */

	}
	/* Get Card Type */
	if (Ret == SDC_RET_OK) {/* Ver2.00 or later SD Memory Card*/
		bool CCS;
		uint32_t OCR = SDC_OCR_27_36;
		pCardInfo->card_type |= CARD_TYPE_SD;
		Ret = sendAppOpCond(pSDC, 0, true, &OCR, &CCS);
		if (CCS) {	/* High Capacity or Extended Capacity SD Memory Card */
			pCardInfo->card_type |= CARD_TYPE_HC;
		}
	}
	else {	/*Ver2.00 or later SD Memory Card(voltage mismatch) or Ver1.X SD Memory Card
			   or not SD Memory Card*/
		bool CCS;
		uint32_t OCR = SDC_OCR_27_36;
		Ret = sendAppOpCond(pSDC, 0, false, &OCR, &CCS);
		if (Ret == SDC_RET_OK) {
			pCardInfo->card_type |= CARD_TYPE_SD;
		}
		else if (Ret == SDC_RET_BAD_PARAMETERS) {
			return Ret;
		}
		else {	/* MMC Card setup */
			uint32_t OCR;
			/* Enter to Open Drain mode */
			Chip_SDC_PowerControl(pSDC, SDC_POWER_ON, SDC_PWR_OPENDRAIN);
			pCardInfo->msdelay_func(MS_ACQUIRE_DELAY);
			Ret = sendOpCond(pSDC, &OCR);
			if (Ret != SDC_RET_OK) {
				return Ret;
			}

		}
	}

	/* Read CID */
	getCID(pSDC, pCardInfo->cid);

	/* RCA send, for SD get RCA */
	if (pCardInfo->card_type & CARD_TYPE_SD) {
		getAddr(pSDC, &pCardInfo->rca);
	}
	else {
		pCardInfo->rca = 1;
		setAddr(pSDC, pCardInfo->rca);
		Chip_SDC_PowerControl(pSDC, SDC_POWER_ON, 0);	/* enter to push-pull mode */
	}

	/* Get CSD */
	getCSD(pSDC, pCardInfo->rca, pCardInfo->csd);

	/* Compute card size, block size and no. of blocks  based on CSD response recived. */
	if (pCardInfo->cid[0]) {
		processCSD(pSDC, pCardInfo);

		if (setTranState(pSDC, pCardInfo->rca) != SDC_RET_OK) {
			return 0;
		}

		if (Chip_SDMMC_GetCardState(pSDC, pCardInfo) != SDMMC_TRAN_ST) {
			return 0;
		}

		if (setCardParams(pSDC, pCardInfo) != 0) {
			return 0;
		}
	}

	return (pCardInfo->cid[0]) ? 1 : 0;
}

/* Performs the read of data from the SD/MMC card */
int32_t Chip_SDMMC_ReadBlocks(LPC_SDC_T *pSDC,
							  SDMMC_CARD_T *pCardInfo,
							  void *buffer,
							  int32_t startBlock,
							  int32_t blockNum)
{
	int32_t Ret = SDC_RET_FAILED;
	SDC_DATA_TRANSFER_T Transfer;
	SDMMC_EVENT_T  Event;
	int32_t tfr_blk_cnt, start_blk = startBlock, block_cnt = blockNum;
	int32_t ByteNum = blockNum *  MMC_SECTOR_SIZE;
	uint32_t buf_ptr = (uint32_t) buffer;

	/* if card is not acquired return immediately */
	if (( startBlock < 0) || ( (startBlock + blockNum) > pCardInfo->blocknr) ) {
		return 0;
	}

	/* Put to tran state */
	if (setTranState(pSDC, pCardInfo->rca) != SDC_RET_OK) {
		return 0;
	}

	/* Do data transfers in a loop */
	Transfer.BlockSize = SDC_BLOCK_SIZE_512;
	Transfer.Dir = SDC_TRANSFER_DIR_FROMCARD;
#ifdef SDC_DMA_ENABLE
	Transfer.DMAUsed = true;
#else
	Transfer.DMAUsed = false;
#endif
	Transfer.Mode = SDC_TRANSFER_MODE_BLOCK;
	Transfer.Timeout = DATA_TIMER_VALUE_R;

	while (block_cnt > 0) {
		if (block_cnt > SDMMC_MAX_BLK) {
			tfr_blk_cnt = SDMMC_MAX_BLK;
		}
		else {
			tfr_blk_cnt = block_cnt;
		}

#ifdef SDC_DMA_ENABLE
		/* Enable Receive interrupts */
		Chip_SDC_SetIntMask(pSDC, SDC_MASK0_DATA | SDC_MASK0_RXDATAERR);

		/* Get DMA channel */
		Event.DmaChannel = Chip_GPDMA_GetFreeChannel(LPC_GPDMA, GPDMA_CONN_SDC);

		/* Start the DMA transfer */
		Chip_GPDMA_Transfer(LPC_GPDMA, Event.DmaChannel,
							GPDMA_CONN_SDC,
							buf_ptr,
							GPDMA_TRANSFERTYPE_P2M_CONTROLLER_PERIPHERAL,
							(tfr_blk_cnt * MMC_SECTOR_SIZE));
#else
		/* Enable Receive interrupts */
		Chip_SDC_SetIntMask(SDC_MASK0_DATA | SDC_MASK0_RXDATAERR | SDC_MASK0_RXFIFO);

		Event.Buffer = buf_ptr;
		Event.Size = (tfr_blk_cnt * MMC_SECTOR_SIZE);
		Event.Index = 0;
		Event.Dir = 1;
#endif
		/* Call Event Setup function */
		pCardInfo->evsetup_cb((void *) &Event);

		/* Set transfer information */
		Transfer.BlockNum = tfr_blk_cnt;
		Chip_SDC_SetDataTransfer(pSDC, &Transfer);

		/* Send command for reading data */
		Ret = readBlocks(pSDC, pCardInfo->card_type, start_blk, tfr_blk_cnt);
		if (Ret != SDC_RET_OK) {
			ByteNum = 0;
			goto send_end;
		}

		/* Wait for transfer Finish */
		if ((pCardInfo->waitfunc_cb()) != 0) {
			ByteNum = 0;
			goto send_end;
		}

#ifdef SDC_DMA_ENABLE
		Chip_GPDMA_Stop(LPC_GPDMA, Event.DmaChannel);
#endif

		/* Send stop transmission */
		if ((tfr_blk_cnt > 1) || (Chip_SDMMC_GetCardState(pSDC, pCardInfo) == SDMMC_DATA_ST)) {
			/* Send Stop transmission command */
			stopTranmission(pSDC, pCardInfo->rca);
		}

		/* Wait for card enter to tran state */
		while (Chip_SDMMC_GetCardState(pSDC, pCardInfo) != SDMMC_TRAN_ST) {}

		/* Update block counter, buffer address */
		block_cnt -= tfr_blk_cnt;
		buf_ptr += (tfr_blk_cnt * MMC_SECTOR_SIZE);
		start_blk += tfr_blk_cnt;
	}

send_end:
	/* Stop the DMA in case of error */
	if (ByteNum == 0) {
#ifdef SDC_DMA_ENABLE
		Chip_GPDMA_Stop(LPC_GPDMA, Event.DmaChannel);
#endif

		if ((blockNum > 1) || (Chip_SDMMC_GetCardState(pSDC, pCardInfo) == SDMMC_DATA_ST)) {
			/* Send Stop transmission command */
			stopTranmission(pSDC, pCardInfo->rca);
		}

		/* Wait for card enter to tran state */
		while (Chip_SDMMC_GetCardState(pSDC, pCardInfo) != SDMMC_TRAN_ST) {}
	}

	return ByteNum;
}

/* Performs write of data to the SD/MMC card */
int32_t Chip_SDMMC_WriteBlocks(LPC_SDC_T *pSDC,
							   SDMMC_CARD_T *pCardInfo,
							   void *buffer,
							   int32_t startBlock,
							   int32_t blockNum)
{
	int32_t Ret = SDC_RET_FAILED;
	SDC_DATA_TRANSFER_T Transfer;
	SDMMC_EVENT_T  Event;
	int32_t tfr_blk_cnt, start_blk = startBlock, block_cnt = blockNum;
	int32_t ByteNum = blockNum *  MMC_SECTOR_SIZE;
	uint32_t buf_ptr = (uint32_t) buffer;

	/* if card is not acquired return immediately */
	if (( startBlock < 0) || ( (startBlock + blockNum) > pCardInfo->blocknr) ) {
		return 0;
	}

	/* Put to tran state */
	if (setTranState(pSDC, pCardInfo->rca) != SDC_RET_OK) {
		return 0;
	}

	/* Do the trnasfers */
	Transfer.BlockSize = SDC_BLOCK_SIZE_512;
	Transfer.Dir = SDC_TRANSFER_DIR_TOCARD;
#ifdef SDC_DMA_ENABLE
	Transfer.DMAUsed = true;
#else
	Transfer.DMAUsed = false;
#endif
	Transfer.Mode = SDC_TRANSFER_MODE_BLOCK;
	Transfer.Timeout = DATA_TIMER_VALUE_W;
	while (block_cnt > 0) {
		if (block_cnt > SDMMC_MAX_BLK) {
			tfr_blk_cnt = SDMMC_MAX_BLK;
		}
		else {
			tfr_blk_cnt = block_cnt;
		}

		/* Send Write block(s) command */
		Ret = writeBlocks(pSDC, pCardInfo->card_type, start_blk, tfr_blk_cnt);
		if (Ret != SDC_RET_OK) {
			return 0;
		}

		/* Wait for card enter to rcv state */
		while (Chip_SDMMC_GetCardState(pSDC, pCardInfo) != SDMMC_RCV_ST) {}

#ifdef SDC_DMA_ENABLE
		/* Enable Transmit interrupts */
		Chip_SDC_SetIntMask(pSDC, SDC_MASK0_DATA | SDC_MASK0_TXDATAERR);

		/* Get DMA channel */
		Event.DmaChannel = Chip_GPDMA_GetFreeChannel(LPC_GPDMA, GPDMA_CONN_SDC);

		/* Start the DMA transfer */
		Chip_GPDMA_Transfer(LPC_GPDMA, Event.DmaChannel,
							buf_ptr,
							GPDMA_CONN_SDC,
							GPDMA_TRANSFERTYPE_M2P_CONTROLLER_PERIPHERAL,
							(tfr_blk_cnt * MMC_SECTOR_SIZE));
#else
		/* Enable Transmit interrupts */
		Chip_SDC_SetIntMask(SDC_MASK0_DATA | SDC_MASK0_TXDATAERR | SDC_MASK0_TXFIFO);

		Event.Buffer = buf_ptr;
		Event.Size = (tfr_blk_cnt * MMC_SECTOR_SIZE);
		Event.Index = 0;
		Event.Dir = 0;
#endif
		/* Call Event Setup function */
		pCardInfo->evsetup_cb((void *) &Event);

		/* Set transfer information */
		Transfer.BlockNum = tfr_blk_cnt;
		Chip_SDC_SetDataTransfer(pSDC, &Transfer);

		/* Wait for transfer done */
		if ((pCardInfo->waitfunc_cb()) != 0) {
			ByteNum = 0;
			goto end_send;
		}

#ifdef SDC_DMA_ENABLE
		Chip_GPDMA_Stop(LPC_GPDMA, Event.DmaChannel);
#endif

		if ((tfr_blk_cnt > 1) || (Chip_SDMMC_GetCardState(pSDC, pCardInfo) == SDMMC_RCV_ST)) {
			/* Send Stop transmission command */
			stopTranmission(pSDC, pCardInfo->rca);
		}

		/*Wait for card enter to tran state*/
		while (Chip_SDMMC_GetCardState(pSDC, pCardInfo) != SDMMC_TRAN_ST) {}

		/* Update cnt */
		block_cnt -= tfr_blk_cnt;
		buf_ptr += (tfr_blk_cnt * MMC_SECTOR_SIZE);
		start_blk += tfr_blk_cnt;
	}

end_send:
	if (ByteNum == 0) {
#ifdef SDC_DMA_ENABLE
		Chip_GPDMA_Stop(LPC_GPDMA, Event.DmaChannel);
#endif

		if ((blockNum > 1) || (Chip_SDMMC_GetCardState(pSDC, pCardInfo) == SDMMC_RCV_ST)) {
			/* Send Stop transmission command */
			stopTranmission(pSDC, pCardInfo->rca);
		}

		/* Wait for card enter to tran state */
		while (Chip_SDMMC_GetCardState(pSDC, pCardInfo) != SDMMC_TRAN_ST) {}
	}

	return ByteNum;
}

#endif /* defined(CHIP_LPC177X_8X) || defined(CHIP_LPC4XX) */

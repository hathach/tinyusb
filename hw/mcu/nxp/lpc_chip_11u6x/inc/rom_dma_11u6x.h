/*
 * @brief LPC11u6x DMA ROM API declarations and functions
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

#ifndef __ROM_DMA_11U6X_H_
#define __ROM_DMA_11U6X_H_

#ifdef __cplusplus
extern "C" {
#endif

/** @defgroup CHIP_DMAROM_11U6X CHIP: LPC11U6X DMA ROM API declarations and functions
 * @ingroup ROMAPI_11U6X
 * @{
 */

/**
 * @brief LPC11U6X DMA ROM driver handle structure
 */
typedef void *DMA_HANDLE_T;

/**
 * @brief	LPC11U6X DMA ROM driver callback function
 * @param	res0	: error code
 * @param	res1	: 0 = INTA is issued, 1 = INTB is issued
 */
typedef void  (*DMA_CALLBK_T)(uint32_t res0, uint32_t res1);

/**
 * @brief LPC11U6X DMA ROM event values
 */
#define DMA_EVENT_SW_REQ		0
#define DMA_EVENT_PERIPH_REQ	1
#define DMA_EVENT_HW_REQ		2

/**
 * @brief LPC11U6X DMA ROM hardware trigger values
 */
#define DMA_HWTRIG_BURST_SIZE(n)	(n)			/*!< Burst size is 2^n, n <= 15 */
#define DMA_HWTRIG_SRC_BURST_WRAP	(1 << 4)	/*!< Source burst wrapping enabled */
#define DMA_HWTRIG_TRIGGER_BURST	(1 << 6)	/*!< Hardware trigger cause a burst transfer */

/**
 * @brief LPC11U6X DMA ROM channel configuration structure
 */
typedef struct DMA_CHANNEL {
	uint8_t event;					/*!< Event type selection for DMA transfer, a value of DMA_EVENT_* types */
	uint8_t hd_trigger;				/*!< In case hardware trigger is enabled, OR'ed values of type DMA_HWTRIG_* */
	uint8_t Priority;				/*!< Priority level 0 - 7, 0 = lowest, 7 = highest */
	DMA_CALLBK_T callback_func_pt;	/*!< Callback function, only invoked when INTA or INTB is enabled */
} DMA_CHANNEL_T ;

/**
 * @brief LPC11U6X DMA ROM task configuration values
 */
#define DMA_TASK_PINGPONG			(1 << 0)	/*!< Linked with previous task for Ping_Pong transfer */
#define DMA_TASK_SWTRIG				(1 << 1)	/*!< The trigger for this channel is set immediately */
#define DMA_TASK_CLEARTRIG			(1 << 2)	/*!< The trigger is cleared when this task is finished */
#define DMA_TASK_SELINTA			(1 << 3)	/*!< The IntA flag for this channel will be set when this task is finished */
#define DMA_TASK_SELINTB			(1 << 4)	/*!< The IntB flag for this channel will be set when this task is finished */

/**
 * @brief LPC11U6X DMA ROM data type values
 */
#define DMA_DATA_SIZE(n)			(n)			/*!< Data width, 0: 8-bit, 1: 16-bit, 2: 32-bit, 3: reserved */
/* DMA_DATA_SRCINC = 0 : The source address is not incremented for each transfer */
/* DMA_DATA_SRCINC = 1 : The source address is incremented by the amount specified by Width for each transfer */
/* DMA_DATA_SRCINC = 2 : The source address is incremented by 2 times the amount specified by Width for each transfer */
/* DMA_DATA_SRCINC = 3 : The source address is incremented by 4 times the amount specified by Width for each transfer */
#define DMA_DATA_SRCINC(n)			(n << 2)	/*!< Source address incrementation */
/* DMA_DATA_DSTINC = 0 : The destination address is not incremented for each transfer */
/* DMA_DATA_DSTINC = 1 : The destination address is incremented by the amount specified by Width for each transfer */
/* DMA_DATA_DSTINC = 2 : The destination address is incremented by 2 times the amount specified by Width for each transfer */
/* DMA_DATA_DSTINC = 3 : The destination address is incremented by 4 times the amount specified by Width for each transfer */
#define DMA_DATA_DSTINC(n)			(n << 4)	/*!< Destination address incrementation */

/**
 * @brief LPC11U6X DMA ROM task structure
 */
typedef struct DMA_TASK {
	uint8_t ch_num;					/*!< DMA channel number */
	uint8_t config;					/*!< Configuration of this task, OR'ed values of DMA_TASK_* */
	uint8_t data_type;				/*!< Data type, OR'ed values of DMA_DATA_* */
	uint16_t data_length;			/*!< 0: 1 transfer, 1: 2 transfer, … 1023: 1024 transfers */
	uint32_t src;					/*!< Source data end address */
	uint32_t dst;					/*!< Destination end address */
	uint32_t task_addr; 			/*!< the address of RAM for saving this task. Must be 16 butes and aligned on a 16 byte boundary */
} DMA_TASK_T ;

/**
 * LPC11U6X DMA ROM driver APIs structure
 */
typedef struct DMAD_API {
	void (*dma_isr)(DMA_HANDLE_T* handle);
	uint32_t (*dma_get_mem_size)( void);
	DMA_HANDLE_T* (*dma_setup)( uint32_t base_addr, uint8_t *ram );
	ErrorCode_t (*dma_init)( DMA_HANDLE_T* handle, DMA_CHANNEL_T *channel, DMA_TASK_T *task);
	ErrorCode_t (*dma_link)( DMA_HANDLE_T* handle, DMA_TASK_T *task, uint8_t valid);
	ErrorCode_t (*dma_set_valid)( DMA_HANDLE_T* handle, uint8_t chl_num);
	ErrorCode_t (*dma_pause)( DMA_HANDLE_T* handle, uint8_t chl_num);
	ErrorCode_t (*dma_unpause)( DMA_HANDLE_T* handle, uint8_t chl_num);
	ErrorCode_t (*dma_abort)( DMA_HANDLE_T* handle, uint8_t chl_num);
} DMAD_API_T ;

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* __ROM_DMA_11U6X_H_ */

/*
 * @brief LPC17xx/40xx EEPROM driver
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

#ifndef __EEPROM_17XX_40XX_H_
#define __EEPROM_17XX_40XX_H_

#ifdef __cplusplus
extern "C" {
#endif

/** @defgroup EEPROM_17XX_40XX CHIP: LPC17xx/40xx EEPROM driver
 * @ingroup CHIP_17XX_40XX_Drivers
 * @{
 */

#if defined(CHIP_LPC177X_8X) || defined(CHIP_LPC40XX)

/**
 * @brief EEPROM register block structure
 */
typedef struct {				/*  EEPROM Structure */
	__IO uint32_t CMD;			/*!< EEPROM command register */
	__IO uint32_t ADDR;			/*!< EEPROM address register */
	__O  uint32_t WDATA;		/*!< EEPROM write data register */
	__I  uint32_t RDATA;		/*!< EEPROM read data register */
	__IO uint32_t WSTATE;		/*!< EEPROM wait state register */
	__IO uint32_t CLKDIV;		/*!< EEPROM clock divider register */
	__IO uint32_t PWRDWN;		/*!< EEPROM power-down register */
	uint32_t RESERVED0[975];
	__O  uint32_t INTENCLR;		/*!< EEPROM interrupt enable clear */
	__O  uint32_t INTENSET;		/*!< EEPROM interrupt enable set */
	__I  uint32_t INTSTAT;		/*!< EEPROM interrupt status */
	__I  uint32_t INTEN;		/*!< EEPROM interrupt enable */
	__O  uint32_t INTSTATCLR;	/*!< EEPROM interrupt status clear */
	__O  uint32_t INTSTATSET;	/*!< EEPROM interrupt status set */
} LPC_EEPROM_T;

#define EEPROM_PAGE_SIZE                64		/*!< EEPROM byes per page */
#define EEPROM_PAGE_NUM                 63		/*!<  EEPROM pages */

/*
 * @brief Macro defines for EEPROM command register
 */
#define EEPROM_CMD_8BITS_READ           (0)		/*!< EEPROM 8-bit read command */
#define EEPROM_CMD_16BITS_READ          (1)		/*!< EEPROM 16-bit read command */
#define EEPROM_CMD_32BITS_READ          (2)		/*!< EEPROM 32-bit read command */
#define EEPROM_CMD_8BITS_WRITE          (3)		/*!< EEPROM 8-bit write command */
#define EEPROM_CMD_16BITS_WRITE         (4)		/*!< EEPROM 16-bit write command */
#define EEPROM_CMD_32BITS_WRITE         (5)		/*!< EEPROM 32-bit write command */
#define EEPROM_CMD_ERASE_PRG_PAGE       (6)		/*!< EEPROM erase/program command */
#define EEPROM_CMD_RDPREFETCH           (1 << 3)/*!< EEPROM read pre-fetch enable */

/*
 * @brief Macro defines for EEPROM power down register
 */
#define EEPROM_PWRDWN                   (1 << 0)

/*
 * @brief Macro defines for EEPROM interrupt related registers
 */
#define EEPROM_INT_ENDOFRW                 (1 << 26)
#define EEPROM_INT_ENDOFPROG               (1 << 28)

/**
 * @brief EEPROM Mode type definition
 */
typedef enum IP_EEPROM_RWSIZE {
	EEPROM_RWSIZE_8BITS = 1,
	EEPROM_RWSIZE_16BITS = 2,
	EEPROM_RWSIZE_32BITS = 4
} EEPROM_RWSIZE_T;

/**
 * @brief	Put EEPROM device in power down mode
 * @param	pEEPROM		: The base of EEPROM peripheral on the chip
 * @return	Nothing
 */
STATIC INLINE void Chip_EEPROM_EnablePowerDown(LPC_EEPROM_T *pEEPROM)
{
	pEEPROM->PWRDWN = EEPROM_PWRDWN;
}

/**
 * @brief	Bring EEPROM device out of power down mode
 * @param	pEEPROM		: The base of EEPROM peripheral on the chip
 * @return	Nothing
 */
STATIC INLINE void Chip_EEPROM_DisablePowerDown(LPC_EEPROM_T *pEEPROM)
{
	pEEPROM->PWRDWN = 0;
}

/**
 * @brief	Initializes EEPROM
 * @param	pEEPROM	: The base of EEPROM peripheral on the chip
 * @return	Nothing
 */
void Chip_EEPROM_Init(LPC_EEPROM_T *pEEPROM);

/**
 * @brief	De-initializes EEPROM
 * @param	pEEPROM	: The base of EEPROM peripheral on the chip
 * @return	Nothing
 */
STATIC INLINE void Chip_EEPROM_DeInit(LPC_EEPROM_T *pEEPROM)
{
	/* Enable EEPROM power down mode */
	Chip_EEPROM_EnablePowerDown(pEEPROM);
}

/**
 * @brief	Select an EEPROM command
 * @param	pEEPROM	: pointer to EEPROM peripheral block
 * @param	cmd	: EEPROM command.
 * @return	Nothing
 * @note	 cmd is or-ed bits value of EEPROM_CMD_[8|16|32]BITS_READ/EEPROM_CMD_[8|16|32]BITS_WRITE
 * with EEPROM_CMD_RDPREFETCH flag.
 *		Read and erase/program operations are started on the EEPROM device as a side-effect of calling this function.
 * Write operations are started as a side-effect of writing data to data register.
 */
STATIC INLINE void Chip_EEPROM_SetCmd(LPC_EEPROM_T *pEEPROM, uint32_t cmd)
{
	pEEPROM->CMD = cmd;
}

/**
 * @brief	Set EEPROM address
 * @param	pEEPROM	: pointer to EEPROM peripheral block
 * @param	pageAddr	: Page address.
 * @param	pageOffset	: Page address.
 * @return	Nothing
 */
STATIC INLINE void Chip_EEPROM_SetAddr(LPC_EEPROM_T *pEEPROM, uint32_t pageAddr, uint32_t pageOffset)
{
	pEEPROM->ADDR = (pageAddr << 6) | pageOffset;
}

/**
 * @brief	Write EEPROM data
 * @param	pEEPROM	: pointer to EEPROM peripheral block
 * @param	data	: EEPROM data.
 * @return	Nothing
 */
STATIC INLINE void Chip_EEPROM_WriteData(LPC_EEPROM_T *pEEPROM, uint32_t data)
{
	pEEPROM->WDATA = data;
}

/**
 * @brief	Read EEPROM data
 * @param	pEEPROM	: pointer to EEPROM peripheral block
 * @return	data
 */
STATIC INLINE uint32_t Chip_EEPROM_ReadData(LPC_EEPROM_T *pEEPROM)
{
	return pEEPROM->RDATA;
}

/**
 * @brief	Set EEPROM wait state
 * @param	pEEPROM	: pointer to EEPROM peripheral block
 * @param	ws	: Wait State value.
 * @return	Nothing
 */
STATIC INLINE void Chip_EEPROM_SetWaitState(LPC_EEPROM_T *pEEPROM, uint32_t ws)
{
	pEEPROM->WSTATE = ws;
}

/**
 * @brief	Write data to EEPROM at specific address
 * @param	pEEPROM		: The base of EEPROM peripheral on the chip
 * @param	pageOffset	: offset of data in page register(0 - 63)
 * @param	pageAddress: page address (0-62)
 * @param	pData		: buffer that contain data that will be written to buffer
 * @param	wsize			: Write size:<br>
 *                  - EEPROM_RWSIZE_8BITS    : 8-bit read/write mode<br>
 *                  - EEPROM_RWSIZE_16BITS   : 16-bit read/write mode<br>
 *                  - EEPROM_RWSIZE_32BITS   : 32-bit read/write mode<br>
 * @param	byteNum		: number written data (bytes)
 * @return	SUCCESS on successful write of data, or ERROR
 * @note	This function actually write data into EEPROM memory and automatically
 * write into next page if current page is overflowed
 */
Status Chip_EEPROM_Write(LPC_EEPROM_T *pEEPROM, uint16_t pageOffset,
						 uint16_t pageAddress,
						 void *pData,
						 EEPROM_RWSIZE_T wsize,
						 uint32_t byteNum);

/**
 * @brief	Read data to EEPROM at specific address
 * @param	pEEPROM		: The base of EEPROM peripheral on the chip
 * @param	pageOffset	: offset of data in page register(0 - 63)
 * @param	pageAddress: page address (0-62)
 * @param	pData		: buffer that contain data read from read data register
 * @param	rsize		: Read size:<br>
 *                  - EEPROM_RWSIZE_8BITS    : 8-bit read/write mode<br>
 *                  - EEPROM_RWSIZE_16BITS   : 16-bit read/write mode<br>
 *                  - EEPROM_RWSIZE_32BITS   : 32-bit read/write mode<br>
 * @param	byteNum		: number read data (bytes)
 * @return	Nothing
 */
void Chip_EEPROM_Read(LPC_EEPROM_T *pEEPROM, uint16_t pageOffset,
					  uint16_t pageAddress,
					  void *pData,
					  EEPROM_RWSIZE_T rsize,
					  uint32_t byteNum);

/**
 * @brief	Erase a page at the specific address
 * @param	pEEPROM		: The base of EEPROM peripheral on the chip
 * @param	address		: EEPROM page address (0-62)
 * @return	Nothing
 */
void Chip_EEPROM_Erase(LPC_EEPROM_T *pEEPROM, uint16_t address);

/**
 * @brief	Enable EEPROM interrupt
 * @param	pEEPROM		: pointer to EEPROM peripheral block
 * @param	mask		: interrupt mask (or-ed bits value of EEPROM_INT_*)
 * @return	Nothing
 */
STATIC INLINE void Chip_EEPROM_EnableInt(LPC_EEPROM_T *pEEPROM, uint32_t mask)
{
	pEEPROM->INTENSET =  mask;
}

/**
 * @brief	Disable EEPROM interrupt
 * @param	pEEPROM		: pointer to EEPROM peripheral block
 * @param	mask		: interrupt mask (or-ed bits value of EEPROM_INT_*)
 * @return	Nothing
 */
STATIC INLINE void Chip_EEPROM_DisableInt(LPC_EEPROM_T *pEEPROM, uint32_t mask)
{
	pEEPROM->INTENCLR =  mask;
}

/**
 * @brief	Get the value of the EEPROM interrupt enable register
 * @param	pEEPROM		: pointer to EEPROM peripheral block
 * @return	Or-ed bits value of EEPROM_INT_*
 */
STATIC INLINE uint32_t Chip_EEPROM_GetIntEnable(LPC_EEPROM_T *pEEPROM)
{
	return pEEPROM->INTEN;
}

/**
 * @brief	Get EEPROM interrupt status
 * @param	pEEPROM		: pointer to EEPROM peripheral block
 * @return	Or-ed bits value of EEPROM_INT_*
 */
STATIC INLINE uint32_t Chip_EEPROM_GetIntStatus(LPC_EEPROM_T *pEEPROM)
{
	return pEEPROM->INTSTAT;
}

/**
 * @brief	Set EEPROM interrupt status
 * @param	pEEPROM		: pointer to EEPROM peripheral block
 * @param	mask		: interrupt mask (or-ed bits value of EEPROM_INT_*)
 * @return	Nothing
 */
STATIC INLINE void Chip_EEPROM_SetIntStatus(LPC_EEPROM_T *pEEPROM, uint32_t mask)
{
	pEEPROM->INTSTATSET =  mask;
}

/**
 * @brief	Clear EEPROM interrupt status
 * @param	pEEPROM		: pointer to EEPROM peripheral block
 * @param	mask		: interrupt mask (or-ed bits value of EEPROM_INT_*)
 * @return	Nothing
 */
STATIC INLINE void Chip_EEPROM_ClearIntStatus(LPC_EEPROM_T *pEEPROM, uint32_t mask)
{
	pEEPROM->INTSTATCLR =  mask;
}

#endif /* defined(CHIP_LPC177X_8X) || defined(CHIP_LPC40XX) */

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* __EEPROM_17XX_40XX_H_ */

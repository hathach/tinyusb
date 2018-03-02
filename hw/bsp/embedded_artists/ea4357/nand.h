/*****************************************************************************
 *
 *   Copyright(C) 2011, Embedded Artists AB
 *   All rights reserved.
 *
 ******************************************************************************
 * Software that is described herein is for illustrative purposes only
 * which provides customers with programming information regarding the
 * products. This software is supplied "AS IS" without any warranties.
 * Embedded Artists AB assumes no responsibility or liability for the
 * use of the software, conveys no license or title under any patent,
 * copyright, or mask work right to the product. Embedded Artists AB
 * reserves the right to make changes in the software without
 * notification. Embedded Artists AB also make no representation or
 * warranty that such application will be suitable for the specified
 * use without further testing or modification.
 *****************************************************************************/
#ifndef __NAND_H
#define __NAND_H

#define NAND_NUM_BLOCKS (1024)

extern uint32_t nand_init (void);
extern uint32_t nand_getPageSize(void);
extern uint32_t nand_getBlockSize(void);
extern uint32_t nand_getRedundantSize(void);
extern uint32_t nand_isBlockValid(uint32_t blockNum);
uint32_t nand_readPage(uint32_t block, uint32_t page, uint8_t* pageBuf);
uint32_t nand_writePage(uint32_t block, uint32_t page, uint8_t* pageBuf);
uint32_t nand_eraseBlock(uint32_t block);


#endif /* end __NAND_H */
/****************************************************************************
**                            End Of File
*****************************************************************************/

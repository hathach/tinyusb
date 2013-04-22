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
#ifndef __NORFLASH_H
#define __NORFLASH_H

#define NORFLASH_SIZE           0x400000  /*Bytes or 0x200000 16bit words*/
#define NORFLASH_BLOCK_SIZE     0x10000   /*Bytes, or 32K 16bit words*/
#define NORFLASH_BASE           0x1C000000

//16 bit data
#define GET_ADDR(addr)  (volatile uint16_t *)(NORFLASH_BASE | ((addr) << 1))

typedef struct
{
  uint32_t device_size;  /* Device size in bytes */
  uint32_t num_sectors;  /* Number of sectors */
  uint32_t sector_size;  /* Sector size in bytes */
  uint32_t num_blocks;   /* Number of blocks */
  uint32_t block_size;   /* Block size in bytes */
} geometry_t;

extern uint32_t norflash_init(void);

extern void norflash_getGeometry(geometry_t* geometry);

extern uint32_t norflash_eraseBlock(uint32_t addr);
extern uint32_t norflash_eraseSector(uint32_t addr);
extern uint32_t norflash_eraseEntireChip(void);

extern uint32_t norflash_writeWord(uint32_t addr, uint16_t data);
extern uint32_t norflash_writeBuff(uint32_t addr, uint16_t* data, uint16_t len);

extern void norflash_secid_read(uint16_t SST_SecID[8], uint16_t user_SecID[8]);
extern uint32_t norflash_secid_getLockStatus(void);
extern uint32_t norflash_secid_lockOut(void);
extern uint32_t norflash_secid_writeWord(uint16_t target, uint16_t* data, uint16_t len);

#endif /* end __NORFLASH_H */
/****************************************************************************
**                            End Of File
*****************************************************************************/

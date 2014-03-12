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


/******************************************************************************
 * Includes
 *****************************************************************************/
#include "../../board.h"

#if BOARD == BOARD_EA4357

#include "lpc_types.h"
#include "lpc43xx_scu.h"
#include "lpc43xx_timer.h"
#include "nand.h"


/******************************************************************************
 * Defines and typedefs
 *****************************************************************************/

#define K9F1G_CLE   ((volatile uint8_t *)0x1D100000)
#define K9F1G_ALE   ((volatile uint8_t *)0x1D080000)
#define K9F1G_DATA  ((volatile uint8_t *)0x1D000000)

#define K9FXX_WAIT()

#define K9FXX_READ_1            0x00                
#define K9FXX_READ_2            0x30                

#define K9FXX_SET_ADDR_A        0x00                
#define K9FXX_SET_ADDR_B        0x01                
#define K9FXX_SET_ADDR_C        0x50                
#define K9FXX_READ_ID           0x90                
#define K9FXX_RESET             0xff                
#define K9FXX_BLOCK_PROGRAM_1   0x80                
#define K9FXX_BLOCK_PROGRAM_2   0x10                
#define K9FXX_BLOCK_ERASE_1     0x60                
#define K9FXX_BLOCK_ERASE_2     0xd0                
#define K9FXX_READ_STATUS       0x70                
#define K9FXX_BUSY              (1 << 6)            
#define K9FXX_OK                (1 << 0)   

#define ID_MARKER_CODE (0xEC)
#define ID_SAMSUNG     (0xF1)

#define ID_PAGE_SZ_1KB (0x00)
#define ID_PAGE_SZ_2KB (0x01)
#define ID_PAGE_SZ_4KB (0x02)
#define ID_PAGE_SZ_8KB (0x03)

#define ID_BLOCK_SZ_64KB  (0x00)
#define ID_BLOCK_SZ_128KB (0x01)
#define ID_BLOCK_SZ_256KB (0x02)
#define ID_BLOCK_SZ_512KB (0x03)

#define ID_PAGE_SZ_1KB (0x00)
#define ID_PAGE_SZ_2KB (0x01)
#define ID_PAGE_SZ_4KB (0x02)
#define ID_PAGE_SZ_8KB (0x03)

#define ID_REDUND_SZ_8  (0x00)
#define ID_REDUND_SZ_16 (0x01)



/* This macro could be changed to check the ready pin */
#define WAIT_READY() (TIM_Waitus(25))
         

/******************************************************************************
 * External global variables
 *****************************************************************************/

/******************************************************************************
 * Local variables
 *****************************************************************************/

static uint32_t pageSize   = 0;
static uint32_t blockSize  = 0;
static uint32_t reduntSize = 0;


/******************************************************************************
 * Local Functions
 *****************************************************************************/

static void pinConfig(void)
{
	/* Set up EMC pin */
	scu_pinmux(	 2	,	 9	,	MD_PLN_FAST	,	3	);//A0
	scu_pinmux(	 2	,	10	,	MD_PLN_FAST	,	3	);//A1
	scu_pinmux(	 2	,	11	,	MD_PLN_FAST	,	3	);//A2
	scu_pinmux(	 2	,	12	,	MD_PLN_FAST	,	3	);//A3
	scu_pinmux(	 2	,	13	,	MD_PLN_FAST	,	3	);//A4
	scu_pinmux(	 1	,	 0	,	MD_PLN_FAST	,	2	);//A5
	scu_pinmux(	 1	,	 1	,	MD_PLN_FAST	,	2	);//A6
	scu_pinmux(	 1	,	 2	,	MD_PLN_FAST	,	2	);//A7
	scu_pinmux(	 2	,	 8	,	MD_PLN_FAST	,	3	);//A8
	scu_pinmux(	 2	,	 7	,	MD_PLN_FAST	,	3	);//A9
	scu_pinmux(	 2	,	 6	,	MD_PLN_FAST	,	2	);//A10
	scu_pinmux(	 2	,	 2	,	MD_PLN_FAST	,	2	);//A11
	scu_pinmux(	 2	,	 1	,	MD_PLN_FAST	,	2	);//A12
	scu_pinmux(	 2	,	 0	,	MD_PLN_FAST	,	2	);//A13
	scu_pinmux(	 6	,	 8	,	MD_PLN_FAST	,	1	);//A14
	scu_pinmux(	 6	,	 7	,	MD_PLN_FAST	,	1	);//A15
	scu_pinmux(	13	,	16	,	MD_PLN_FAST	,	2	);//A16
	scu_pinmux(	13	,	15	,	MD_PLN_FAST	,	2	);//A17
	scu_pinmux(	14	,	 0	,	MD_PLN_FAST	,	3	);//A18
	scu_pinmux(	14	,	 1	,	MD_PLN_FAST	,	3	);//A19
	scu_pinmux(	14	,	 2	,	MD_PLN_FAST	,	3	);//A20
	scu_pinmux(	14	,	 3	,	MD_PLN_FAST	,	3	);//A21
	scu_pinmux(	14	,	 4	,	MD_PLN_FAST	,	3	);//A22
	scu_pinmux(	10	,	 4	,	MD_PLN_FAST	,	3	);//A23
	
	scu_pinmux(	 1	,	 7	,	MD_PLN_FAST	,	3	);//D0
	scu_pinmux(	 1	,	 8	,	MD_PLN_FAST	,	3	);//D1
	scu_pinmux(	 1	,	 9	,	MD_PLN_FAST	,	3	);//D2
	scu_pinmux(	 1	,	10	,	MD_PLN_FAST	,	3	);//D3
	scu_pinmux(	 1	,	11	,	MD_PLN_FAST	,	3	);//D4
	scu_pinmux(	 1	,	12	,	MD_PLN_FAST	,	3	);//D5
	scu_pinmux(	 1	,	13	,	MD_PLN_FAST	,	3	);//D6
	scu_pinmux(	 1	,	14	,	MD_PLN_FAST	,	3	);//D7
	scu_pinmux(	 5	,	 4	,	MD_PLN_FAST	,	2	);//D8
	scu_pinmux(	 5	,	 5	,	MD_PLN_FAST	,	2	);//D9
	scu_pinmux(	 5	,	 6	,	MD_PLN_FAST	,	2	);//D10
	scu_pinmux(	 5	,	 7	,	MD_PLN_FAST	,	2	);//D11
	scu_pinmux(	 5	,	 0	,	MD_PLN_FAST	,	2	);//D12
	scu_pinmux(	 5	,	 1	,	MD_PLN_FAST	,	2	);//D13
	scu_pinmux(	 5	,	 2	,	MD_PLN_FAST	,	2	);//D14
	scu_pinmux(	 5	,	 3	,	MD_PLN_FAST	,	2	);//D15
	scu_pinmux(	13	,	 2	,	MD_PLN_FAST	,	2	);//D16
	scu_pinmux(	13	,	 3	,	MD_PLN_FAST	,	2	);//D17
	scu_pinmux(	13	,	 4	,	MD_PLN_FAST	,	2	);//D18
	scu_pinmux(	13	,	 5	,	MD_PLN_FAST	,	2	);//D19
	scu_pinmux(	13	,	 6	,	MD_PLN_FAST	,	2	);//D20
	scu_pinmux(	13	,	 7	,	MD_PLN_FAST	,	2	);//D21
	scu_pinmux(	13	,	 8	,	MD_PLN_FAST	,	2	);//D22
	scu_pinmux(	13	,	 9	,	MD_PLN_FAST	,	2	);//D23
	scu_pinmux(	14	,	 5	,	MD_PLN_FAST	,	3	);//D24
	scu_pinmux(	14	,	 6	,	MD_PLN_FAST	,	3	);//D25
	scu_pinmux(	14	,	 7	,	MD_PLN_FAST	,	3	);//D26
	scu_pinmux(	14	,	 8	,	MD_PLN_FAST	,	3	);//D27
	scu_pinmux(	14	,	 9	,	MD_PLN_FAST	,	3	);//D28
	scu_pinmux(	14	,	10	,	MD_PLN_FAST	,	3	);//D29
	scu_pinmux(	14	,	11	,	MD_PLN_FAST	,	3	);//D30
	scu_pinmux(	14	,	12	,	MD_PLN_FAST	,	3	);//D31
		
	scu_pinmux(	 1	,	 3	,	MD_PLN_FAST	,	3	);//OE
	scu_pinmux(	 1	,	 6	,	MD_PLN_FAST	,	3	);//WE
	
	scu_pinmux(	 1	,	 4	,	MD_PLN_FAST	,	3	);//BLS0
	scu_pinmux(	 6	,	 6	,	MD_PLN_FAST	,	1	);//BLS1	
	scu_pinmux(	13	,	13	,	MD_PLN_FAST	,	2	);//BLS2
	scu_pinmux(	13	,	10	,	MD_PLN_FAST	,	2	);//BLS3
	
	scu_pinmux(	 1	,	 5	,	MD_PLN_FAST	,	3	);//CS0	
	scu_pinmux(	 6	,	 3	,	MD_PLN_FAST	,	3	);//CS1
	scu_pinmux(	13	,	12	,	MD_PLN_FAST	,	2	);//CS2
	scu_pinmux(	13	,	11	,	MD_PLN_FAST	,	2	);//CS3
}


static uint32_t nandReadId(void)
{
  uint8_t a, b, c, d;
  volatile uint8_t *pCLE;
  volatile uint8_t *pALE;
  volatile uint8_t *pData;
  
  pCLE  = K9F1G_CLE;
  pALE  = K9F1G_ALE;
  pData = K9F1G_DATA;
    
  *pCLE = K9FXX_READ_ID;
  *pALE = 0;
    
  a = *pData;
  b = *pData;
  c = *pData;
  d = *pData;
  
    
  return (a << 24) | (b << 16) | (c << 8) | d;
}

static uint8_t nandStatus(void)
{
  uint8_t status = 0;
  volatile uint8_t *pCLE;
  volatile uint8_t *pALE;
  volatile uint8_t *pData;
  
  pCLE  = K9F1G_CLE;
  pALE  = K9F1G_ALE;
  pData = K9F1G_DATA;
    
  *pCLE = K9FXX_READ_STATUS;
  *pALE = 0;
    
  status = *pData;
      
  /* remove bits not used */
  return (status & 0xC1);  
}

static void nandWaitReady(void) 
{
  while( !(nandStatus() & (1<<6)) );
}

/******************************************************************************
 * Public Functions
 *****************************************************************************/



/******************************************************************************
 *
 * Description:
 *    Initialize the NAND Flash
 *
 * Returns:
 *    TRUE if initialization successful; otherwise FALSE
 *
 *****************************************************************************/
uint32_t nand_init (void)
{
  uint32_t nandId = 0;
  TIM_TIMERCFG_Type timerCfg;

//  LPC_SC->PCONP     |= 0x00000800;
  LPC_EMC->CONTROL   = 0x00000001;
  LPC_EMC->CONFIG    = 0x00000000;

  pinConfig();

  TIM_ConfigStructInit(TIM_TIMER_MODE, &timerCfg);
  TIM_Init(LPC_TIMER0, TIM_TIMER_MODE, &timerCfg);

  LPC_EMC->STATICCONFIG1   = 0x00000080;

  LPC_EMC->STATICWAITWEN1  = 0x00000002; 
  LPC_EMC->STATICWAITOEN1  = 0x00000002; 
  LPC_EMC->STATICWAITRD1   = 0x00000008; 
  LPC_EMC->STATICWAITPAG1  = 0x0000001f; 
  LPC_EMC->STATICWAITWR1   = 0x00000008; 
  LPC_EMC->STATICWAITTURN1 = 0x0000000f;

  nandId = nandReadId();

  if ((nandId & 0xffff0000) != 
    (((uint32_t)(ID_MARKER_CODE) << 24) | ID_SAMSUNG << 16)) {
    /* unknown NAND chip */
    return FALSE;
  }

  pageSize   = 1024 * (1 << (nandId & 0x03));  
  blockSize  = 64*1024 * (1 << ((nandId>>4) & 0x03));
  reduntSize = 8 * (1 << ((nandId >> 1) & 0x1));

                         
  return TRUE;
}

/******************************************************************************
 *
 * Description:
 *    Get the page size of the NAND flash
 *
 * Returns:
 *    page size in bytes
 *
 *****************************************************************************/
uint32_t nand_getPageSize(void)
{
  return pageSize;
}

/******************************************************************************
 *
 * Description:
 *    Get the block size of the NAND flash
 *
 * Returns:
 *    block size in bytes
 *
 *****************************************************************************/
uint32_t nand_getBlockSize(void)
{
  return blockSize;
}

/******************************************************************************
 *
 * Description:
 *    Get the redundant (spare) size per page
 *
 * Returns:
 *    redundant/spare size in bytes
 *
 *****************************************************************************/
uint32_t nand_getRedundantSize(void)
{
  return reduntSize * (pageSize/512);
}

/******************************************************************************
 *
 * Description:
 *    Check if a block is valid
 *
 * Returns:
 *    TRUE if the block is valid; otherwise FALSE
 *
 *****************************************************************************/
uint32_t nand_isBlockValid(uint32_t block)
{
  uint32_t addr = 0;
  uint32_t page = 0;

  volatile uint8_t *pCLE;
  volatile uint8_t *pALE;
  volatile uint8_t *pData;

  
  pCLE  = K9F1G_CLE;
  pALE  = K9F1G_ALE;
  pData = K9F1G_DATA;

  if (block >= NAND_NUM_BLOCKS) {
    return FALSE;
  }

  addr = block * (blockSize/pageSize);

  /* 
   * Check page 0 and page 1 in each block. If the first byte
   * in the spare area (of either page 0 or page 1) is != 0xFF 
   * the block is invalid.
   */

  nandWaitReady();

  for (page = 0; page < 2; page++) {
    addr += page;

    *pCLE = K9FXX_READ_1;
    *pALE = (uint8_t) (pageSize & 0x00FF);
    *pALE = (uint8_t)((pageSize & 0xFF00) >> 8);
    *pALE = (uint8_t)((addr & 0x00FF));
    *pALE = (uint8_t)((addr & 0xFF00) >> 8);
    *pCLE = K9FXX_READ_2;

    WAIT_READY();

    if (*pData != 0xFF) {
      return FALSE;
    }
        
  }

  return TRUE;
}


/******************************************************************************
 *
 * Description:
 *    Read a page from the NAND memory
 *
 * Params:
 *    block - block number to read from
 *    page  - page with�n block to read from
 *    pageBuf - data is copied to this buffer. The size must be at least 
 *              pageSize.
 *
 * Returns:
 *    TRUE if read successful; otherwise FALSE
 *
 *****************************************************************************/
uint32_t nand_readPage(uint32_t block, uint32_t page, uint8_t* pageBuf)
{
  uint32_t i = 0;
  uint32_t addr = 0;

  volatile uint8_t *pCLE;
  volatile uint8_t *pALE;
  volatile uint8_t *pData;

  
  pCLE  = K9F1G_CLE;
  pALE  = K9F1G_ALE;
  pData = K9F1G_DATA;

  if (block >= NAND_NUM_BLOCKS) {
    return FALSE;
  }

  if (page >= blockSize/pageSize) {
    return FALSE;
  }

  addr = block * (blockSize/pageSize) + page;

  /*
   * Always reading from start of a page address.
   * This means that the column address is always 0.
   */

  *pCLE = K9FXX_READ_1;
  *pALE = 0;
  *pALE = 0;
  *pALE = (uint8_t)((addr & 0x00FF));
  *pALE = (uint8_t)((addr & 0xFF00) >> 8);
  *pCLE = K9FXX_READ_2;

  WAIT_READY(); 
  

  for (i = 0; i < pageSize; i++) {
    *pageBuf++ = *pData;  
  }


  return TRUE;
}

/******************************************************************************
 *
 * Description:
 *    Write a page of data to the NAND memory
 *
 * Params:
 *    block - block number to write to
 *    page  - page within block to write to
 *    pageBuf - data is copied from this buffer. The size must be at least 
 *              pageSize.
 *
 * Returns:
 *    TRUE if write successful; otherwise FALSE
 *
 *****************************************************************************/
uint32_t nand_writePage(uint32_t block, uint32_t page, uint8_t* pageBuf)
{
  uint32_t i = 0;
  uint32_t addr = 0;

  volatile uint8_t *pCLE;
  volatile uint8_t *pALE;
  volatile uint8_t *pData;

  
  pCLE  = K9F1G_CLE;
  pALE  = K9F1G_ALE;
  pData = K9F1G_DATA;

  if (block >= NAND_NUM_BLOCKS) {
    return FALSE;
  }

  if (page >= blockSize/pageSize) {
    return FALSE;
  }

  addr = block * (blockSize/pageSize) + page;

  /*
   * Always writing to start of a page address.
   * This means that the column address is always 0.
   */ 

  *pCLE = K9FXX_BLOCK_PROGRAM_1;
  *pALE = 0; 
  *pALE = 0; 
  *pALE = (uint8_t)((addr & 0x00FF));
  *pALE = (uint8_t)((addr & 0xFF00) >> 8);


  for (i = 0; i < pageSize; i++) {
    *pData = *pageBuf++;
  }

  *pCLE = K9FXX_BLOCK_PROGRAM_2;

  TIM_Waitus(700);  
  nandWaitReady();
  
  return ((nandStatus() & 0x01) != 0x01); 
}

/******************************************************************************
 *
 * Description:
 *    Erase a block
 *
 * Params:
 *    block - block number to erase
 *
 * Returns:
 *    TRUE if eras successful; otherwise FALSE
 *
 *****************************************************************************/
uint32_t nand_eraseBlock(uint32_t block)
{
  uint32_t addr = 0;

  volatile uint8_t *pCLE;
  volatile uint8_t *pALE;

  pCLE  = K9F1G_CLE;
  pALE  = K9F1G_ALE;

  if (block >= NAND_NUM_BLOCKS) {
    return FALSE;
  }
    
  addr = block * (blockSize/pageSize);

  *pCLE = K9FXX_BLOCK_ERASE_1;
  *pALE = (uint8_t)(addr & 0x00FF);
  *pALE = (uint8_t)((addr & 0xFF00) >> 8);
  *pCLE = K9FXX_BLOCK_ERASE_2;

  TIM_Waitus(700);
  nandWaitReady();

  return ((nandStatus() & 0x01) != 0x01); 
}

#endif

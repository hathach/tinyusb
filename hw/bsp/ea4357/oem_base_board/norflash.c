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
#include "norflash.h"


/******************************************************************************
 * Defines and typedefs
 *****************************************************************************/

#define CMD_SWID      0x90
#define CMD_CFI_QRY   0x98
#define CMD_ID_EXIT   0xF0

#define CMD_ERASE_BLOCK  0x0050
#define CMD_ERASE_SECTOR 0x0030
#define CMD_ERASE_CHIP   0x0010

#define MAN_ID_SST         0x00BF
#define DEV_ID_SST39VF3201 0x235B 

/******************************************************************************
 * External global variables
 *****************************************************************************/

/******************************************************************************
 * Local variables
 *****************************************************************************/

static geometry_t chip_info;

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

#if 0
static void getIdString(uint16_t idString[11])
{
  int i = 0;

  *(GET_ADDR(0x5555)) = 0x00AA;
  *(GET_ADDR(0x2aaa)) = 0x0055;
  *(GET_ADDR(0x5555)) = CMD_CFI_QRY;

  for (i = 0; i < 11; i++) {
    idString[i] = *(GET_ADDR(0x10 + i));
  }

  *(GET_ADDR(0x5555)) = 0x00AA;
  *(GET_ADDR(0x2aaa)) = 0x0055;
  *(GET_ADDR(0x5555)) = CMD_ID_EXIT;
}
#endif

static void getGeoInfo(uint16_t info[14])
{
  int i = 0;

  *(GET_ADDR(0x5555)) = 0x00AA;
  *(GET_ADDR(0x2aaa)) = 0x0055;
  *(GET_ADDR(0x5555)) = CMD_CFI_QRY;

  for (i = 0; i < 14; i++) {
    info[i] = *(GET_ADDR(0x27 + i));
  }

  *(GET_ADDR(0x5555)) = 0x00AA;
  *(GET_ADDR(0x2aaa)) = 0x0055;
  *(GET_ADDR(0x5555)) = CMD_ID_EXIT;
}

static uint32_t getProductId(void)
{
  uint16_t manuid = 0;
  uint16_t devid = 0;
  uint32_t result = 0;

  *(GET_ADDR(0x5555)) = 0x00AA;
  *(GET_ADDR(0x2aaa)) = 0x0055;
  *(GET_ADDR(0x5555)) = CMD_SWID;

  manuid = *(GET_ADDR(0x00));
  devid  = *(GET_ADDR(0x01));

  result = ((manuid << 16) | devid);

  *(GET_ADDR(0x5555)) = 0x00AA;
  *(GET_ADDR(0x2aaa)) = 0x0055;
  *(GET_ADDR(0x5555)) = CMD_ID_EXIT;

  return result;
}

/******************************************************************************
 *
 * Description:
 *    When the SST39VF160x/320x are in the internal Program operation, any 
 *    attempt to read DQ7 will produce the complement of the true data. Once 
 *    the Program operation is completed, DQ7 will produce true data. Note 
 *    that even though DQ7 may have valid data immediately following the 
 *    completion of an internal Write operation, the remaining data outputs 
 *    may still be invalid: valid data on the entire data bus will appear in 
 *    subsequent successive Read cycles after an interval of 1 �s. During 
 *    internal Erase operation, any attempt to read DQ7 will produce a '0'. 
 *    Once the internal Erase operation is completed, DQ7 will produce a '1'.
 *
 * Parameters:
 *    addr      The device address 
 *    data      The original (true) data
 *    timeout   Maximum number of loops to delay
 *
 * Returns:
 *    TRUE if success
 *
 *****************************************************************************/
static uint16_t check_data_polling(uint32_t addr, uint16_t data, uint32_t timeout)
{
  volatile uint16_t *p = (uint16_t*) addr;
  uint16_t true_data = data & 0x80;
  int i;

  for (i = 0; i < timeout; i++)
  {
    if ( true_data == (*p &0x80) )
    {
      TIM_Waitus(1);
      return (TRUE);
    } 
  }  
  return (FALSE);
}

/******************************************************************************
 *
 * Description:
 *    During the internal Program or Erase operation, any consecutive attempts 
 *    to read DQ6 will produce alternating �1�s and �0�s, i.e., toggling 
 *    between 1 and 0. When the internal Program or Erase operation is 
 *    completed, the DQ6 bit will stop toggling. The device is then ready 
 *    for the next operation. 
 *
 * Parameters:
 *    addr      The device address 
 *    timeout   Maximum number of loops to delay
 *
 * Returns:
 *    TRUE if success
 *
 *****************************************************************************/
static uint16_t check_toggle_ready(uint32_t addr, uint32_t timeout)
{
  volatile uint16_t *p = (uint16_t*) addr;
  uint16_t predata, currdata;
  int i;

  predata = *p & 0x40;
  for (i = 0; i < timeout; i++)
  {
    currdata = *p & 0x40;
    if (predata == currdata)
    {
      TIM_Waitus(1);
      return (TRUE);
    }
    predata = currdata;
  }
  return (FALSE);
}

/******************************************************************************
 * Public Functions
 *****************************************************************************/



/******************************************************************************
 *
 * Description:
 *    Initialize the NOR Flash
 *
 *****************************************************************************/
uint32_t norflash_init()
{
  uint32_t prodId = 0;

//   LPC_SC->PCONP      |= 0x00000800;

  LPC_EMC->CONTROL = 0x00000001;

  LPC_EMC->CONFIG  = 0x00000000;

  //Disable Auto-Byte Addressing (on boards designed for LPC24xx)
  //LPC_SC->SCS |= 0x00000001;

  pinConfig();

  LPC_EMC->STATICCONFIG0   = 0x00000081;

  LPC_EMC->STATICWAITWEN0  = 0x00000003; /* ( n + 1 ) -> 4 clock cycles */
  LPC_EMC->STATICWAITOEN0  = 0x00000003; /* ( n ) -> 0 clock cycles */
  LPC_EMC->STATICWAITRD0   = 0x00000006; /* ( n + 1 ) -> 7 clock cycles */
  LPC_EMC->STATICWAITPAG0  = 0x00000003; /* ( n + 1 ) -> 4 clock cycles */
  LPC_EMC->STATICWAITWR0   = 0x00000005; /* ( n + 2 ) -> 7 clock cycles */
  LPC_EMC->STATICWAITTURN0 = 0x00000003; /* ( n + 1 ) -> 4 clock cycles */

#if 0
  M32(0x40086400) = 3;//SFSP8_0, pin config P8_0, FUNC3, (PUP_DISABLE | PDN_DISABLE | INBUF_DISABLE | FILTER_ENABLE) 
  M32(0x40086404) = 3;//SFSP8_1, pin config P8_1, FUNC3, (PUP_DISABLE | PDN_DISABLE | INBUF_DISABLE | FILTER_ENABLE) 
  M32(0x40086408) = 3;//SFSP8_2, pin config P8_2, FUNC3, (PUP_DISABLE | PDN_DISABLE | INBUF_DISABLE | FILTER_ENABLE) 
  M32(0x4008640C) = 3;//SFSP8_3, pin config P8_3, FUNC3, (PUP_DISABLE | PDN_DISABLE | SLEWRATE_SLOW | INBUF_DISABLE | FILTER_ENABLE) 
  M32(0x40086410) = 3;//SFSP8_4, pin config P8_4, FUNC3, (PUP_DISABLE | PDN_DISABLE | SLEWRATE_SLOW | INBUF_DISABLE | FILTER_ENABLE) 
  M32(0x40086414) = 3;//SFSP8_5, pin config P8_5, FUNC3, (PUP_DISABLE | PDN_DISABLE | SLEWRATE_SLOW | INBUF_DISABLE | FILTER_ENABLE) 
  M32(0x40086418) = 3;//SFSP8_6, pin config P8_6, FUNC3, (PUP_DISABLE | PDN_DISABLE | SLEWRATE_SLOW | INBUF_DISABLE | FILTER_ENABLE) 
  M32(0x4008641C) = 3;//SFSP8_7, pin config P8_7, FUNC3, (PUP_DISABLE | PDN_DISABLE | SLEWRATE_SLOW | INBUF_DISABLE | FILTER_ENABLE) 
#endif

  prodId = getProductId();

  if (prodId == ((MAN_ID_SST << 16) | DEV_ID_SST39VF3201)) {

    uint16_t info[14];
    getGeoInfo(info);
    chip_info.device_size = 1 << info[0];
    chip_info.num_sectors = ((info[7] << 8) | info[6]) + 1;
    chip_info.sector_size = ((info[9] << 8) | info[8]) * 256;
    chip_info.num_blocks =  ((info[11] << 8) | info[10]) + 1;
    chip_info.block_size = ((info[13] << 8) | info[12]) * 256;

    return TRUE;
  }

  return FALSE;
}

void norflash_getGeometry(geometry_t* geometry)
{
  *geometry = chip_info;
}


uint32_t norflash_eraseSector(uint32_t addr)
{
  volatile uint16_t* p;

  *(GET_ADDR(0x5555)) 	= 0x00AA;
  *(GET_ADDR(0x2AAA)) 	= 0x0055;
  *(GET_ADDR(0x5555)) 	= 0x0080;
  *(GET_ADDR(0x5555)) 	= 0x00AA;
  *(GET_ADDR(0x2AAA)) 	= 0x0055;

  p  = (uint16_t*) addr;
  *p = CMD_ERASE_SECTOR;

  return check_data_polling(addr, 0xffff, 500000);
}

uint32_t norflash_eraseBlock(uint32_t addr)
{
  volatile uint16_t* p;

  *(GET_ADDR(0x5555)) 	= 0x00AA;
  *(GET_ADDR(0x2AAA)) 	= 0x0055;
  *(GET_ADDR(0x5555)) 	= 0x0080;
  *(GET_ADDR(0x5555)) 	= 0x00AA;
  *(GET_ADDR(0x2AAA)) 	= 0x0055;

  p  = (uint16_t*) addr;
  *p = CMD_ERASE_BLOCK;

  return check_toggle_ready(addr, 500000);
}

uint32_t norflash_eraseEntireChip(void)
{
  *(GET_ADDR(0x5555)) 	= 0x00AA;
  *(GET_ADDR(0x2AAA)) 	= 0x0055;
  *(GET_ADDR(0x5555)) 	= 0x0080;
  *(GET_ADDR(0x5555)) 	= 0x00AA;
  *(GET_ADDR(0x2AAA)) 	= 0x0055;
  *(GET_ADDR(0x5555)) 	= CMD_ERASE_CHIP;

  return check_toggle_ready(NORFLASH_BASE, 500000);
}

uint32_t norflash_writeWord(uint32_t addr, uint16_t data)
{
  volatile uint16_t *p;

  *(GET_ADDR(0x5555)) = 0x00AA;
  *(GET_ADDR(0x2AAA)) = 0x0055;
  *(GET_ADDR(0x5555)) = 0x00A0;

  p  = (uint16_t*) addr;
  *p = data;

  return check_toggle_ready(addr, 500000);
}

uint32_t norflash_writeBuff(uint32_t addr, uint16_t* data, uint16_t len)
{
  uint16_t i;
  for (i = 0; i < len; i++)
  {
    if (!norflash_writeWord(addr, data[i]))
    {
      return (FALSE);
    }
  }
  return (TRUE);
}

/******************************************************************************
 *
 * Description:
 *    Reads the security information from the chip. For an explanation
 *    see the user manual. 
 *
 * Parameters:
 *    SST_SecID   The factory programmed security segment 
 *    User_SecID  The user defined security segment
 *
 *****************************************************************************/
void norflash_secid_read(uint16_t SST_SecID[8], uint16_t User_SecID[8])
{
  uint16_t i;

  *(GET_ADDR(0x5555)) = 0x00AA;
  *(GET_ADDR(0x2AAA)) = 0x0055;
  *(GET_ADDR(0x5555)) = 0x0088;

  for (i = 0; i < 7; i++)
  {
    SST_SecID[i] = *(GET_ADDR(i));          // SST security is 0x00 - 0x07
    User_SecID[i] = *(GET_ADDR(i + 0x10)); // User security is 0x10 - 0x17
  }

  // exit command
  *(GET_ADDR(0x5555)) = CMD_ID_EXIT;
}

/******************************************************************************
 *
 * Description:
 *    Checks if the user defined security segment has been locked or not.
 *    See the user manual for more information. 
 *
 * Returns:
 *    TRUE if the segment is locked
 *
 *****************************************************************************/
uint32_t norflash_secid_getLockStatus(void)
{
  uint16_t status;

  *(GET_ADDR(0x5555)) = 0x00AA;
  *(GET_ADDR(0x2AAA)) = 0x0055;
  *(GET_ADDR(0x5555)) = 0x0088;

  // read status
  status = *(GET_ADDR(0xff));
  status &= 0x0008;

  *(GET_ADDR(0x5555)) = 0x00AA;
  *(GET_ADDR(0x2AAA)) = 0x0055;
  *(GET_ADDR(0x5555)) = CMD_ID_EXIT;

  if (!status)
    return TRUE; // locked
  return FALSE;  // not locked
}

/******************************************************************************
 *
 * Description:
 *    Lock the user security segment. CANNOT BE UNDONE. 
 *    See the user manual for more information. 
 *
 * Returns:
 *    TRUE if the segment is locked after programming
 *
 *****************************************************************************/
uint32_t norflash_secid_lockOut()
{
  // Code not verified. Use at own risk
#if 0
  *(GET_ADDR(0x5555)) = 0x00AA;
  *(GET_ADDR(0x2AAA)) = 0x0055;
  *(GET_ADDR(0x5555)) = 0x0085;
  *(GET_ADDR(0x0   )) = 0x0000;  // Write 0x0000 to any address
  
  if (check_toggle_ready(GET_ADDR(0x0), 500000)) 
  {
    return norflash_secid_getLockStatus();
  }
#endif    
  return FALSE;  
}

/******************************************************************************
 *
 * Description:
 *    Writes data to the user security segment (0x0010 - 0x0017). 
 *    See the user manual for more information. 
 *
 * Parameters:
 *    target   Must be in the range 0x10 to 0x17 
 *    data     The data to write
 *    len      The number of words to write
 * 
 * Returns:
 *    TRUE if the programming was successful
 *
 *****************************************************************************/
uint32_t norflash_secid_writeWord(uint16_t target, uint16_t* data, uint16_t len)
{
  // Code not verified. Use at own risk
#if 0
  uint16_t i;

  if ((target < 0x10) || (target > 0x17))
    return FALSE;

  if ((len > 8) || ((target + len) > 0x17))
    return FALSE;

  for (i = 0; i < len; i++)
  {
    *(GET_ADDR(0x5555)) = 0x00AA;
    *(GET_ADDR(0x2AAA)) = 0x0055;
    *(GET_ADDR(0x5555)) = 0x00A5;
    *(GET_ADDR(target + i)) = data;

    data++;

    /* Read the toggle bit to detect end-of-programming for User Sec ID.
       Do Not use Data# Polling for User_SecID_Word_Program!! */
    if (!check_toggle_ready(GET_ADDR(target + i), 500000))
      return FALSE;
  }
#endif

  return TRUE;
}

#endif

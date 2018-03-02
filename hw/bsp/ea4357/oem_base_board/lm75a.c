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

/*
 * NOTE: I2C must have been initialized before calling any functions in this
 * file.
 */

/******************************************************************************
 * Includes
 *****************************************************************************/
#include "../../board.h"

#if BOARD == BOARD_EA4357

#include <string.h>
#include <stdio.h>
#include "lpc_types.h"
#include "lpc43xx_i2c.h"


/******************************************************************************
 * Defines and typedefs
 *****************************************************************************/

#define I2C_PORT (LPC_I2C0)

#define LM75A_I2C_ADDR    (0x48)

#define LM75A_CMD_TEMP 0x00

#define I2CDEV LPC_I2C2


/******************************************************************************
 * External global variables
 *****************************************************************************/

/******************************************************************************
 * Local variables
 *****************************************************************************/


/******************************************************************************
 * Local Functions
 *****************************************************************************/
static Status I2CWrite(uint32_t addr, uint8_t* buf, uint32_t len) 
{
  I2C_M_SETUP_Type i2cData;

	i2cData.sl_addr7bit = addr;
	i2cData.tx_data = buf;
	i2cData.tx_length = len;
	i2cData.rx_data = NULL;
	i2cData.rx_length = 0;
	i2cData.retransmissions_max = 3;

  return I2C_MasterTransferData(I2C_PORT, &i2cData, I2C_TRANSFER_POLLING);
}

static Status I2CRead(uint32_t addr, uint8_t* buf, uint32_t len) 
{
  I2C_M_SETUP_Type i2cData;

	i2cData.sl_addr7bit = addr;
	i2cData.tx_data = NULL;
	i2cData.tx_length = 0;
	i2cData.rx_data = buf;
	i2cData.rx_length = len;
	i2cData.retransmissions_max = 3;

  return I2C_MasterTransferData(I2C_PORT, &i2cData, I2C_TRANSFER_POLLING);
}

/******************************************************************************
 * Public Functions
 *****************************************************************************/

/******************************************************************************
 *
 * Description:
 *    Initialize the EEPROM Driver
 *
 *****************************************************************************/
void lm75a_init (void)
{

}

/******************************************************************************
 *
 * Description:
 *    Read the temperature
 *
 * Params: None
 *
 * Returns:
 *   The measured temperature x 100, i.e. 26.50 degrees returned as 2650
 *
 *****************************************************************************/
int32_t lm75a_readTemp(void)
{
    uint8_t temp[2];
    int16_t t = 0;

    I2CWrite(LM75A_I2C_ADDR, LM75A_CMD_TEMP, 1);
    I2CRead(LM75A_I2C_ADDR, temp, 2);

    /* 11 MSB bits used. Celcius is calculated as Temp data * 1/8 */
    t = ((temp[0] << 8) | (temp[1]));

    return ((t * 100) >> 8);

}


#endif

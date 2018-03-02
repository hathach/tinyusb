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
#include "lpc43xx_i2c.h"
#include "acc.h"

/******************************************************************************
 * Defines and typedefs
 *****************************************************************************/
#define I2C_PORT (LPC_I2C0)

#define ACC_I2C_ADDR    (0x1D)

#define ACC_ADDR_XOUTL  0x00
#define ACC_ADDR_XOUTH  0x01
#define ACC_ADDR_YOUTL  0x02
#define ACC_ADDR_YOUTX  0x03
#define ACC_ADDR_ZOUTL  0x04
#define ACC_ADDR_ZOUTH  0x05
#define ACC_ADDR_XOUT8  0x06
#define ACC_ADDR_YOUT8  0x07
#define ACC_ADDR_ZOUT8  0x08
#define ACC_ADDR_STATUS 0x09
#define ACC_ADDR_DETSRC 0x0A
#define ACC_ADDR_TOUT   0x0B
#define ACC_ADDR_I2CAD  0x0D
#define ACC_ADDR_USRINF 0x0E
#define ACC_ADDR_WHOAMI 0x0F
#define ACC_ADDR_XOFFL  0x10
#define ACC_ADDR_XOFFH  0x11
#define ACC_ADDR_YOFFL  0x12
#define ACC_ADDR_YOFFH  0x13
#define ACC_ADDR_ZOFFL  0x14
#define ACC_ADDR_ZOFFH  0x15
#define ACC_ADDR_MCTL   0x16
#define ACC_ADDR_INTRST 0x17
#define ACC_ADDR_CTL1   0x18
#define ACC_ADDR_CTL2   0x19
#define ACC_ADDR_LDTH   0x1A
#define ACC_ADDR_PDTH   0x1B
#define ACC_ADDR_PW     0x1C
#define ACC_ADDR_LT     0x1D
#define ACC_ADDR_TW     0x1E

#define ACC_MCTL_MODE(m) ((m) << 0)
#define ACC_MCTL_GLVL(g) ((g) << 2)


#define ACC_STATUS_DRDY 0x01
#define ACC_STATUS_DOVR 0x02
#define ACC_STATUS_PERR 0x04


/******************************************************************************
 * External global variables
 *****************************************************************************/

/******************************************************************************
 * Local variables
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


static uint8_t getStatus(void)
{
    uint8_t buf[1];

    buf[0] = ACC_ADDR_STATUS;
    I2CWrite(ACC_I2C_ADDR, buf, 1);
    I2CRead(ACC_I2C_ADDR, buf, 1);

    return buf[0];
}

static uint8_t getModeControl(void)
{
    uint8_t buf[1];

    buf[0] = ACC_ADDR_MCTL;
    I2CWrite(ACC_I2C_ADDR, buf, 1);
    I2CRead(ACC_I2C_ADDR, buf, 1);

    return buf[0];
}

static void setModeControl(uint8_t mctl)
{
    uint8_t buf[2];

    buf[0] = ACC_ADDR_MCTL;
    buf[1] = mctl;
    I2CWrite(ACC_I2C_ADDR, buf, 2);
}

/******************************************************************************
 * Local Functions
 *****************************************************************************/

/******************************************************************************
 * Public Functions
 *****************************************************************************/

/******************************************************************************
 *
 * Description:
 *    Initialize the ISL29003 Device
 *
 *****************************************************************************/
void acc_init (void)
{

    /* set to measurement mode by default */

    setModeControl( (ACC_MCTL_MODE(ACC_MODE_MEASURE)
            | ACC_MCTL_GLVL(ACC_RANGE_2G) ));

}

/******************************************************************************
 *
 * Description:
 *    Read accelerometer data
 *
 * Params:
 *   [out] x - read x value
 *   [out] y - read y value
 *   [out] z - read z value
 *
 *****************************************************************************/
void acc_read (int8_t *x, int8_t *y, int8_t *z)
{
    uint8_t buf[1];

    /* wait for ready flag */
    while ((getStatus() & ACC_STATUS_DRDY) == 0);

    /*
     * Have experienced problems reading all registers
     * at once. Change to reading them one-by-one.
     */
    buf[0] = ACC_ADDR_XOUT8;
    I2CWrite(ACC_I2C_ADDR, buf, 1);
    I2CRead(ACC_I2C_ADDR, buf, 1);

    *x = (int8_t)buf[0];

    buf[0] = ACC_ADDR_YOUT8;
    I2CWrite(ACC_I2C_ADDR, buf, 1);
    I2CRead(ACC_I2C_ADDR, buf, 1);

    *y = (int8_t)buf[0];

    buf[0] = ACC_ADDR_ZOUT8;
    I2CWrite(ACC_I2C_ADDR, buf, 1);
    I2CRead(ACC_I2C_ADDR, buf, 1);

    *z = (int8_t)buf[0];
}

/******************************************************************************
 *
 * Description:
 *    Set the g-Range
 *
 * Params:
 *   [in] range - the g-Range
 *
 *****************************************************************************/
void acc_setRange(acc_range_t range)
{
    uint8_t mctl = 0;

    mctl = getModeControl();

    mctl &= ~(0x03 << 2);
    mctl |= ACC_MCTL_GLVL(range);

    setModeControl(mctl);
}

/******************************************************************************
 *
 * Description:
 *    Set sensor mode
 *
 * Params:
 *   [in] mode - the mode to set
 *
 *****************************************************************************/
void acc_setMode(acc_mode_t mode)
{
    uint8_t mctl = 0;

    mctl = getModeControl();

    mctl &= ~(0x03 << 0);
    mctl |= ACC_MCTL_MODE(mode);

    setModeControl(mctl);
}

#endif

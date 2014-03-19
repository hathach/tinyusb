#include "../../board.h"

#if BOARD == BOARD_EA4357

#include "lpc43xx_i2c.h"
#include "lpc43xx_scu.h"
#include "uda1380.h"

//Uda1380 link to I2C0 only
#define UDA1380_I2C		LPC_I2C0



/*********************************************************************//**
 * @brief 		Initialize Uda1380
 * @param[in]	i2cClockFreq	I2C clock frequency that Uda1380 operate
 * @param[in] i2sClockFreq  I2S bit clock frequency
 * @return 		None
 **********************************************************************/
int32_t Uda1380_Init(uint32_t i2cClockFreq, uint32_t i2sClockFreq)
{
  int32_t ret;
	uint8_t clk;
	
// 	// Config Pin for I2C_SDA and I2C_SCL of I2C0
// 	scu_pinmux(	2	,	3	,	MD_PLN_FAST,	FUNC1	);
// 	scu_pinmux(	2	,	4	,	MD_PLN_FAST,	FUNC1	);

	I2C_Init(UDA1380_I2C, i2cClockFreq);

	/* Enable I2C1 operation */
	I2C_Cmd(UDA1380_I2C, ENABLE);

	/* Reset */
	ret = Uda1380_WriteData(UDA1380_REG_L3, 0 );
	if(ret != UDA1380_FUNC_OK)
		return ret;

    /* Write clock settings */
	ret = Uda1380_WriteData(UDA1380_REG_I2S,0 );
	if(ret != UDA1380_FUNC_OK)
		return ret;

   	ret = Uda1380_WriteData(UDA1380_REG_MSTRMUTE,0);
	if(ret != UDA1380_FUNC_OK)
		return ret;
	
	ret = Uda1380_WriteData(UDA1380_REG_MIXSDO,0);
	if(ret != UDA1380_FUNC_OK)
		return ret;

#if UDA1380_SYSCLK_USED //Use SYSCLK
	ret = Uda1380_WriteData(UDA1380_REG_EVALCLK, 
	       EVALCLK_DEC_EN | EVALCLK_DAC_EN | EVALCLK_INT_EN | EVALCLK_DAC_SEL_SYSCLK );
	if(ret != UDA1380_FUNC_OK)
		return ret;
	
	ret = Uda1380_WriteData(UDA1380_REG_PWRCTRL,
		              PWR_PON_HP_EN | PWR_PON_DAC_EN | PWR_PON_BIAS_EN);
	if(ret != UDA1380_FUNC_OK)
		return ret;
	
#else //Use WSPLL	
	if(i2sClockFreq >= 6250 && i2sClockFreq < 12500)
		clk = EVALCLK_WSPLL_SEL6_12K;
	else if(i2sClockFreq >= 12501 && i2sClockFreq < 25000)  
		clk = EVALCLK_WSPLL_SEL12_25K;
	else if(i2sClockFreq >= 25001 && i2sClockFreq < 50000)
		clk = EVALCLK_WSPLL_SEL25_50K;
	else if(i2sClockFreq >= 50001 && i2sClockFreq < 100000)
		clk = EVALCLK_WSPLL_SEL50_100K;
	else
		clk= 0;
		
	ret = Uda1380_WriteData(UDA1380_REG_EVALCLK, 
	                 EVALCLK_DEC_EN | EVALCLK_DAC_EN | EVALCLK_INT_EN | EVALCLK_DAC_SEL_WSPLL | clk);
	if(ret != UDA1380_FUNC_OK)
		return ret;
	
	ret = Uda1380_WriteData(UDA1380_REG_PWRCTRL,
		              PWR_PON_PLL_EN | PWR_PON_HP_EN | PWR_PON_DAC_EN | PWR_PON_BIAS_EN);
	if(ret != UDA1380_FUNC_OK)
		return ret;
#endif
	return UDA1380_FUNC_OK;
}

/*********************************************************************//**
 * @brief 		Write data to a register of Uda1380
 * @param[in]	reg	Register address
 * @param[out] data  data value.
 * @return 		None
 **********************************************************************/
int32_t Uda1380_WriteData(uint8_t reg, uint16_t data)
{
	I2C_M_SETUP_Type i2cData;
	uint8_t i2cBuf[UDA1380_CMD_BUFF_SIZE];

	i2cBuf[0] = reg;
	i2cBuf[1] = (data >> 8) & 0xFF;
	i2cBuf[2] = data & 0xFF;
	
	i2cData.sl_addr7bit = UDA1380_SLAVE_ADDR;
	i2cData.tx_length = UDA1380_CMD_BUFF_SIZE;
    i2cData.tx_data = i2cBuf;
	i2cData.rx_data = NULL;
	i2cData.rx_length = 0;
	i2cData.retransmissions_max = 3;	
	
	if (I2C_MasterTransferData(UDA1380_I2C, &i2cData, I2C_TRANSFER_POLLING) == SUCCESS)
	{
	    uint16_t dataTmp; 
	    if(Uda1380_ReadData(reg, &dataTmp) != UDA1380_FUNC_OK) {
			return UDA1380_FUNC_ERR;
	    }
		if(dataTmp != data)
			return UDA1380_FUNC_ERR;
		
		return UDA1380_FUNC_OK;
	}

	return UDA1380_FUNC_ERR;
}


/*********************************************************************//**
 * @brief 		Read data stored in register of Uda1380
 * @param[in]	reg	Register address
 * @param[out] data  point to the buffer which is used for storing data.
 * @return 		None
 **********************************************************************/
int32_t Uda1380_ReadData(uint8_t reg, uint16_t *data)
{
	I2C_M_SETUP_Type i2cData;
	uint8_t i2cBuf[UDA1380_CMD_BUFF_SIZE];

	if(data == NULL)
		return UDA1380_FUNC_ERR;

	i2cBuf[0] = reg;
	
	i2cData.sl_addr7bit = UDA1380_SLAVE_ADDR;
	i2cData.tx_length = 1;
    i2cData.tx_data = i2cBuf;
	i2cData.rx_data = &i2cBuf[1];
	i2cData.rx_length = UDA1380_CMD_BUFF_SIZE - 1;
	i2cData.retransmissions_max = 3;	
	
	if (I2C_MasterTransferData(UDA1380_I2C, &i2cData, I2C_TRANSFER_POLLING) == SUCCESS)
	{
	    *data = i2cBuf[1] << 8 | i2cBuf[2];
		return UDA1380_FUNC_OK;
	}

	return UDA1380_FUNC_ERR;
}

#endif


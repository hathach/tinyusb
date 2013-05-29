/**************************************************************************/
/*!
    @file     board_ea4357.c
    @author   hathach (tinyusb.org)

    @section LICENSE

    Software License Agreement (BSD License)

    Copyright (c) 2013, hathach (tinyusb.org)
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:
    1. Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
    3. Neither the name of the copyright holders nor the
    names of its contributors may be used to endorse or promote products
    derived from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ''AS IS'' AND ANY
    EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    INCLUDING NEGLIGENCE OR OTHERWISE ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

    This file is part of the tinyusb stack.
*/
/**************************************************************************/

#include "../board.h"

#if BOARD == BOARD_EA4357


#define BOARD_UART_PORT           LPC_USART0
#define BOARD_UART_PIN_PORT 0x0f
#define BOARD_UART_PIN_TX   10 // PF.10 : UART0_TXD
#define BOARD_UART_PIN_RX   11 // PF.11 : UART0_RXD

void board_init(void)
{
  SystemInit();
  CGU_Init();
  SysTick_Config(CGU_GetPCLKFrequency(CGU_PERIPHERAL_M4CORE) / CFG_TICKS_PER_SECOND); // 1 msec tick timer

  // USB0 Power: EA4357 channel B U20 GPIO26 active low (base board), P2_3 on LPC4357
  scu_pinmux(0x2, 3, MD_PUP | MD_EZI, FUNC7);		// USB0 VBus Power
  
  // USB1 Power: EA4357 channel A U20 is enabled by SJ5 connected to pad 1-2, no more action required

  // TODO Device only USB0
  // 1.5Kohm pull-up resistor is needed on the USB DP data signal. GPIO28 (base), P9_5 (LPC4357) controls
  scu_pinmux(0x9, 5, MD_PUP|MD_EZI|MD_ZI, FUNC4);		// GPIO5[18]
  GPIO_SetDir(5, BIT_(18), 1); // output
  GPIO_ClearValue(5, BIT_(18));


  // init I2C and set up MIC2555 to have 15k pull-down on USB1 D+ & D-
  I2C_Init(LPC_I2C0, 100000);
  I2C_Cmd(LPC_I2C0, ENABLE);

  pca9532_init(); // Leds Init

//  ASSERT_INT(0x058d, mic255_get_vendorid(), (void) 0); // verify vendor id
//  ASSERT( mic255_regs_write(6, BIN8(1100)), (void) 0); // pull down D+/D- for host

#if CFG_UART_ENABLE
  //------------- UART init -------------//
	scu_pinmux(BOARD_UART_PIN_PORT, BOARD_UART_PIN_TX, MD_PDN             , FUNC1);
	scu_pinmux(BOARD_UART_PIN_PORT, BOARD_UART_PIN_RX, MD_PLN|MD_EZI|MD_ZI, FUNC1);

	UART_CFG_Type UARTConfigStruct;
  UART_ConfigStructInit(&UARTConfigStruct);
	UARTConfigStruct.Baud_rate = CFG_UART_BAUDRATE;
	UARTConfigStruct.Clock_Speed = 0;

	UART_Init(BOARD_UART_PORT, &UARTConfigStruct);
	UART_TxCmd(BOARD_UART_PORT, ENABLE); // Enable UART Transmit
#endif

#if CFG_PRINTF_TARGET == PRINTF_TARGET_SWO
#endif
}

//--------------------------------------------------------------------+
// LEDS
//--------------------------------------------------------------------+
void board_leds(uint32_t on_mask, uint32_t off_mask)
{
  pca9532_setLeds( on_mask << 8, off_mask << 8);
}

//--------------------------------------------------------------------+
// UART
//--------------------------------------------------------------------+
#if CFG_UART_ENABLE
uint32_t board_uart_send(uint8_t *buffer, uint32_t length)
{
  return UART_Send(BOARD_UART_PORT, buffer, length, BLOCKING);
}

uint32_t board_uart_recv(uint8_t *buffer, uint32_t length)
{
  return UART_Receive(BOARD_UART_PORT, buffer, length, BLOCKING);
}
#endif


/******************************************************************************
 *
 * Description:
 *   Initialize the trim potentiometer, i.e. ADC connected to TrimPot on
 *   Base Board.
 *
 *****************************************************************************/
//void trimpot_init(void)
//{
//  // pinsel for AD0.3 on p7.5
//	scu_pinmux(	7	,	5	,	PDN_DISABLE | PUP_DISABLE	| INBUF_DISABLE,	0	);
//  LPC_SCU->ENAIO0 |= (1<<3);
//
//  ADC_Init(LPC_ADC0, 400000, 10);
//
//	ADC_IntConfig(LPC_ADC0, ADC_ADINTEN2, DISABLE);
//	ADC_ChannelCmd(LPC_ADC0, ADC_CH_TRIMPOT, ENABLE);
//}

//------------- MIC2555 external OTG transceiver on USB1 -------------//

// MIC2555 1YML = 0101111, 0YML = 0101101
//#define MIC255_ADDR BIN8(0101111)

//static uint8_t mic255_regs_read(uint8_t regs_addr)
//{
//  uint8_t value;
//
//  ASSERT( SUCCESS == I2C_MasterTransferData(
//      LPC_I2C0,
//      & (I2C_M_SETUP_Type)
//      {
//        .sl_addr7bit         = MIC255_ADDR,
//        .retransmissions_max = 3,
//
//        .tx_data             = &regs_addr,
//        .tx_length           = 1,
//
//        .rx_data             = &value,
//        .rx_length           = 1
//      },
//      I2C_TRANSFER_POLLING), 0xFF);
//
//  return value;
//}

//static bool mic255_regs_write(uint8_t regs_addr, uint8_t data)
//{
//  uint8_t xfer_data[2] = { regs_addr, data} ;
//
//  ASSERT( SUCCESS == I2C_MasterTransferData(
//      LPC_I2C0,
//      & (I2C_M_SETUP_Type)
//      {
//        .sl_addr7bit         = MIC255_ADDR,
//        .retransmissions_max = 3,
//
//        .tx_data             = xfer_data,
//        .tx_length           = 2,
//      },
//      I2C_TRANSFER_POLLING), false);
//
//  return true;
//}


//static uint16_t mic255_get_vendorid(void)
//{
//  uint8_t vendor_low  = mic255_regs_read(0);
//  uint8_t vendor_high = mic255_regs_read(1);
//
//  return (vendor_high << 8) | vendor_low;
//}

#endif

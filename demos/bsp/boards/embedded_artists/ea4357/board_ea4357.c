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

#include "../../board.h"

#if BOARD == BOARD_EA4357


#define BOARD_UART_PORT           LPC_USART0
#define BOARD_UART_PIN_PORT       0x0f
#define BOARD_UART_PIN_TX         10 // PF.10 : UART0_TXD
#define BOARD_UART_PIN_RX         11 // PF.11 : UART0_RXD

void board_init(void)
{
  CGU_Init();

#if TUSB_CFG_OS == TUSB_OS_NONE // TODO may move to main.c
  SysTick_Config(CGU_GetPCLKFrequency(CGU_PERIPHERAL_M4CORE) / CFG_TICKS_PER_SECOND); // 1 msec tick timer
#endif

  //------------- USB -------------//
  // USB0 Power: EA4357 channel B U20 GPIO26 active low (base board), P2_3 on LPC4357
  scu_pinmux(0x2, 3, MD_PUP | MD_EZI, FUNC7);		// USB0 VBus Power
  
  // USB1 Power: EA4357 channel A U20 is enabled by SJ5 connected to pad 1-2, no more action required
  // TODO Remove R170, R171, solder a pair of 15K to USB1 D+/D- to test with USB1 Host

  //------------- LED -------------//
  I2C_Init(LPC_I2C0, 100000);
  I2C_Cmd(LPC_I2C0, ENABLE);
  pca9532_init();

#if CFG_UART_ENABLE
  //------------- UART -------------//
  scu_pinmux(BOARD_UART_PIN_PORT, BOARD_UART_PIN_TX, MD_PDN, FUNC1);
  scu_pinmux(BOARD_UART_PIN_PORT, BOARD_UART_PIN_RX, MD_PLN | MD_EZI | MD_ZI, FUNC1);

  UART_CFG_Type UARTConfigStruct;
  UART_ConfigStructInit(&UARTConfigStruct);
  UARTConfigStruct.Baud_rate = CFG_UART_BAUDRATE;
  UARTConfigStruct.Clock_Speed = 0;

  UART_Init(BOARD_UART_PORT, &UARTConfigStruct);
  UART_TxCmd(BOARD_UART_PORT, ENABLE); // Enable UART Transmit
#endif

  //------------- NAND Flash (K9FXX) Size = 128M, Page Size = 2K, Block Size = 128K, Number of Block = 1024 -------------//
//  nand_init();


#if 0
	//------------- Ethernet -------------//
	LPC_CREG->CREG6 &= ~0x7;

	/* RMII mode setup only */
	LPC_CREG->CREG6 |= 0x4;

	scu_pinmux(0x1, 18, (MD_EHS | MD_PLN | MD_ZI)         , FUNC3); // ENET TXD0
	scu_pinmux(0x1, 20, (MD_EHS | MD_PLN | MD_ZI)         , FUNC3); // ENET TXD1
	scu_pinmux(0x0, 1 , (MD_EHS | MD_PLN | MD_ZI)         , FUNC6); // ENET TX Enable

	scu_pinmux(0x1, 15, (MD_EHS | MD_PLN | MD_EZI | MD_ZI), FUNC3); // ENET RXD0
	scu_pinmux(0x0, 0 , (MD_EHS | MD_PLN | MD_EZI | MD_ZI), FUNC2); // ENET RXD1
	scu_pinmux(0x1, 16, (MD_EHS | MD_PLN | MD_EZI | MD_ZI), FUNC7); // ENET RX Data Valid

	scu_pinmux(0x1, 19, (MD_EHS | MD_PLN | MD_EZI | MD_ZI), FUNC0); // ENET REF CLK
	scu_pinmux(0x1, 17, (MD_EHS | MD_PLN | MD_EZI | MD_ZI), FUNC3); // ENET MDIO
	scu_pinmux(0xC,	1 , (MD_EHS | MD_PLN | MD_ZI)         , FUNC3); // ENET MDC
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

uint8_t  board_uart_getchar(void)
{
  return UART_ReceiveByte(BOARD_UART_PORT);
}
void board_uart_putchar(uint8_t c)
{
  UART_Send(BOARD_UART_PORT, &c, 1, BLOCKING);
}
#endif

#endif

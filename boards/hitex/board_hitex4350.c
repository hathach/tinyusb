/**************************************************************************/
/*!
    @file     board_hitex4350.c
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
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

    This file is part of the tinyusb stack.
*/
/**************************************************************************/

#include "../board.h"

#if BOARD == BOARD_HITEX4350

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+

//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+

//--------------------------------------------------------------------+
// IMPLEMENTATION
//--------------------------------------------------------------------+
void board_init(void)
{
  CGU_Init();
  SysTick_Config(CGU_GetPCLKFrequency(CGU_PERIPHERAL_M4CORE) / TUSB_CFG_TICKS_HZ); // 1 msec tick timer

  //------------- USB Bus power HOST ONLY-------------//
  // Hitex VBUS0 is P2_3
  scu_pinmux(0x2, 3, MD_PUP | MD_EZI, FUNC7);     	// USB0_PWR_EN, USB0 VBus function
  scu_pinmux(0x6, 3, MD_PUP | MD_EZI, FUNC1);     		// P6_3 USB0_PWR_EN, USB0 VBus function

  // Hitex VBUS1 is P9_5
  scu_pinmux(0x9, 5, MD_PUP | MD_EZI, FUNC2);				// USB1_PWR_EN, USB1 VBus function

  //------------- LEDs init -------------//
  // TODO Leds for hitex

  //------------- UART init -------------//
  #if CFG_UART_ENABLE
	scu_pinmux(BOARD_UART_PIN_PORT, BOARD_UART_PIN_TX, MD_PDN             , FUNC1);
	scu_pinmux(BOARD_UART_PIN_PORT, BOARD_UART_PIN_RX, MD_PLN|MD_EZI|MD_ZI, FUNC1);

	UART_CFG_Type UARTConfigStruct;
  UART_ConfigStructInit(&UARTConfigStruct);
	UARTConfigStruct.Baud_rate = CFG_UART_BAUDRATE;
	UARTConfigStruct.Clock_Speed = 0;

	UART_Init(BOARD_UART_PORT, &UARTConfigStruct);
	UART_TxCmd(BOARD_UART_PORT, ENABLE); // Enable UART Transmit
  #endif

}

//--------------------------------------------------------------------+
// LEDS
//--------------------------------------------------------------------+
void board_leds(uint32_t on_mask, uint32_t off_mask)
{
  // TODO LED for hitex
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

#endif

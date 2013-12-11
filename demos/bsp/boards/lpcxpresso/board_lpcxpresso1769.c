/**************************************************************************/
/*!
    @file     board_lpcxpresso1769.c
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

#if BOARD == BOARD_LPCXPRESSO1769

#define BOARD_UART_PORT   LPC_UART3

void board_init(void)
{
  SystemInit();
  SysTick_Config(SystemCoreClock / CFG_TICKS_PER_SECOND); // 1 msec tick timer

  // Leds Init
  GPIO_SetDir(CFG_LED_PORT, BIT_(CFG_LED_PIN), 1);

  //------------- USB Device -------------//
  // VBUS sense is wrongly connected to P0_5 (instead of P1_30). So we need to always pull P1_30 to high
  // so that USB device block can work. However, Device Controller (thus tinyusb) cannot able to determine
  // if device is disconnected or not
  PINSEL_ConfigPin( &(PINSEL_CFG_Type) {
      .Portnum = 1, .Pinnum = 30,
      .Funcnum = 2, .Pinmode = PINSEL_PINMODE_PULLUP} );

#if CFG_UART_ENABLE
  //------------- UART init -------------//

  PINSEL_CFG_Type PinCfg =
  {
      .Portnum   = 0,
      .Pinnum    = 0, // TXD is P0.0
      .Funcnum   = 2,
      .OpenDrain = 0,
      .Pinmode   = 0
  };
	PINSEL_ConfigPin(&PinCfg);

	PinCfg.Portnum = 0;
	PinCfg.Pinnum  = 1; // RXD is P0.1
	PINSEL_ConfigPin(&PinCfg);

	UART_CFG_Type UARTConfigStruct;
  UART_ConfigStructInit(&UARTConfigStruct);
	UARTConfigStruct.Baud_rate = CFG_UART_BAUDRATE;

	UART_Init(BOARD_UART_PORT, &UARTConfigStruct);
	UART_TxCmd(BOARD_UART_PORT, ENABLE); // Enable UART Transmit
#endif
}

//--------------------------------------------------------------------+
// LEDS
//--------------------------------------------------------------------+
void board_leds(uint32_t on_mask, uint32_t off_mask)
{
  if (on_mask & BIT_(0))
  {
    GPIO_SetValue(CFG_LED_PORT, BIT_(CFG_LED_PIN));
  }else if (off_mask & BIT_(0))
  {
    GPIO_ClearValue(CFG_LED_PORT, BIT_(CFG_LED_PIN));
  }
}

//--------------------------------------------------------------------+
// UART
//--------------------------------------------------------------------+
#if CFG_UART_ENABLE
uint32_t board_uart_send(uint8_t *buffer, uint32_t length)
{
  return UART_Send(BOARD_UART_PORT, buffer, length, BLOCKING);
}

uint8_t  board_uart_getchar(void)
{
  return UART_ReceiveByte(BOARD_UART_PORT);
}

#endif

#endif

/*
 * board_ea4357.c
 *
 *  Created on: Jan 17, 2013
 *      Author: hathach
 */

/*
 * Software License Agreement (BSD License)
 * Copyright (c) 2013, hathach (tinyusb.org)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 * This file is part of the tiny usb stack.
 */

#include "board.h"

#if BOARD == BOARD_EA4357

#define UART_PORT   LPC_USART0

#if 0
static const struct {
  uint8_t port;
  uint8_t pin;
}leds[CFG_LED_NUMBER] = { {0, 8} };
#endif

void board_init(void)
{
  SystemInit();
  CGU_Init();

  SysTick_Config(CGU_GetPCLKFrequency(CGU_PERIPHERAL_M4CORE) / CFG_TICKS_PER_SECOND); // 1 msec tick timer

  // USB Host Power Enable
  // USB0
  // TODO USB1

#if 0
  // Leds Init
	uint8_t i;
	for (i=0; i<CFG_LED_NUMBER; i++)
	{
	  scu_pinmux(leds[i].port, leds[i].pin, MD_PUP|MD_EZI|MD_ZI, 0); // MD_PDN
	  GPIO_SetDir(leds[i].port, BIT_(leds[i].pin), 1); // output
	}
#endif

#if CFG_UART_ENABLE
  // pinsel for UART
	scu_pinmux(0xF ,10 , MD_PDN, FUNC1); 	              // PF.10 : UART0_TXD
	scu_pinmux(0xF ,11 , MD_PLN|MD_EZI|MD_ZI, FUNC1); 	// PF.11 : UART0_RXD

	UART_CFG_Type UARTConfigStruct;
  UART_ConfigStructInit(&UARTConfigStruct);
	UARTConfigStruct.Baud_rate = CFG_UART_BAUDRATE;
	UARTConfigStruct.Clock_Speed = 0;

	UART_Init(UART_PORT, &UARTConfigStruct);
	UART_TxCmd(UART_PORT, ENABLE); // Enable UART Transmit
#endif

#if CFG_PRINTF_TARGET == PRINTF_TARGET_SWO
#endif
}

//--------------------------------------------------------------------+
// LEDS
//--------------------------------------------------------------------+
void board_leds(uint32_t mask, uint32_t state) __attribute__ ((warning("not supported yet")));
void board_leds(uint32_t mask, uint32_t state)
{
#if 0
  uint8_t i;
  for(i=0; i<CFG_LED_NUMBER; i++)
  {
    if ( mask & BIT_(i) )
    {
      (mask & state) ? GPIO_SetValue(leds[i].port, BIT_(leds[i].pin)) : GPIO_ClearValue(leds[i].port, BIT_(leds[i].pin)) ;
    }
  }
#endif
}

//--------------------------------------------------------------------+
// UART
//--------------------------------------------------------------------+
#if CFG_UART_ENABLE
uint32_t board_uart_send(uint8_t *buffer, uint32_t length)
{
  return UART_Send(UART_PORT, buffer, length, BLOCKING);
}

uint32_t board_uart_recv(uint8_t *buffer, uint32_t length)
{
  return UART_Receive(UART_PORT, buffer, length, BLOCKING);
}
#endif

#endif

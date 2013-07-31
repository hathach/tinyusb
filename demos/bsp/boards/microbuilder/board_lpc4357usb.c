/**************************************************************************/
/*!
    @file     board_lpc4357usb.c
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

#if BOARD == BOARD_LPC4357USB

#define BOARD_UART_PORT       (LPC_USART0)
#define BOARD_UART_PIN_PORT   (0x0F)
#define BOARD_UART_PIN_TX     (10) // PF.10 : UART0_TXD
#define BOARD_UART_PIN_RX     (11) // PF.11 : UART0_RXD

#define BOARD_LED0_PORT       (0x0C)
#define BOARD_LED0_PIN        (2)   // PC.2 = User LED
#define BOARD_LED0_FUNCTION   (4)   // GPIO multiplexed as function 4 on PC.2
#define BOARD_LED0_GPIO_PORT  (6)
#define BOARD_LED0_GPIO_PIN   (1)   // PC.2 = GPIO 6[1]

void board_init(void)
{
  SystemInit();
  CGU_Init();
  
  /* Setup the systick time for 1ms ticks */
  SysTick_Config(CGU_GetPCLKFrequency(CGU_PERIPHERAL_M4CORE) / CFG_TICKS_PER_SECOND);
  
  /* Configure LED0 as GPIO */
  scu_pinmux(BOARD_LED0_PORT, BOARD_LED0_PIN, MD_PDN, BOARD_LED0_FUNCTION);
  GPIO_SetDir(BOARD_LED0_GPIO_PORT, (1 << BOARD_LED0_GPIO_PIN), 1);

  /* Init I2C @ 400kHz */
  I2C_Init(LPC_I2C0, 400000);
  I2C_Cmd(LPC_I2C0, ENABLE);

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
  if (on_mask & 0x01)
  {
    LPC_GPIO_PORT->SET[BOARD_LED0_GPIO_PORT] = (1 << BOARD_LED0_GPIO_PIN);
  }
  
  if (off_mask & 0x01)
  {
    LPC_GPIO_PORT->CLR[BOARD_LED0_GPIO_PORT] = (1 << BOARD_LED0_GPIO_PIN);
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

uint32_t board_uart_recv(uint8_t *buffer, uint32_t length)
{
  return UART_Receive(BOARD_UART_PORT, buffer, length, BLOCKING);
}
#endif

#endif

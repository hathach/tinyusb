/**************************************************************************/
/*!
    @file     board_mcb4300.c
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

#if BOARD == BOARD_MCB4300

//--------------------------------------------------------------------+
// INCLUDE
//--------------------------------------------------------------------+
#define BOARD_MAX_LEDS      8

#define BOARD_UART_PORT     LPC_USART0
#define BOARD_UART_PIN_PORT 2
#define BOARD_UART_PIN_TX   0
#define BOARD_UART_PIN_RX   1

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+
static const uint8_t ledports[] = {6, 6, 6, 6, 6, 4, 4, 4};
static const uint8_t ledbits[] = {24, 25, 26, 27, 28, 12, 13, 14};

const static struct {
  uint8_t port;
  uint8_t pin;
}leds[BOARD_MAX_LEDS] = {
    {6, 24}, {6, 25}, {6, 26}, {6, 27},
    {4, 28}, {4, 12}, {4, 13}, {4, 14}};

//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+

//--------------------------------------------------------------------+
// IMPLEMENTATION
//--------------------------------------------------------------------+
void board_init(void)
{
  CGU_Init();

#if TUSB_CFG_OS == TUSB_OS_NONE // TODO may move to main.c
  SysTick_Config(CGU_GetPCLKFrequency(CGU_PERIPHERAL_M4CORE) / TUSB_CFG_TICKS_HZ); // 1 msec tick timer
#endif

  //------------- USB Bus power HOST ONLY-------------//
  // Keil VBUS0 is P6_3
  scu_pinmux(0x6, 3, MD_PUP | MD_EZI, FUNC1);     		// P6_3 USB0_PWR_EN, USB0 VBus function

  // Keil VBUS1 is P9_5
  scu_pinmux(0x9, 5, MD_PUP | MD_EZI, FUNC2);				// P9_5 USB1_PWR_EN, USB1 VBus function

  //------------- LEDs init, J21 must be installed -------------//
  LPC_SCU->SFSPD_10  =  4;              // GPIO6[24]
  LPC_SCU->SFSPD_11  =  4;              // GPIO6[25]
  LPC_SCU->SFSPD_12  =  4;              // GPIO6[26]
  LPC_SCU->SFSPD_13  =  4;              // GPIO6[27]
  LPC_SCU->SFSPD_14  =  4;              // GPIO6[28]
  LPC_SCU->SFSP9_0   =  0;              // GPIO4[12]
  LPC_SCU->SFSP9_1   =  0;              // GPIO4[13]
  LPC_SCU->SFSP9_2   =  0;              // GPIO4[14]

  for(uint32_t i=0; i<BOARD_MAX_LEDS; i++)
  {
    GPIO_SetDir(leds[i].port, BIT_(leds[i].pin), 1); // output
  }

  //------------- UART -------------//
  scu_pinmux(BOARD_UART_PIN_PORT, BOARD_UART_PIN_TX, MD_PDN             , FUNC1);
  scu_pinmux(BOARD_UART_PIN_PORT, BOARD_UART_PIN_RX, MD_PLN|MD_EZI|MD_ZI, FUNC1);

  UART_CFG_Type UARTConfigStruct;
  UART_ConfigStructInit(&UARTConfigStruct);
  UARTConfigStruct.Baud_rate = CFG_UART_BAUDRATE;
  UARTConfigStruct.Clock_Speed = 0;

  UART_Init(BOARD_UART_PORT, &UARTConfigStruct);
  UART_TxCmd(BOARD_UART_PORT, ENABLE); // Enable UART Transmit
}

//--------------------------------------------------------------------+
// LEDS
//--------------------------------------------------------------------+
void board_leds(uint32_t on_mask, uint32_t off_mask)
{
  for (uint32_t i=0; i<BOARD_MAX_LEDS; i++)
  {
    if ( on_mask & BIT_(i))
    {
      GPIO_SetValue(leds[i].port, BIT_(leds[i].pin));
    }else if ( off_mask & BIT_(i)) // on_mask take precedence over off_mask
    {
      GPIO_ClearValue(leds[i].port, BIT_(leds[i].pin));
    }
  }
}

//--------------------------------------------------------------------+
// BUTTONS
//--------------------------------------------------------------------+
uint32_t board_buttons(void)
{
  return 0; // TODO buttons for mcb4300
}

//--------------------------------------------------------------------+
// UART
//--------------------------------------------------------------------+
uint8_t  board_uart_getchar(void)
{
  return UART_ReceiveByte(BOARD_UART_PORT);
}

void board_uart_putchar(uint8_t c)
{
  UART_Send(BOARD_UART_PORT, &c, 1, BLOCKING);
}

#endif

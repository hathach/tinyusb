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

static const struct {
  uint8_t mux_port;
  uint8_t mux_pin;

  uint8_t gpio_port;
  uint8_t gpio_pin;
}buttons[] =
{
    {0x0a, 3, 4, 10 }, // Joystick up
    {0x09, 1, 4, 13 }, // Joystick down
    {0x0a, 2, 4, 9  }, // Joystick left
    {0x09, 0, 4, 12 }, // Joystick right
    {0x0a, 1, 4, 8  }, // Joystick press
    {0x02, 7, 0, 7  }, // SW6
};

enum {
  BOARD_BUTTON_COUNT = sizeof(buttons) / sizeof(buttons[0])
};

void board_init(void)
{
  CGU_Init();

#if TUSB_CFG_OS == TUSB_OS_NONE // TODO may move to main.c
  SysTick_Config(CGU_GetPCLKFrequency(CGU_PERIPHERAL_M4CORE) / TUSB_CFG_TICKS_HZ); // 1 msec tick timer
#endif

  //------------- USB -------------//
  // USB0 Power: EA4357 channel B U20 GPIO26 active low (base board), P2_3 on LPC4357
  scu_pinmux(0x02, 3, MD_PUP | MD_EZI, FUNC7);		// USB0 VBus Power

  #if TUSB_CFG_CONTROLLER_0_MODE & TUSB_MODE_DEVICE
  scu_pinmux(0x09, 5, GPIO_PDN, FUNC4); // P9_5 (GPIO5[18]) (GPIO28 on oem base) as USB connect, active low.
  GPIO_SetDir(5, BIT_(18), 1);
  #endif

  // USB1 Power: EA4357 channel A U20 is enabled by SJ5 connected to pad 1-2, no more action required
  // TODO Remove R170, R171, solder a pair of 15K to USB1 D+/D- to test with USB1 Host

  //------------- LED -------------//
  I2C_Init(LPC_I2C0, 100000);
  I2C_Cmd(LPC_I2C0, ENABLE);
  pca9532_init();

  //------------- BUTTON -------------//
  for(uint8_t i=0; i<BOARD_BUTTON_COUNT; i++)
  {
    scu_pinmux(buttons[i].mux_port, buttons[i].mux_pin, GPIO_NOPULL, FUNC0);
    GPIO_SetDir(buttons[i].gpio_port, BIT_(buttons[i].gpio_pin), 0);
  }

  //------------- UART -------------//
  scu_pinmux(BOARD_UART_PIN_PORT, BOARD_UART_PIN_TX, MD_PDN, FUNC1);
  scu_pinmux(BOARD_UART_PIN_PORT, BOARD_UART_PIN_RX, MD_PLN | MD_EZI | MD_ZI, FUNC1);

  UART_CFG_Type UARTConfigStruct;
  UART_ConfigStructInit(&UARTConfigStruct);
  UARTConfigStruct.Baud_rate   = CFG_UART_BAUDRATE;
  UARTConfigStruct.Clock_Speed = 0;

  UART_Init(BOARD_UART_PORT, &UARTConfigStruct);
  UART_TxCmd(BOARD_UART_PORT, ENABLE); // Enable UART Transmit

  //------------- NAND Flash (K9FXX) Size = 128M, Page Size = 2K, Block Size = 128K, Number of Block = 1024 -------------//
//  nand_init();
}

//--------------------------------------------------------------------+
// LEDS
//--------------------------------------------------------------------+
void board_leds(uint32_t on_mask, uint32_t off_mask)
{
  pca9532_setLeds( on_mask << 8, off_mask << 8);
}

//--------------------------------------------------------------------+
// BUTTONS
//--------------------------------------------------------------------+
static bool button_read(uint8_t id)
{
  return !BIT_TEST_( GPIO_ReadValue(buttons[id].gpio_port), buttons[id].gpio_pin ); // button is active low
}

uint32_t board_buttons(void)
{
  uint32_t result = 0;

  for(uint8_t i=0; i<BOARD_BUTTON_COUNT; i++) result |= (button_read(i) ? BIT_(i) : 0);

  return result;
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

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

#define BOARD_LED_PORT                  (0)
#define BOARD_LED_PIN                   (22)

const static struct {
  uint8_t port;
  uint8_t pin;
} buttons[] =
{
    {2, 3  }, // Joystick up
    {0, 15 }, // Joystick down
    {2, 4  }, // Joystick left
    {0, 16 }, // Joystick right
    {0, 17 }, // Joystick press
    {0, 4  }, // SW3
//    {1, 31 }, // SW4 (require to remove J28)
};

enum {
  BOARD_BUTTON_COUNT = sizeof(buttons) / sizeof(buttons[0])
};

#define BOARD_UART_PORT   LPC_UART3

void board_init(void)
{
  SystemInit();

#if TUSB_CFG_OS == TUSB_OS_NONE // TODO may move to main.c
  SysTick_Config(SystemCoreClock / TUSB_CFG_TICKS_HZ); // 1 msec tick timer
#endif

  //------------- LED -------------//
  GPIO_SetDir(BOARD_LED_PORT, BIT_(BOARD_LED_PIN), 1);

  //------------- BUTTON -------------//
  for(uint8_t i=0; i<BOARD_BUTTON_COUNT; i++) GPIO_SetDir(buttons[i].port, BIT_(buttons[i].pin), 0);

#if MODE_DEVICE_SUPPORTED
  //------------- USB Device -------------//
  // VBUS sense is wrongly connected to P0_5 (instead of P1_30). So we need to always pull P1_30 to high
  // so that USB device block can work. However, Device Controller (thus tinyusb) cannot able to determine
  // if device is disconnected or not
  PINSEL_ConfigPin( &(PINSEL_CFG_Type) {
      .Portnum = 1, .Pinnum = 30,
      .Funcnum = 2, .Pinmode = PINSEL_PINMODE_PULLUP} );

  //P0_21 instead of P2_9 as USB connect
#endif

  //------------- UART -------------//
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
}

//--------------------------------------------------------------------+
// LEDS
//--------------------------------------------------------------------+
void board_leds(uint32_t on_mask, uint32_t off_mask)
{
  if (on_mask & BIT_(0))
  {
    GPIO_SetValue(BOARD_LED_PORT, BIT_(BOARD_LED_PIN));
  }else if (off_mask & BIT_(0))
  {
    GPIO_ClearValue(BOARD_LED_PORT, BIT_(BOARD_LED_PIN));
  }
}

//--------------------------------------------------------------------+
// BUTTONS
//--------------------------------------------------------------------+
static bool button_read(uint8_t id)
{
  return !BIT_TEST_( GPIO_ReadValue(buttons[id].port), buttons[id].pin ); // button is active low
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
void board_uart_putchar(uint8_t c)
{
  UART_Send(BOARD_UART_PORT, &c, 1, BLOCKING);
}

uint8_t  board_uart_getchar(void)
{
  return UART_ReceiveByte(BOARD_UART_PORT);
}

#endif

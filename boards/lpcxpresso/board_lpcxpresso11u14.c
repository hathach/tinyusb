/**************************************************************************/
/*!
    @file     board_lpcxpresso11u14.c
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

#if BOARD == BOARD_LPCXPRESSO11U14

#define LED_PORT                  (0)
#define LED_PIN                   (7)
#define LED_ON                    (1)

const static struct {
  uint8_t port;
  uint8_t pin;
} buttons[] =
{
    {1, 22 }, // Joystick up
    {1, 20 }, // Joystick down
    {1, 23 }, // Joystick left
    {1, 21 }, // Joystick right
    {1, 19 }, // Joystick press
    {0, 1  }, // SW3
//    {1, 4  }, // SW4 (require to remove J28)
};


enum {
  BOARD_BUTTON_COUNT = sizeof(buttons) / sizeof(buttons[0])
};

void board_init(void)
{
  SystemInit();

#if TUSB_CFG_OS == TUSB_OS_NONE // TODO may move to main.c
  SysTick_Config(SystemCoreClock / TUSB_CFG_TICKS_HZ); // 1 msec tick timer
#endif

  GPIOInit();

  //------------- LED -------------//
  GPIOSetDir(LED_PORT, LED_PIN, 1);

  //------------- BUTTON -------------//
  for(uint8_t i=0; i<BOARD_BUTTON_COUNT; i++) GPIOSetDir(buttons[i].port, buttons[i].pin, 0);

  //------------- UART -------------//
  UARTInit(CFG_UART_BAUDRATE);
}

//--------------------------------------------------------------------+
// LEDS
//--------------------------------------------------------------------+
void board_leds(uint32_t on_mask, uint32_t off_mask)
{
  if (on_mask & BIT_(0))
  {
    GPIOSetBitValue(LED_PORT, LED_PIN, LED_ON);
  }else if (off_mask & BIT_(0))
  {
    GPIOSetBitValue(LED_PORT, LED_PIN, 1 - LED_ON);
  }
}

//--------------------------------------------------------------------+
// Buttons
//--------------------------------------------------------------------+
static bool button_read(uint8_t id)
{
  return !GPIOGetPinValue(buttons[id].port, buttons[id].pin); // button is active low
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
  UARTSend(&c, 1);
}

uint8_t  board_uart_getchar(void)
{
//  *buffer = get_key(); TODO cannot find available code for uart getchar
  return 0;
}

#endif

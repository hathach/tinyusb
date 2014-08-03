/**************************************************************************/
/*!
    @file     board_rf1ghznode.c
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

#if BOARD == BOARD_LPCXPRESSO11U68

#define LED_PORT                  (1)
#define LED_PIN                   (31)
#define LED_ON                    (0)
#define LED_OFF                   (1)

const static struct {
  uint8_t port;
  uint8_t pin;
} buttons[] = { { 0, 1 } };

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
    GPIOSetBitValue(LED_PORT, LED_PIN, LED_OFF);
  }
}

//--------------------------------------------------------------------+
// Buttons
//--------------------------------------------------------------------+
uint32_t board_buttons(void)
{
//  for(uint8_t i=0; i<BOARD_BUTTON_COUNT; i++) GPIOGetPinValue(buttons[i].port, buttons[i].pin);
  return GPIOGetPinValue(buttons[0].port, buttons[0].pin) ? 0 : 1; // button is active low
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

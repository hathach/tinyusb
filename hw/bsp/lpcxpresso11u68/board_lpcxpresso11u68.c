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

#ifdef BOARD_LPCXPRESSO11U68

#include "../board.h"

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

// required by lpcopen chip layer
uint32_t const OscRateIn = 0;
uint32_t const RTCOscRateIn = 0;

// required by startup
void SystemInit(void)
{
  Chip_SystemInit();
}

void board_init(void)
{
  SystemInit();

#if CFG_TUSB_OS == OPT_OS_NONE // TODO may move to main.c
  SysTick_Config(SystemCoreClock / BOARD_TICKS_HZ); // 1 msec tick timer
#endif

  Chip_GPIO_Init(LPC_GPIO);

  //------------- LED -------------//
  Chip_GPIO_SetPinDIROutput(LPC_GPIO, LED_PORT, BOARD_LED0);

  //------------- BUTTON -------------//
  //for(uint8_t i=0; i<BOARD_BUTTON_COUNT; i++) GPIOSetDir(buttons[i].port, buttons[i].pin, 0);

  //------------- UART -------------//
  //UARTInit(CFG_UART_BAUDRATE);
}

/*------------------------------------------------------------------*/
/* TUSB HAL MILLISECOND
 *------------------------------------------------------------------*/
#if CFG_TUSB_OS == OPT_OS_NONE

volatile uint32_t system_ticks = 0;

void SysTick_Handler (void)
{
  system_ticks++;
}

uint32_t tusb_hal_millis(void)
{
  return board_tick2ms(system_ticks);
}

#endif

//--------------------------------------------------------------------+
// LEDS
//--------------------------------------------------------------------+
void board_led_control(uint32_t id, bool state)
{
  if (state)
  {
    Chip_GPIO_SetValue(LPC_GPIO, LED_PORT, 1 << id);
  }else
  {
    Chip_GPIO_ClearValue(LPC_GPIO, LED_PORT, 1 << id);
  }
}

//--------------------------------------------------------------------+
// Buttons
//--------------------------------------------------------------------+
uint32_t board_buttons(void)
{
//  for(uint8_t i=0; i<BOARD_BUTTON_COUNT; i++) GPIOGetPinValue(buttons[i].port, buttons[i].pin);
//  return GPIOGetPinValue(buttons[0].port, buttons[0].pin) ? 0 : 1; // button is active low
}

//--------------------------------------------------------------------+
// UART
//--------------------------------------------------------------------+
void board_uart_putchar(uint8_t c)
{
  //UARTSend(&c, 1);
}

uint8_t  board_uart_getchar(void)
{
//  *buffer = get_key(); TODO cannot find available code for uart getchar
  return 0;
}

#endif

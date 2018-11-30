/**************************************************************************/
/*!
    @file     board_lpcexpresso1347.c
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

#ifdef BOARD_LPCXPRESSO1347

#include "../board.h"

#define LED_PORT                  (0)
#define LED_PIN                   (7)
#define LED_ON                    (1)
#define LED_OFF                   (0)

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

// required by lpcopen chip layer
uint32_t const OscRateIn = 0;
uint32_t const ExtRateIn = 0;

// required by startup
void SystemInit(void)
{
  Chip_SystemInit();
}

void board_init(void)
{
  Chip_SystemInit();

#if CFG_TUSB_OS == OPT_OS_NONE // TODO may move to main.c
  SysTick_Config(SystemCoreClock / BOARD_TICKS_HZ); // 1 msec tick timer
#endif

  Chip_GPIO_Init(LPC_GPIO_PORT);

  //------------- LED -------------//
  Chip_GPIO_SetPinDIROutput(LPC_GPIO_PORT, 0, BOARD_LED0);

  //------------- BUTTON -------------//
//  for(uint8_t i=0; i<BOARD_BUTTON_COUNT; i++) GPIOSetDir(buttons[i].port, BIT_(buttons[i].pin), 0);

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
    Chip_GPIO_SetValue(LPC_GPIO_PORT, 0, 1 << id);
  }else
  {
    Chip_GPIO_ClearValue(LPC_GPIO_PORT, 0, 1 << id);
  }
}

//--------------------------------------------------------------------+
// BUTTONS
//--------------------------------------------------------------------+
static bool button_read(uint8_t id)
{
//  return !GPIOGetPinValue(buttons[id].port, buttons[id].pin); // button is active low
}

uint32_t board_buttons(void)
{
  uint32_t result = 0;

//  for(uint8_t i=0; i<BOARD_BUTTON_COUNT; i++) result |= (button_read(i) ? BIT_(i) : 0);

  return result;
}

//--------------------------------------------------------------------+
// UART
//--------------------------------------------------------------------+
void board_uart_putchar(uint8_t c)
{
//  UARTSend(&c, 1);
}

uint8_t  board_uart_getchar(void)
{
  return 0;
}

#endif

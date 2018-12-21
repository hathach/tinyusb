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

#define LED_PORT    0
#define LED_PIN     7

static const struct {
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

/* System oscillator rate and RTC oscillator rate */
const uint32_t OscRateIn = 12000000;
const uint32_t ExtRateIn = 0;

/* Pin muxing table, only items that need changing from their default pin
   state are in this table. */
static const PINMUX_GRP_T pinmuxing[] = 
{
  {0,  1,  (IOCON_FUNC1 | IOCON_RESERVED_BIT_7 | IOCON_MODE_INACT)},	/* PIO0_1 used for CLKOUT */
  {0,  2,  (IOCON_FUNC1 | IOCON_RESERVED_BIT_7 | IOCON_MODE_PULLUP)},	/* PIO0_2 used for SSEL */
  {0,  3,  (IOCON_FUNC1 | IOCON_RESERVED_BIT_7 | IOCON_MODE_INACT)},	/* PIO0_3 used for USB_VBUS */
  {0,  6,  (IOCON_FUNC1 | IOCON_RESERVED_BIT_7 | IOCON_MODE_INACT)},	/* PIO0_6 used for USB_CONNECT */
  {0,  8,  (IOCON_FUNC1 | IOCON_RESERVED_BIT_7 | IOCON_MODE_INACT)},	/* PIO0_8 used for MISO0 */
  {0,  9,  (IOCON_FUNC1 | IOCON_RESERVED_BIT_7 | IOCON_MODE_INACT)},	/* PIO0_9 used for MOSI0 */
  {0,  11, (IOCON_FUNC2 | IOCON_ADMODE_EN      | IOCON_FILT_DIS)},	/* PIO0_11 used for AD0 */
  {0,  18, (IOCON_FUNC1 | IOCON_RESERVED_BIT_7 | IOCON_MODE_INACT)},	/* PIO0_18 used for RXD */
  {0,  19, (IOCON_FUNC1 | IOCON_RESERVED_BIT_7 | IOCON_MODE_INACT)},	/* PIO0_19 used for TXD */
  {1,  29, (IOCON_FUNC1 | IOCON_RESERVED_BIT_7 | IOCON_MODE_INACT)},	/* PIO1_29 used for SCK0 */
};

// Invoked by startup code
void SystemInit(void)
{
  /* Enable IOCON clock */
  Chip_Clock_EnablePeriphClock(SYSCTL_CLOCK_IOCON);
  Chip_IOCON_SetPinMuxing(LPC_IOCON, pinmuxing, sizeof(pinmuxing) / sizeof(PINMUX_GRP_T));
  Chip_SetupXtalClocking();
}

void board_init(void)
{
  SystemCoreClockUpdate();

#if CFG_TUSB_OS == OPT_OS_NONE
  SysTick_Config(SystemCoreClock / BOARD_TICKS_HZ); // 1 msec tick timer
#endif

  Chip_GPIO_Init(LPC_GPIO_PORT);

  //------------- LED -------------//
  Chip_GPIO_SetPinDIROutput(LPC_GPIO_PORT, LED_PORT, LED_PIN);

  //------------- BUTTON -------------//
//  for(uint8_t i=0; i<BOARD_BUTTON_COUNT; i++) GPIOSetDir(buttons[i].port, TU_BIT(buttons[i].pin), 0);

  //------------- UART -------------//
  //UARTInit(CFG_UART_BAUDRATE);

  // USB
  Chip_USB_Init(); // Setup PLL clock, and power
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
void board_led_control(bool state)
{
  Chip_GPIO_SetPinState(LPC_GPIO_PORT, LED_PORT, LED_PIN, state);
}


//--------------------------------------------------------------------+
// BUTTONS
//--------------------------------------------------------------------+
#if 0
static bool button_read(uint8_t id)
{
  (void) id;
//  return !GPIOGetPinValue(buttons[id].port, buttons[id].pin); // button is active low
  return 0;
}
#endif

uint32_t board_buttons(void)
{
  uint32_t result = 0;

//  for(uint8_t i=0; i<BOARD_BUTTON_COUNT; i++) result |= (button_read(i) ? TU_BIT(i) : 0);

  return result;
}

//--------------------------------------------------------------------+
// UART
//--------------------------------------------------------------------+
void board_uart_putchar(uint8_t c)
{
  (void) c;
//  UARTSend(&c, 1);
}

uint8_t  board_uart_getchar(void)
{
  return 0;
}

#endif

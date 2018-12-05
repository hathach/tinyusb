/**************************************************************************/
/*!
 @file    board_ea4088qs.c
 @author  hathach (tinyusb.org)

 @section LICENSE

 Software License Agreement (BSD License)

 Copyright (c) 2018, hathach (tinyusb.org)
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

#ifdef BOARD_EA4088QS

#include "../board.h"
#include "tusb.h"

#define LED_PORT      2
#define LED_PIN       19

//--------------------------------------------------------------------+
// MACRO TYPEDEF CONSTANT ENUM DECLARATION
//--------------------------------------------------------------------+

/* System oscillator rate and RTC oscillator rate */
const uint32_t OscRateIn = 12000000;
const uint32_t RTCOscRateIn = 32768;

/* Pin muxing configuration */
static const PINMUX_GRP_T pinmuxing[] =
{
	/* LEDs */
	{2, 19, (IOCON_FUNC0 | IOCON_MODE_INACT)},
};

static const PINMUX_GRP_T pin_usb_mux[] =
{
	// USB1 as Host
	{0, 29, (IOCON_FUNC1 | IOCON_MODE_INACT)}, // D+1
	{0, 30, (IOCON_FUNC1 | IOCON_MODE_INACT)}, // D-1
	{1, 18, (IOCON_FUNC1 | IOCON_MODE_INACT)}, // UP LED1
	{1, 19, (IOCON_FUNC2 | IOCON_MODE_INACT)}, // PPWR1

	// USB2 as Device
	{0, 31, (IOCON_FUNC1 | IOCON_MODE_INACT)}, // D+2
	{0, 13, (IOCON_FUNC1 | IOCON_MODE_INACT)}, // UP LED
	{0, 14, (IOCON_FUNC3 | IOCON_MODE_INACT)}, // CONNECT2

	/* VBUS is not connected on this board, so leave the pin at default setting. */
	/*Chip_IOCON_PinMux(LPC_IOCON, 1, 30, IOCON_MODE_INACT, IOCON_FUNC2);*/ /* USB VBUS */
};

// Invoked by startup code
void SystemInit(void)
{
  /* Enable IOCON clock */
  Chip_IOCON_SetPinMuxing(LPC_IOCON, pinmuxing, sizeof(pinmuxing) / sizeof(PINMUX_GRP_T));
  Chip_SetupXtalClocking();
}

void board_init(void)
{
  SystemCoreClockUpdate();

#if CFG_TUSB_OS == OPT_OS_NONE
  SysTick_Config(SystemCoreClock / BOARD_TICKS_HZ);
#elif CFG_TUSB_OS == OPT_OS_FREERTOS
  // If freeRTOS is used, IRQ priority is limit by max syscall ( smaller is higher )
  NVIC_SetPriority(USB_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY );
#endif

  Chip_GPIO_Init(LPC_GPIO);

  //------------- LED -------------//
  Chip_GPIO_SetPinDIROutput(LPC_GPIO, LED_PORT, LED_PIN);

  //------------- BUTTON -------------//
//  for(uint8_t i=0; i<BOARD_BUTTON_COUNT; i++) GPIO_SetDir(buttons[i].port, BIT_(buttons[i].pin), 0);


  //------------- UART -------------//

	//------------- USB -------------//
  // Port1 as Host, Port2: Device
  Chip_USB_Init();

  enum {
    USBCLK  = 0x1B // Host + Device + OTG + AHB
  };

  LPC_USB->OTGClkCtrl = USBCLK;
  while ( (LPC_USB->OTGClkSt & USBCLK) != USBCLK );

  // USB1 = host, USB2 = device
  LPC_USB->StCtrl = 0x3;

  Chip_IOCON_SetPinMuxing(LPC_IOCON, pin_usb_mux, sizeof(pin_usb_mux) / sizeof(PINMUX_GRP_T));
}


//------------- LED -------------//
void board_led_control(bool state)
{
  Chip_GPIO_SetPinState(LPC_GPIO, LED_PORT, LED_PIN, state);
}

//------------- Buttons -------------//
static bool button_read(uint8_t id)
{
//  return !BIT_TEST_( GPIO_ReadValue(buttons[id].gpio_port), buttons[id].gpio_pin ); // button is active low
}

uint32_t board_buttons(void)
{
  uint32_t result = 0;

//  for(uint8_t i=0; i<BOARD_BUTTON_COUNT; i++) result |= (button_read(i) ? BIT_(i) : 0);

  return result;
}


//------------- UART -------------//
uint8_t  board_uart_getchar(void)
{
  //return UART_ReceiveByte(BOARD_UART_PORT);
}
void board_uart_putchar(uint8_t c)
{
  //UART_Send(BOARD_UART_PORT, &c, 1, BLOCKING);
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

#endif

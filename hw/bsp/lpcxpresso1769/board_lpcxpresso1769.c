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


#ifdef BOARD_LPCXPRESSO1769

#include "bsp/board.h"

#define LED_PORT               0

static const struct {
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

/* Pin muxing configuration */
static const PINMUX_GRP_T pinmuxing[] = {
	{0,  0,   IOCON_MODE_INACT | IOCON_FUNC2},	/* TXD3 */
	{0,  1,   IOCON_MODE_INACT | IOCON_FUNC2},	/* RXD3 */
	{0,  4,   IOCON_MODE_INACT | IOCON_FUNC2},	/* CAN-RD2 */
	{0,  5,   IOCON_MODE_INACT | IOCON_FUNC2},	/* CAN-TD2 */
	{0,  22,  IOCON_MODE_INACT | IOCON_FUNC0},	/* Led 0 */
	{0,  23,  IOCON_MODE_INACT | IOCON_FUNC1},	/* ADC 0 */
	{0,  26,  IOCON_MODE_INACT | IOCON_FUNC2},	/* DAC */

	/* ENET */
	{0x1, 0,  IOCON_MODE_INACT | IOCON_FUNC1},	/* ENET_TXD0 */
	{0x1, 1,  IOCON_MODE_INACT | IOCON_FUNC1},	/* ENET_TXD1 */
	{0x1, 4,  IOCON_MODE_INACT | IOCON_FUNC1},	/* ENET_TX_EN */
	{0x1, 8,  IOCON_MODE_INACT | IOCON_FUNC1},	/* ENET_CRS */
	{0x1, 9,  IOCON_MODE_INACT | IOCON_FUNC1},	/* ENET_RXD0 */
	{0x1, 10, IOCON_MODE_INACT | IOCON_FUNC1},	/* ENET_RXD1 */
	{0x1, 14, IOCON_MODE_INACT | IOCON_FUNC1},	/* ENET_RX_ER */
	{0x1, 15, IOCON_MODE_INACT | IOCON_FUNC1},	/* ENET_REF_CLK */
	{0x1, 16, IOCON_MODE_INACT | IOCON_FUNC1},	/* ENET_MDC */
	{0x1, 17, IOCON_MODE_INACT | IOCON_FUNC1},	/* ENET_MDIO */
	{0x1, 27, IOCON_MODE_INACT | IOCON_FUNC1},	/* CLKOUT */

	/* Joystick buttons. */
	{2, 3,  IOCON_MODE_INACT | IOCON_FUNC0},	/* JOYSTICK_UP */
	{0, 15, IOCON_MODE_INACT | IOCON_FUNC0},	/* JOYSTICK_DOWN */
	{2, 4,  IOCON_MODE_INACT | IOCON_FUNC0},	/* JOYSTICK_LEFT */
	{0, 16, IOCON_MODE_INACT | IOCON_FUNC0},	/* JOYSTICK_RIGHT */
	{0, 17, IOCON_MODE_INACT | IOCON_FUNC0},	/* JOYSTICK_PRESS */

};

/* System oscillator rate and RTC oscillator rate */
const uint32_t OscRateIn = 12000000;
const uint32_t RTCOscRateIn = 32768;

// Invoked by startup code
void SystemInit(void)
{
	/* Enable IOCON clock */
  Chip_IOCON_Init(LPC_IOCON);
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
  Chip_GPIO_SetPinDIROutput(LPC_GPIO, LED_PORT, BOARD_LED0);

  //------------- BUTTON -------------//
//  for(uint8_t i=0; i<BOARD_BUTTON_COUNT; i++) GPIO_SetDir(buttons[i].port, BIT_(buttons[i].pin), 0);

#if TUSB_OPT_DEVICE_ENABLED
  //------------- USB Device -------------//
  // VBUS sense is wrongly connected to P0_5 (instead of P1_30). So we need to always pull P1_30 to high
  // so that USB device block can work. However, Device Controller (thus tinyusb) cannot able to determine
  // if device is disconnected or not
  PINSEL_ConfigPin( &(PINSEL_CFG_Type) {
      .Portnum = 1, .Pinnum = 30,
      .Funcnum = 2, .Pinmode = PINSEL_PINMODE_PULLUP} );

  //P0_21 instead of P2_9 as USB connect
#endif

#if 0
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
#endif
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
  (void) id;
  Chip_GPIO_SetPinState(LPC_GPIO, LED_PORT, BOARD_LED0, state);
}

//--------------------------------------------------------------------+
// BUTTONS
//--------------------------------------------------------------------+
static bool button_read(uint8_t id)
{
//  return !BIT_TEST_( GPIO_ReadValue(buttons[id].port), buttons[id].pin ); // button is active low
  return false;
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
//  UART_Send(BOARD_UART_PORT, &c, 1, BLOCKING);
}

uint8_t  board_uart_getchar(void)
{
//  return UART_ReceiveByte(BOARD_UART_PORT);
}

#endif

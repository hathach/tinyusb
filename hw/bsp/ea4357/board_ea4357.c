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

#ifdef BOARD_EA4357

#include "../board.h"
#include "pca9532.h"
#include "tusb.h"

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

/*------------------------------------------------------------------*/
/* BOARD API
 *------------------------------------------------------------------*/

/* System configuration variables used by chip driver */
const uint32_t ExtRateIn = 0;
const uint32_t OscRateIn = 12000000;

static const PINMUX_GRP_T pinmuxing[] =
{
  // USB

  /*  I2S  */
  {0x3,  0,  (SCU_PINIO_FAST | SCU_MODE_FUNC2)}, //I2S0_TX_CLK
  {0xC, 12,  (SCU_PINIO_FAST | SCU_MODE_FUNC6)}, //I2S0_TX_SDA
  {0xC, 13,  (SCU_PINIO_FAST | SCU_MODE_FUNC6)}, //I2S0_TX_WS
  {0x6,  0,  (SCU_PINIO_FAST | SCU_MODE_FUNC4)}, //I2S0_RX_SCK
  {0x6,  1,  (SCU_PINIO_FAST | SCU_MODE_FUNC3)}, //I2S0_RX_WS
  {0x6,  2,  (SCU_PINIO_FAST | SCU_MODE_FUNC3)}, //I2S0_RX_SDA
};

/* Pin clock mux values, re-used structure, value in first index is meaningless */
static const PINMUX_GRP_T pinclockmuxing[] =
{
  {0, 0,  (SCU_MODE_INACT | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_HIGHSPEEDSLEW_EN | SCU_MODE_FUNC0)},
  {0, 1,  (SCU_MODE_INACT | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_HIGHSPEEDSLEW_EN | SCU_MODE_FUNC0)},
  {0, 2,  (SCU_MODE_INACT | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_HIGHSPEEDSLEW_EN | SCU_MODE_FUNC0)},
  {0, 3,  (SCU_MODE_INACT | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_HIGHSPEEDSLEW_EN | SCU_MODE_FUNC0)},
};

// Invoked by startup code
void SystemInit(void)
{
	/* Setup system level pin muxing */
	Chip_SCU_SetPinMuxing(pinmuxing, sizeof(pinmuxing) / sizeof(PINMUX_GRP_T));

	/* Clock pins only, group field not used */
	for (int i = 0; i < (sizeof(pinclockmuxing) / sizeof(pinclockmuxing[0])); i++)
	{
		Chip_SCU_ClockPinMuxSet(pinclockmuxing[i].pinnum, pinclockmuxing[i].modefunc);
	}

  Chip_SetupXtalClocking();
}

void board_init(void)
{
  SystemCoreClockUpdate();

#if CFG_TUSB_OS == OPT_OS_NONE
  SysTick_Config( SystemCoreClock / BOARD_TICKS_HZ );
#endif

  Chip_GPIO_Init(LPC_GPIO_PORT);

  //------------- LED -------------//
  /* Init I2C */
  Chip_SCU_I2C0PinConfig(I2C0_STANDARD_FAST_MODE);
  Chip_I2C_Init(I2C0);
  Chip_I2C_SetClockRate(I2C0, 100000);
  Chip_I2C_SetMasterEventHandler(I2C0, Chip_I2C_EventHandlerPolling);

  pca9532_init();

#if 0
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
#endif

  //------------- USB -------------//
  enum {
    USBMODE_DEVICE = 2,
    USBMODE_HOST   = 3
  };

  enum {
    USBMODE_VBUS_LOW  = 0,
    USBMODE_VBUS_HIGH = 1
  };

  /* USB0
   * For USB Device operation; insert jumpers in position 1-2 in JP17/JP18/JP19. GPIO28 controls USB
   * connect functionality and LED32 lights when the USB Device is connected. SJ4 has pads 1-2 shorted
   * by default. LED33 is controlled by GPIO27 and signals USB-up state. GPIO54 is used for VBUS
   * sensing.
   * For USB Host operation; insert jumpers in position 2-3 in JP17/JP18/JP19. USB Host power is
   * controlled via distribution switch U20 (found in schematic page 11). Signal GPIO26 is active low and
   * enables +5V on VBUS2. LED35 light whenever +5V is present on VBUS2. GPIO55 is connected to
   * status feedback from the distribution switch. GPIO54 is used for VBUS sensing. 15Kohm pull-down
   * resistors are always active
   */
#if CFG_TUSB_RHPORT0_MODE
  Chip_USB0_Init();

  // Reset controller
  LPC_USB0->USBCMD_D |= 0x02;
  while( LPC_USB0->USBCMD_D & 0x02 ) {}

  // Set mode
  #if CFG_TUSB_RHPORT0_MODE & OPT_MODE_HOST
    LPC_USB0->USBMODE_H = USBMODE_HOST | (USBMODE_VBUS_HIGH << 5);

    LPC_USB0->PORTSC1_D |= (1<<24); // FIXME force full speed for debugging
  #else // TODO OTG
    LPC_USB0->USBMODE_D = USBMODE_DEVICE;
    LPC_USB0->OTGSC = (1<<3) | (1<<0) /*| (1<<16)| (1<<24)| (1<<25)| (1<<26)| (1<<27)| (1<<28)| (1<<29)| (1<<30)*/;
  #endif
#endif

  /* USB1
   * When USB channel #1 is used as USB Host, 15Kohm pull-down resistors are needed on the USB data
   * signals. These are activated inside the USB OTG chip (U31), and this has to be done via the I2C
   * interface of GPIO52/GPIO53.
   * J20 is the connector to use when USB Host is used. In order to provide +5V to the external USB
   * device connected to this connector (J20), channel A of U20 must be enabled. It is enabled by default
   * since SJ5 is normally connected between pin 1-2. LED34 lights green when +5V is available on J20.
   * JP15 shall not be inserted. JP16 has no effect
   *
   * When USB channel #1 is used as USB Device, a 1.5Kohm pull-up resistor is needed on the USB DP
   * data signal. There are two methods to create this. JP15 is inserted and the pull-up resistor is always
   * enabled. Alternatively, the pull-up resistor is activated inside the USB OTG chip (U31), and this has to
   * be done via the I2C interface of GPIO52/GPIO53. In the latter case, JP15 shall not be inserted.
   * J19 is the connector to use when USB Device is used. Normally it should be a USB-B connector for
   * creating a USB Device interface, but the mini-AB connector can also be used in this case. The status
   * of VBUS can be read via U31.
   * JP16 shall not be inserted.
   */
#if CFG_TUSB_RHPORT1_MODE
  Chip_USB1_Init();

  // Reset controller
  LPC_USB1->USBCMD_D |= 0x02;
  while( LPC_USB1->USBCMD_D & 0x02 ) {}

  // Set mode
  #if CFG_TUSB_RHPORT1_MODE & OPT_MODE_HOST
    LPC_USB1->USBMODE_H = USBMODE_HOST | (USBMODE_VBUS_HIGH << 5);
  #else // TODO OTG
    LPC_USB1->USBMODE_D = USBMODE_DEVICE;
  #endif

  // USB1 as fullspeed
  LPC_USB1->PORTSC1_D |= (1<<24);
#endif

  // USB0 Vbus Power: P2_3 on EA4357 channel B U20 GPIO26 active low (base board)
  Chip_SCU_PinMuxSet(2, 3, SCU_MODE_PULLUP | SCU_MODE_INBUFF_EN | SCU_MODE_FUNC7);

  #if CFG_TUSB_RHPORT0_MODE & OPT_MODE_DEVICE
  // P9_5 (GPIO5[18]) (GPIO28 on oem base) as USB connect, active low.
  Chip_SCU_PinMuxSet(9, 5, SCU_MODE_PULLDOWN | SCU_MODE_FUNC4);
  Chip_GPIO_SetPinDIROutput(LPC_GPIO_PORT, 5, 18);
  #endif

  // USB1 Power: EA4357 channel A U20 is enabled by SJ5 connected to pad 1-2, no more action required
  // TODO Remove R170, R171, solder a pair of 15K to USB1 D+/D- to test with USB1 Host
}

// LED
void board_led_control(bool state)
{
  if (state)
  {
    pca9532_setLeds( LED1, 0 );
  }else
  {
    pca9532_setLeds( 0, LED1);
  }
}

//--------------------------------------------------------------------+
// BUTTONS
//--------------------------------------------------------------------+
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

//--------------------------------------------------------------------+
// UART
//--------------------------------------------------------------------+
uint8_t  board_uart_getchar(void)
{
  //return UART_ReceiveByte(BOARD_UART_PORT);
}
void board_uart_putchar(uint8_t c)
{
  //UART_Send(BOARD_UART_PORT, &c, 1, BLOCKING);
}

#endif

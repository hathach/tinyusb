/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * This file is part of the TinyUSB stack.
 */

#include "chip.h"
#include "../board.h"
#include "pca9532.h"

#define UART_DEV        LPC_USART0
#define UART_PORT       0x0f
#define UART_PIN_TX     10
#define UART_PIN_RX     11

// P9_1 joystick down
#define BUTTON_PORT     4
#define BUTTON_PIN      13

//static const struct {
//  uint8_t mux_port;
//  uint8_t mux_pin;
//
//  uint8_t gpio_port;
//  uint8_t gpio_pin;
//}buttons[] =
//{
//    {0x0a, 3, 4, 10 }, // Joystick up
//    {0x09, 1, 4, 13 }, // Joystick down
//    {0x0a, 2, 4, 9  }, // Joystick left
//    {0x09, 0, 4, 12 }, // Joystick right
//    {0x0a, 1, 4, 8  }, // Joystick press
//    {0x02, 7, 0, 7  }, // SW6
//};

#ifdef BOARD_TUD_RHPORT
  #define PORT_SUPPORT_DEVICE(_n)  (BOARD_TUD_RHPORT == _n)
#else
  #define PORT_SUPPORT_DEVICE(_n)  0
#endif

#ifdef BOARD_TUH_RHPORT
  #define PORT_SUPPORT_HOST(_n)    (BOARD_TUH_RHPORT == _n)
#else
  #define PORT_SUPPORT_HOST(_n)    0
#endif

/*------------------------------------------------------------------*/
/* BOARD API
 *------------------------------------------------------------------*/

/* System configuration variables used by chip driver */
const uint32_t OscRateIn = 12000000;
const uint32_t ExtRateIn = 0;

static const PINMUX_GRP_T pinmuxing[] =
{
  // Button ( Joystick down )
  {0x9, 1,  (SCU_MODE_INBUFF_EN | SCU_MODE_INACT | SCU_MODE_FUNC0 | SCU_MODE_PULLUP)},

  // UART
  {UART_PORT, UART_PIN_TX, SCU_MODE_PULLDOWN | SCU_MODE_FUNC1},
  {UART_PORT, UART_PIN_RX, SCU_MODE_INACT | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_FUNC1},

  // USB
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
#ifdef __USE_LPCOPEN
	extern void (* const g_pfnVectors[])(void);
  unsigned int *pSCB_VTOR = (unsigned int *) 0xE000ED08;
	*pSCB_VTOR = (unsigned int) g_pfnVectors;

#if __FPU_USED == 1
	fpuInit();
#endif
#endif // __USE_LPCOPEN

	/* Setup system level pin muxing */
	Chip_SCU_SetPinMuxing(pinmuxing, sizeof(pinmuxing) / sizeof(PINMUX_GRP_T));

	/* Clock pins only, group field not used */
	for (int i = 0; i <(int)  (sizeof(pinclockmuxing) / sizeof(pinclockmuxing[0])); i++)
	{
		Chip_SCU_ClockPinMuxSet(pinclockmuxing[i].pinnum, pinclockmuxing[i].modefunc);
	}

  Chip_SetupXtalClocking();
}

void board_init(void)
{
  SystemCoreClockUpdate();

#if CFG_TUSB_OS == OPT_OS_NONE
  // 1ms tick timer
  SysTick_Config(SystemCoreClock / 1000);
#elif CFG_TUSB_OS == OPT_OS_FREERTOS
  // If freeRTOS is used, IRQ priority is limit by max syscall ( smaller is higher )
  NVIC_SetPriority(USB0_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);
#endif

  Chip_GPIO_Init(LPC_GPIO_PORT);

  // LED via pca9532 I2C
  Chip_SCU_I2C0PinConfig(I2C0_STANDARD_FAST_MODE);
  Chip_I2C_Init(I2C0);
  Chip_I2C_SetClockRate(I2C0, 100000);
  Chip_I2C_SetMasterEventHandler(I2C0, Chip_I2C_EventHandlerPolling);

  pca9532_init();

  // Button
  Chip_GPIO_SetPinDIRInput(LPC_GPIO_PORT, BUTTON_PORT, BUTTON_PIN);

  //------------- UART -------------//
	Chip_UART_Init(UART_DEV);
	Chip_UART_SetBaud(UART_DEV, CFG_BOARD_UART_BAUDRATE);
	Chip_UART_ConfigData(UART_DEV, UART_LCR_WLEN8 | UART_LCR_SBS_1BIT | UART_LCR_PARITY_DIS);
	Chip_UART_TXEnable(UART_DEV);

  //------------- USB -------------//
  enum {
    USBMODE_DEVICE = 2,
    USBMODE_HOST   = 3
  };

  enum {
    USBMODE_VBUS_LOW  = 0,
    USBMODE_VBUS_HIGH = 1
  };

  /* From EA4357 user manual
   *
   * USB0 Device operation:
   * - Insert jumpers in position 1-2 in JP17/JP18/JP19.
   * - GPIO28 controls USB connect functionality
   * - LED32 lights when the USB Device is connected. SJ4 has pads 1-2 shorted by default.
   * - LED33 is controlled by GPIO27 and signals USB-up state. GPIO54 is used for VBUS
   * sensing.
   *
   * USB0 Host operation:
   * - insert jumpers in position 2-3 in JP17/JP18/JP19.
   * - USB Host power is controlled via distribution switch U20 (found in schematic page 11).
   * - Signal GPIO26 is active low and enables +5V on VBUS2.
   * - LED35 light whenever +5V is present on VBUS2.
   * - GPIO55 is connected to status feedback from the distribution switch.
   * - GPIO54 is used for VBUS sensing. 15Kohm pull-down resistors are always active
   *
   * Note:
   * - Insert jumpers in position 2-3 in JP17/JP18/JP19
   * - Insert jumpers in JP31 (OTG)
   */
#if PORT_SUPPORT_DEVICE(0) || PORT_SUPPORT_HOST(0)
  Chip_USB0_Init();
#endif

  /* From EA4357 user manual
   *
   * For USB1 Device:
   * - a 1.5Kohm pull-up resistor is needed on the USB DP data signal. There are two methods to create this.
   * JP15 is inserted and the pull-up resistor is always enabled. Alternatively, the pull-up resistor is activated
   * inside the USB OTG chip (U31), and this has to be done via the I2C interface of GPIO52/GPIO53. In the latter case,
   * JP15 shall not be inserted.
   * - J19 is the connector to use when USB Device is used. Normally it should be a USB-B connector for
   * creating a USB Device interface, but the mini-AB connector can also be used in this case. The status
   * of VBUS can be read via U31.
   * - JP16 shall not be inserted.
   *
   * For USB1 Host:
   * - 15Kohm pull-down resistors are needed on the USB data signals. These are activated inside the USB OTG chip (U31),
   * and this has to be done via the I2C interface of GPIO52/GPIO53.
   * - J20 is the connector to use when USB Host is used. In order to provide +5V to the external USB
   * device connected to this connector (J20), channel A of U20 must be enabled. It is enabled by default
   * since SJ5 is normally connected between pin 1-2.
   * - LED34 lights green when +5V is available on J20.
   * - JP15 shall not be inserted. JP16 has no effect
   */
#if PORT_SUPPORT_DEVICE(1) || PORT_SUPPORT_HOST(1)
  Chip_USB1_Init();
#endif

  // USB0 Vbus Power: P2_3 on EA4357 channel B U20 GPIO26 active low (base board)
  Chip_SCU_PinMuxSet(2, 3, SCU_MODE_PULLUP | SCU_MODE_INBUFF_EN | SCU_MODE_FUNC7);

  #if PORT_SUPPORT_DEVICE(0)
  // P9_5 (GPIO5[18]) (GPIO28 on oem base) as USB connect, active low.
  Chip_SCU_PinMuxSet(9, 5, SCU_MODE_PULLDOWN | SCU_MODE_FUNC4);
  Chip_GPIO_SetPinDIROutput(LPC_GPIO_PORT, 5, 18);
  #endif

  // USB1 Power: EA4357 channel A U20 is enabled by SJ5 connected to pad 1-2, no more action required
  // TODO Remove R170, R171, solder a pair of 15K to USB1 D+/D- to test with USB1 Host
}

//--------------------------------------------------------------------+
// USB Interrupt Handler
//--------------------------------------------------------------------+
void USB0_IRQHandler(void)
{
  #if PORT_SUPPORT_DEVICE(0)
    tud_int_handler(0);
  #endif

  #if PORT_SUPPORT_HOST(0)
    tuh_int_handler(0);
  #endif
}

void USB1_IRQHandler(void)
{
  #if PORT_SUPPORT_DEVICE(1)
    tud_int_handler(1);
  #endif

  #if PORT_SUPPORT_HOST(1)
    tuh_int_handler(1);
  #endif
}

//--------------------------------------------------------------------+
// Board porting API
//--------------------------------------------------------------------+

void board_led_write(bool state)
{
  if (state)
  {
    pca9532_setLeds( LED1, 0 );
  }else
  {
    pca9532_setLeds( 0, LED1);
  }
}

uint32_t board_button_read(void)
{
  // active low
  return Chip_GPIO_GetPinState(LPC_GPIO_PORT, BUTTON_PORT, BUTTON_PIN) ? 0 : 1;
}

int board_uart_read(uint8_t* buf, int len)
{
  return Chip_UART_Read(UART_DEV, buf, len);
}

int board_uart_write(void const * buf, int len)
{
  uint8_t const* buf8 = (uint8_t const*) buf;
  for(int i=0; i<len; i++)
  {
    while ((Chip_UART_ReadLineStatus(UART_DEV) & UART_LSR_THRE) == 0) {}
    Chip_UART_SendByte(UART_DEV, buf8[i]);
  }

  return len;
}

#if CFG_TUSB_OS == OPT_OS_NONE
volatile uint32_t system_ticks = 0;
void SysTick_Handler (void)
{
  system_ticks++;
}

uint32_t board_millis(void)
{
  return system_ticks;
}
#endif

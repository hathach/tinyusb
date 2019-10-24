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

#define LED_PORT              1
#define LED_PIN               12
#define LED_STATE_ON          0

#define BUTTON_PORT           0
#define BUTTON_PIN            7
#define BUTTON_STATE_ACTIVE   0

#define BOARD_UART_PORT           LPC_USART0
#define BOARD_UART_PIN_PORT       0x0f
#define BOARD_UART_PIN_TX         10 // PF.10 : UART0_TXD
#define BOARD_UART_PIN_RX         11 // PF.11 : UART0_RXD

/*------------------------------------------------------------------*/
/* BOARD API
 *------------------------------------------------------------------*/

/* System configuration variables used by chip driver */
const uint32_t OscRateIn = 12000000;
const uint32_t ExtRateIn = 0;

static const PINMUX_GRP_T pinmuxing[] =
{
  // LED P2.12 as GPIO 1.12
  {2, 11, (SCU_MODE_INBUFF_EN | SCU_MODE_PULLDOWN | SCU_MODE_FUNC0)},

  // Button P2.7 as GPIO 0.7
  {2, 7,  (SCU_MODE_PULLUP | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_FUNC0)},

  // USB
  {2, 6, (SCU_MODE_PULLUP | SCU_MODE_INBUFF_EN | SCU_MODE_FUNC4)}, // USB1_PWR_EN
  {2, 5, (SCU_MODE_INACT | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_FUNC2)}, // USB1_VBUS
  {1, 7, (SCU_MODE_PULLUP | SCU_MODE_INBUFF_EN | SCU_MODE_FUNC4)}, // USB0_PWRN_EN

  // SPIFI
	{3, 3,  (SCU_PINIO_FAST | SCU_MODE_FUNC3)},	/* SPIFI CLK */
	{3, 4,  (SCU_PINIO_FAST | SCU_MODE_FUNC3)},	/* SPIFI D3 */
	{3, 5,  (SCU_PINIO_FAST | SCU_MODE_FUNC3)},	/* SPIFI D2 */
	{3, 6,  (SCU_PINIO_FAST | SCU_MODE_FUNC3)},	/* SPIFI D1 */
	{3, 7,  (SCU_PINIO_FAST | SCU_MODE_FUNC3)},	/* SPIFI D0 */
	{3, 8,  (SCU_PINIO_FAST | SCU_MODE_FUNC3)}	/* SPIFI CS/SSEL */
};

// Invoked by startup code
extern void (* const g_pfnVectors[])(void);
void SystemInit(void)
{
  // Remap isr vector
	*((uint32_t *) 0xE000ED08) = (uint32_t) &g_pfnVectors;

	// Set up pinmux
	Chip_SCU_SetPinMuxing(pinmuxing, sizeof(pinmuxing) / sizeof(PINMUX_GRP_T));

	//------------- Set up clock -------------//
	Chip_Clock_SetBaseClock(CLK_BASE_SPIFI, CLKIN_IRC, true, false);	// change SPIFI to IRC during clock programming
	LPC_SPIFI->CTRL |= SPIFI_CTRL_FBCLK(1);								            // and set FBCLK in SPIFI controller

	Chip_SetupCoreClock(CLKIN_CRYSTAL, MAX_CLOCK_FREQ, true);

	/* Reset and enable 32Khz oscillator */
	LPC_CREG->CREG0 &= ~((1 << 3) | (1 << 2));
	LPC_CREG->CREG0 |= (1 << 1) | (1 << 0);

	/* Setup a divider E for main PLL clock switch SPIFI clock to that divider.
	   Divide rate is based on CPU speed and speed of SPI FLASH part. */
#if (MAX_CLOCK_FREQ > 180000000)
	Chip_Clock_SetDivider(CLK_IDIV_E, CLKIN_MAINPLL, 5);
#else
	Chip_Clock_SetDivider(CLK_IDIV_E, CLKIN_MAINPLL, 4);
#endif
	Chip_Clock_SetBaseClock(CLK_BASE_SPIFI, CLKIN_IDIVE, true, false);

	/* Setup system base clocks and initial states. This won't enable and
	   disable individual clocks, but sets up the base clock sources for
	   each individual peripheral clock. */
	Chip_Clock_SetBaseClock(CLK_BASE_USB1, CLKIN_IDIVD, true, true);
}

void board_init(void)
{
  SystemCoreClockUpdate();

#if CFG_TUSB_OS == OPT_OS_NONE
  // 1ms tick timer
  SysTick_Config(SystemCoreClock / 1000);
#elif CFG_TUSB_OS == OPT_OS_FREERTOS
  // If freeRTOS is used, IRQ priority is limit by max syscall ( smaller is higher )
  //NVIC_SetPriority(USB0_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY );
#endif

  Chip_GPIO_Init(LPC_GPIO_PORT);

  // LED
  Chip_GPIO_SetPinDIROutput(LPC_GPIO_PORT, LED_PORT, LED_PIN);

  // Button
  Chip_GPIO_SetPinDIRInput(LPC_GPIO_PORT, BUTTON_PORT, BUTTON_PIN);

#if 0
  //------------- UART -------------//
  scu_pinmux(BOARD_UART_PIN_PORT, BOARD_UART_PIN_TX, MD_PDN, FUNC1);
  scu_pinmux(BOARD_UART_PIN_PORT, BOARD_UART_PIN_RX, MD_PLN | MD_EZI | MD_ZI, FUNC1);

  UART_CFG_Type UARTConfigStruct;
  UART_ConfigStructInit(&UARTConfigStruct);
  UARTConfigStruct.Baud_rate   = CFG_BOARD_UART_BAUDRATE;
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

//	Chip_GPIO_SetPinDIROutput(LPC_GPIO_PORT, 5, 6);							/* GPIO5[6] = USB1_PWR_EN */
//	Chip_GPIO_SetPinState(LPC_GPIO_PORT, 5, 6, true);							/* GPIO5[6] output high */
#endif
}

//--------------------------------------------------------------------+
// Board porting API
//--------------------------------------------------------------------+

void board_led_write(bool state)
{
  Chip_GPIO_SetPinState(LPC_GPIO_PORT, LED_PORT, LED_PIN, state ? LED_STATE_ON : (1-LED_STATE_ON));
}

uint32_t board_button_read(void)
{
  return BUTTON_STATE_ACTIVE == Chip_GPIO_GetPinState(LPC_GPIO_PORT, BUTTON_PORT, BUTTON_PIN);
}

int board_uart_read(uint8_t* buf, int len)
{
  //return UART_ReceiveByte(BOARD_UART_PORT);
  (void) buf; (void) len;
  return 0;
}

int board_uart_write(void const * buf, int len)
{
  //UART_Send(BOARD_UART_PORT, &c, 1, BLOCKING);
  (void) buf; (void) len;
  return 0;
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

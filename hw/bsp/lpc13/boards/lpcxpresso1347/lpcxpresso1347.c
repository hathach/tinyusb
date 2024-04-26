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
#include "bsp/board_api.h"

//--------------------------------------------------------------------+
// Forward USB interrupt events to TinyUSB IRQ Handler
//--------------------------------------------------------------------+
void USB_IRQHandler(void)
{
  tud_int_handler(0);
}

//--------------------------------------------------------------------+
// MACRO TYPEDEF CONSTANT ENUM
//--------------------------------------------------------------------+
#define LED_PORT      0
#define LED_PIN       7

// Joytick Down if connected to LPCXpresso Base board
#define BUTTON_PORT   1
#define BUTTON_PIN    20

//static const struct {
//  uint8_t port;
//  uint8_t pin;
//} buttons[] =
//{
//    {1, 22 }, // Joystick up
//    {1, 20 }, // Joystick down
//    {1, 23 }, // Joystick left
//    {1, 21 }, // Joystick right
//    {1, 19 }, // Joystick press
//    {0, 1  }, // SW3
//};

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
  // 1ms tick timer
  SysTick_Config(SystemCoreClock / 1000);
#elif CFG_TUSB_OS == OPT_OS_FREERTOS
  // If freeRTOS is used, IRQ priority is limit by max syscall ( smaller is higher )
  NVIC_SetPriority(USB0_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY );
#endif

  Chip_GPIO_Init(LPC_GPIO_PORT);

  // LED
  Chip_GPIO_SetPinDIROutput(LPC_GPIO_PORT, LED_PORT, LED_PIN);

  // Button
  Chip_GPIO_SetPinDIRInput(LPC_GPIO_PORT, BUTTON_PORT, BUTTON_PIN);

  // USB: Setup PLL clock, and power
  Chip_USB_Init();
}

//--------------------------------------------------------------------+
// Board porting API
//--------------------------------------------------------------------+

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

void board_led_write(bool state)
{
  Chip_GPIO_SetPinState(LPC_GPIO_PORT, LED_PORT, LED_PIN, state);
}

uint32_t board_button_read(void)
{
  // active low
  return Chip_GPIO_GetPinState(LPC_GPIO_PORT, BUTTON_PORT, BUTTON_PIN) ? 0 : 1;
}

int board_uart_read(uint8_t* buf, int len)
{
  (void) buf; (void) len;
  return 0;
}

int board_uart_write(void const * buf, int len)
{
  (void) buf; (void) len;
  return 0;
}

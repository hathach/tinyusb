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

#define LED_PORT      2
#define LED_PIN       19

#define BUTTON_PORT   2
#define BUTTON_PIN    10

/* System oscillator rate and RTC oscillator rate */
const uint32_t OscRateIn = 12000000;
const uint32_t RTCOscRateIn = 32768;

/* Pin muxing configuration */
static const PINMUX_GRP_T pinmuxing[] =
{
  // LED
  {2, 19, (IOCON_FUNC0 | IOCON_MODE_INACT)},

  // Button
  {2, 10, (IOCON_FUNC0 | IOCON_MODE_INACT | IOCON_MODE_PULLUP)},
};

static const PINMUX_GRP_T pin_usb_mux[] =
{
  // USB1 as Host
  {0, 29, (IOCON_FUNC1 | IOCON_MODE_INACT)}, // D+1
  {0, 30, (IOCON_FUNC1 | IOCON_MODE_INACT)}, // D-1
  {1, 18, (IOCON_FUNC1 | IOCON_MODE_INACT)}, // UP LED1
  {1, 19, (IOCON_FUNC2 | IOCON_MODE_INACT)}, // PPWR1
//  {2, 14, (IOCON_FUNC2 | IOCON_MODE_INACT)}, // VBUS1
//  {2, 15, (IOCON_FUNC2 | IOCON_MODE_INACT)}, // OVRCR1

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
  Chip_IOCON_Init(LPC_IOCON);
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
  NVIC_SetPriority(USB_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY );
#endif

  Chip_GPIO_Init(LPC_GPIO);

  // LED
  Chip_GPIO_SetPinDIROutput(LPC_GPIO, LED_PORT, LED_PIN);

  // Button
  Chip_GPIO_SetPinDIRInput(LPC_GPIO, BUTTON_PORT, BUTTON_PIN);

  // UART

  //------------- USB -------------//
  Chip_IOCON_SetPinMuxing(LPC_IOCON, pin_usb_mux, sizeof(pin_usb_mux) / sizeof(PINMUX_GRP_T));

  // Port1 as Host, Port2: Device
  Chip_USB_Init();

  enum {
    USBCLK  = 0x1B // Host + Device + OTG + AHB
  };

  LPC_USB->OTGClkCtrl = USBCLK;
  while ( (LPC_USB->OTGClkSt & USBCLK) != USBCLK ) {}

  // USB1 = host, USB2 = device
  LPC_USB->StCtrl = 0x3;
}

//--------------------------------------------------------------------+
// Board porting API
//--------------------------------------------------------------------+

void board_led_write(bool state)
{
  Chip_GPIO_SetPinState(LPC_GPIO, LED_PORT, LED_PIN, state);
}

uint32_t board_button_read(void)
{
  // active low
  return Chip_GPIO_GetPinState(LPC_GPIO, BUTTON_PORT, BUTTON_PIN) ? 0 : 1;
}

int board_uart_read(uint8_t* buf, int len)
{
  //return UART_ReceiveByte(BOARD_UART_PORT);
  (void) buf;
  (void) len;
  return 0;
}

int board_uart_write(void const * buf, int len)
{
  //UART_Send(BOARD_UART_PORT, &c, 1, BLOCKING);
  (void) buf;
  (void) len;
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

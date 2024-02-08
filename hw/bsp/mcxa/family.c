/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2018, hathach (tinyusb.org)
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

#include "bsp/board_api.h"
#include "fsl_device_registers.h"
#include "fsl_gpio.h"
#include "fsl_lpuart.h"
#include "board.h"

#include "pin_mux.h"
#include "clock_config.h"


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

//--------------------------------------------------------------------+
// MACRO TYPEDEF CONSTANT ENUM
//--------------------------------------------------------------------+

//--------------------------------------------------------------------+
// Forward USB interrupt events to TinyUSB IRQ Handler
//--------------------------------------------------------------------+
void USB0_FS_IRQHandler(void)
{
  tud_int_handler(0);
}

void board_init(void)
{
  BOARD_InitPins();
  BOARD_InitBootClocks();
  CLOCK_SetupExtClocking(XTAL0_CLK_HZ);

  // 1ms tick timer
  SysTick_Config(SystemCoreClock / 1000);

  // LED
  CLOCK_EnableClock(LED_CLK);
  gpio_pin_config_t LED_RED_config = {
      .pinDirection = kGPIO_DigitalOutput,
      .outputLogic = 0U
  };
  
  /* Initialize GPIO functionality on pin PIO3_12 (pin 38)  */
  GPIO_PinInit(LED_GPIO, LED_PIN, &LED_RED_config);
  board_led_write(1);

  // Button
#ifdef BUTTON_GPIO
  CLOCK_EnableClock(BUTTON_CLK);
  gpio_pin_config_t const button_config = { kGPIO_DigitalInput, 0};
  GPIO_PinInit(BUTTON_GPIO, BUTTON_PIN, &button_config);
#endif

#ifdef UART_DEV

  CLOCK_SetClockDiv(kCLOCK_DivLPUART0, 1u);
  CLOCK_AttachClk(kFRO12M_to_LPUART0);
  RESET_PeripheralReset(kLPUART0_RST_SHIFT_RSTn);

  lpuart_config_t uart_config;
  LPUART_GetDefaultConfig(&uart_config);
  uart_config.baudRate_Bps = 115200;
  uart_config.enableTx     = true;
  uart_config.enableRx     = true; 
  LPUART_Init(UART_DEV, &uart_config, 12000000u);


#endif

  // USB VBUS
  /* PORT0 PIN22 configured as USB0_VBUS */

#if PORT_SUPPORT_DEVICE(0)
  // Port0 is Full Speed
  //CLOCK_EnableClock(kCLOCK_Usb0Ram);
  //CLOCK_EnableClock(kCLOCK_Usb0Fs);
  RESET_PeripheralReset(kUSB0_RST_SHIFT_RSTn);
  CLOCK_EnableUsbfsClock();
#endif

}

//--------------------------------------------------------------------+
// Board porting API
//--------------------------------------------------------------------+

void board_led_write(bool state)
{
  GPIO_PinWrite(LED_GPIO, LED_PIN, state ? LED_STATE_ON : (1-LED_STATE_ON));
}

uint32_t board_button_read(void)
{
#ifdef BUTTON_GPIO
  return BUTTON_STATE_ACTIVE == GPIO_PinRead(BUTTON_GPIO, BUTTON_PIN);
#else
  return 1;
#endif
}

int board_uart_read(uint8_t* buf, int len)
{
  (void) buf; (void) len;
  return 0;
}

int board_uart_write(void const * buf, int len)
{
#ifdef UART_DEV
  LPUART_WriteBlocking(UART_DEV, (uint8_t const *) buf, len);
  return len;
#else
  (void) buf; (void) len;
  return 0;
#endif
}

#if CFG_TUSB_OS == OPT_OS_NONE
volatile uint32_t system_ticks = 0;
void SysTick_Handler(void)
{
  system_ticks++;
}

uint32_t board_millis(void)
{
  return system_ticks;
}
#endif

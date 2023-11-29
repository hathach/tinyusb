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
// USB Interrupt Handler
//--------------------------------------------------------------------+
void USB_IRQHandler(void)
{
  #if CFG_TUD_ENABLED
    tud_int_handler(0);
  #endif

  #if CFG_TUH_ENABLED
    tuh_int_handler(0, true);
  #endif
}

//--------------------------------------------------------------------+
// MACRO TYPEDEF CONSTANT ENUM
//--------------------------------------------------------------------+
#define LED_PORT              0
#define LED_PIN               22
#define LED_STATE_ON          1

// JOYSTICK_DOWN if using LPCXpresso Base Board
#define BUTTON_PORT           0
#define BUTTON_PIN            15
#define BUTTON_STATE_ACTIVE   0

#define BOARD_UART_PORT   LPC_UART3

/* System oscillator rate and RTC oscillator rate */
const uint32_t OscRateIn = 12000000;
const uint32_t RTCOscRateIn = 32768;

/* Pin muxing configuration */
static const PINMUX_GRP_T pinmuxing[] =
{
  {0,  0,   IOCON_MODE_INACT | IOCON_FUNC2},	/* TXD3 */
  {0,  1,   IOCON_MODE_INACT | IOCON_FUNC2},	/* RXD3 */
  {LED_PORT, LED_PIN,  IOCON_MODE_INACT | IOCON_FUNC0},	/* Led 0 */

  /* Joystick buttons. */
//  {2, 3,  IOCON_MODE_INACT | IOCON_FUNC0},	/* JOYSTICK_UP */
  {BUTTON_PORT, BUTTON_PIN, IOCON_FUNC0 | IOCON_MODE_PULLUP},	/* JOYSTICK_DOWN */
//  {2, 4,  IOCON_MODE_INACT | IOCON_FUNC0},	/* JOYSTICK_LEFT */
//  {0, 16, IOCON_MODE_INACT | IOCON_FUNC0},	/* JOYSTICK_RIGHT */
//  {0, 17, IOCON_MODE_INACT | IOCON_FUNC0},	/* JOYSTICK_PRESS */
};

static const PINMUX_GRP_T pin_usb_mux[] =
{
  {0, 29, IOCON_MODE_INACT | IOCON_FUNC1}, // D+
  {0, 30, IOCON_MODE_INACT | IOCON_FUNC1}, // D-
  {2,  9, IOCON_MODE_INACT | IOCON_FUNC1}, // Soft Connect

  {1, 19, IOCON_MODE_INACT | IOCON_FUNC2}, // USB_PPWR (Host mode)

  // VBUS is not connected on this board, so leave the pin at default setting.
  /// Chip_IOCON_PinMux(LPC_IOCON, 1, 30, IOCON_MODE_INACT, IOCON_FUNC2);  // USB VBUS
};

// Invoked by startup code
void SystemInit(void)
{
#ifdef __USE_LPCOPEN
	extern void (* const g_pfnVectors[])(void);
  unsigned int *pSCB_VTOR = (unsigned int *) 0xE000ED08;
	*pSCB_VTOR = (unsigned int) g_pfnVectors;
#endif

  Chip_IOCON_Init(LPC_IOCON);
  Chip_IOCON_SetPinMuxing(LPC_IOCON, pinmuxing, sizeof(pinmuxing) / sizeof(PINMUX_GRP_T));
  Chip_SetupXtalClocking();

  Chip_SYSCTL_SetFLASHAccess(FLASHTIM_100MHZ_CPU);
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
	UARTConfigStruct.Baud_rate = CFG_BOARD_UART_BAUDRATE;

	UART_Init(BOARD_UART_PORT, &UARTConfigStruct);
	UART_TxCmd(BOARD_UART_PORT, ENABLE); // Enable UART Transmit
#endif

	//------------- USB -------------//
  Chip_IOCON_SetPinMuxing(LPC_IOCON, pin_usb_mux, sizeof(pin_usb_mux) / sizeof(PINMUX_GRP_T));
	Chip_USB_Init();

  enum {
    USBCLK_DEVCIE = 0x12,     // AHB + Device
    USBCLK_HOST   = 0x19,     // AHB + Host + OTG
//    0x1B // Host + Device + OTG + AHB
  };

  uint32_t const clk_en = CFG_TUD_ENABLED ? USBCLK_DEVCIE : USBCLK_HOST;

  LPC_USB->OTGClkCtrl = clk_en;
  while ( (LPC_USB->OTGClkSt & clk_en) != clk_en );

#if CFG_TUH_ENABLED
  // set portfunc to host !!!
  LPC_USB->StCtrl = 0x3; // should be 1
#endif
}

//--------------------------------------------------------------------+
// Board porting API
//--------------------------------------------------------------------+

void board_led_write(bool state)
{
  Chip_GPIO_SetPinState(LPC_GPIO, LED_PORT, LED_PIN, state ? LED_STATE_ON : (1-LED_STATE_ON));
}

uint32_t board_button_read(void)
{
  return BUTTON_STATE_ACTIVE == Chip_GPIO_GetPinState(LPC_GPIO, BUTTON_PORT, BUTTON_PIN);
}

int board_uart_read(uint8_t* buf, int len)
{
//  return UART_ReceiveByte(BOARD_UART_PORT);
  (void) buf; (void) len;
  return 0;
}

int board_uart_write(void const * buf, int len)
{
//  UART_Send(BOARD_UART_PORT, &c, 1, BLOCKING);
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

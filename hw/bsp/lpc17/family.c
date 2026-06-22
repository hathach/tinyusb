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

/* metadata:
   manufacturer: NXP
*/

#include "chip.h"
#include "bsp/board_api.h"
#include "board.h"

// Invoked by startup code
void SystemInit(void) {
#ifdef __USE_LPCOPEN
  extern void (* const g_pfnVectors[])(void);
  unsigned int* pSCB_VTOR = (unsigned int*) 0xE000ED08;
  *pSCB_VTOR = (unsigned int) g_pfnVectors;
#endif

  Chip_IOCON_Init(LPC_IOCON);
  Chip_IOCON_SetPinMuxing(LPC_IOCON, pinmuxing, sizeof(pinmuxing) / sizeof(PINMUX_GRP_T));
  Chip_SetupXtalClocking();

  Chip_SYSCTL_SetFLASHAccess(FLASHTIM_100MHZ_CPU);
}

void board_init(void) {
  SystemCoreClockUpdate();

#if CFG_TUSB_OS == OPT_OS_NONE
  // 1ms tick timer
  SysTick_Config(SystemCoreClock / 1000);
#elif CFG_TUSB_OS == OPT_OS_FREERTOS
  // Explicitly disable systick to prevent its ISR from running before scheduler start
  SysTick->CTRL &= ~1U;
  // If freeRTOS is used, IRQ priority is limit by max syscall ( smaller is higher )
  NVIC_SetPriority(USB_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY );
#endif

  Chip_GPIO_Init(LPC_GPIO);
  Chip_GPIO_SetPinDIROutput(LPC_GPIO, LED_PORT, LED_PIN);
  Chip_GPIO_SetPinDIRInput(LPC_GPIO, BUTTON_PORT, BUTTON_PIN);

  //------------- UART -------------//
  // Pin muxing for UART3 (TXD3=P0.0, RXD3=P0.1) is configured in board.h pinmuxing[]
  Chip_UART_Init(BOARD_UART_PORT);
  Chip_UART_SetBaud(BOARD_UART_PORT, CFG_BOARD_UART_BAUDRATE);
  Chip_UART_ConfigData(BOARD_UART_PORT, (UART_LCR_WLEN8 | UART_LCR_SBS_1BIT | UART_LCR_PARITY_DIS));
  Chip_UART_SetupFIFOS(BOARD_UART_PORT, (UART_FCR_FIFO_EN | UART_FCR_TRG_LEV0));
  Chip_UART_TXEnable(BOARD_UART_PORT);

  //------------- USB -------------//
  Chip_IOCON_SetPinMuxing(LPC_IOCON, pin_usb_mux, sizeof(pin_usb_mux) / sizeof(PINMUX_GRP_T));
  Chip_USB_Init();

  enum {
    USBCLK_DEVCIE = 0x12,     // AHB + Device
    USBCLK_HOST = 0x19,     // AHB + Host + OTG
//    0x1B // Host + Device + OTG + AHB
  };

#if CFG_TUD_ENABLED
  uint32_t const clk_en = USBCLK_DEVCIE;
#else
  uint32_t const clk_en = USBCLK_HOST;
#endif

  LPC_USB->OTGClkCtrl = clk_en;
  while ((LPC_USB->OTGClkSt & clk_en) != clk_en) {}

#if CFG_TUH_ENABLED
  // set portfunc to host !!!
  LPC_USB->StCtrl = 0x3; // should be 1
#endif
}

//--------------------------------------------------------------------+
// Board porting API
//--------------------------------------------------------------------+
void board_led_write(bool state) {
  Chip_GPIO_SetPinState(LPC_GPIO, LED_PORT, LED_PIN, state ? LED_STATE_ON : (1 - LED_STATE_ON));
}

uint32_t board_button_read(void) {
  return BUTTON_STATE_ACTIVE == Chip_GPIO_GetPinState(LPC_GPIO, BUTTON_PORT, BUTTON_PIN);
}

int board_uart_read(uint8_t* buf, int len) {
  int count = 0;
  while (count < len) {
    if (BOARD_UART_PORT->LSR & UART_LSR_RDR) {
      buf[count] = (uint8_t) BOARD_UART_PORT->RBR;
      count++;
    } else {
      break;
    }
  }
  return count;
}

int board_uart_write(void const* buf, int len) {
  const uint8_t *p = (const uint8_t *) buf;
  int count = 0;
  while (count < len) {
    if (BOARD_UART_PORT->LSR & UART_LSR_THRE) {
      BOARD_UART_PORT->THR = p[count];
      count++;
    } else {
      break;
    }
  }
  return count;
}

#if CFG_TUSB_OS == OPT_OS_NONE
volatile uint32_t system_ticks = 0;

void SysTick_Handler(void) {
  system_ticks++;
}

uint32_t tusb_time_millis_api(void) {
  return system_ticks;
}

//--------------------------------------------------------------------+
// USB Interrupt Handler
//--------------------------------------------------------------------+
void USB_IRQHandler(void) {
  #if CFG_TUD_ENABLED
  tud_int_handler(0);
  #endif

  #if CFG_TUH_ENABLED
  tuh_int_handler(0, true);
  #endif
}

#endif

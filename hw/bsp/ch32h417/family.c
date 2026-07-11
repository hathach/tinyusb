/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2026 Ha Thach (tinyusb.org)
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
 */

/* metadata:
   manufacturer: WCH
*/

// TinyUSB runs entirely on the V3F core (core 0, the boot core, HCLK = 100 MHz at the vendor
// default clock tree): single image at flash 0x0, no core-1 (V5F) wake-up. The vendor's own
// USBSS demo supports this Run_Core_V3F configuration.

#include "ch32h417.h"

#include "bsp/board_api.h"
#include "board.h"

//--------------------------------------------------------------------+
// Forward USB interrupt events to TinyUSB IRQ Handler
//--------------------------------------------------------------------+

// Prototypes for the (weak) vector-table handlers overridden here
void USBSS_IRQHandler(void);
void USBSS_LINK_IRQHandler(void);
void USBHS_IRQHandler(void);
void SysTick0_Handler(void);

// Single rhport 0: SPEED=super compiles the USB3 dcd (USBSS + USBSS_LINK vectors); SPEED=high
// the USB2 dcd (USBHS vector); the active dcd's dcd_int_handler dispatches on its own flag
// registers, so all vectors share ONE forwarder. Aliases (not identical function bodies) are
// required: gcc's identical-code-folding would otherwise rewrite one interrupt handler as a
// plain call into another, whose mret then skips the caller's epilogue and leaks its stack
// frame on every interrupt. Use the standard riscv interrupt attribute - WCH's
// "WCH-Interrupt-fast" is only understood by WCH's own gcc fork.
__attribute__((interrupt)) void USBSS_IRQHandler(void) {
  tusb_int_handler(0, true);
}

void USBSS_LINK_IRQHandler(void) __attribute__((alias("USBSS_IRQHandler")));
void USBHS_IRQHandler(void) __attribute__((alias("USBSS_IRQHandler")));

#if CFG_TUD_WCH_USB30_FALLBACK
// the USB3 dcd uses TIM12 as the SuperSpeed-training timeout for the USB2 fallback
void TIM12_IRQHandler(void);
void TIM12_IRQHandler(void) __attribute__((alias("USBSS_IRQHandler")));
#endif

//--------------------------------------------------------------------+
// SysTick (V3F core uses SysTick0; its ISR register holds the flags of both SysTicks)
//--------------------------------------------------------------------+

#if CFG_TUSB_OS == OPT_OS_NONE
volatile uint32_t system_ticks = 0;

__attribute__((interrupt)) void SysTick0_Handler(void) {
  SysTick0->ISR &= ~(1u << 0); // clear count-reached flag
  system_ticks++;
}

uint32_t tusb_time_millis_api(void) {
  return system_ticks;
}
#endif

//--------------------------------------------------------------------+
// Board Init
//--------------------------------------------------------------------+

void board_init(void) {
  SystemInit(); // clock tree: SYSCLK 400 MHz, V3F core (HCLK) 100 MHz from HSE
  SystemAndCoreClockUpdate();

#if CFG_TUSB_OS == OPT_OS_NONE
  // 1 ms tick on SysTick0 (counts HCLK, auto-reload)
  SysTick0->ISR &= ~(1u << 0);
  SysTick0->CMP = SystemCoreClock / 1000 - 1;
  SysTick0->CNT = 0;
  SysTick0->CTLR = 0xF;
  NVIC_SetPriority(SysTick0_IRQn, 0xF0);
  NVIC_EnableIRQ(SysTick0_IRQn);
#endif

  // USART1 TX on PA9 (AF7) - wired to the on-board WCH-LinkE virtual COM port
#ifdef CFG_BOARD_UART_BAUDRATE
  RCC_HB2PeriphClockCmd(RCC_HB2Periph_AFIO | RCC_HB2Periph_USART1 | RCC_HB2Periph_GPIOA, ENABLE);
  GPIO_PinAFConfig(GPIOA, GPIO_PinSource9, GPIO_AF7);
  GPIO_InitTypeDef gpio_init = {
    .GPIO_Pin = GPIO_Pin_9,
    .GPIO_Speed = GPIO_Speed_Very_High,
    .GPIO_Mode = GPIO_Mode_AF_PP,
  };
  GPIO_Init(GPIOA, &gpio_init);

  USART_InitTypeDef usart_init = {
    .USART_BaudRate = CFG_BOARD_UART_BAUDRATE,
    .USART_WordLength = USART_WordLength_8b,
    .USART_StopBits = USART_StopBits_1,
    .USART_Parity = USART_Parity_No,
    .USART_Mode = USART_Mode_Tx,
    .USART_HardwareFlowControl = USART_HardwareFlowControl_None,
  };
  USART_Init(USART1, &usart_init);
  USART_Cmd(USART1, ENABLE);
#endif

  // LED
  RCC_HB2PeriphClockCmd(LED_CLOCK, ENABLE);
  GPIO_InitTypeDef led_init = {
    .GPIO_Pin = LED_PIN,
    .GPIO_Speed = GPIO_Speed_Very_High,
    .GPIO_Mode = GPIO_Mode_Out_PP,
  };
  GPIO_Init(LED_PORT, &led_init);
  board_led_write(false);
}

//--------------------------------------------------------------------+
// Board porting API
//--------------------------------------------------------------------+

void board_led_write(bool state) {
  GPIO_WriteBit(LED_PORT, LED_PIN, (state ^ (LED_STATE_ON == 0)) ? Bit_SET : Bit_RESET);
}

uint32_t board_button_read(void) {
  return 0; // no user button on this board
}

size_t board_get_unique_id(uint8_t id[], size_t max_len) {
  volatile uint32_t* ch32_uuid = ((volatile uint32_t*) 0x1FFFF7E8UL); // ESIG unique ID (96 bit)
  uint32_t uid[3] = {ch32_uuid[0], ch32_uuid[1], ch32_uuid[2]};
  const size_t len = max_len < sizeof(uid) ? max_len : sizeof(uid);
  memcpy(id, uid, len); // byte copy: id[] need not be 4-byte aligned
  return len;
}

int board_uart_read(uint8_t* buf, int len) {
  (void) buf;
  (void) len;
  return 0;
}

int board_uart_write(void const* buf, int len) {
  int txsize = len;
  const char* bufc = (const char*) buf;
  while (txsize--) {
    while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET) {}
    USART_SendData(USART1, (uint16_t) (uint8_t) *bufc++);
  }
  while (USART_GetFlagStatus(USART1, USART_FLAG_TC) == RESET) {}
  return len;
}

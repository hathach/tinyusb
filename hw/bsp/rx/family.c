/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2021, Koji Kitayama
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
#include "board.h"
#include "interrupt_handlers.h"

#define SYSTEM_PRCR_PRC1      (1<<1)
#define SYSTEM_PRCR_PRKEY     (0xA5u<<8)

#define CMT_CMCR_CKS_DIV_128  2
#define CMT_CMCR_CMIE         (1<<6)

#define IRQ_PRIORITY_CMT0     5
#define IRQ_PRIORITY_USBI0    6

#define SCI_SSR_FER           (1<<4)
#define SCI_SSR_ORER          (1<<5)
#define SCI_SCR_TEIE          (1u<<2)
#define SCI_SCR_RE            (1u<<4)
#define SCI_SCR_TE            (1u<<5)
#define SCI_SCR_RIE           (1u<<6)
#define SCI_SCR_TIE           (1u<<7)

// Board-specific pin/peripheral init (implemented per board)
void board_pin_init(void);

//--------------------------------------------------------------------+
// SCI UART interrupt handlers
//--------------------------------------------------------------------+
typedef struct {
  uint8_t *buf;
  uint32_t cnt;
} sci_buf_t;
static volatile sci_buf_t sci_buf[2];

void BOARD_SCI_TXI_HANDLER(void)
{
  uint8_t *buf = sci_buf[0].buf;
  uint32_t cnt = sci_buf[0].cnt;

  if (!buf || !cnt) {
    BOARD_UART_SCI.SCR.BYTE &= ~(SCI_SCR_TEIE | SCI_SCR_TE | SCI_SCR_TIE);
    return;
  }
  BOARD_UART_SCI.TDR = *buf;
  if (--cnt) {
    ++buf;
  } else {
    buf = NULL;
    BOARD_UART_SCI.SCR.BIT.TIE  = 0;
    BOARD_UART_SCI.SCR.BIT.TEIE = 1;
  }
  sci_buf[0].buf = buf;
  sci_buf[0].cnt = cnt;
}

void BOARD_SCI_TEI_HANDLER(void)
{
  BOARD_UART_SCI.SCR.BYTE &= ~(SCI_SCR_TEIE | SCI_SCR_TE | SCI_SCR_TIE);
}

void BOARD_SCI_RXI_HANDLER(void)
{
  uint8_t *buf = sci_buf[1].buf;
  uint32_t cnt = sci_buf[1].cnt;

  if (!buf || !cnt ||
      (BOARD_UART_SCI.SSR.BYTE & (SCI_SSR_FER | SCI_SSR_ORER))) {
    sci_buf[1].buf = NULL;
    BOARD_UART_SCI.SSR.BYTE   = 0;
    BOARD_UART_SCI.SCR.BYTE  &= ~(SCI_SCR_RE | SCI_SCR_RIE);
    return;
  }
  *buf = BOARD_UART_SCI.RDR;
  if (--cnt) {
    ++buf;
  } else {
    buf = NULL;
    BOARD_UART_SCI.SCR.BYTE &= ~(SCI_SCR_RE | SCI_SCR_RIE);
  }
  sci_buf[1].buf = buf;
  sci_buf[1].cnt = cnt;
}

//--------------------------------------------------------------------+
// Forward USB interrupt events to TinyUSB IRQ Handler
//--------------------------------------------------------------------+
void BOARD_USB_IRQ_HANDLER(void)
{
#if CFG_TUH_ENABLED
  tuh_int_handler(0, true);
#endif
#if CFG_TUD_ENABLED
  tud_int_handler(0);
#endif
}

//--------------------------------------------------------------------+
// Board init
//--------------------------------------------------------------------+
void board_init(void)
{
#if CFG_TUSB_OS == OPT_OS_NONE
  /* Enable CMT0 */
  SYSTEM.PRCR.WORD = SYSTEM_PRCR_PRKEY | SYSTEM_PRCR_PRC1;
  MSTP(CMT0)       = 0;
  SYSTEM.PRCR.WORD = SYSTEM_PRCR_PRKEY;
  /* Setup 1ms tick timer */
  CMT0.CMCNT      = 0;
  CMT0.CMCOR      = BOARD_PCLK / 1000 / 128;
  CMT0.CMCR.WORD  = CMT_CMCR_CMIE | CMT_CMCR_CKS_DIV_128;
  IR(CMT0, CMI0)  = 0;
  IPR(CMT0, CMI0) = IRQ_PRIORITY_CMT0;
  IEN(CMT0, CMI0) = 1;
  CMT.CMSTR0.BIT.STR0 = 1;
#endif

  /* Board-specific: pin mux, SCI, USB pin config */
  board_pin_init();

  /* Enable USB0 module */
  unsigned short oldPRCR = SYSTEM.PRCR.WORD;
  SYSTEM.PRCR.WORD = SYSTEM_PRCR_PRKEY | SYSTEM_PRCR_PRC1;
  MSTP(USB0) = 0;
  SYSTEM.PRCR.WORD = SYSTEM_PRCR_PRKEY | oldPRCR;

  /* USB IRQ */
  IR(USB0, USBI0)  = 0;
  IPR(USB0, USBI0) = IRQ_PRIORITY_USBI0;
}

//--------------------------------------------------------------------+
// Board porting API
//--------------------------------------------------------------------+

void board_led_write(bool state)
{
  BOARD_LED_WRITE(state);
}

uint32_t board_button_read(void)
{
  return BOARD_BUTTON_READ();
}

int board_uart_read(uint8_t* buf, int len)
{
  sci_buf[1].buf = buf;
  sci_buf[1].cnt = len;
  BOARD_UART_SCI.SCR.BYTE |= SCI_SCR_RE | SCI_SCR_RIE;
  while (BOARD_UART_SCI.SCR.BIT.RE) ;
  return len - sci_buf[1].cnt;
}

int board_uart_write(void const *buf, int len)
{
  sci_buf[0].buf = (uint8_t*)(uintptr_t) buf;
  sci_buf[0].cnt = len;
  BOARD_UART_SCI.SCR.BYTE |= SCI_SCR_TE | SCI_SCR_TIE;
  while (BOARD_UART_SCI.SCR.BIT.TE) ;
  return len;
}

//--------------------------------------------------------------------+
// Tick timer
//--------------------------------------------------------------------+
#if CFG_TUSB_OS == OPT_OS_NONE
volatile uint32_t system_ticks = 0;
void INT_Excep_CMT0_CMI0(void)
{
  ++system_ticks;
}

uint32_t tusb_time_millis_api(void)
{
  return system_ticks;
}
#else
uint32_t SystemCoreClock = BOARD_CPUCLK;
#endif

//--------------------------------------------------------------------+
// Newlib syscall stubs
//--------------------------------------------------------------------+
int close(int fd)
{
  (void)fd;
  return -1;
}

int fstat(int fd, void *pstat)
{
  (void)fd;
  (void)pstat;
  return 0;
}

off_t lseek(int fd, off_t pos, int whence)
{
  (void)fd;
  (void)pos;
  (void)whence;
  return 0;
}

int isatty(int fd)
{
  (void)fd;
  return 1;
}

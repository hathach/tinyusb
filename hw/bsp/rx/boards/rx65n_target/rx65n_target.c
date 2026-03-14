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

/* How to connect JLink and RX65n Target and option board
 * (For original comment https://github.com/hathach/tinyusb/pull/922#issuecomment-869786131)
 *
 * To enable JTAG, RX65N requires following connections on main board.
 * - short EJ2 jumper header, to disable onboard E2L.
 * - short EMLE(J1-2) and 3V3(J1-14 or J2-10), to enable In-Circuit Emulator.
 *
 * Note: For RX65N-Cloud-Kit, the option board's JTAG pins to some switches or floating.
 * To use JLink with the option board, I think some further modifications will be necessary.
 *
 * | Function  | RX65N pin  | main board | option board | JLink connector |
 * |:---------:|:----------:|:----------:|:------------:|:---------------:|
 * | 3V3       | VCC        |   J1-14    | CN5-6        |    1            |
 * | TRST      | P34        |   J1-16    | CN5-7        |    3            |
 * | GND       | VSS        |   J1-12    | CN5-5        |    4            |
 * | TDI       | P30        |   J1-20    | CN5-10       |    5            |
 * | TMS       | P31        |   J1-19    | USER_SW      |    7            |
 * | TCK/FINEC | P27        |   J1-21    | N/A          |    9            |
 * | TDO       | P26        |   J1-22    | CN5-9        |   13            |
 * | nRES      | RES#       |   J1-10    | RESET_SW     |   15            |
 *
 * JLink firmware needs to update to V6.96 or newer version to avoid
 * [a bug](https://forum.segger.com/index.php/Thread/7758-SOLVED-Bug-in-JLink-from-V6-88b-regarding-RX65N)
 * regarding downloading.
 */

#include "iodefine.h"
#include "board.h"

#define SYSTEM_PRCR_PRC1      (1<<1)
#define SYSTEM_PRCR_PRKEY     (0xA5u<<8)
#define MPC_PFS_ISEL          (1<<6)

#define IRQ_PRIORITY_SCI5     5
#define SCI_PCLK              60000000

void HardwareSetup(void)
{
  FLASH.ROMCIV.WORD = 1;
  while (FLASH.ROMCIV.WORD) ;
  FLASH.ROMCE.WORD = 1;
  while (!FLASH.ROMCE.WORD) ;

  SYSTEM.PRCR.WORD = 0xA503u;
  if (!SYSTEM.RSTSR1.BYTE) {
    RTC.RCR4.BYTE = 0;
    RTC.RCR3.BYTE = 12;
    while (12 != RTC.RCR3.BYTE) ;
  }
  SYSTEM.SOSCCR.BYTE = 1;

  if (SYSTEM.HOCOCR.BYTE) {
    SYSTEM.HOCOCR.BYTE = 0;
    while (!SYSTEM.OSCOVFSR.BIT.HCOVF) ;
  }
  SYSTEM.PLLCR.WORD  = 0x1D10u; /* HOCO x 15 */
  SYSTEM.PLLCR2.BYTE = 0;
  while (!SYSTEM.OSCOVFSR.BIT.PLOVF) ;

  SYSTEM.SCKCR.LONG  = 0x21C11222u;
  SYSTEM.SCKCR2.WORD = 0x0041u;
  SYSTEM.ROMWT.BYTE  = 0x02u;
  while (0x02u != SYSTEM.ROMWT.BYTE) ;
  SYSTEM.SCKCR3.WORD = 0x400u;
  SYSTEM.PRCR.WORD   = 0xA500u;
}

void board_pin_init(void)
{
  /* Setup software configurable interrupts for USB */
  ICU.SLIBR_USBI0.BYTE = IRQ_USB0_USBI0;
  ICU.SLIPRCR.BYTE     = 1;

  /* Unlock MPC registers */
  MPC.PWPR.BIT.B0WI  = 0;
  MPC.PWPR.BIT.PFSWE = 1;

  /* Button PB1 */
  PORTB.PMR.BIT.B1 = 0U;
  PORTB.PDR.BIT.B1 = 0U;

  /* LED PD6 (open-drain, active low) */
  PORTD.PODR.BIT.B6 = 1U;
  PORTD.ODR1.BIT.B4 = 1U;
  PORTD.PMR.BIT.B6  = 0U;
  PORTD.PDR.BIT.B6  = 1U;

  /* UART TXD5 => PA4, RXD5 => PA3 */
  PORTA.PMR.BIT.B4 = 1U;
  PORTA.PCR.BIT.B4 = 1U;
  MPC.PA4PFS.BYTE  = 0b01010;
  PORTA.PMR.BIT.B3 = 1U;
  MPC.PA3PFS.BYTE  = 0b01010;

  /* USB VBUS -> P16 */
  PORT1.PMR.BIT.B6 = 1U;
  MPC.P16PFS.BYTE  = MPC_PFS_ISEL | 0b10001;

  /* Lock MPC registers */
  MPC.PWPR.BIT.PFSWE = 0;
  MPC.PWPR.BIT.B0WI  = 1;

  /* Enable SCI5 */
  SYSTEM.PRCR.WORD   = SYSTEM_PRCR_PRKEY | SYSTEM_PRCR_PRC1;
  MSTP(SCI5)         = 0;
  SYSTEM.PRCR.WORD   = SYSTEM_PRCR_PRKEY;
  SCI5.SEMR.BIT.ABCS = 1;
  SCI5.SEMR.BIT.BGDM = 1;
  SCI5.BRR           = (SCI_PCLK / (8 * 115200)) - 1;
  IR(SCI5,  RXI5)    = 0;
  IR(SCI5,  TXI5)    = 0;
  IS(SCI5,  TEI5)    = 0;
  IR(ICU, GROUPBL0)  = 0;
  IPR(SCI5, RXI5)    = IRQ_PRIORITY_SCI5;
  IPR(SCI5, TXI5)    = IRQ_PRIORITY_SCI5;
  IPR(ICU,GROUPBL0)  = IRQ_PRIORITY_SCI5;
  IEN(SCI5, RXI5)    = 1;
  IEN(SCI5, TXI5)    = 1;
  IEN(ICU,GROUPBL0)  = 1;
  EN(SCI5, TEI5)     = 1;
}

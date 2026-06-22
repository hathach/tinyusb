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

/* How to connect JLink and GR-CITRUS
 *
 * GR-CITRUS needs to solder some pads to enable JTAG interface.
 * - Short the following pads individually with solder.
 *   - J4
 *   - J5
 * - Short EMLE pad and 3.3V(GR-CITRUS pin name) with a wire.
 *
 * The pads are [the back side of GR-CITRUS](https://www.slideshare.net/MinaoYamamoto/grcitrusrx631/2).
 *
 * Connect the pins between GR-CITRUS and JLink as follows.
 *
 * | Function  | GR-CITRUS pin | JLink pin No.| note     |
 * |:---------:|:-------------:|:------------:|:--------:|
 * | VTref     |   3.3V        |   1          |          |
 * | TRST      |   5           |   3          |          |
 * | GND       |   GND         |   4          |          |
 * | TDI       |   3           |   5          |          |
 * | TMS       |   2           |   7          |          |
 * | TCK/FINEC |   14          |   9          | short J4 |
 * | TDO       |   9           |  13          | short J5 |
 * | nRES      |   RST         |  15          |          |
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

#define IRQ_PRIORITY_SCI0     5
#define SCI_PCLK              48000000

void HardwareSetup(void)
{
  SYSTEM.PRCR.WORD     = 0xA503u;
  SYSTEM.SOSCCR.BYTE   = 0x01u;
  SYSTEM.MOSCWTCR.BYTE = 0x0Du;
  SYSTEM.PLLWTCR.BYTE  = 0x0Eu;
  SYSTEM.PLLCR.WORD    = 0x0F00u;
  SYSTEM.MOSCCR.BYTE   = 0x00u;
  SYSTEM.PLLCR2.BYTE   = 0x00u;
  for (unsigned i = 0; i < 2075u; ++i) __asm("nop");
  SYSTEM.SCKCR.LONG    = 0x21021211u;
  SYSTEM.SCKCR2.WORD   = 0x0033u;
  SYSTEM.SCKCR3.WORD   = 0x0400u;
  SYSTEM.SYSCR0.WORD   = 0x5A01;
  SYSTEM.MSTPCRB.BIT.MSTPB15 = 0;
  SYSTEM.PRCR.WORD     = 0xA500u;
}

void board_pin_init(void)
{
  /* Unlock MPC registers */
  MPC.PWPR.BIT.B0WI  = 0;
  MPC.PWPR.BIT.PFSWE = 1;

  /* LED PA0 */
  PORTA.PMR.BIT.B0  = 0U;
  PORTA.PODR.BIT.B0 = 0U;
  PORTA.PDR.BIT.B0  = 1U;

  /* UART TXD0 => P20, RXD0 => P21 */
  PORT2.PMR.BIT.B0 = 1U;
  PORT2.PCR.BIT.B0 = 1U;
  MPC.P20PFS.BYTE  = 0b01010;
  PORT2.PMR.BIT.B1 = 1U;
  MPC.P21PFS.BYTE  = 0b01010;

  /* USB VBUS -> P16 DPUPE -> P14 */
  PORT1.PMR.BIT.B4 = 1U;
  PORT1.PMR.BIT.B6 = 1U;
  MPC.P14PFS.BYTE  = 0b10001;
  MPC.P16PFS.BYTE  = MPC_PFS_ISEL | 0b10001;
  MPC.PFUSB0.BIT.PUPHZS = 1;

  /* Lock MPC registers */
  MPC.PWPR.BIT.PFSWE = 0;
  MPC.PWPR.BIT.B0WI  = 1;

  /* Enable SCI0 */
  SYSTEM.PRCR.WORD = SYSTEM_PRCR_PRKEY | SYSTEM_PRCR_PRC1;
  MSTP(SCI0) = 0;
  SYSTEM.PRCR.WORD = SYSTEM_PRCR_PRKEY;
  SCI0.BRR = (SCI_PCLK / (32 * 115200)) - 1;
  IR(SCI0,  RXI0)  = 0;
  IR(SCI0,  TXI0)  = 0;
  IR(SCI0,  TEI0)  = 0;
  IPR(SCI0, RXI0) = IRQ_PRIORITY_SCI0;
  IPR(SCI0, TXI0) = IRQ_PRIORITY_SCI0;
  IPR(SCI0, TEI0) = IRQ_PRIORITY_SCI0;
  IEN(SCI0, RXI0) = 1;
  IEN(SCI0, TXI0) = 1;
  IEN(SCI0, TEI0) = 1;
}

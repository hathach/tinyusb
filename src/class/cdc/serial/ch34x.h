/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2023 Heiko Kuester
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

#ifndef _CH34X_H_
#define _CH34X_H_

#include <stdint.h>

//#define BIT(nr) ( (uint32_t)1 << (nr) )

#define CH34X_BUFFER_SIZE 2

// The following defines have been taken over from Linux driver /drivers/usb/serial/ch341.c
// Note: updated driver can also be found in https://github.com/WCHSoftGroup/ch341ser_linux/tree/main/driver

#define DEFAULT_BAUD_RATE 9600

/* flags for IO-Bits */
#define CH341_BIT_RTS (1 << 6)
#define CH341_BIT_DTR (1 << 5)

/******************************/
/* interrupt pipe definitions */
/******************************/
/* always 4 interrupt bytes */
/* first irq byte normally 0x08 */
/* second irq byte base 0x7d + below */
/* third irq byte base 0x94 + below */
/* fourth irq byte normally 0xee */

/* second interrupt byte */
#define CH341_MULT_STAT 0x04 /* multiple status since last interrupt event */

/* status returned in third interrupt answer byte, inverted in data from irq */
#define CH341_BIT_CTS 0x01
#define CH341_BIT_DSR 0x02
#define CH341_BIT_RI  0x04
#define CH341_BIT_DCD 0x08
#define CH341_BITS_MODEM_STAT 0x0f /* all bits */

/* Break support - the information used to implement this was gleaned from
 * the Net/FreeBSD uchcom.c driver by Takanori Watanabe.  Domo arigato.
 */

// USB requests
#define CH341_REQ_READ_VERSION 0x5F // dec 95
#define CH341_REQ_WRITE_REG    0x9A
#define CH341_REQ_READ_REG     0x95
#define CH341_REQ_SERIAL_INIT  0xA1
#define CH341_REQ_MODEM_CTRL   0xA4

// CH34x registers
#define CH341_REG_BREAK        0x05
#define CH341_REG_PRESCALER    0x12
#define CH341_REG_DIVISOR      0x13
#define CH341_REG_LCR          0x18
#define CH341_REG_LCR2         0x25

#define CH341_NBREAK_BITS      0x01

// line control bits
#define CH341_LCR_ENABLE_RX    0x80
#define CH341_LCR_ENABLE_TX    0x40
#define CH341_LCR_MARK_SPACE   0x20
#define CH341_LCR_PAR_EVEN     0x10
#define CH341_LCR_ENABLE_PAR   0x08
#define CH341_LCR_STOP_BITS_2  0x04
#define CH341_LCR_CS8          0x03
#define CH341_LCR_CS7          0x02
#define CH341_LCR_CS6          0x01
#define CH341_LCR_CS5          0x00

#define CH341_QUIRK_LIMITED_PRESCALER TU_BIT(0)
#define CH341_QUIRK_SIMULATE_BREAK  TU_BIT(1)

#define CH341_CLKRATE   48000000
#define CH341_CLK_DIV(ps, fact) (1 << (12 - 3 * (ps) - (fact)))
#define CH341_MIN_RATE(ps)  (CH341_CLKRATE / (CH341_CLK_DIV((ps), 1) * 512))

/* Supported range is 46 to 3000000 bps. */
#define CH341_MIN_BPS TU_DIV_CEIL(CH341_CLKRATE, CH341_CLK_DIV(0, 0) * 256)
#define CH341_MAX_BPS (CH341_CLKRATE / (CH341_CLK_DIV(3, 0) * 2))

// error codes
#define EINVAL    22  /* Invalid argument */

#endif /* _CH34X_H_ */

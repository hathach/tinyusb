/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2021, Ha Thach (tinyusb.org)
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

#ifndef _CI_HS_LPC18_43_H_
#define _CI_HS_LPC18_43_H_

#ifdef __cplusplus
 extern "C" {
#endif

// LPCOpen for 18xx & 43xx
#include "chip.h"

static const ci_hs_controller_t _ci_controller[] =
{
  { .reg_base = LPC_USB0_BASE, .irqnum = USB0_IRQn, .ep_count = 6 },
  { .reg_base = LPC_USB1_BASE, .irqnum = USB1_IRQn, .ep_count = 4 }
};

void dcd_int_enable(uint8_t rhport)
{
  NVIC_EnableIRQ(_ci_controller[rhport].irqnum);
}

void dcd_int_disable(uint8_t rhport)
{
  NVIC_DisableIRQ(_ci_controller[rhport].irqnum);
}

#ifdef __cplusplus
 }
#endif

#endif

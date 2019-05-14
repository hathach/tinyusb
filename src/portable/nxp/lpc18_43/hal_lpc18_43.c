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

#include "tusb.h"

#if (CFG_TUSB_MCU == OPT_MCU_LPC18XX || CFG_TUSB_MCU == OPT_MCU_LPC43XX)

#include "chip.h"

extern void hal_dcd_isr(uint8_t rhport);
extern void hal_hcd_isr(uint8_t hostid);

#if CFG_TUSB_RHPORT0_MODE
void USB0_IRQHandler(void)
{
  #if TUSB_OPT_HOST_ENABLED
    hal_hcd_isr(0);
  #endif

  #if TUSB_OPT_DEVICE_ENABLED
    hal_dcd_isr(0);
  #endif
}
#endif

#if CFG_TUSB_RHPORT1_MODE
void USB1_IRQHandler(void)
{
  #if TUSB_OPT_HOST_ENABLED
    hal_hcd_isr(1);
  #endif

  #if TUSB_OPT_DEVICE_ENABLED
    hal_dcd_isr(1);
  #endif
}
#endif

#endif

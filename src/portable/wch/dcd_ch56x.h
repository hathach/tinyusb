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
 *
 * This file is part of the TinyUSB stack.
 */

#ifndef TUSB_DCD_CH56X_H_
#define TUSB_DCD_CH56X_H_

#include "common/tusb_common.h"
#include "device/dcd.h"

// CH565/CH569 high-speed peripherals (USB2.0 and USB3.0 controllers) can only DMA from/to the
// RAMX region (CH569 datasheet 2.2.2 Note 2), with 16-byte (128-bit) aligned buffer addresses.
// The BSP linker script provides a NOLOAD ".dmadata" section placed in RAMX.
#define CH56X_RAMX_ADDR   0x20020000UL

#ifndef CFG_TUD_WCH_DMA_SECTION
  #define CFG_TUD_WCH_DMA_SECTION TU_ATTR_SECTION(".dmadata")
#endif

TU_ATTR_ALWAYS_INLINE static inline bool ch56x_is_dma_capable(const void *buf) {
  return ((uint32_t)(uintptr_t)buf >= CH56X_RAMX_ADDR) && (((uint32_t)(uintptr_t)buf & 15u) == 0);
}

//--------------------------------------------------------------------+
// USB2.0 high-speed controller internals (dcd_ch56x_usbhs.c). Exposed so the USB3.0 dcd
// (dcd_ch56x_usb30.c) can drive the USB2 controller for runtime fallback on the same rhport
// when CFG_TUD_WCH_USB30_FALLBACK is enabled.
//--------------------------------------------------------------------+
bool ch56x_usb2_init(uint8_t rhport);
void ch56x_usb2_deinit(void);
void ch56x_usb2_int_enable(void);
void ch56x_usb2_int_disable(void);
void ch56x_usb2_int_handler(uint8_t rhport);
void ch56x_usb2_edpt0_status_complete(uint8_t rhport, const tusb_control_request_t *request);
bool ch56x_usb2_edpt_open(uint8_t rhport, const tusb_desc_endpoint_t *desc_edpt);
void ch56x_usb2_edpt_close(uint8_t rhport, uint8_t ep_addr);
void ch56x_usb2_edpt_close_all(uint8_t rhport);
bool ch56x_usb2_edpt_xfer(uint8_t rhport, uint8_t ep_addr, uint8_t *buffer, uint16_t total_bytes);
void ch56x_usb2_edpt_stall(uint8_t rhport, uint8_t ep_addr);
void ch56x_usb2_edpt_clear_stall(uint8_t rhport, uint8_t ep_addr);
void ch56x_usb2_sof_enable(uint8_t rhport, bool en);

#endif // TUSB_DCD_CH56X_H_

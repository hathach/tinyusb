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

#ifndef _TUSB_USBH_CLASSDRIVER_H_
#define _TUSB_USBH_CLASSDRIVER_H_

#include "osal/osal.h"
#include "common/tusb_fifo.h"

#ifdef __cplusplus
 extern "C" {
#endif

//--------------------------------------------------------------------+
// Class Driver API
//--------------------------------------------------------------------+

typedef struct {
  #if CFG_TUSB_DEBUG >= 2
  char const* name;
  #endif

  void (* const init       )(void);
  bool (* const open       )(uint8_t rhport, uint8_t dev_addr, tusb_desc_interface_t const * itf_desc, uint16_t max_len);
  bool (* const set_config )(uint8_t dev_addr, uint8_t itf_num);
  bool (* const xfer_cb    )(uint8_t dev_addr, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes);
  void (* const close      )(uint8_t dev_addr);
} usbh_class_driver_t;

// Call by class driver to tell USBH that it has complete the enumeration
void usbh_driver_set_config_complete(uint8_t dev_addr, uint8_t itf_num);

uint8_t usbh_get_rhport(uint8_t dev_addr);

uint8_t* usbh_get_enum_buf(void);

//--------------------------------------------------------------------+
// USBH Endpoint API
//--------------------------------------------------------------------+

// Open an endpoint
bool usbh_edpt_open(uint8_t rhport, uint8_t dev_addr, tusb_desc_endpoint_t const * desc_ep);

// Submit a usb transfer
bool usbh_edpt_xfer(uint8_t dev_addr, uint8_t ep_addr, uint8_t * buffer, uint16_t total_bytes);

// Claim an endpoint before submitting a transfer.
// If caller does not make any transfer, it must release endpoint for others.
bool usbh_edpt_claim(uint8_t dev_addr, uint8_t ep_addr);

bool usbh_edpt_release(uint8_t dev_addr, uint8_t ep_addr);

// Check if endpoint transferring is complete
bool usbh_edpt_busy(uint8_t dev_addr, uint8_t ep_addr);

#ifdef __cplusplus
 }
#endif

#endif

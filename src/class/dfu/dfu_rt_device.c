/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Sylvain Munaut <tnt@246tNt.com>
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

#include "tusb_option.h"

#if (TUSB_OPT_DEVICE_ENABLED && CFG_TUD_DFU_RT)

#include "dfu_rt_device.h"
#include "device/usbd_pvt.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+
typedef enum {
  DFU_REQUEST_DETACH      = 0,
  DFU_REQUEST_DNLOAD      = 1,
  DFU_REQUEST_UPLOAD      = 2,
  DFU_REQUEST_GETSTATUS   = 3,
  DFU_REQUEST_CLRSTATUS   = 4,
  DFU_REQUEST_GETSTATE    = 5,
  DFU_REQUEST_ABORT       = 6,
} dfu_requests_t;

//--------------------------------------------------------------------+
// USBD Driver API
//--------------------------------------------------------------------+
void dfu_rtd_init(void)
{
}

void dfu_rtd_reset(uint8_t rhport)
{
  (void) rhport;
}

bool dfu_rtd_open(uint8_t rhport, tusb_desc_interface_t const * itf_desc, uint16_t *p_length)
{
  (void) rhport;

  // Ensure this is DFU Runtime
  TU_ASSERT(itf_desc->bInterfaceSubClass == TUD_DFU_APP_SUBCLASS);
  TU_ASSERT(itf_desc->bInterfaceProtocol == DFU_PROTOCOL_RT);

  uint8_t const * p_desc = tu_desc_next( itf_desc );
  (*p_length) = sizeof(tusb_desc_interface_t);

  if ( TUSB_DESC_FUNCTIONAL == tu_desc_type(p_desc) )
  {
    (*p_length) += p_desc[DESC_OFFSET_LEN];
    p_desc = tu_desc_next(p_desc);
  }

  return true;
}

bool dfu_rtd_control_complete(uint8_t rhport, tusb_control_request_t const * request)
{
  (void) rhport;

  //------------- Class Specific Request -------------//
  TU_VERIFY(request->bmRequestType_bit.type == TUSB_REQ_TYPE_CLASS);
  TU_VERIFY(request->bmRequestType_bit.recipient == TUSB_REQ_RCPT_INTERFACE);

  return true;
}

bool dfu_rtd_control_request(uint8_t rhport, tusb_control_request_t const * request)
{
  (void) rhport;

  //------------- Class Specific Request -------------//
  TU_ASSERT(request->bmRequestType_bit.type == TUSB_REQ_TYPE_CLASS);
  TU_ASSERT(request->bmRequestType_bit.recipient == TUSB_REQ_RCPT_INTERFACE);

  switch ( request->bRequest )
  {
    case DFU_REQUEST_DETACH:
      tud_control_status(rhport, request);
      tud_dfu_rt_reboot_to_dfu();
    break;

    default: return false; // stall unsupported request
  }

  return true;
}

bool dfu_rtd_xfer_cb(uint8_t rhport, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes)
{
  (void) rhport;
  (void) ep_addr;
  (void) result;
  (void) xferred_bytes;
  return true;
}

#endif

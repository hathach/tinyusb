/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2020, Ha Thach (tinyusb.org)
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

#if CFG_TUH_ENABLED && 0

#include "tusb.h"
#include "usbh_classdriver.h"

typedef struct
{
  tusb_control_request_t request TU_ATTR_ALIGNED(4);
  uint8_t* buffer;
  tuh_control_complete_cb_t complete_cb;

  uint8_t daddr;
} usbh_control_xfer_t;

static usbh_control_xfer_t _xfer;
static uint8_t _stage = CONTROL_STAGE_IDLE;

//--------------------------------------------------------------------+
// MACRO TYPEDEF CONSTANT ENUM DECLARATION
//--------------------------------------------------------------------+

uint8_t usbh_control_xfer_stage(void)
{
  return _stage;
}

bool usbh_control_xfer (uint8_t dev_addr, tusb_control_request_t const* request, void* buffer, tuh_control_complete_cb_t complete_cb)
{
  // TODO need to claim the endpoint first
  const uint8_t rhport = usbh_get_rhport(dev_addr);

  _ctrl_xfer.xfer.daddr       = dev_addr;
  _ctrl_xfer.xfer.request     = (*request);
  _ctrl_xfer.xfer.buffer      = buffer;
  _ctrl_xfer.xfer.complete_cb = complete_cb;

  _stage = CONTROL_STAGE_SETUP;

  // Send setup packet
  TU_ASSERT( hcd_setup_send(rhport, dev_addr, (uint8_t const*) &_ctrl_xfer.xfer.request) );

  return true;
}

void usbh_control_xfer_abort(uint8_t dev_addr)
{
  if (_ctrl_xfer.xfer.daddr == dev_addr) _stage = CONTROL_STAGE_IDLE;
}

static void _xfer_complete(uint8_t dev_addr, xfer_result_t result)
{
  TU_LOG2("\r\n");
  if (_ctrl_xfer.xfer.complete_cb) _ctrl_xfer.xfer.complete_cb(dev_addr, &_ctrl_xfer.xfer.request, result);
}


#endif

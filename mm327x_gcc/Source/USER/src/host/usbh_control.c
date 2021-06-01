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

#if TUSB_OPT_HOST_ENABLED

#include "tusb.h"
#include "usbh_hcd.h"

enum
{
  STAGE_SETUP,
  STAGE_DATA,
  STAGE_ACK
};

typedef struct
{
  tusb_control_request_t request TU_ATTR_ALIGNED(4);

  uint8_t stage;
  uint8_t* buffer;
  tuh_control_complete_cb_t complete_cb;
} usbh_control_xfer_t;

static usbh_control_xfer_t _ctrl_xfer;

//CFG_TUSB_MEM_SECTION CFG_TUSB_MEM_ALIGN
//static uint8_t _tuh_ctrl_buf[CFG_TUH_ENUMERATION_BUFSZIE];

//--------------------------------------------------------------------+
// MACRO TYPEDEF CONSTANT ENUM DECLARATION
//--------------------------------------------------------------------+

bool tuh_control_xfer (uint8_t dev_addr, tusb_control_request_t const* request, void* buffer, tuh_control_complete_cb_t complete_cb)
{
  // TODO need to claim the endpoint first

  usbh_device_t* dev = &_usbh_devices[dev_addr];
  const uint8_t rhport = dev->rhport;

  _ctrl_xfer.request     = (*request);
  _ctrl_xfer.buffer      = buffer;
  _ctrl_xfer.stage       = STAGE_SETUP;
  _ctrl_xfer.complete_cb = complete_cb;

  TU_LOG2("Control Setup: ");
  TU_LOG2_VAR(request);
  TU_LOG2("\r\n");

  // Send setup packet
  TU_ASSERT( hcd_setup_send(rhport, dev_addr, (uint8_t const*) &_ctrl_xfer.request) );

  return true;
}

static void _xfer_complete(uint8_t dev_addr, xfer_result_t result)
{
  TU_LOG2("\r\n");
  if (_ctrl_xfer.complete_cb) _ctrl_xfer.complete_cb(dev_addr, &_ctrl_xfer.request, result);
}

bool usbh_control_xfer_cb (uint8_t dev_addr, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes)
{
  (void) ep_addr;
  (void) xferred_bytes;

  usbh_device_t* dev = &_usbh_devices[dev_addr];
  const uint8_t rhport = dev->rhport;

  tusb_control_request_t const * request = &_ctrl_xfer.request;

  if (XFER_RESULT_SUCCESS != result)
  {
    TU_LOG2("Control failed: result = %d\r\n", result);

    // terminate transfer if any stage failed
    _xfer_complete(dev_addr, result);
  }else
  {
    switch(_ctrl_xfer.stage)
    {
      case STAGE_SETUP:
        _ctrl_xfer.stage = STAGE_DATA;
        if (request->wLength)
        {
          // Note: initial data toggle is always 1
          hcd_edpt_xfer(rhport, dev_addr, tu_edpt_addr(0, request->bmRequestType_bit.direction), _ctrl_xfer.buffer, request->wLength);
          return true;
        }
        __attribute__((fallthrough));

      case STAGE_DATA:
        _ctrl_xfer.stage = STAGE_ACK;

        if (request->wLength)
        {
          TU_LOG2("Control data:\r\n");
          TU_LOG2_MEM(_ctrl_xfer.buffer, request->wLength, 2);
        }

        // data toggle is always 1
        hcd_edpt_xfer(rhport, dev_addr, tu_edpt_addr(0, 1-request->bmRequestType_bit.direction), NULL, 0);
      break;

      case STAGE_ACK:
        _xfer_complete(dev_addr, result);
      break;

      default: return false;
    }
  }

  return true;
}

#endif

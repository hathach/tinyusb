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

#if (TUSB_OPT_DEVICE_ENABLED && CFG_TUD_DFU_RUNTIME) || (TUSB_OPT_DEVICE_ENABLED && CFG_TUD_DFU_RUNTIME_AND_MODE)

#include "dfu_rt_device.h"
#include "device/usbd_pvt.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+

//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+
typedef struct TU_ATTR_PACKED
{
    dfu_mode_device_status_t status;
    dfu_mode_state_t state;
    uint8_t attrs;
} dfu_rt_state_ctx_t;

// Only a single dfu state is allowed
CFG_TUSB_MEM_SECTION static dfu_rt_state_ctx_t _dfu_state_ctx;

static void dfu_rt_getstatus_reply(uint8_t rhport, tusb_control_request_t const * request);

//--------------------------------------------------------------------+
// USBD Driver API
//--------------------------------------------------------------------+
void dfu_rtd_init(void)
{
  _dfu_state_ctx.state = APP_IDLE;
  _dfu_state_ctx.status = DFU_STATUS_OK;
  _dfu_state_ctx.attrs = tud_dfu_runtime_init_attrs_cb();
  dfu_debug_print_context();
}

void dfu_rtd_reset(uint8_t rhport)
{
  if (((_dfu_state_ctx.attrs & DFU_FUNC_ATTR_WILL_DETACH_BITMASK) == 0)
     && (_dfu_state_ctx.state == DFU_REQUEST_DETACH))
  {
    tud_dfu_runtime_reboot_to_dfu_cb();
  }

  _dfu_state_ctx.state = APP_IDLE;
  _dfu_state_ctx.status = DFU_STATUS_OK;
  _dfu_state_ctx.attrs = tud_dfu_runtime_init_attrs_cb();
  dfu_debug_print_context();
}

uint16_t dfu_rtd_open(uint8_t rhport, tusb_desc_interface_t const * itf_desc, uint16_t max_len)
{
  (void) rhport;
  (void) max_len;

  // Ensure this is DFU Runtime
  TU_VERIFY((itf_desc->bInterfaceSubClass == TUD_DFU_APP_SUBCLASS) &&
            (itf_desc->bInterfaceProtocol == DFU_PROTOCOL_RT), 0);

  uint8_t const * p_desc = tu_desc_next( itf_desc );
  uint16_t drv_len = sizeof(tusb_desc_interface_t);

  if ( TUSB_DESC_FUNCTIONAL == tu_desc_type(p_desc) )
  {
    drv_len += tu_desc_len(p_desc);
    p_desc   = tu_desc_next(p_desc);
  }

  return drv_len;
}

// Invoked when a control transfer occurred on an interface of this class
// Driver response accordingly to the request and the transfer stage (setup/data/ack)
// return false to stall control endpoint (e.g unsupported request)
bool dfu_rtd_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const * request)
{
  // nothing to do with DATA or ACK stage
  if ( stage != CONTROL_STAGE_SETUP ) return true;

  TU_VERIFY(request->bmRequestType_bit.recipient == TUSB_REQ_RCPT_INTERFACE);

  // dfu-util will try to claim the interface with SET_INTERFACE request before sending DFU request
  if ( TUSB_REQ_TYPE_STANDARD == request->bmRequestType_bit.type &&
       TUSB_REQ_SET_INTERFACE == request->bRequest )
  {
    tud_control_status(rhport, request);
    return true;
  }

  // Handle class request only from here
  TU_VERIFY(request->bmRequestType_bit.type == TUSB_REQ_TYPE_CLASS);

  TU_LOG2("  DFU Request: %s\r\n", tu_lookup_find(&_dfu_request_table, request->bRequest));
  switch (request->bRequest)
  {
    case DFU_REQUEST_DETACH:
    {
      if (_dfu_state_ctx.state == APP_IDLE)
      {
        _dfu_state_ctx.state = APP_DETACH;
        if ((_dfu_state_ctx.attrs & DFU_FUNC_ATTR_WILL_DETACH_BITMASK) == 1)
        {
          tud_dfu_runtime_reboot_to_dfu_cb();
        } else {
          tud_dfu_runtime_detach_start_timer_cb(request->wValue);
        }
      } else {
        TU_LOG2("  DFU Unexpected request during state %s: %u\r\n", tu_lookup_find(&_dfu_mode_state_table, _dfu_state_ctx.state), request->bRequest);
        return false;
      }
    }
    break;

    case DFU_REQUEST_GETSTATUS:
    {
      dfu_status_req_payload_t resp;
      resp.bStatus = _dfu_state_ctx.status;
      memset((uint8_t *)&resp.bwPollTimeout, 0x00, 3);  // Value is ignored
      resp.bState = _dfu_state_ctx.state;
      resp.iString = ( tud_dfu_runtime_get_status_desc_table_index_cb ) ? tud_dfu_runtime_get_status_desc_table_index_cb() : 0;

      tud_control_xfer(rhport, request, &resp, sizeof(dfu_status_req_payload_t));
    }
    break;

    case DFU_REQUEST_GETSTATE:
    {
      tud_control_xfer(rhport, request, &_dfu_state_ctx.state, 1);
    }
    break;

    default:
    {
      TU_LOG2("  DFU Nonstandard Runtime Request: %u\r\n", request->bRequest);
      return ( tud_dfu_runtime_req_nonstandard_cb ) ? tud_dfu_runtime_req_nonstandard_cb(rhport, stage, request) : false;
    }
    break;
  }

  return true;
}

void tud_dfu_runtime_set_status(dfu_mode_device_status_t status)
{
  _dfu_state_ctx.status = status;
}

void tud_dfu_runtime_detach_timer_elapsed()
{
  if (_dfu_state_ctx.state == DFU_REQUEST_DETACH)
  {
    _dfu_state_ctx.state = APP_IDLE;
  }
}

#endif

/*
 * SPDX-FileCopyrightText: Copyright (c) 2019 Sylvain Munaut <tnt@246tNt.com>
 * SPDX-FileCopyrightText: Copyright (c) 2019 Ha Thach (tinyusb.org)
 * SPDX-License-Identifier: MIT
 *
 * This file is part of the TinyUSB stack.
 */

#include "tusb_option.h"

#if (CFG_TUD_ENABLED && CFG_TUD_DFU_RUNTIME)

#include "device/usbd.h"
#include "device/usbd_pvt.h"

#include "dfu_rt_device.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+

// Level where CFG_TUSB_DEBUG must be at least for this driver is logged
#ifndef CFG_TUD_DFU_RUNTIME_LOG_LEVEL
  #define CFG_TUD_DFU_RUNTIME_LOG_LEVEL   CFG_TUD_LOG_LEVEL
#endif

#define TU_LOG_DRV(...)   TU_LOG(CFG_TUD_DFU_RUNTIME_LOG_LEVEL, __VA_ARGS__)

//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+
static struct {
  uint8_t attrs; // functional descriptor bmAttributes
  uint8_t state; // APP_IDLE or APP_DETACH
} _dfu_rt;

//--------------------------------------------------------------------+
// USBD Driver API
//--------------------------------------------------------------------+
void dfu_rtd_init(void) {
  tu_varclr(&_dfu_rt);
}

bool dfu_rtd_deinit(void) {
  tu_varclr(&_dfu_rt);
  return true;
}

void dfu_rtd_reset(uint8_t rhport) {
  (void) rhport;
  _dfu_rt.state = APP_IDLE;
}

// DFU 1.1 5.1: without bitWillDetach, the bus reset after DFU_DETACH is the cue to enter DFU mode
void dfu_rtd_bus_reset(void) {
  if (_dfu_rt.state == APP_DETACH && !(_dfu_rt.attrs & DFU_ATTR_WILL_DETACH)) {
    _dfu_rt.state = APP_IDLE; // consume first: the callback may not return, or may run tud_task()
    tud_dfu_runtime_reboot_to_dfu_cb();
  }
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
    _dfu_rt.attrs = ((tusb_desc_dfu_functional_t const *) p_desc)->bAttributes;
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
  TU_VERIFY(request->bmRequestType_bit.recipient == TUSB_REQ_RCPT_INTERFACE);

  if (stage == CONTROL_STAGE_ACK && request->bmRequestType_bit.type == TUSB_REQ_TYPE_CLASS &&
      request->bRequest == DFU_REQUEST_DETACH) {
    // status stage done: the host has its answer, so detaching or rebooting is now safe
    _dfu_rt.state = APP_DETACH;
    if (_dfu_rt.attrs & DFU_ATTR_WILL_DETACH) {
      tud_dfu_runtime_reboot_to_dfu_cb();
    }
  }

  // nothing else to do with DATA or ACK stage
  if ( stage != CONTROL_STAGE_SETUP ) return true;

  // dfu-util will try to claim the interface with SET_INTERFACE request before sending DFU request
  if ( TUSB_REQ_TYPE_STANDARD == request->bmRequestType_bit.type &&
       TUSB_REQ_SET_INTERFACE == request->bRequest )
  {
    tud_control_status(rhport, request);
    return true;
  }

  // Handle class request only from here
  TU_VERIFY(request->bmRequestType_bit.type == TUSB_REQ_TYPE_CLASS);

  switch (request->bRequest)
  {
    case DFU_REQUEST_DETACH:
    {
      TU_LOG_DRV("  DFU RT Request: DETACH\r\n");
      tud_control_status(rhport, request);
    }
    break;

    case DFU_REQUEST_GETSTATUS:
    {
      TU_LOG_DRV("  DFU RT Request: GETSTATUS\r\n");
      TU_VERIFY(request->bmRequestType_bit.direction == TUSB_DIR_IN);
      dfu_status_response_t resp;
      // Status = OK, Poll timeout is ignored during RT, IString = 0
      TU_VERIFY(tu_memset_s(&resp, sizeof(resp), 0x00, sizeof(resp))==0);
      resp.bState = _dfu_rt.state;
      tud_control_xfer(rhport, request, &resp, sizeof(dfu_status_response_t));
    }
    break;

    case DFU_REQUEST_GETSTATE:
    {
      TU_LOG_DRV("  DFU RT Request: GETSTATE\r\n");
      TU_VERIFY(request->bmRequestType_bit.direction == TUSB_DIR_IN);
      tud_control_xfer(rhport, request, &_dfu_rt.state, 1);
    }
    break;

    default:
    {
      TU_LOG_DRV("  DFU RT Unexpected Request: %d\r\n", request->bRequest);
      _dfu_rt.state = APP_IDLE; // DFU 1.1 A.2.2: any other request cancels a pending detach
      return false; // stall unsupported request
    }
  }

  return true;
}

#endif

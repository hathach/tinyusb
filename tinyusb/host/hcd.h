/**************************************************************************/
/*!
    @file     hcd.h
    @author   hathach (tinyusb.org)

    @section LICENSE

    Software License Agreement (BSD License)

    Copyright (c) 2013, hathach (tinyusb.org)
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:
    1. Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
    3. Neither the name of the copyright holders nor the
    names of its contributors may be used to endorse or promote products
    derived from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ''AS IS'' AND ANY
    EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    INCLUDING NEGLIGENCE OR OTHERWISE ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

	  This file is part of the tinyusb stack.
*/
/**************************************************************************/

/** \ingroup group_usbh
 * \defgroup Group_HCD Host Controller Driver (HCD)
 *  @{ */

#ifndef _TUSB_HCD_H_
#define _TUSB_HCD_H_

#ifdef __cplusplus
 extern "C" {
#endif

#include "common/common.h"

#if MODE_HOST_SUPPORTED
// Max number of endpoints per device
enum {
  HCD_MAX_ENDPOINT = TUSB_CFG_HOST_HUB + TUSB_CFG_HOST_HID_KEYBOARD + TUSB_CFG_HOST_HID_MOUSE + TUSB_CFG_HOST_HID_GENERIC +
                     TUSB_CFG_HOST_MSC*2 + TUSB_CFG_HOST_CDC*3,

  HCD_MAX_XFER     = HCD_MAX_ENDPOINT*2,
};
#endif

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+
typedef struct {
  uint8_t dev_addr;
  uint8_t xfer_type;
  uint8_t index;
  uint8_t reserved;
} pipe_handle_t;

static inline bool pipehandle_is_valid(pipe_handle_t pipe_hdl) ATTR_CONST ATTR_ALWAYS_INLINE ATTR_WARN_UNUSED_RESULT;
static inline bool pipehandle_is_valid(pipe_handle_t pipe_hdl)
{
  return pipe_hdl.dev_addr > 0;
}

static inline bool pipehandle_is_equal(pipe_handle_t x, pipe_handle_t y) ATTR_CONST ATTR_ALWAYS_INLINE ATTR_WARN_UNUSED_RESULT;
static inline bool pipehandle_is_equal(pipe_handle_t x, pipe_handle_t y)
{
  return (x.dev_addr == y.dev_addr) && (x.xfer_type == y.xfer_type) && (x.index == y.index);
}

//--------------------------------------------------------------------+
// USBH-HCD API
//--------------------------------------------------------------------+
tusb_error_t hcd_init(void) ATTR_WARN_UNUSED_RESULT;
void hcd_isr(uint8_t hostid);

//--------------------------------------------------------------------+
// PIPE API
//--------------------------------------------------------------------+
// TODO control xfer should be used via usbh layer
tusb_error_t  hcd_pipe_control_open(uint8_t dev_addr, uint8_t max_packet_size) ATTR_WARN_UNUSED_RESULT;
tusb_error_t  hcd_pipe_control_xfer(uint8_t dev_addr, tusb_control_request_t const * p_request, uint8_t data[]) ATTR_WARN_UNUSED_RESULT;
tusb_error_t  hcd_pipe_control_close(uint8_t dev_addr) ATTR_WARN_UNUSED_RESULT;

pipe_handle_t hcd_pipe_open(uint8_t dev_addr, tusb_descriptor_endpoint_t const * endpoint_desc, uint8_t class_code) ATTR_WARN_UNUSED_RESULT;
tusb_error_t  hcd_pipe_queue_xfer(pipe_handle_t pipe_hdl, uint8_t buffer[], uint16_t total_bytes) ATTR_WARN_UNUSED_RESULT; // only queue, not transferring yet
tusb_error_t  hcd_pipe_xfer(pipe_handle_t pipe_hdl, uint8_t buffer[], uint16_t total_bytes, bool int_on_complete)  ATTR_WARN_UNUSED_RESULT;
tusb_error_t  hcd_pipe_close(pipe_handle_t pipe_hdl) /*ATTR_WARN_UNUSED_RESULT*/;

bool hcd_pipe_is_busy(pipe_handle_t pipe_hdl) ATTR_PURE;
bool hcd_pipe_is_error(pipe_handle_t pipe_hdl) ATTR_PURE;
bool hcd_pipe_is_stalled(pipe_handle_t pipe_hdl) ATTR_PURE; // stalled also counted as error

uint8_t hcd_pipe_get_endpoint_addr(pipe_handle_t pipe_hdl) ATTR_PURE;
tusb_error_t hcd_pipe_clear_stall(pipe_handle_t pipe_hdl);

#if 0
tusb_error_t hcd_pipe_cancel()ATTR_WARN_UNUSED_RESULT;
#endif

//--------------------------------------------------------------------+
// PORT API
//--------------------------------------------------------------------+
/// return the current connect status of roothub port
bool hcd_port_connect_status(uint8_t hostid) ATTR_PURE ATTR_WARN_UNUSED_RESULT; // TODO make inline if possible
void hcd_port_reset(uint8_t hostid);
tusb_speed_t hcd_port_speed_get(uint8_t hostid) ATTR_PURE ATTR_WARN_UNUSED_RESULT; // TODO make inline if possible
void hcd_port_unplug(uint8_t hostid); // called by usbh to instruct hcd that it can execute unplug procedure

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_HCD_H_ */

/// @}

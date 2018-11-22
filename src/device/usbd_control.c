/**************************************************************************/
/*!
    @file     usbd_control.c
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
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

    This file is part of the tinyusb stack.
*/
/**************************************************************************/

#include "tusb_option.h"

#if TUSB_OPT_DEVICE_ENABLED

#define _TINY_USB_SOURCE_FILE_

#include "tusb.h"
#include "device/usbd_pvt.h"

enum
{
  EDPT_CTRL_OUT = 0x00,
  EDPT_CTRL_IN  = 0x80
};

typedef struct
{
  tusb_control_request_t request;

  void* buffer;
  uint16_t total_len;
  uint16_t total_transferred;

  bool (*complete_cb) (uint8_t, tusb_control_request_t const *);
} usbd_control_xfer_t;

static usbd_control_xfer_t _control_state;

CFG_TUSB_MEM_SECTION CFG_TUSB_MEM_ALIGN uint8_t _usbd_ctrl_buf[CFG_TUD_ENDOINT0_SIZE];

void usbd_control_reset (uint8_t rhport)
{
  tu_varclr(&_control_state);
}

void usbd_control_stall(uint8_t rhport)
{
  dcd_edpt_stall(rhport, 0);
}

bool usbd_control_status(uint8_t rhport, tusb_control_request_t const * request)
{
  // status direction is reversed to one in the setup packet
  return dcd_edpt_xfer(rhport, request->bmRequestType_bit.direction ? EDPT_CTRL_OUT : EDPT_CTRL_IN, NULL, 0);
}


// Each transaction is up to endpoint0's max packet size
static bool start_control_data_xact(uint8_t rhport)
{
  uint16_t const xact_len = tu_min16(_control_state.total_len - _control_state.total_transferred, CFG_TUD_ENDOINT0_SIZE);

  uint8_t ep_addr = EDPT_CTRL_OUT;

  if ( _control_state.request.bmRequestType_bit.direction == TUSB_DIR_IN )
  {
    ep_addr = EDPT_CTRL_IN;
    memcpy(_usbd_ctrl_buf, _control_state.buffer, xact_len);
  }

  return dcd_edpt_xfer(rhport, ep_addr, _usbd_ctrl_buf, xact_len);
}

// TODO may find a better way
void usbd_control_set_complete_callback( bool (*fp) (uint8_t, tusb_control_request_t const * ) )
{
  _control_state.complete_cb = fp;
}

bool usbd_control_xfer(uint8_t rhport, tusb_control_request_t const * request, void* buffer, uint16_t len)
{
  _control_state.request = (*request);
  _control_state.buffer = buffer;
  _control_state.total_len = tu_min16(len, request->wLength);
  _control_state.total_transferred = 0;

  if ( buffer != NULL && len )
  {
    // Data stage
    TU_ASSERT( start_control_data_xact(rhport) );
  }else
  {
    // Status stage
    TU_ASSERT( usbd_control_status(rhport, request) );
  }

  return true;
}

// callback when a transaction complete on DATA stage of control endpoint
bool usbd_control_xfer_cb (uint8_t rhport, uint8_t ep_addr, tusb_event_t event, uint32_t xferred_bytes)
{
  if ( _control_state.request.bmRequestType_bit.direction == TUSB_DIR_OUT )
  {
    memcpy(_control_state.buffer, _usbd_ctrl_buf, xferred_bytes);
  }

  _control_state.total_transferred += xferred_bytes;
  _control_state.buffer += xferred_bytes;

  if ( _control_state.total_len == _control_state.total_transferred || xferred_bytes < CFG_TUD_ENDOINT0_SIZE )
  {
    // DATA stage is complete
    bool is_ok = true;

    // invoke complete callback if set
    // callback can still stall control in status phase e.g out data does not make sense
    if ( _control_state.complete_cb )
    {
      is_ok = _control_state.complete_cb(rhport, &_control_state.request);
    }

    if ( is_ok )
    {
      // Send status
      TU_ASSERT( usbd_control_status(rhport, &_control_state.request) );
    }else
    {
      // stall due to callback
      usbd_control_stall(rhport);
    }
  }
  else
  {
    // More data to transfer
    TU_ASSERT( start_control_data_xact(rhport) );
  }

  return true;
}

#endif

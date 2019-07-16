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

#include "tusb_option.h"

#if TUSB_OPT_DEVICE_ENABLED

#include "tusb.h"
#include "device/usbd_pvt.h"
#include "dcd.h"

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
  (void) rhport;
  tu_varclr(&_control_state);
}

bool tud_control_status(uint8_t rhport, tusb_control_request_t const * request)
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

bool tud_control_xfer(uint8_t rhport, tusb_control_request_t const * request, void* buffer, uint16_t len)
{
  _control_state.request = (*request);
  _control_state.buffer = buffer;
  _control_state.total_len = tu_min16(len, request->wLength);
  _control_state.total_transferred = 0;

  if ( len )
  {
    TU_ASSERT(buffer);

    // Data stage
    TU_ASSERT( start_control_data_xact(rhport) );
  }else
  {
    // Status stage
    TU_ASSERT( tud_control_status(rhport, request) );
  }

  return true;
}

// callback when a transaction complete on DATA stage of control endpoint
bool usbd_control_xfer_cb (uint8_t rhport, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes)
{
  (void) result;
  (void) ep_addr;

  if ( _control_state.request.bmRequestType_bit.direction == TUSB_DIR_OUT )
  {
    TU_VERIFY(_control_state.buffer);
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
      TU_ASSERT( tud_control_status(rhport, &_control_state.request) );
    }else
    {
      // Stall both IN and OUT control endpoint
      dcd_edpt_stall(rhport, EDPT_CTRL_OUT);
      dcd_edpt_stall(rhport, EDPT_CTRL_IN);
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

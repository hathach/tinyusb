/**************************************************************************/
/*!
    @file     usbd.c
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
#include "control.h"
#include "device/usbd_pvt.h"

control_t control_state;

void controld_reset(uint8_t rhport) {
    control_state.current_stage = CONTROL_STAGE_SETUP;
}

void controld_init(void) {
}

// Helper to send STATUS (zero length) packet
// Note dir is value of direction bit in setup packet (i.e DATA stage direction)
static inline bool dcd_control_status(uint8_t rhport, uint8_t dir)
{
  uint8_t ep_addr = 0;
  // Invert the direction.
  if (dir == TUSB_DIR_OUT) {
    ep_addr |= TUSB_DIR_IN_MASK;
  }
  // status direction is reversed to one in the setup packet
  return dcd_edpt_xfer(rhport, ep_addr, NULL, 0);
}

static inline void dcd_control_stall(uint8_t rhport)
{
  dcd_edpt_stall(rhport, 0 | TUSB_DIR_IN_MASK);
}


// return len of descriptor and change pointer to descriptor's buffer
static uint16_t get_descriptor(uint8_t rhport, tusb_control_request_t const * const p_request, uint8_t const ** pp_buffer)
{
  (void) rhport;

  tusb_desc_type_t const desc_type = (tusb_desc_type_t) tu_u16_high(p_request->wValue);
  uint8_t const desc_index = tu_u16_low( p_request->wValue );

  uint8_t const * desc_data = NULL ;
  uint16_t len = 0;

  switch(desc_type)
  {
    case TUSB_DESC_DEVICE:
      desc_data = (uint8_t const *) usbd_desc_set->device;
      len       = sizeof(tusb_desc_device_t);
    break;

    case TUSB_DESC_CONFIGURATION:
      desc_data = (uint8_t const *) usbd_desc_set->config;
      len       = ((tusb_desc_configuration_t const*) desc_data)->wTotalLength;
    break;

    case TUSB_DESC_STRING:
      // String Descriptor always uses the desc set from user
      if ( desc_index < tud_desc_set.string_count )
      {
        desc_data = tud_desc_set.string_arr[desc_index];
        TU_VERIFY( desc_data != NULL, 0 );

        len  = desc_data[0];  // first byte of descriptor is its size
      }else
      {
        // out of range
        /* The 0xee string is indeed a Microsoft USB extension.
         * It can be used to tell Windows what driver it should use for the device !!!
         */
        return 0;
      }
    break;

    case TUSB_DESC_DEVICE_QUALIFIER:
      // TODO If not highspeed capable stall this request otherwise
      // return the descriptor that could work in highspeed
      return 0;
    break;

    default: return 0;
  }

  TU_ASSERT( desc_data != NULL, 0);

  // up to Host's length
  len = tu_min16(p_request->wLength, len );
  (*pp_buffer) = desc_data;

  return len;
}

tusb_error_t controld_xfer_cb(uint8_t rhport, uint8_t edpt_addr, tusb_event_t event, uint32_t xferred_bytes) {
    if (control_state.current_stage == CONTROL_STAGE_STATUS && xferred_bytes == 0) {
        control_state.current_stage = CONTROL_STAGE_SETUP;
        return TUSB_ERROR_NONE;
    }
    tusb_error_t error = TUSB_ERROR_NONE;
    control_state.total_transferred += xferred_bytes;
    tusb_control_request_t const *p_request = &control_state.current_request;

    if (p_request->wLength == control_state.total_transferred || xferred_bytes < 64) {
        control_state.current_stage = CONTROL_STAGE_STATUS;
        dcd_control_status(rhport, p_request->bmRequestType_bit.direction);

        // Do the user callback after queueing the STATUS packet because the callback could be slow.
        if ( TUSB_REQ_RCPT_INTERFACE == p_request->bmRequestType_bit.recipient )
        {
          tud_control_interface_control_complete_cb(rhport, tu_u16_low(p_request->wIndex), p_request);
        }
    } else {
        if (TUSB_REQ_RCPT_INTERFACE == p_request->bmRequestType_bit.recipient) {
          error = tud_control_interface_control_cb(rhport, tu_u16_low(p_request->wIndex), p_request, control_state.total_transferred);
        } else {
          error = controld_process_control_request(rhport, p_request, control_state.total_transferred);
        }
    }
    return error;
}

// This tracks the state of a control request.
tusb_error_t controld_process_setup_request(uint8_t rhport, tusb_control_request_t const * p_request) {
  tusb_error_t error = TUSB_ERROR_NONE;
  memcpy(&control_state.current_request, p_request, sizeof(tusb_control_request_t));
  if (p_request->wLength == 0) {
      control_state.current_stage = CONTROL_STAGE_STATUS;
  } else {
      control_state.current_stage = CONTROL_STAGE_DATA;
      control_state.total_transferred = 0;
  }

  if ( TUSB_REQ_RCPT_INTERFACE == p_request->bmRequestType_bit.recipient )
  {
    error = tud_control_interface_control_cb(rhport, tu_u16_low(p_request->wIndex), p_request, 0);
  } else {
    error = controld_process_control_request(rhport, p_request, 0);
  }

  if (error != TUSB_ERROR_NONE) {
    dcd_control_stall(rhport); // Stall errored requests
  } else if (control_state.current_stage == CONTROL_STAGE_STATUS) {
    dcd_control_status(rhport, p_request->bmRequestType_bit.direction);
  }
  return error;
}

// This handles the actual request and its response.
tusb_error_t controld_process_control_request(uint8_t rhport, tusb_control_request_t const * p_request, uint16_t bytes_already_sent)
{
  tusb_error_t error = TUSB_ERROR_NONE;
  uint8_t ep_addr = 0;
  if (p_request->bmRequestType_bit.direction == TUSB_DIR_IN) {
      ep_addr |= TUSB_DIR_IN_MASK;
  }

  //------------- Standard Request e.g in enumeration -------------//
  if( TUSB_REQ_RCPT_DEVICE    == p_request->bmRequestType_bit.recipient &&
      TUSB_REQ_TYPE_STANDARD  == p_request->bmRequestType_bit.type ) {
    switch (p_request->bRequest) {
      case TUSB_REQ_GET_DESCRIPTOR: {
        uint8_t  const * buffer = NULL;
        uint16_t const   len    = get_descriptor(rhport, p_request, &buffer);

        if (len) {
          uint16_t remaining_bytes = len - bytes_already_sent;
          if (remaining_bytes > 64) {
              remaining_bytes = 64;
          }
          memcpy(_shared_control_buffer, buffer + bytes_already_sent, remaining_bytes);
          dcd_edpt_xfer(rhport, ep_addr, _shared_control_buffer, remaining_bytes);
        } else {
          return TUSB_ERROR_FAILED;
        }
        break;
      }
      case TUSB_REQ_GET_CONFIGURATION:
        memcpy(_shared_control_buffer, &control_state.config, 1);
        dcd_edpt_xfer(rhport, ep_addr, _shared_control_buffer, 1);
        break;
      case TUSB_REQ_SET_ADDRESS:
        dcd_set_address(rhport, (uint8_t) p_request->wValue);
        break;
      case TUSB_REQ_SET_CONFIGURATION:
        control_state.config = p_request->wValue;
        tud_control_set_config_cb (rhport, control_state.config);
        break;
      default:
        return TUSB_ERROR_FAILED;
    }
  } else if (p_request->bmRequestType_bit.recipient == TUSB_REQ_RCPT_ENDPOINT &&
             p_request->bmRequestType_bit.type == TUSB_REQ_TYPE_STANDARD) {
    //------------- Endpoint Request -------------//
    switch (p_request->bRequest) {
      case TUSB_REQ_GET_STATUS: {
        uint16_t status = dcd_edpt_stalled(rhport, tu_u16_low(p_request->wIndex)) ? 0x0001 : 0x0000;
        memcpy(_shared_control_buffer, &status, 2);

        dcd_edpt_xfer(rhport, ep_addr, _shared_control_buffer, 2);
        break;
      }
      case TUSB_REQ_CLEAR_FEATURE:
        // only endpoint feature is halted/stalled
        dcd_edpt_clear_stall(rhport, tu_u16_low(p_request->wIndex));
        break;
      case TUSB_REQ_SET_FEATURE:
        // only endpoint feature is halted/stalled
        dcd_edpt_stall(rhport, tu_u16_low(p_request->wIndex));
        break;
      default:
        return TUSB_ERROR_FAILED;
    }
  } else {
    //------------- Unsupported Request -------------//
    return TUSB_ERROR_FAILED;
  }
  return error;
}

#endif

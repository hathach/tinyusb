/**************************************************************************/
/*!
    @file     dcd.h
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

/** \ingroup group_usbd
 * \defgroup group_dcd Device Controller Driver (DCD)
 *  @{ */

#ifndef _TUSB_DCD_H_
#define _TUSB_DCD_H_

#include "common/tusb_common.h"

#ifdef __cplusplus
 extern "C" {
#endif

enum
{
  DCD_XFER_SUCCESS = 0,
  DCD_XFER_FAILED,
  DCD_XFER_STALLED
};

typedef enum
{
  DCD_EVENT_BUS_RESET = 1,
  DCD_EVENT_UNPLUGGED,
  DCD_EVENT_SOF,
  DCD_EVENT_SUSPENDED,
  DCD_EVENT_RESUME,

  DCD_EVENT_SETUP_RECEIVED,
  DCD_EVENT_XFER_COMPLETE,

  USBD_EVT_FUNC_CALL
} dcd_eventid_t;

typedef struct ATTR_ALIGNED(4)
{
  uint8_t rhport;
  uint8_t event_id;

  union {
    // USBD_EVT_SETUP_RECEIVED
    tusb_control_request_t setup_received;

    // USBD_EVT_XFER_COMPLETE
    struct {
      uint8_t  ep_addr;
      uint8_t  result;
      uint32_t len;
    }xfer_complete;

    // USBD_EVT_FUNC_CALL
    struct {
      void (*func) (void*);
      void* param;
    }func_call;
  };
} dcd_event_t;

TU_VERIFY_STATIC(sizeof(dcd_event_t) <= 12, "size is not correct");

/*------------------------------------------------------------------*/
/* Device API (Weak is optional)
 *------------------------------------------------------------------*/
bool dcd_init             (uint8_t rhport);
void dcd_set_address      (uint8_t rhport, uint8_t dev_addr);
void dcd_set_config       (uint8_t rhport, uint8_t config_num);

void dcd_connect          (uint8_t rhport) ATTR_WEAK;
void dcd_disconnect       (uint8_t rhport) ATTR_WEAK;

/*------------------------------------------------------------------*/
/* Event Function
 * Called by DCD to notify USBD
 *------------------------------------------------------------------*/
void dcd_event_handler(dcd_event_t const * event, bool in_isr);

// helper to send bus signal event
static inline void dcd_event_bus_signal (uint8_t rhport, dcd_eventid_t eid, bool in_isr)
{
  dcd_event_t event = { .rhport = 0, .event_id = eid, };
  dcd_event_handler(&event, in_isr);
}

// helper to send setup received
static inline void dcd_event_setup_recieved(uint8_t rhport, uint8_t const * setup, bool in_isr)
{
  dcd_event_t event = { .rhport = 0, .event_id = DCD_EVENT_SETUP_RECEIVED };
  memcpy(&event.setup_received, setup, 8);

  dcd_event_handler(&event, true);
}

// helper to send transfer complete event
static inline void dcd_event_xfer_complete (uint8_t rhport, uint8_t ep_addr, uint32_t xferred_bytes, uint8_t result, bool in_isr)
{
  dcd_event_t event = { .rhport = 0, .event_id = DCD_EVENT_XFER_COMPLETE };

  event.xfer_complete.ep_addr = ep_addr;
  event.xfer_complete.len     = xferred_bytes;
  event.xfer_complete.result  = result;

  dcd_event_handler(&event, in_isr);
}


/*------------------------------------------------------------------*/
/* Endpoint API
 *------------------------------------------------------------------*/

//------------- Non-control Endpoints -------------//
bool dcd_edpt_open        (uint8_t rhport, tusb_desc_endpoint_t const * p_endpoint_desc);
bool dcd_edpt_xfer        (uint8_t rhport, uint8_t ep_addr, uint8_t * buffer, uint16_t total_bytes);
bool dcd_edpt_busy        (uint8_t rhport, uint8_t ep_addr);

void dcd_edpt_stall       (uint8_t rhport, uint8_t ep_addr);
void dcd_edpt_clear_stall (uint8_t rhport, uint8_t ep_addr);
bool dcd_edpt_stalled     (uint8_t rhport, uint8_t ep_addr);

//------------- Control Endpoint -------------//
bool dcd_control_xfer     (uint8_t rhport, uint8_t dir, uint8_t * buffer, uint16_t length);

// Helper to send STATUS (zero length) packet
// Note dir is value of direction bit in setup packet (i.e DATA stage direction)
static inline bool dcd_control_status(uint8_t rhport, uint8_t dir)
{
  // status direction is reversed to one in the setup packet
  return dcd_control_xfer(rhport, 1-dir, NULL, 0);
}

static inline void dcd_control_stall(uint8_t rhport)
{
  dcd_edpt_stall(rhport, 0);
}

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_DCD_H_ */

/// @}

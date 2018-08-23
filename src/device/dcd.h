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

typedef enum
{
  USBD_BUS_EVENT_RESET = 1,
  USBD_BUS_EVENT_UNPLUGGED,
  USBD_BUS_EVENT_SOF,
  USBD_BUS_EVENT_SUSPENDED,
  USBD_BUS_EVENT_RESUME
}usbd_bus_event_type_t;

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
void dcd_bus_event        (uint8_t rhport, usbd_bus_event_type_t bus_event);
void dcd_setup_received   (uint8_t rhport, uint8_t const* p_request);
void dcd_xfer_complete    (uint8_t rhport, uint8_t ep_addr, uint32_t xferred_bytes, bool succeeded);

static inline void dcd_control_complete(uint8_t rhport, uint32_t xferred_bytes)
{
  // all control complete is successful !!
  dcd_xfer_complete(rhport, 0, xferred_bytes, true);
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

// Note input dir is value of direction bit in setup packet (i.e DATA stage direction)
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

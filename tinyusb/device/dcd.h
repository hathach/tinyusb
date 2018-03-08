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

#include "common/common.h"

#ifdef __cplusplus
 extern "C" {
#endif

typedef enum
{
  USBD_BUS_EVENT_RESET = 1,
  USBD_BUS_EVENT_UNPLUGGED,
  USBD_BUS_EVENT_SUSPENDED,
  USBD_BUS_EVENT_RESUME
}usbd_bus_event_type_t;

// TODO move Hal
typedef struct {
  uint8_t coreid;
  uint8_t index; // must be zero to indicate control
} endpoint_handle_t;

static inline bool edpt_equal(endpoint_handle_t x, endpoint_handle_t y)
{
  return (x.coreid == y.coreid) && (x.index == y.index);
}

//------------- Controller API -------------//
bool hal_dcd_init        (uint8_t coreid);
void hal_dcd_connect     (uint8_t coreid);
void hal_dcd_disconnect  (uint8_t coreid);
void hal_dcd_set_address (uint8_t coreid, uint8_t dev_addr);
void hal_dcd_set_config  (uint8_t coreid, uint8_t config_num);

/*------------- Event function -------------*/
void hal_dcd_bus_event(uint8_t coreid, usbd_bus_event_type_t bus_event);
void hal_dcd_setup_received(uint8_t coreid, uint8_t const* p_request);

//------------- PIPE API -------------//
bool hal_dcd_control_xfer(uint8_t coreid, tusb_direction_t dir, uint8_t * p_buffer, uint16_t length, bool int_on_complete);
void hal_dcd_control_stall(uint8_t coreid);

bool hal_dcd_pipe_open(uint8_t coreid, tusb_descriptor_endpoint_t const * p_endpoint_desc, endpoint_handle_t* eh);


tusb_error_t dcd_pipe_queue_xfer(endpoint_handle_t edpt_hdl, uint8_t * buffer, uint16_t total_bytes); // only queue, not transferring yet
tusb_error_t hal_dcd_pipe_xfer(endpoint_handle_t edpt_hdl, uint8_t * buffer, uint16_t total_bytes, bool int_on_complete);

bool dcd_pipe_is_busy(endpoint_handle_t edpt_hdl);

// TODO coreid + endpoint address are part of endpoint handle, not endpoint handle, data toggle also need to be reset
void hal_dcd_pipe_stall(endpoint_handle_t edpt_hdl);
void hal_dcd_pipe_clear_stall(uint8_t coreid, uint8_t edpt_addr);

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_DCD_H_ */

/// @}

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

typedef struct {
  uint8_t coreid;
  uint8_t reserved; // TODO redundant, cannot be control as control uses separated API
  uint8_t index;
  uint8_t class_code;
} endpoint_handle_t;

static inline bool endpointhandle_is_valid(endpoint_handle_t edpt_hdl) ATTR_CONST ATTR_ALWAYS_INLINE ATTR_WARN_UNUSED_RESULT;
static inline bool endpointhandle_is_valid(endpoint_handle_t edpt_hdl)
{
  return (edpt_hdl.class_code != 0);
}

static inline bool endpointhandle_is_equal(endpoint_handle_t x, endpoint_handle_t y) ATTR_CONST ATTR_ALWAYS_INLINE ATTR_WARN_UNUSED_RESULT;
static inline bool endpointhandle_is_equal(endpoint_handle_t x, endpoint_handle_t y)
{
  return (x.coreid == y.coreid) && (x.index == y.index) && (x.class_code == y.class_code);
}

tusb_error_t dcd_init(void) ATTR_WARN_UNUSED_RESULT;

void dcd_isr(uint8_t coreid);

//------------- Controller API -------------//
void dcd_controller_connect           (uint8_t coreid);
void dcd_controller_disconnect        (uint8_t coreid);
void dcd_controller_set_address       (uint8_t coreid, uint8_t dev_addr);
void dcd_controller_set_configuration (uint8_t coreid);

//------------- PIPE API -------------//
tusb_error_t dcd_pipe_control_xfer(uint8_t coreid, tusb_direction_t dir, uint8_t * p_buffer, uint16_t length, bool int_on_complete);
void dcd_pipe_control_stall(uint8_t coreid);

endpoint_handle_t dcd_pipe_open(uint8_t coreid, tusb_descriptor_endpoint_t const * p_endpoint_desc, uint8_t class_code) ATTR_WARN_UNUSED_RESULT;
tusb_error_t dcd_pipe_queue_xfer(endpoint_handle_t edpt_hdl, uint8_t * buffer, uint16_t total_bytes) ATTR_WARN_UNUSED_RESULT; // only queue, not transferring yet
tusb_error_t dcd_pipe_xfer(endpoint_handle_t edpt_hdl, uint8_t * buffer, uint16_t total_bytes, bool int_on_complete)  ATTR_WARN_UNUSED_RESULT;
tusb_error_t dcd_pipe_stall(endpoint_handle_t edpt_hdl);
bool dcd_pipe_is_busy(endpoint_handle_t edpt_hdl) ATTR_WARN_UNUSED_RESULT ;

// TODO coreid + endpoint address are part of endpoint handle, not endpoint handle, data toggle also need to be reset
tusb_error_t dcd_pipe_clear_stall(uint8_t coreid, uint8_t edpt_addr);

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_DCD_H_ */

/// @}

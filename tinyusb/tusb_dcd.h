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
/* Controller API
 *------------------------------------------------------------------*/
bool tusb_dcd_init             (uint8_t rhport);
void tusb_dcd_connect          (uint8_t rhport);
void tusb_dcd_disconnect       (uint8_t rhport);
void tusb_dcd_set_address      (uint8_t rhport, uint8_t dev_addr);
void tusb_dcd_set_config       (uint8_t rhport, uint8_t config_num);

/*------------------------------------------------------------------*/
/* Event Function
 * Called by DCD to notify USBD
 *------------------------------------------------------------------*/
void tusb_dcd_bus_event        (uint8_t rhport, usbd_bus_event_type_t bus_event);
void tusb_dcd_setup_received   (uint8_t rhport, uint8_t const* p_request);
void tusb_dcd_xfer_complete    (uint8_t rhport, uint8_t ep_addr, uint32_t xferred_bytes, bool succeeded);

static inline void tusb_dcd_control_complete(uint8_t rhport)
{
  // TODO all control complete is successful !!
  tusb_dcd_xfer_complete(rhport, 0, 0, true);
}

/*------------------------------------------------------------------*/
/* Endpoint API
 *------------------------------------------------------------------*/
//------------- Control Endpoint -------------//
bool tusb_dcd_control_xfer     (uint8_t rhport, tusb_dir_t dir, uint8_t * buffer, uint16_t length);

//------------- Other Endpoints -------------//
bool tusb_dcd_edpt_open        (uint8_t rhport, tusb_desc_endpoint_t const * p_endpoint_desc);
bool tusb_dcd_edpt_xfer        (uint8_t rhport, uint8_t ep_addr, uint8_t * buffer, uint16_t total_bytes);
bool tusb_dcd_edpt_busy        (uint8_t rhport, uint8_t ep_addr);

void tusb_dcd_edpt_stall       (uint8_t rhport, uint8_t ep_addr);
void tusb_dcd_edpt_clear_stall (uint8_t rhport, uint8_t ep_addr);

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_DCD_H_ */

/// @}

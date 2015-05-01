/**************************************************************************/
/*!
    @file     usbd_dcd.h
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

/** \ingroup group_usbd
 *  @{ */

#ifndef _TUSB_USBD_DCD_H_
#define _TUSB_USBD_DCD_H_

//--------------------------------------------------------------------+
// INCLUDE
//--------------------------------------------------------------------+
#include "common/common.h"

#ifdef _TEST_
#include "dcd.h"
#endif

#ifdef __cplusplus
 extern "C" {
#endif

enum {
  USBD_INTERFACE_NUM_MAX = 16 // USB specs specify up to 16 endpoints per device
};

typedef enum
{
  USBD_BUS_EVENT_RESET = 1,
  USBD_BUS_EVENT_UNPLUGGED,
  USBD_BUS_EVENT_SUSPENDED,
  USBD_BUS_EVENT_RESUME
}usbd_bus_event_type_t;

typedef struct {
  volatile uint8_t state;
  uint8_t interface2class[USBD_INTERFACE_NUM_MAX]; // determine interface number belongs to which class
}usbd_device_info_t;

extern usbd_device_info_t usbd_devices[CONTROLLER_DEVICE_NUMBER];
//--------------------------------------------------------------------+
// callback from DCD ISR
//--------------------------------------------------------------------+
void usbd_dcd_bus_event_isr(uint8_t coreid, usbd_bus_event_type_t bus_event);
void usbd_setup_received_isr(uint8_t coreid, tusb_control_request_t * p_request);
void usbd_xfer_isr(endpoint_handle_t edpt_hdl, tusb_event_t event, uint32_t xferred_bytes);


#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_USBD_DCD_H_ */

/** @} */

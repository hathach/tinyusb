/**************************************************************************/
/*!
    @file     usbh_hcd.h
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

/** \ingroup Group_HCD
 *  @{ */

#ifndef _TUSB_USBH_HCD_H_
#define _TUSB_USBH_HCD_H_

#ifdef __cplusplus
 extern "C" {
#endif

//--------------------------------------------------------------------+
// INCLUDE
//--------------------------------------------------------------------+
#include "common/common.h"
#include "osal/osal.h"

#ifdef _TEST_
#include "hcd.h"
#endif

//--------------------------------------------------------------------+
// USBH-HCD common data structure
//--------------------------------------------------------------------+
typedef struct ATTR_ALIGNED(4){
  uint8_t core_id;
  uint8_t hub_addr;
  uint8_t hub_port;
  uint8_t reserve;
} usbh_enumerate_t;

typedef struct {
  //------------- port -------------//
  uint8_t core_id;
  uint8_t hub_addr;
  uint8_t hub_port;
  uint8_t speed;

  //------------- device descriptor -------------//
  uint16_t vendor_id;
  uint16_t product_id;
  uint8_t  configure_count; // bNumConfigurations alias

  //------------- configuration descriptor -------------//
  uint8_t interface_count; // bNumInterfaces alias

  //------------- device -------------//
  volatile uint8_t state;             // device state, value from enum tusbh_device_state_t
  uint32_t flag_supported_class;      // a bitmap of supported class

  //------------- control pipe -------------//
  struct {
    volatile uint8_t pipe_status;
//    uint8_t xferred_bytes; TODO not yet necessary
    tusb_control_request_t request;

    OSAL_SEM_DEF(semaphore);          // TODO move to semaphore pool ?
    osal_semaphore_handle_t sem_hdl;  // used to synchronize with HCD when control xfer complete

    OSAL_MUTEX_DEF(mutex);            // TODO move to mutex pool ?
    osal_mutex_handle_t mutex_hdl;    // used to exclusively occupy control pipe
  } control;
} usbh_device_info_t;

extern usbh_device_info_t usbh_devices[TUSB_CFG_HOST_DEVICE_MAX+1]; // including zero-address

//--------------------------------------------------------------------+
// callback from HCD ISR
//--------------------------------------------------------------------+
void usbh_xfer_isr(pipe_handle_t pipe_hdl, uint8_t class_code, tusb_event_t event, uint32_t xferred_bytes);
void usbh_hcd_rhport_plugged_isr(uint8_t hostid);
void usbh_hcd_rhport_unplugged_isr(uint8_t hostid);

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_USBH_HCD_H_ */

/** @} */

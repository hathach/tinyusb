/**************************************************************************/
/*!
    @file     usbh.h
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

/** \ingroup group_usbh USB Host Core (USBH)
 *  @{ */

#ifndef _TUSB_USBH_H_
#define _TUSB_USBH_H_

#ifdef __cplusplus
 extern "C" {
#endif

//--------------------------------------------------------------------+
// INCLUDE
//--------------------------------------------------------------------+
#include "osal/osal.h" // TODO refractor move to common.h ?
#include "hcd.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+
typedef enum tusb_interface_status_{
  TUSB_INTERFACE_STATUS_READY = 0,
  TUSB_INTERFACE_STATUS_BUSY,
  TUSB_INTERFACE_STATUS_COMPLETE,
  TUSB_INTERFACE_STATUS_ERROR,
  TUSB_INTERFACE_STATUS_INVALID_PARA
} tusb_interface_status_t;

typedef struct {
  void (* const init) (void);
  tusb_error_t (* const open_subtask)(uint8_t, tusb_descriptor_interface_t const *, uint16_t*);
  void (* const isr) (pipe_handle_t, tusb_event_t, uint32_t);
  void (* const close) (uint8_t);
} host_class_driver_t;
//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+

//--------------------------------------------------------------------+
// APPLICATION API
//--------------------------------------------------------------------+
//tusb_error_t tusbh_configuration_set     (uint8_t dev_addr, uint8_t configure_number) ATTR_WARN_UNUSED_RESULT;
tusb_device_state_t tuh_device_get_state (uint8_t dev_addr) ATTR_WARN_UNUSED_RESULT ATTR_PURE;
static inline bool tuh_device_is_configured(uint8_t dev_addr) ATTR_ALWAYS_INLINE ATTR_WARN_UNUSED_RESULT ATTR_PURE;
static inline bool tuh_device_is_configured(uint8_t dev_addr)
{
  return tuh_device_get_state(dev_addr) == TUSB_DEVICE_STATE_CONFIGURED;
}
uint32_t tuh_device_get_mounted_class_flag(uint8_t dev_addr);

//--------------------------------------------------------------------+
// APPLICATION CALLBACK
//--------------------------------------------------------------------+
ATTR_WEAK uint8_t tuh_device_attached_cb (tusb_descriptor_device_t const *p_desc_device)  ATTR_WARN_UNUSED_RESULT;
ATTR_WEAK void    tuh_device_mount_succeed_cb (uint8_t dev_addr);
ATTR_WEAK void    tuh_device_mount_failed_cb(tusb_error_t error, tusb_descriptor_device_t const *p_desc_device); // TODO refractor remove desc_device

//--------------------------------------------------------------------+
// CLASS-USBH & INTERNAL API
//--------------------------------------------------------------------+
#ifdef _TINY_USB_SOURCE_FILE_


OSAL_TASK_FUNCTION (usbh_enumeration_task, p_task_para);
tusb_error_t usbh_init(void);

tusb_error_t usbh_control_xfer_subtask(uint8_t dev_addr, uint8_t bmRequestType, uint8_t bRequest,
                                       uint16_t wValue, uint16_t wIndex, uint16_t wLength, uint8_t* data);


#endif

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_USBH_H_ */

/** @} */

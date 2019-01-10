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
  uint8_t class_code;

  void (* const init) (void);
  bool (* const open)(uint8_t rhport, uint8_t dev_addr, tusb_desc_interface_t const * itf_desc, uint16_t* outlen);
  void (* const isr) (uint8_t dev_addr, uint8_t ep_addr, xfer_result_t result, uint32_t len);
  void (* const close) (uint8_t);
} host_class_driver_t;
//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+

//--------------------------------------------------------------------+
// APPLICATION API
//--------------------------------------------------------------------+
void tuh_task(void);

tusb_device_state_t tuh_device_get_state (uint8_t dev_addr);
static inline bool tuh_device_is_configured(uint8_t dev_addr)
{
  return tuh_device_get_state(dev_addr) == TUSB_DEVICE_STATE_CONFIGURED;
}

//--------------------------------------------------------------------+
// APPLICATION CALLBACK
//--------------------------------------------------------------------+
ATTR_WEAK uint8_t tuh_device_attached_cb (tusb_desc_device_t const *p_desc_device);

/** Callback invoked when device is mounted (configured) */
ATTR_WEAK void tuh_mount_cb (uint8_t dev_addr);

/** Callback invoked when device is unmounted (bus reset/unplugged) */
ATTR_WEAK void tuh_umount_cb(uint8_t dev_addr);

//--------------------------------------------------------------------+
// CLASS-USBH & INTERNAL API
//--------------------------------------------------------------------+
#ifdef _TINY_USB_SOURCE_FILE_

bool usbh_init(void);

bool usbh_control_xfer (uint8_t dev_addr, tusb_control_request_t* request, uint8_t* data);

#endif

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_USBH_H_ */

/** @} */

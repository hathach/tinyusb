/*
 * usbd_host.h
 *
 *  Created on: Jan 19, 2013
 *      Author: hathach
 */

/*
 * Software License Agreement (BSD License)
 * Copyright (c) 2013, hathach (tinyusb.net)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 * This file is part of the tiny usb stack.
 */

/** \file
 *  \brief TBD
 *
 *  \note TBD
 */

/** \ingroup TBD
 *  \defgroup TBD
 *  \brief TBD
 *
 *  @{
 */

#ifndef _TUSB_USBD_HOST_H_
#define _TUSB_USBD_HOST_H_

#ifdef __cplusplus
 extern "C" {
#endif

//--------------------------------------------------------------------+
// INCLUDE
//--------------------------------------------------------------------+
#include "hcd.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+
enum {
  TUSB_FLAGS_CLASS_UNSPECIFIED          = BIT_(0)    , ///< 0
  TUSB_FLAGS_CLASS_AUDIO                = BIT_(1)    , ///< 1
  TUSB_FLAGS_CLASS_CDC                  = BIT_(2)    , ///< 2
  TUSB_FLAGS_CLASS_HID_GENERIC          = BIT_(3)    , ///< 3
  TUSB_FLAGS_CLASS_RESERVED_4           = BIT_(4)    , ///< 4
  TUSB_FLAGS_CLASS_PHYSICAL             = BIT_(5)    , ///< 5
  TUSB_FLAGS_CLASS_IMAGE                = BIT_(6)    , ///< 6
  TUSB_FLAGS_CLASS_PRINTER              = BIT_(7)    ,  ///< 7
  TUSB_FLAGS_CLASS_MSC                  = BIT_(8)    ,  ///< 8
  TUSB_FLAGS_CLASS_HUB                  = BIT_(9)    ,  ///< 9
  TUSB_FLAGS_CLASS_CDC_DATA             = BIT_(10)   ,  ///< 10
  TUSB_FLAGS_CLASS_SMART_CARD           = BIT_(11)   ,  ///< 11
  TUSB_FLAGS_CLASS_RESERVED_12          = BIT_(12)   , ///< 12
  TUSB_FLAGS_CLASS_CONTENT_SECURITY     = BIT_(13)   ,  ///< 13
  TUSB_FLAGS_CLASS_VIDEO                = BIT_(14)   ,  ///< 14
  TUSB_FLAGS_CLASS_PERSONAL_HEALTHCARE  = BIT_(15)   ,  ///< 15
  TUSB_FLAGS_CLASS_AUDIO_VIDEO          = BIT_(16)   ,  ///< 16
  // reserved from 17 to 20
  TUSB_FLAGS_CLASS_RESERVED_20          = BIT_(20)    , ///< 3

  TUSB_FLAGS_CLASS_HID_KEYBOARD         = BIT_(21)    , ///< 3
  TUSB_FLAGS_CLASS_HID_MOUSE            = BIT_(22)    , ///< 3

  // reserved from 25 to 26
  TUSB_FLAGS_CLASS_RESERVED_25          = BIT_(25)    , ///< 3
  TUSB_FLAGS_CLASS_RESERVED_26          = BIT_(26)    , ///< 3

  TUSB_FLAGS_CLASS_DIAGNOSTIC           = BIT_(27),
  TUSB_FLAGS_CLASS_WIRELESS_CONTROLLER  = BIT_(28),
  TUSB_FLAGS_CLASS_MISC                 = BIT_(29),
  TUSB_FLAGS_CLASS_APPLICATION_SPECIFIC = BIT_(30),
  TUSB_FLAGS_CLASS_VENDOR_SPECIFIC      = BIT_(31)
};

/// Device Status
enum {
  TUSB_DEVICE_STATUS_UNPLUG = 0,
  TUSB_DEVICE_STATUS_READY = BIT_(0),

  TUSB_DEVICE_STATUS_REMOVING = BIT_(2),
  TUSB_DEVICE_STATUS_SAFE_REMOVE = BIT_(3),
};

typedef uint8_t  tusbh_device_status_t;
typedef uint32_t tusbh_flag_class_t;

typedef struct { // TODO internal structure, re-order members
  uint8_t core_id;
  tusb_speed_t speed;
  uint8_t hub_addr;
  uint8_t hub_port;

  uint16_t vendor_id;
  uint16_t product_id;
  uint8_t configure_count;

  tusbh_device_status_t status;
  pipe_handle_t pipe_control;
  tusb_std_request_t request_control;

#if 0 // TODO allow configure for vendor/product
  struct {
    uint8_t interface_count;
    uint8_t attributes;
  } configuration;
#endif

} usbh_device_info_t;

typedef enum {
  PIPE_STATUS_AVAILABLE = 0,
  PIPE_STATUS_BUSY,
  PIPE_STATUS_COMPLETE
} pipe_status_t;

typedef uint32_t tusb_handle_device_t;

typedef struct ATTR_ALIGNED(4){
  uint8_t core_id;
  uint8_t hub_address;
  uint8_t hub_port;
  uint8_t connect_status;
} usbh_enumerate_t;

//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+

//--------------------------------------------------------------------+
// APPLICATION API
//--------------------------------------------------------------------+
void         tusbh_device_mounting_cb (tusb_error_t const error, tusb_handle_device_t const device_hdl);
void         tusbh_device_mounted_cb (tusb_error_t const error, tusb_handle_device_t const device_hdl);
tusb_error_t tusbh_configuration_set     (tusb_handle_device_t const device_hdl, uint8_t const configure_number) ATTR_WARN_UNUSED_RESULT;
tusbh_device_status_t tusbh_device_status_get (tusb_handle_device_t const device_hdl) ATTR_WARN_UNUSED_RESULT;


//--------------------------------------------------------------------+
// CLASS-USBD API
//--------------------------------------------------------------------+
#ifdef _TINY_USB_SOURCE_FILE_

tusb_error_t usbh_init(void);
pipe_status_t usbh_pipe_status_get(pipe_handle_t pipe_hdl) ATTR_WARN_UNUSED_RESULT;
void usbh_enum_task(void);

#endif

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_USBD_HOST_H_ */

/** @} */

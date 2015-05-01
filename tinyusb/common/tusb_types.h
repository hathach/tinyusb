/**************************************************************************/
/*!
    @file     tusb_types.h
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

/** \ingroup group_usb_definitions
 *  \defgroup USBDef_Type USB Types
 *  @{ */

#ifndef _TUSB_TUSB_TYPES_H_
#define _TUSB_TUSB_TYPES_H_

#ifdef __cplusplus
 extern "C" {
#endif

/// defined base on EHCI specs value for Endpoint Speed
typedef enum
{
  TUSB_SPEED_FULL = 0,
  TUSB_SPEED_LOW     ,
  TUSB_SPEED_HIGH
}tusb_speed_t;

/// defined base on USB Specs Endpoint's bmAttributes
typedef enum
{
  TUSB_XFER_CONTROL = 0 ,
  TUSB_XFER_ISOCHRONOUS ,
  TUSB_XFER_BULK        ,
  TUSB_XFER_INTERRUPT
}tusb_xfer_type_t;

typedef enum
{
  TUSB_DIR_HOST_TO_DEV      = 0,
  TUSB_DIR_DEV_TO_HOST      = 1,

  TUSB_DIR_DEV_TO_HOST_MASK = 0x80
}tusb_direction_t;


/// USB Descriptor Types (section 9.4 table 9-5)
typedef enum
{
  TUSB_DESC_TYPE_DEVICE                    = 0x01 ,
  TUSB_DESC_TYPE_CONFIGURATION             = 0x02 ,
  TUSB_DESC_TYPE_STRING                    = 0x03 ,
  TUSB_DESC_TYPE_INTERFACE                 = 0x04 ,
  TUSB_DESC_TYPE_ENDPOINT                  = 0x05 ,
  TUSB_DESC_TYPE_DEVICE_QUALIFIER          = 0x06 ,
  TUSB_DESC_TYPE_OTHER_SPEED_CONFIGURATION = 0x07 ,
  TUSB_DESC_TYPE_INTERFACE_POWER           = 0x08 ,
  TUSB_DESC_TYPE_OTG                       = 0x09 ,
  TUSB_DESC_TYPE_DEBUG                     = 0x0A ,
  TUSB_DESC_TYPE_INTERFACE_ASSOCIATION     = 0x0B ,
  TUSB_DESC_TYPE_INTERFACE_CLASS_SPECIFIC  = 0x24
}tusb_std_descriptor_type_t;

typedef enum
{
  TUSB_REQUEST_GET_STATUS =0     , ///< 0
  TUSB_REQUEST_CLEAR_FEATURE     , ///< 1
  TUSB_REQUEST_RESERVED          , ///< 2
  TUSB_REQUEST_SET_FEATURE       , ///< 3
  TUSB_REQUEST_RESERVED2         , ///< 4
  TUSB_REQUEST_SET_ADDRESS       , ///< 5
  TUSB_REQUEST_GET_DESCRIPTOR    , ///< 6
  TUSB_REQUEST_SET_DESCRIPTOR    , ///< 7
  TUSB_REQUEST_GET_CONFIGURATION , ///< 8
  TUSB_REQUEST_SET_CONFIGURATION , ///< 9
  TUSB_REQUEST_GET_INTERFACE     , ///< 10
  TUSB_REQUEST_SET_INTERFACE     , ///< 11
  TUSB_REQUEST_SYNCH_FRAME         ///< 12
}tusb_std_request_code_t;

typedef enum
{
  TUSB_REQUEST_TYPE_STANDARD = 0,
  TUSB_REQUEST_TYPE_CLASS,
  TUSB_REQUEST_TYPE_VENDOR
} tusb_control_request_type_t;

typedef enum
{
  TUSB_REQUEST_RECIPIENT_DEVICE =0,
  TUSB_REQUEST_RECIPIENT_INTERFACE,
  TUSB_REQUEST_RECIPIENT_ENDPOINT,
  TUSB_REQUEST_RECIPIENT_OTHER
} tusb_std_request_recipient_t;

typedef enum
{
  TUSB_CLASS_UNSPECIFIED          = 0    , ///< 0
  TUSB_CLASS_AUDIO                = 1    , ///< 1
  TUSB_CLASS_CDC                  = 2    , ///< 2
  TUSB_CLASS_HID                  = 3    , ///< 3
  TUSB_CLASS_RESERVED_4           = 4    , ///< 4
  TUSB_CLASS_PHYSICAL             = 5    , ///< 5
  TUSB_CLASS_IMAGE                = 6    , ///< 6
  TUSB_CLASS_PRINTER              = 7    , ///< 7
  TUSB_CLASS_MSC                  = 8    , ///< 8
  TUSB_CLASS_HUB                  = 9    , ///< 9
  TUSB_CLASS_CDC_DATA             = 10   , ///< 10
  TUSB_CLASS_SMART_CARD           = 11   , ///< 11
  TUSB_CLASS_RESERVED_12          = 12   , ///< 12
  TUSB_CLASS_CONTENT_SECURITY     = 13   , ///< 13
  TUSB_CLASS_VIDEO                = 14   , ///< 14
  TUSB_CLASS_PERSONAL_HEALTHCARE  = 15   , ///< 15
  TUSB_CLASS_AUDIO_VIDEO          = 16   , ///< 16

  TUSB_CLASS_MAPPED_INDEX_START   = 17   , // TODO Map DIAGNOSTIC, WIRELESS_CONTROLLER, MISC, VENDOR_SPECIFIC to this to minimize the array

  TUSB_CLASS_DIAGNOSTIC           = 0xDC ,
  TUSB_CLASS_WIRELESS_CONTROLLER  = 0xE0 ,
  TUSB_CLASS_MISC                 = 0xEF ,
  TUSB_CLASS_APPLICATION_SPECIFIC = 0xFE ,
  TUSB_CLASS_VENDOR_SPECIFIC      = 0xFF
}tusb_std_class_code_t;

typedef enum
{
  MISC_SUBCLASS_COMMON = 2
}misc_subclass_type_t;

typedef enum
{
  MISC_PROTOCOL_IAD = 1
}misc_protocol_type_t;

enum {
  TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP = BIT_(5),
  TUSB_DESC_CONFIG_ATT_SELF_POWER    = BIT_(6),
  TUSB_DESC_CONFIG_ATT_BUS_POWER     = BIT_(7)
};

#define TUSB_DESC_CONFIG_POWER_MA(x)  ((x)/2)

/// Device State
typedef enum
{
  TUSB_DEVICE_STATE_UNPLUG = 0  ,
  TUSB_DEVICE_STATE_ADDRESSED   ,
  TUSB_DEVICE_STATE_CONFIGURED  ,
  TUSB_DEVICE_STATE_SUSPENDED   ,

  TUSB_DEVICE_STATE_REMOVING    ,
  TUSB_DEVICE_STATE_SAFE_REMOVE ,

  TUSB_DEVICE_STATE_INVALID_PARAMETER
}tusb_device_state_t;

typedef enum
{
  TUSB_EVENT_NONE = 0,
  TUSB_EVENT_XFER_COMPLETE,
  TUSB_EVENT_XFER_ERROR,
  TUSB_EVENT_XFER_STALLED,

  TUSB_EVENT_BUS_RESET, // TODO refractor
  TUSB_EVENT_SETUP_RECEIVED,
}tusb_event_t;

enum {
  DESCRIPTOR_OFFSET_LENGTH = 0,
  DESCRIPTOR_OFFSET_TYPE   = 1
};

enum {
  INTERFACE_INVALID_NUMBER = 0xff
};

static inline uint8_t std_class_code_to_index(uint8_t std_class_code) ATTR_CONST ATTR_ALWAYS_INLINE;
static inline uint8_t std_class_code_to_index(uint8_t std_class_code)
{
  return  (std_class_code <= TUSB_CLASS_AUDIO_VIDEO          ) ? std_class_code                    :
          (std_class_code == TUSB_CLASS_DIAGNOSTIC           ) ? TUSB_CLASS_MAPPED_INDEX_START     :
          (std_class_code == TUSB_CLASS_WIRELESS_CONTROLLER  ) ? TUSB_CLASS_MAPPED_INDEX_START + 1 :
          (std_class_code == TUSB_CLASS_MISC                 ) ? TUSB_CLASS_MAPPED_INDEX_START + 2 :
          (std_class_code == TUSB_CLASS_APPLICATION_SPECIFIC ) ? TUSB_CLASS_MAPPED_INDEX_START + 3 :
          (std_class_code == TUSB_CLASS_VENDOR_SPECIFIC      ) ? TUSB_CLASS_MAPPED_INDEX_START + 4 : 0;
}


#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_TUSB_TYPES_H_ */

/** @} */

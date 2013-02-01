/*
 * tusb_option.h
 *
 *  Created on: Nov 26, 2012
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
 * This file is part of the tinyUSB stack.
 */

/** \file
 *  \brief Configure File
 *
 *  \note TBD
 */

/** 
 *  \defgroup Group_TinyUSB_Configure Configuration tusb_option.h
 *  @{
 */

#ifndef _TUSB_TUSB_OPTION_H_
#define _TUSB_TUSB_OPTION_H_

#ifdef __cplusplus
 extern "C" {
#endif

#define TUSB_VERSION_YEAR   00
#define TUSB_VERSION_MONTH  00
#define TUSB_VERSION_WEEK   0
#define TUSB_VERSION_NAME   "alpha"
#define TUSB_VERSION        XSTRING_(TUSB_VERSION_YEAR) "." XSTRING_(TUSB_VERSION_MONTH)

/// define this symbol will make tinyusb look for external configure file
#include "tusb_config.h"

//--------------------------------------------------------------------+
// COMMON OPTIONS
//--------------------------------------------------------------------+
/// 0: no debug information 3: most debug information provided
#ifndef TUSB_CFG_DEBUG
  #define TUSB_CFG_DEBUG 3
  #warning TUSB_CFG_DEBUG is not defined, default value is 3
#endif

/// USB RAM Section Placement, MCU's usb controller often has limited access to specific RAM region. This will be used to declare internal variables as follow:
/// uint8_t tinyusb_data[10] TUSB_CFG_ATTR_USBRAM;
/// if your mcu's usb controller has no such limit, define TUSB_CFG_ATTR_USBRAM as empty macro.
#ifndef TUSB_CFG_ATTR_USBRAM
 #error TUSB_CFG_ATTR_USBRAM is not defined, please help me know how to place data in accessible RAM for usb controller
#endif

#if TUSB_CFG_OS == TUSB_OS_NONE
  #ifndef TUSB_CFG_OS_TICKS_PER_SECOND
    #error TUSB_CFG_OS_TICKS_PER_SECOND is required to use with OS_NONE
  #endif
#endif

#ifndef TUSB_CFG_CONFIGURATION_MAX
  #define TUSB_CFG_CONFIGURATION_MAX 1
  #warning TUSB_CFG_CONFIGURATION_MAX is not defined, default value is 1
#endif

//--------------------------------------------------------------------+
// HOST OPTIONS
//--------------------------------------------------------------------+
#ifdef TUSB_CFG_HOST
  #ifndef TUSB_CFG_HOST_CONTROLLER_NUM
    #define TUSB_CFG_HOST_CONTROLLER_NUM 1
    #warning TUSB_CFG_HOST_CONTROLLER_NUM is not defined, default value is 1
  #endif

  #ifndef TUSB_CFG_HOST_DEVICE_MAX
    #define TUSB_CFG_HOST_DEVICE_MAX 1
    #warning TUSB_CFG_HOST_DEVICE_MAX is not defined, default value is 1
  #endif

  #if TUSB_CFG_HOST_HID_KEYBOARD
    #if !defined(TUSB_CFG_HOST_HID_KEYBOARD_ENDPOINT_SIZE)
      #define TUSB_CFG_HOST_HID_KEYBOARD_ENDPOINT_SIZE 64
      #warning TUSB_CFG_HOST_HID_KEYBOARD_ENDPOINT_SIZE is not defined, default value is 64
    #endif

    #if !defined(TUSB_CFG_HOST_HID_KEYBOARD_NO_INSTANCES_PER_DEVICE)
      #define TUSB_CFG_HOST_HID_KEYBOARD_NO_INSTANCES_PER_DEVICE 1
    #endif
  #endif // end TUSB_CFG_HOST_HID_KEYBOARD

  #define HOST_CLASS_HID   ( (defined TUSB_CFG_HOST_HID_KEYBOARD) )
  #define HOST_EHCI
#endif // end TUSB_CFG_HOST

//--------------------------------------------------------------------+
// DEVICE OPTIONS
//--------------------------------------------------------------------+
//#define TUSB_CFG_DEVICE

#define DEVICE_CLASS_HID ( (defined TUSB_CFG_DEVICE_HID_KEYBOARD) || (defined TUSB_CFG_DEVICE_HID_MOUSE) )

// TODO Device APP
#define USB_MAX_IF_NUM          8
#define USB_MAX_EP_NUM          5

#define USB_FS_MAX_BULK_PACKET  64
#define USB_HS_MAX_BULK_PACKET  USB_FS_MAX_BULK_PACKET /* Full speed device only */

// Control Endpoint
#define USB_MAX_PACKET0         64

/* HID In/Out Endpoint Address */
#define    HID_KEYBOARD_EP_IN       USB_ENDPOINT_IN(1)
//#define  HID_KEYBOARD_EP_OUT      USB_ENDPOINT_OUT(1)
#define    HID_MOUSE_EP_IN          USB_ENDPOINT_IN(4)

/* CDC Endpoint Address */
#define  CDC_NOTIFICATION_EP                USB_ENDPOINT_IN(2)
#define  CDC_DATA_EP_OUT                    USB_ENDPOINT_OUT(3)
#define  CDC_DATA_EP_IN                     USB_ENDPOINT_IN(3)

#define  CDC_NOTIFICATION_EP_MAXPACKETSIZE  8
#define  CDC_DATA_EP_MAXPACKET_SIZE         16

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_TUSB_OPTION_H_ */

/** @} */

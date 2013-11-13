/**************************************************************************/
/*!
    @file     tusb_descriptors.h
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

#ifndef _TUSB_DESCRIPTORS_H_
#define _TUSB_DESCRIPTORS_H_

#include "tusb.h"

#define CFG_VENDORID            0x1FC9 // NXP
//#define CFG_PRODUCTID           0x4567 // use auto product id to prevent conflict with pc's driver


#define ENDPOINT_OUT_LOGICAL_TO_PHYSICAL(addr)      (addr)
#define ENDPOINT_IN_LOGICAL_TO_PHYSICAL(addr)       ((addr) | 0x80)

#define INTERFACE_INDEX_CDC           0
#define INTERFACE_INDEX_HID_KEYBOARD (INTERFACE_INDEX_CDC          + 2*TUSB_CFG_DEVICE_CDC        )
#define INTERFACE_INDEX_HID_MOUSE    (INTERFACE_INDEX_HID_KEYBOARD + TUSB_CFG_DEVICE_HID_KEYBOARD )
#define INTERFACE_INDEX_HID_GENERIC  (INTERFACE_INDEX_HID_MOUSE    + TUSB_CFG_DEVICE_HID_MOUSE    )
#define INTERFACE_INDEX_MASS_STORAGE (INTERFACE_INDEX_HID_GENERIC  + TUSB_CFG_DEVICE_HID_GENERIC  )

#define TOTAL_INTEFACES              (2*TUSB_CFG_DEVICE_CDC + TUSB_CFG_DEVICE_HID_KEYBOARD + TUSB_CFG_DEVICE_HID_MOUSE + \
                                      TUSB_CFG_DEVICE_HID_GENERIC + TUSB_CFG_DEVICE_MSC)

/* HID In/Out Endpoint Address */
#define    HID_KEYBOARD_EP_IN       ENDPOINT_IN_LOGICAL_TO_PHYSICAL(1)
#define    HID_MOUSE_EP_IN          ENDPOINT_IN_LOGICAL_TO_PHYSICAL(4)

/* CDC Endpoint Address */
#define  CDC_NOTIFICATION_EP                ENDPOINT_IN_LOGICAL_TO_PHYSICAL(2)
#define  CDC_DATA_EP_OUT                    ENDPOINT_OUT_LOGICAL_TO_PHYSICAL(3)
#define  CDC_DATA_EP_IN                     ENDPOINT_IN_LOGICAL_TO_PHYSICAL(3)

#define  CDC_NOTIFICATION_EP_MAXPACKETSIZE  8
#define  CDC_DATA_EP_MAXPACKET_SIZE         16

#define MSC_EDPT_IN   ENDPOINT_IN_LOGICAL_TO_PHYSICAL(3)
#define MSC_EDPT_OUT  ENDPOINT_OUT_LOGICAL_TO_PHYSICAL(3)

// Interface Assosication Descriptor if device is CDC + other class
#define IAD_DESC_REQUIRED ( 0 && TUSB_CFG_DEVICE_CDC && (TOTAL_INTEFACES > 2) )


// each combination of interfaces need to have different productid, as windows will bind & remember device driver after the
// first plug.
#ifndef CFG_PRODUCTID
// Bitmap: MassStorage | Generic | Mouse | Key | CDC
#define PRODUCTID_BITMAP(interface, n)  ( (TUSB_CFG_DEVICE_##interface) << (n) )
#define CFG_PRODUCTID                   (0x4000 | ( PRODUCTID_BITMAP(CDC, 0) | PRODUCTID_BITMAP(HID_KEYBOARD, 1) | \
                                         PRODUCTID_BITMAP(HID_MOUSE, 2) | PRODUCTID_BITMAP(HID_GENERIC, 3) | \
                                         PRODUCTID_BITMAP(MSC, 4) ) )
#endif

//--------------------------------------------------------------------+
// CONFIGURATION DESCRIPTOR
//--------------------------------------------------------------------+
typedef ATTR_PACKED_STRUCT(struct)
{
  tusb_descriptor_configuration_t                configuration;

  //------------- CDC -------------//
#if TUSB_CFG_DEVICE_CDC
  #if IAD_DESC_REQUIRED
  tusb_descriptor_interface_association_t      cdc_iad;
  #endif

  //CDC Control Interface
  tusb_descriptor_interface_t                  cdc_comm_interface;
  cdc_desc_func_header_t                       cdc_header;
  cdc_desc_func_abstract_control_management_t  cdc_acm;
  cdc_desc_func_union_t                        cdc_union;
  tusb_descriptor_endpoint_t                   cdc_endpoint_notification;

  //CDC Data Interface
  tusb_descriptor_interface_t                  cdc_data_interface;
  tusb_descriptor_endpoint_t                   cdc_endpoint_out;
  tusb_descriptor_endpoint_t                   cdc_endpoint_in;
#endif

  //------------- HID Keyboard -------------//
#if TUSB_CFG_DEVICE_HID_KEYBOARD
  tusb_descriptor_interface_t                    keyboard_interface;
  tusb_hid_descriptor_hid_t                      keyboard_hid;
  tusb_descriptor_endpoint_t                     keyboard_endpoint;
#endif

//------------- HID Mouse -------------//
#if TUSB_CFG_DEVICE_HID_MOUSE
  tusb_descriptor_interface_t                    mouse_interface;
  tusb_hid_descriptor_hid_t                      mouse_hid;
  tusb_descriptor_endpoint_t                     mouse_endpoint;
#endif

//------------- Mass Storage -------------//
#if TUSB_CFG_DEVICE_MSC
  tusb_descriptor_interface_t                    msc_interface;
  tusb_descriptor_endpoint_t                     msc_endpoint_in;
  tusb_descriptor_endpoint_t                     msc_endpoint_out;
#endif

} app_descriptor_configuration_t;

//--------------------------------------------------------------------+
// STRINGS DESCRIPTOR
//--------------------------------------------------------------------+
tusb_descriptor_string_t * const desc_str_table[TUSB_CFG_DEVICE_STRING_DESCRIPTOR_COUNT];

//--------------------------------------------------------------------+
// Export descriptors
//--------------------------------------------------------------------+
extern tusb_descriptor_device_t app_tusb_desc_device;
extern app_descriptor_configuration_t app_tusb_desc_configuration;

extern uint8_t app_tusb_keyboard_desc_report[];
extern uint8_t app_tusb_mouse_desc_report[];

#endif

/*
 * tusb_descriptors.h
 *
 *  Created on: Nov 26, 2012
 *      Author: hathachtware License Agreement (BSD License)
 * Copyright (c) 2012, hathach (tinyusb.org)All rights reserved.
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
 * This file is part of the tinyUSB stack
 */

#ifndef _TUSB_DESCRIPTORS_H_
#define _TUSB_DESCRIPTORS_H_

#include "tusb.h"

#define INTERFACES_OF_CDC           (TUSB_CFG_DEVICE_CDC ? 2 : 0)
#define INTERFACES_OF_HID_KEYBOARD  (TUSB_CFG_DEVICE_HID_KEYBOARD ? 1 : 0)
#define INTERFACES_OF_HID_MOUSE     (TUSB_CFG_DEVICE_HID_MOUSE ? 1 : 0)
#define INTERFACES_OF_HID_GENERIC   (TUSB_CFG_DEVICE_HID_GENERIC ? 1 : 0)
#define INTERFACES_OF_MASS_STORAGE  (TUSB_CFG_DEVICE_MSC ? 1 : 0)

#define INTERFACE_INDEX_CDC           0
#define INTERFACE_INDEX_HID_KEYBOARD (INTERFACE_INDEX_CDC          + INTERFACES_OF_CDC          )
#define INTERFACE_INDEX_HID_MOUSE    (INTERFACE_INDEX_HID_KEYBOARD + INTERFACES_OF_HID_KEYBOARD )
#define INTERFACE_INDEX_HID_GENERIC  (INTERFACE_INDEX_HID_MOUSE    + INTERFACES_OF_HID_MOUSE    )
#define INTERFACE_INDEX_MASS_STORAGE (INTERFACE_INDEX_HID_GENERIC  + INTERFACES_OF_HID_GENERIC  )

#define TOTAL_INTEFACES              (INTERFACES_OF_CDC + INTERFACES_OF_HID_KEYBOARD + INTERFACES_OF_HID_MOUSE + \
                                      INTERFACES_OF_HID_GENERIC + INTERFACES_OF_MASS_STORAGE)

// USB Interface Assosication Descriptor
#define  USB_DEVICE_CLASS_IAD        USB_DEVICE_CLASS_MISCELLANEOUS
#define  USB_DEVICE_SUBCLASS_IAD     0x02
#define  USB_DEVICE_PROTOCOL_IAD     0x01

// Interface Assosication Descriptor if device is CDC + other class
#define IAD_DESC_REQUIRED ( TUSB_CFG_DEVICE_CDC && (TOTAL_INTEFACES > 2) )


// each combination of interfaces need to have different productid, as windows will bind & remember device driver after the
// first plug.
#ifndef TUSB_CFG_PRODUCT_ID
// Bitmap: MassStorage | Generic | Mouse | Key | CDC
#define PRODUCTID_BITMAP(interface, n)  ( (INTERFACES_OF_##interface ? 1 : 0) << (n) )
#define TUSB_CFG_PRODUCT_ID             (0x2000 | ( PRODUCTID_BITMAP(CDC, 0) | PRODUCTID_BITMAP(HID_KEYBOARD, 1) | \
                                         PRODUCTID_BITMAP(HID_MOUSE, 2) | PRODUCTID_BITMAP(HID_GENERIC, 3) | \
                                         PRODUCTID_BITMAP(MASS_STORAGE, 4) ) )
#endif

//--------------------------------------------------------------------+
// CONFIGURATION DESCRIPTOR
//--------------------------------------------------------------------+
typedef ATTR_PACKED_STRUCT(struct)
{
  tusb_descriptor_configuration_t                configuration;

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
typedef ATTR_PACKED_STRUCT(struct)
{
  //------------- index 0 -------------//
  struct {
    uint8_t const bLength;
    uint8_t const bDescriptorType;
    uint16_t id;
  }language;

  //------------- index 1 -------------//
  struct {
    uint8_t const bLength;
    uint8_t const bDescriptorType;
    uint16_t unicode_string[sizeof(TUSB_CFG_DEVICE_STRING_MANUFACTURER)-1]; // exclude null-character
  } manufacturer;

  //------------- index 2 -------------//
  struct {
    uint8_t const bLength;
    uint8_t const bDescriptorType;
    uint16_t unicode_string[sizeof(TUSB_CFG_DEVICE_STRING_PRODUCT)-1]; // exclude null-character
  } product;

  //------------- index 3 -------------//
  struct {
    uint8_t const bLength;
    uint8_t const bDescriptorType;
    uint16_t unicode_string[sizeof(TUSB_CFG_DEVICE_STRING_SERIAL)-1]; // exclude null-character
  } serial;

  //------------- more string index -------------//

} app_descriptor_string_t;

//--------------------------------------------------------------------+
// Export descriptors
//--------------------------------------------------------------------+
extern tusb_descriptor_device_t app_tusb_desc_device;
extern app_descriptor_configuration_t app_tusb_desc_configuration;
extern app_descriptor_string_t app_tusb_desc_strings;

extern uint8_t app_tusb_keyboard_desc_report[];
extern uint8_t app_tusb_mouse_desc_report[];

#endif

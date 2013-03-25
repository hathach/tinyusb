/*
 * descriptor_test.c
 *
 *  Created on: Feb 5, 2013
 *      Author: hathach
 */

/*
 * Software License Agreement (BSD License)
 * Copyright (c) 2012, hathach (tinyusb.org)
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

#include "tusb_option.h"
#include "descriptor_test.h"

TUSB_CFG_ATTR_USBRAM ATTR_ALIGNED(4)
tusb_descriptor_device_t const desc_device =
{
    .bLength            = sizeof(tusb_descriptor_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,
    .bDeviceClass       = 0x00,
    .bDeviceSubClass    = 0x00,
    .bDeviceProtocol    = 0x00,

    .bMaxPacketSize0    = 64,

    .idVendor           = 0x1FC9,
    .idProduct          = 0x4000,
    .bcdDevice          = 0x0100,

    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,

    .bNumConfigurations = 0x02
} ;
//
TUSB_CFG_ATTR_USBRAM ATTR_ALIGNED(4)
const uint8_t keyboard_report_descriptor[] = {
  HID_USAGE_PAGE ( HID_USAGE_PAGE_DESKTOP     ),
  HID_USAGE      ( HID_USAGE_DESKTOP_KEYBOARD ),
  HID_COLLECTION ( HID_COLLECTION_APPLICATION ),
    HID_USAGE_PAGE ( HID_USAGE_PAGE_KEYBOARD ),
      HID_USAGE_MIN    ( 224                                    ),
      HID_USAGE_MAX    ( 231                                    ),
      HID_LOGICAL_MIN  ( 0                                      ),
      HID_LOGICAL_MAX  ( 1                                      ),

      HID_REPORT_COUNT ( 8                                      ), /* 8 bits */
      HID_REPORT_SIZE  ( 1                                      ),
      HID_INPUT        ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE ), /* maskable modifier key */

      HID_REPORT_COUNT ( 1                                      ),
      HID_REPORT_SIZE  ( 8                                      ),
      HID_INPUT        ( HID_CONSTANT                           ), /* reserved */

    HID_USAGE_PAGE  ( HID_USAGE_PAGE_LED                   ),
      HID_USAGE_MIN    ( 1                                       ),
      HID_USAGE_MAX    ( 5                                       ),
      HID_REPORT_COUNT ( 5                                       ),
      HID_REPORT_SIZE  ( 1                                       ),
      HID_OUTPUT       ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE  ), /* 5-bit Led report */

      HID_REPORT_COUNT ( 1                                       ),
      HID_REPORT_SIZE  ( 3                                       ), /* led padding */
      HID_OUTPUT       ( HID_CONSTANT                            ),

    HID_USAGE_PAGE (HID_USAGE_PAGE_KEYBOARD),
      HID_USAGE_MIN    ( 0                                       ),
      HID_USAGE_MAX    ( 101                                     ),
      HID_LOGICAL_MIN  ( 0                                       ),
      HID_LOGICAL_MAX  ( 101                                     ),

      HID_REPORT_COUNT ( 6                                   ),
      HID_REPORT_SIZE  ( 8                                   ),
      HID_INPUT        ( HID_DATA | HID_ARRAY | HID_ABSOLUTE ), /* keycodes array 6 items */
  HID_COLLECTION_END
};

TUSB_CFG_ATTR_USBRAM ATTR_ALIGNED(4)
const app_configuration_desc_t desc_configuration =
{
    .configuration =
    {
        .bLength             = sizeof(tusb_descriptor_configuration_t),
        .bDescriptorType     = TUSB_DESC_CONFIGURATION,

        .wTotalLength        = sizeof(app_configuration_desc_t) - 1, // exclude termination
        .bNumInterfaces      = 1,

        .bConfigurationValue = 1,
        .iConfiguration      = 0x00,
        .bmAttributes        = TUSB_DESC_CONFIG_ATT_BUS_POWER,
        .bMaxPower           = TUSB_DESC_CONFIG_POWER_MA(100)
    },

    ///// USB HID Keyboard interface
    .keyboard_interface =
    {
        .bLength            = sizeof(tusb_descriptor_interface_t),
        .bDescriptorType    = TUSB_DESC_INTERFACE,
        .bInterfaceNumber   = 1,
        .bAlternateSetting  = 0x00,
        .bNumEndpoints      = 1,
        .bInterfaceClass    = TUSB_CLASS_HID,
        .bInterfaceSubClass = HID_SUBCLASS_BOOT,
        .bInterfaceProtocol = HID_PROTOCOL_KEYBOARD,
        .iInterface         = 0x00
    },

    .keyboard_hid =
    {
        .bLength           = sizeof(tusb_hid_descriptor_hid_t),
        .bDescriptorType   = HID_DESC_HID,
        .bcdHID            = 0x0111,
        .bCountryCode      = HID_Local_NotSupported,
        .bNumDescriptors   = 1,
        .bReportType       = HID_DESC_REPORT,
        .wReportLength     = sizeof(keyboard_report_descriptor)
    },

    .keyboard_endpoint =
    {
        .bLength          = sizeof(tusb_descriptor_endpoint_t),
        .bDescriptorType  = TUSB_DESC_ENDPOINT,
        .bEndpointAddress = 0x81,
        .bmAttributes     = { .xfer = TUSB_XFER_INTERRUPT },
        .wMaxPacketSize   = 0x08,
        .bInterval        = 0x0A
    },

    .ConfigDescTermination = 0,
};

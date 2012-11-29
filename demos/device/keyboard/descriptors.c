/*
 * descriptors.c
 *
 *  Created on: Nov 26, 2012
 *      Author: hathach
 */

/*
 * Software License Agreement (BSD License)
 * Copyright (c) 2012, hathach (tinyusb.net)
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

#include "descriptors.h"

#ifdef CFG_CLASS_HID_KEYBOARD
ATTR_ALIGNED(4) const uint8_t HID_KeyboardReportDescriptor[] = {
  HID_UsagePage  ( HID_USAGE_PAGE_GENERIC     ),
  HID_Usage      ( HID_USAGE_GENERIC_KEYBOARD ),
  HID_Collection ( HID_Application            ),
    HID_UsagePage (HID_USAGE_PAGE_KEYBOARD),
      HID_UsageMin    (224                                     ),
      HID_UsageMax    (231                                     ),
      HID_LogicalMin  ( 0                                      ),
      HID_LogicalMax  ( 1                                      ),

      HID_ReportCount ( 8                                      ), /* 8 bits */
      HID_ReportSize  ( 1                                      ),
      HID_Input       ( HID_Data | HID_Variable | HID_Absolute ), /* maskable modifier key */

      HID_ReportCount ( 1                                      ),
      HID_ReportSize  (8                                       ),
      HID_Input       (HID_Constant                            ), /* reserved */

    HID_UsagePage  ( HID_USAGE_PAGE_LED                   ),
      HID_UsageMin    (1                                       ),
      HID_UsageMax    (5                                       ),
      HID_ReportCount (5                                       ),
      HID_ReportSize  (1                                       ),
      HID_Output      ( HID_Data | HID_Variable | HID_Absolute ), /* 5-bit Led report */

      HID_ReportCount ( 1                                      ),
      HID_ReportSize  (3                                       ), /* led padding */
      HID_Output      (HID_Constant                            ),

    HID_UsagePage (HID_USAGE_PAGE_KEYBOARD),
      HID_UsageMin    (0                                   ),
      HID_UsageMax    (101                                 ),
      HID_LogicalMin  (0                                       ),
      HID_LogicalMax  (101                                     ),

      HID_ReportCount (6                                   ),
      HID_ReportSize  (8                                   ),
      HID_Input       (HID_Data | HID_Array | HID_Absolute ), /* keycodes array 6 items */
  HID_EndCollection,
};
#endif

#ifdef CFG_CLASS_HID_MOUSE
ATTR_ALIGNED(4) const uint8_t HID_MouseReportDescriptor[] = {
  HID_UsagePage  ( HID_USAGE_PAGE_GENERIC     ),
  HID_Usage      ( HID_USAGE_GENERIC_MOUSE ),
  HID_Collection ( HID_Application            ),
    HID_Usage (HID_USAGE_GENERIC_POINTER),

    HID_Collection ( HID_Physical ),
      HID_UsagePage  ( HID_USAGE_PAGE_BUTTON     ),
        HID_UsageMin    ( 1                                     ),
        HID_UsageMax    ( 3                                     ),
        HID_LogicalMin  ( 0                                      ),
        HID_LogicalMax  ( 1                                      ),

        HID_ReportCount ( 3                                      ), /* Left, Right and Middle mouse*/
        HID_ReportSize  ( 1                                      ),
        HID_Input       ( HID_Data | HID_Variable | HID_Absolute ),

        HID_ReportCount ( 1                                      ),
        HID_ReportSize  ( 5                                      ),
        HID_Input       (HID_Constant                            ), /* reserved */

      HID_UsagePage  ( HID_USAGE_PAGE_GENERIC ),
        HID_Usage       ( HID_USAGE_GENERIC_X                    ),
        HID_Usage       ( HID_USAGE_GENERIC_Y                    ),
        HID_LogicalMin  ( 0x81                                   ), /* -127 */
        HID_LogicalMax  ( 0x7f                                   ), /* 127  */

        HID_ReportCount ( 2                                      ), /* X, Y position */
        HID_ReportSize  ( 8                                      ),
        HID_Input       ( HID_Data | HID_Variable | HID_Relative ), /* relative values */

    HID_EndCollection,

  HID_EndCollection,
};
#endif

/* USB Standard Device Descriptor */
ATTR_ALIGNED(4) const USB_DEVICE_DESCRIPTOR USB_DeviceDescriptor =
{
  .bLength            = sizeof(USB_DEVICE_DESCRIPTOR),
  .bDescriptorType    = USB_DEVICE_DESCRIPTOR_TYPE,
  .bcdUSB             = 0x0200,

  #if IAD_DESC_REQUIRED
  /* Multiple Interfaces Using Interface Association Descriptor (IAD) */
  .bDeviceClass       = USB_DEVICE_CLASS_IAD,
  .bDeviceSubClass    = USB_DEVICE_SUBCLASS_IAD,
  .bDeviceProtocol    = USB_DEVICE_PROTOCOL_IAD,
  #elif defined CFG_CLASS_CDC
  .bDeviceClass       = CDC_COMMUNICATION_INTERFACE_CLASS,
  .bDeviceSubClass    = 0x00,
  .bDeviceProtocol    = 0x00,
  #else
  .bDeviceClass       = 0x00,
  .bDeviceSubClass    = 0x00,
  .bDeviceProtocol    = 0x00,
  #endif

  .bMaxPacketSize0    = USB_MAX_PACKET0,

  .idVendor           = CFG_USB_VENDORID,
  .idProduct          = USB_PRODUCT_ID,
  .bcdDevice          = 0x0100,

  .iManufacturer      = 0x01,
  .iProduct           = 0x02,
  .iSerialNumber      = 0x03,

  .bNumConfigurations = 0x01
};

ATTR_ALIGNED(4) const USB_FS_CONFIGURATION_DESCRIPTOR USB_FsConfigDescriptor =
{
    .Config =
    {
        .bLength             = sizeof(USB_CONFIGURATION_DESCRIPTOR),
        .bDescriptorType     = USB_CONFIGURATION_DESCRIPTOR_TYPE,

        .wTotalLength        = sizeof(USB_FS_CONFIGURATION_DESCRIPTOR) - 1, // exclude termination
        .bNumInterfaces      = TOTAL_INTEFACES,

        .bConfigurationValue = 1,
        .iConfiguration      = 0x00,
        .bmAttributes        = USB_CONFIG_BUS_POWERED,
        .bMaxPower           = USB_CONFIG_POWER_MA(100)
    },

    #if IAD_DESC_REQUIRED
    // IAD points to CDC Interfaces
    .CDC_IAD =
    {
        .bLength           = sizeof(USB_INTERFACE_ASSOCIATION_DESCRIPTOR),
        .bDescriptorType   = USB_INTERFACE_ASSOCIATION_DESCRIPTOR_TYPE,

        .bFirstInterface   = 0,
        .bInterfaceCount   = 2,

        .bFunctionClass    = CDC_COMMUNICATION_INTERFACE_CLASS,
        .bFunctionSubClass = CDC_ABSTRACT_CONTROL_MODEL,
        .bFunctionProtocol = CDC_PROTOCOL_COMMON_AT_COMMANDS,

        .iFunction         = 0
    },
    #endif

    #ifdef CFG_CLASS_CDC
    // USB CDC Serial Interface
    // CDC Control Interface
    .CDC_CCI_Interface =
    {
        .bLength            = sizeof(USB_INTERFACE_DESCRIPTOR),
        .bDescriptorType    = USB_INTERFACE_DESCRIPTOR_TYPE,
        .bInterfaceNumber   = INTERFACE_INDEX_CDC,
        .bAlternateSetting  = 0,
        .bNumEndpoints      = 1,
        .bInterfaceClass    = CDC_COMMUNICATION_INTERFACE_CLASS,
        .bInterfaceSubClass = CDC_ABSTRACT_CONTROL_MODEL,
        .bInterfaceProtocol = CDC_PROTOCOL_COMMON_AT_COMMANDS,
        .iInterface         = 0x00
    },

    .CDC_Header =
    {
        .bFunctionLength    = sizeof(CDC_HEADER_DESCRIPTOR),
        .bDescriptorType    = CDC_CS_INTERFACE,
        .bDescriptorSubtype = CDC_HEADER,
        .bcdCDC             = 0x0120
    },

    .CDC_ACM =
    {
        .bFunctionLength    = sizeof(CDC_ABSTRACT_CONTROL_MANAGEMENT_DESCRIPTOR),
        .bDescriptorType    = CDC_CS_INTERFACE,
        .bDescriptorSubtype = CDC_ABSTRACT_CONTROL_MANAGEMENT,
        .bmCapabilities     = 0x06 // Support Send_Break and Set_Line_Coding, Set_Control_Line_State, Get_Line_Coding, and the notification Serial_State
    },

    .CDC_Union =
    {
        .sUnion =
        {
            .bFunctionLength    = sizeof(CDC_UNION_1SLAVE_DESCRIPTOR),
            .bDescriptorType    = CDC_CS_INTERFACE,
            .bDescriptorSubtype = CDC_UNION,
            .bMasterInterface   = 0
        },
        .bSlaveInterfaces[0] = 1
    },

    .CDC_NotificationEndpoint =
    {
        .bLength          = sizeof(USB_ENDPOINT_DESCRIPTOR),
        .bDescriptorType  = USB_ENDPOINT_DESCRIPTOR_TYPE,
        .bEndpointAddress = CDC_NOTIFICATION_EP,
        .bmAttributes     = USB_ENDPOINT_TYPE_INTERRUPT,
        .wMaxPacketSize   = CDC_NOTIFICATION_EP_MAXPACKETSIZE,
        .bInterval        = 0xff // lowest polling rate
    },

    // CDC Data Interface
    .CDC_DCI_Interface =
    {
        .bLength            = sizeof(USB_INTERFACE_DESCRIPTOR),
        .bDescriptorType    = USB_INTERFACE_DESCRIPTOR_TYPE,
        .bInterfaceNumber   = INTERFACE_INDEX_CDC+1,
        .bAlternateSetting  = 0x00,
        .bNumEndpoints      = 2,
        .bInterfaceClass    = CDC_DATA_INTERFACE_CLASS,
        .bInterfaceSubClass = 0,
        .bInterfaceProtocol = 0,
        .iInterface         = 0x00
    },

    .CDC_DataOutEndpoint =
    {
        .bLength          = sizeof(USB_ENDPOINT_DESCRIPTOR),
        .bDescriptorType  = USB_ENDPOINT_DESCRIPTOR_TYPE,
        .bEndpointAddress = CDC_DATA_EP_OUT,
        .bmAttributes     = USB_ENDPOINT_TYPE_BULK,
        .wMaxPacketSize   = CDC_DATA_EP_MAXPACKET_SIZE,
        .bInterval        = 0
    },

    .CDC_DataInEndpoint =
    {
        .bLength          = sizeof(USB_ENDPOINT_DESCRIPTOR),
        .bDescriptorType  = USB_ENDPOINT_DESCRIPTOR_TYPE,
        .bEndpointAddress = CDC_DATA_EP_IN,
        .bmAttributes     = USB_ENDPOINT_TYPE_BULK,
        .wMaxPacketSize   = CDC_DATA_EP_MAXPACKET_SIZE,
        .bInterval        = 0
    },
    #endif

    #ifdef CFG_CLASS_HID_KEYBOARD
    ///// USB HID Keyboard interface
    .HID_KeyboardInterface =
    {
        .bLength            = sizeof(USB_INTERFACE_DESCRIPTOR),
        .bDescriptorType    = USB_INTERFACE_DESCRIPTOR_TYPE,
        .bInterfaceNumber   = INTERFACE_INDEX_HID_KEYBOARD,
        .bAlternateSetting  = 0x00,
        .bNumEndpoints      = 1,
        .bInterfaceClass    = USB_DEVICE_CLASS_HUMAN_INTERFACE,
        .bInterfaceSubClass = HID_SUBCLASS_BOOT,
        .bInterfaceProtocol = HID_PROTOCOL_KEYBOARD,
        .iInterface         = 0x00
    },

    .HID_KeyboardHID =
    {
        .bLength           = sizeof(HID_DESCRIPTOR),
        .bDescriptorType   = HID_HID_DESCRIPTOR_TYPE,
        .bcdHID            = 0x0111,
        .bCountryCode      = HID_Local_NotSupported,
        .bNumDescriptors   = 1,
        .DescriptorList[0] =
        {
            .bDescriptorType   = HID_REPORT_DESCRIPTOR_TYPE,
            .wDescriptorLength = sizeof(HID_KeyboardReportDescriptor)
        },
    },

    .HID_KeyboardEndpoint =
    {
        .bLength          = sizeof(USB_ENDPOINT_DESCRIPTOR),
        .bDescriptorType  = USB_ENDPOINT_DESCRIPTOR_TYPE,
        .bEndpointAddress = HID_KEYBOARD_EP_IN,
        .bmAttributes     = USB_ENDPOINT_TYPE_INTERRUPT,
        .wMaxPacketSize   = 0x08,
        .bInterval        = 0x0A
    },
    #endif

    #ifdef CFG_CLASS_HID_MOUSE
    .HID_MouseInterface =
    {
        .bLength            = sizeof(USB_INTERFACE_DESCRIPTOR),
        .bDescriptorType    = USB_INTERFACE_DESCRIPTOR_TYPE,
        .bInterfaceNumber   = INTERFACE_INDEX_HID_MOUSE,
        .bAlternateSetting  = 0x00,
        .bNumEndpoints      = 1,
        .bInterfaceClass    = USB_DEVICE_CLASS_HUMAN_INTERFACE,
        .bInterfaceSubClass = HID_SUBCLASS_BOOT,
        .bInterfaceProtocol = HID_PROTOCOL_MOUSE,
        .iInterface         = 0x00
    },

    .HID_MouseHID =
    {
        .bLength           = sizeof(HID_DESCRIPTOR),
        .bDescriptorType   = HID_HID_DESCRIPTOR_TYPE,
        .bcdHID            = 0x0111,
        .bCountryCode      = HID_Local_NotSupported,
        .bNumDescriptors   = 1,
        .DescriptorList[0] =
        {
            .bDescriptorType   = HID_REPORT_DESCRIPTOR_TYPE,
            .wDescriptorLength = sizeof(HID_MouseReportDescriptor)
        },
    },

    .HID_MouseEndpoint =
    {
        .bLength          = sizeof(USB_ENDPOINT_DESCRIPTOR),
        .bDescriptorType  = USB_ENDPOINT_DESCRIPTOR_TYPE,
        .bEndpointAddress = HID_MOUSE_EP_IN,
        .bmAttributes     = USB_ENDPOINT_TYPE_INTERRUPT,
        .wMaxPacketSize   = 0x08,
        .bInterval        = 0x0A
    },
    #endif

    .ConfigDescTermination = 0,
};

ATTR_ALIGNED(4) const USB_STR_DESCRIPTOR USB_StringDescriptor =
{
    .LangID = { .bLength = 0x04, .bDescriptorType = USB_STRING_DESCRIPTOR_TYPE },
    .strLangID= {0x0409}, // US English

    .Manufacturer = { .bLength = USB_STRING_LEN(sizeof(CFG_USB_STRING_MANUFACTURER)-1), .bDescriptorType = USB_STRING_DESCRIPTOR_TYPE },
    .strManufacturer = {'t', 'i', 'n', 'y', 'U', 'S', 'B'},

    .Product = { .bLength = USB_STRING_LEN(sizeof(CFG_USB_STRING_PRODUCT)-1), .bDescriptorType = USB_STRING_DESCRIPTOR_TYPE },
    .strProduct = {'D', 'e', 'v', 'i', 'c', ' ', 'e', 'K', 'e', 'y', 'b', 'o', 'a', 'r', 'd'},

    .Serial = { .bLength = USB_STRING_LEN(USB_STRING_SERIAL_LEN), .bDescriptorType = USB_STRING_DESCRIPTOR_TYPE },
    .strSerial = {'1', '2', '3', '4'}
};

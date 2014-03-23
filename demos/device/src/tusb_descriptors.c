/**************************************************************************/
/*!
    @file     tusb_descriptors.c
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

#include "tusb_descriptors.h"

//--------------------------------------------------------------------+
// Keyboard Report Descriptor
//--------------------------------------------------------------------+
#if TUSB_CFG_DEVICE_HID_KEYBOARD
uint8_t const desc_keyboard_report[] = {
  HID_USAGE_PAGE ( HID_USAGE_PAGE_DESKTOP     ),
  HID_USAGE      ( HID_USAGE_DESKTOP_KEYBOARD ),
  HID_COLLECTION ( HID_COLLECTION_APPLICATION ),
    HID_USAGE_PAGE ( HID_USAGE_PAGE_KEYBOARD ),
      HID_USAGE_MIN    ( 224                                    ),
      HID_USAGE_MAX    ( 231                                    ),
      HID_LOGICAL_MIN  ( 0                                      ),
      HID_LOGICAL_MAX  ( 1                                      ),

      HID_REPORT_SIZE  ( 1                                      ),
      HID_REPORT_COUNT ( 8                                      ), /* 8 bits */
      HID_INPUT        ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE ), /* maskable modifier key */

      HID_REPORT_SIZE  ( 8                                      ),
      HID_REPORT_COUNT ( 1                                      ),
      HID_INPUT        ( HID_CONSTANT                           ), /* reserved */

    HID_USAGE_PAGE  ( HID_USAGE_PAGE_LED                   ),
      HID_USAGE_MIN    ( 1                                       ),
      HID_USAGE_MAX    ( 5                                       ),
      HID_REPORT_COUNT ( 5                                       ),
      HID_REPORT_SIZE  ( 1                                       ),
      HID_OUTPUT       ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE  ), /* 5-bit Led report */

      HID_REPORT_SIZE  ( 3                                       ), /* led padding */
      HID_REPORT_COUNT ( 1                                       ),
      HID_OUTPUT       ( HID_CONSTANT                            ),

    HID_USAGE_PAGE (HID_USAGE_PAGE_KEYBOARD),
      HID_USAGE_MIN    ( 0                                   ),
      HID_USAGE_MAX    ( 101                                 ),
      HID_LOGICAL_MIN  ( 0                                   ),
      HID_LOGICAL_MAX  ( 101                                 ),

      HID_REPORT_SIZE  ( 8                                   ),
      HID_REPORT_COUNT ( 6                                   ),
      HID_INPUT        ( HID_DATA | HID_ARRAY | HID_ABSOLUTE ), /* keycodes array 6 items */
  HID_COLLECTION_END
};
#endif

//--------------------------------------------------------------------+
// Mouse Report Descriptor
//--------------------------------------------------------------------+
#if TUSB_CFG_DEVICE_HID_MOUSE
uint8_t const desc_mouse_report[] = {
  HID_USAGE_PAGE ( HID_USAGE_PAGE_DESKTOP     ),
  HID_USAGE      ( HID_USAGE_DESKTOP_MOUSE    ),
  HID_COLLECTION ( HID_COLLECTION_APPLICATION ),
    HID_USAGE      (HID_USAGE_DESKTOP_POINTER),

    HID_COLLECTION ( HID_COLLECTION_PHYSICAL ),
      HID_USAGE_PAGE  ( HID_USAGE_PAGE_BUTTON ),
        HID_USAGE_MIN    ( 1                                      ),
        HID_USAGE_MAX    ( 3                                      ),
        HID_LOGICAL_MIN  ( 0                                      ),
        HID_LOGICAL_MAX  ( 1                                      ),

        HID_REPORT_SIZE  ( 1                                      ),
        HID_REPORT_COUNT ( 3                                      ), /* Left, Right and Middle mouse*/
        HID_INPUT        ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE ),

        HID_REPORT_SIZE  ( 5                                      ),
        HID_REPORT_COUNT ( 1                                      ),
        HID_INPUT        ( HID_CONSTANT                           ), /* 5 bit padding followed 3 bit buttons */

      HID_USAGE_PAGE  ( HID_USAGE_PAGE_DESKTOP ),
        HID_USAGE        ( HID_USAGE_DESKTOP_X                    ),
        HID_USAGE        ( HID_USAGE_DESKTOP_Y                    ),
        HID_LOGICAL_MIN  ( 0x81                                   ), /* -127 */
        HID_LOGICAL_MAX  ( 0x7f                                   ), /* 127  */

        HID_REPORT_SIZE  ( 8                                      ),
        HID_REPORT_COUNT ( 2                                      ), /* X, Y position */
        HID_INPUT        ( HID_DATA | HID_VARIABLE | HID_RELATIVE ), /* relative values */

        HID_USAGE       ( HID_USAGE_DESKTOP_WHEEL                ), /* mouse scroll */
        HID_LOGICAL_MIN ( 0x81                                   ), /* -127 */
        HID_LOGICAL_MAX ( 0x7f                                   ), /* 127  */
        HID_REPORT_COUNT( 1                                      ),
        HID_REPORT_SIZE ( 8                                      ), /* 8-bit value */
        HID_INPUT       ( HID_DATA | HID_VARIABLE | HID_RELATIVE ), /* relative values */

    HID_COLLECTION_END,

  HID_COLLECTION_END
};
#endif

//--------------------------------------------------------------------+
// USB DEVICE DESCRIPTOR
//--------------------------------------------------------------------+
tusb_descriptor_device_t const desc_device =
{
    .bLength            = sizeof(tusb_descriptor_device_t),
    .bDescriptorType    = TUSB_DESC_TYPE_DEVICE,
    .bcdUSB             = 0x0200,
  #if TUSB_CFG_DEVICE_CDC
    // Use Interface Association Descriptor (IAD) for CDC
    // As required by USB Specs IAD's subclass must be common class (2) and protocol must be IAD (1)
    .bDeviceClass       = TUSB_CLASS_MISC,
    .bDeviceSubClass    = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol    = MISC_PROTOCOL_IAD,
  #else
    .bDeviceClass       = 0x00,
    .bDeviceSubClass    = 0x00,
    .bDeviceProtocol    = 0x00,
  #endif

    .bMaxPacketSize0    = TUSB_CFG_DEVICE_CONTROL_ENDOINT_SIZE,

    .idVendor           = CFG_VENDORID,
    .idProduct          = CFG_PRODUCTID,
    .bcdDevice          = 0x0100,

    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,

    .bNumConfigurations = 0x01 // TODO multiple configurations
};

//--------------------------------------------------------------------+
// USB COFNIGURATION DESCRIPTOR
//--------------------------------------------------------------------+
app_descriptor_configuration_t const desc_configuration =
{
    .configuration =
    {
        .bLength             = sizeof(tusb_descriptor_configuration_t),
        .bDescriptorType     = TUSB_DESC_TYPE_CONFIGURATION,

        .wTotalLength        = sizeof(app_descriptor_configuration_t),
        .bNumInterfaces      = TOTAL_INTEFACES,

        .bConfigurationValue = 1,
        .iConfiguration      = 0x00,
        .bmAttributes        = TUSB_DESC_CONFIG_ATT_BUS_POWER,
        .bMaxPower           = TUSB_DESC_CONFIG_POWER_MA(100)
    },

    #if TUSB_CFG_DEVICE_CDC
    // IAD points to CDC Interfaces
    .cdc_iad =
    {
        .bLength           = sizeof(tusb_descriptor_interface_association_t),
        .bDescriptorType   = TUSB_DESC_TYPE_INTERFACE_ASSOCIATION,

        .bFirstInterface   = INTERFACE_NO_CDC,
        .bInterfaceCount   = 2,

        .bFunctionClass    = TUSB_CLASS_CDC,
        .bFunctionSubClass = CDC_COMM_SUBCLASS_ABSTRACT_CONTROL_MODEL,
        .bFunctionProtocol = CDC_COMM_PROTOCOL_ATCOMMAND,
        .iFunction         = 0
    },

    //------------- CDC Communication Interface -------------//
    .cdc_comm_interface =
    {
        .bLength            = sizeof(tusb_descriptor_interface_t),
        .bDescriptorType    = TUSB_DESC_TYPE_INTERFACE,
        .bInterfaceNumber   = INTERFACE_NO_CDC,
        .bAlternateSetting  = 0,
        .bNumEndpoints      = 1,
        .bInterfaceClass    = TUSB_CLASS_CDC,
        .bInterfaceSubClass = CDC_COMM_SUBCLASS_ABSTRACT_CONTROL_MODEL,
        .bInterfaceProtocol = CDC_COMM_PROTOCOL_ATCOMMAND,
        .iInterface         = 0x00
    },

    .cdc_header =
    {
        .bLength            = sizeof(cdc_desc_func_header_t),
        .bDescriptorType    = TUSB_DESC_TYPE_INTERFACE_CLASS_SPECIFIC,
        .bDescriptorSubType = CDC_FUNC_DESC_HEADER,
        .bcdCDC             = 0x0120
    },

    .cdc_acm =
    {
        .bLength            = sizeof(cdc_desc_func_abstract_control_management_t),
        .bDescriptorType    = TUSB_DESC_TYPE_INTERFACE_CLASS_SPECIFIC,
        .bDescriptorSubType = CDC_FUNC_DESC_ABSTRACT_CONTROL_MANAGEMENT,
        .bmCapabilities     = { // 0x06
            .support_line_request = 1,
            .support_send_break   = 1
        }
    },

    .cdc_union =
    {
        .bLength                  = sizeof(cdc_desc_func_union_t), // plus number of
        .bDescriptorType          = TUSB_DESC_TYPE_INTERFACE_CLASS_SPECIFIC,
        .bDescriptorSubType       = CDC_FUNC_DESC_UNION,
        .bControlInterface        = 0,
        .bSubordinateInterface    = 1,
    },

    .cdc_endpoint_notification =
    {
        .bLength          = sizeof(tusb_descriptor_endpoint_t),
        .bDescriptorType  = TUSB_DESC_TYPE_ENDPOINT,
        .bEndpointAddress = CDC_EDPT_NOTIFICATION_ADDR,
        .bmAttributes     = { .xfer = TUSB_XFER_INTERRUPT },
        .wMaxPacketSize   = { .size = 0x08 },
        .bInterval        = 0x0a
    },

    //------------- CDC Data Interface -------------//
    .cdc_data_interface =
    {
        .bLength            = sizeof(tusb_descriptor_interface_t),
        .bDescriptorType    = TUSB_DESC_TYPE_INTERFACE,
        .bInterfaceNumber   = INTERFACE_NO_CDC+1,
        .bAlternateSetting  = 0x00,
        .bNumEndpoints      = 2,
        .bInterfaceClass    = TUSB_CLASS_CDC_DATA,
        .bInterfaceSubClass = 0,
        .bInterfaceProtocol = 0,
        .iInterface         = 0x04
    },

    .cdc_endpoint_out =
    {
        .bLength          = sizeof(tusb_descriptor_endpoint_t),
        .bDescriptorType  = TUSB_DESC_TYPE_ENDPOINT,
        .bEndpointAddress = CDC_EDPT_DATA_OUT_ADDR,
        .bmAttributes     = { .xfer = TUSB_XFER_BULK },
        .wMaxPacketSize   = { .size = CDC_EDPT_DATA_PACKETSIZE },
        .bInterval        = 0
    },

    .cdc_endpoint_in =
    {
        .bLength          = sizeof(tusb_descriptor_endpoint_t),
        .bDescriptorType  = TUSB_DESC_TYPE_ENDPOINT,
        .bEndpointAddress = CDC_EDPT_DATA_IN_ADDR,
        .bmAttributes     = { .xfer = TUSB_XFER_BULK },
        .wMaxPacketSize   = { .size = CDC_EDPT_DATA_PACKETSIZE },
        .bInterval        = 0
    },
    #endif

    //------------- HID Keyboard -------------//
    #if TUSB_CFG_DEVICE_HID_KEYBOARD
    .keyboard_interface =
    {
        .bLength            = sizeof(tusb_descriptor_interface_t),
        .bDescriptorType    = TUSB_DESC_TYPE_INTERFACE,
        .bInterfaceNumber   = INTERFACE_NO_HID_KEYBOARD,
        .bAlternateSetting  = 0x00,
        .bNumEndpoints      = 1,
        .bInterfaceClass    = TUSB_CLASS_HID,
        .bInterfaceSubClass = HID_SUBCLASS_BOOT,
        .bInterfaceProtocol = HID_PROTOCOL_KEYBOARD,
        .iInterface         = 0x05
    },

    .keyboard_hid =
    {
        .bLength           = sizeof(tusb_hid_descriptor_hid_t),
        .bDescriptorType   = HID_DESC_TYPE_HID,
        .bcdHID            = 0x0111,
        .bCountryCode      = HID_Local_NotSupported,
        .bNumDescriptors   = 1,
        .bReportType       = HID_DESC_TYPE_REPORT,
        .wReportLength     = sizeof(desc_keyboard_report)
    },

    .keyboard_endpoint =
    {
        .bLength          = sizeof(tusb_descriptor_endpoint_t),
        .bDescriptorType  = TUSB_DESC_TYPE_ENDPOINT,
        .bEndpointAddress = HID_KEYBOARD_EDPT_ADDR,
        .bmAttributes     = { .xfer = TUSB_XFER_INTERRUPT },
        .wMaxPacketSize   = { .size = HID_KEYBOARD_EDPT_PACKETSIZE },
        .bInterval        = 0x0A
    },
    #endif

    //------------- HID Mouse -------------//
    #if TUSB_CFG_DEVICE_HID_MOUSE
    .mouse_interface =
    {
        .bLength            = sizeof(tusb_descriptor_interface_t),
        .bDescriptorType    = TUSB_DESC_TYPE_INTERFACE,
        .bInterfaceNumber   = INTERFACE_NO_HID_MOUSE,
        .bAlternateSetting  = 0x00,
        .bNumEndpoints      = 1,
        .bInterfaceClass    = TUSB_CLASS_HID,
        .bInterfaceSubClass = HID_SUBCLASS_BOOT,
        .bInterfaceProtocol = HID_PROTOCOL_MOUSE,
        .iInterface         = 0x06
    },

    .mouse_hid =
    {
        .bLength           = sizeof(tusb_hid_descriptor_hid_t),
        .bDescriptorType   = HID_DESC_TYPE_HID,
        .bcdHID            = 0x0111,
        .bCountryCode      = HID_Local_NotSupported,
        .bNumDescriptors   = 1,
        .bReportType       = HID_DESC_TYPE_REPORT,
        .wReportLength     = sizeof(desc_mouse_report)
    },

    .mouse_endpoint =
    {
        .bLength          = sizeof(tusb_descriptor_endpoint_t),
        .bDescriptorType  = TUSB_DESC_TYPE_ENDPOINT,
        .bEndpointAddress = HID_MOUSE_EDPT_ADDR, // TODO
        .bmAttributes     = { .xfer = TUSB_XFER_INTERRUPT },
        .wMaxPacketSize   = { .size = HID_MOUSE_EDPT_PACKETSIZE },
        .bInterval        = 0x0A
    },
    #endif

    //------------- Mass Storage -------------//
    #if TUSB_CFG_DEVICE_MSC
    .msc_interface =
    {
        .bLength            = sizeof(tusb_descriptor_interface_t),
        .bDescriptorType    = TUSB_DESC_TYPE_INTERFACE,
        .bInterfaceNumber   = INTERFACE_NO_MSC,
        .bAlternateSetting  = 0x00,
        .bNumEndpoints      = 2,
        .bInterfaceClass    = TUSB_CLASS_MSC,
        .bInterfaceSubClass = MSC_SUBCLASS_SCSI,
        .bInterfaceProtocol = MSC_PROTOCOL_BOT,
        .iInterface         = 0x07
    },

    .msc_endpoint_in =
    {
        .bLength          = sizeof(tusb_descriptor_endpoint_t),
        .bDescriptorType  = TUSB_DESC_TYPE_ENDPOINT,
        .bEndpointAddress = MSC_EDPT_IN_ADDR,
        .bmAttributes     = { .xfer = TUSB_XFER_BULK },
        .wMaxPacketSize   = { .size = MSC_EDPT_PACKETSIZE },
        .bInterval        = 1
    },

    .msc_endpoint_out =
    {
        .bLength          = sizeof(tusb_descriptor_endpoint_t),
        .bDescriptorType  = TUSB_DESC_TYPE_ENDPOINT,
        .bEndpointAddress = MSC_EDPT_OUT_ADDR,
        .bmAttributes     = { .xfer = TUSB_XFER_BULK },
        .wMaxPacketSize   = { .size = MSC_EDPT_PACKETSIZE },
        .bInterval        = 1
    },
    #endif
};

//--------------------------------------------------------------------+
// STRING DESCRIPTORS
//--------------------------------------------------------------------+
#define STRING_LEN_UNICODE(n) (2 + (2*(n))) // also includes 2 byte header
#define ENDIAN_BE16_FROM( high, low) ENDIAN_BE16(high << 8 | low)

// array of pointer to string descriptors
uint16_t const * const string_descriptor_arr [] =
{
    [0] = (uint16_t []) { // supported language
        ENDIAN_BE16_FROM( STRING_LEN_UNICODE(1), TUSB_DESC_TYPE_STRING ),
        0x0409 // English
    },

    [1] = (uint16_t []) { // manufacturer
        ENDIAN_BE16_FROM( STRING_LEN_UNICODE(11), TUSB_DESC_TYPE_STRING),
        't', 'i', 'n', 'y', 'u', 's', 'b', '.', 'o', 'r', 'g' // len = 11
    },

    [2] = (uint16_t []) { // product
        ENDIAN_BE16_FROM( STRING_LEN_UNICODE(14), TUSB_DESC_TYPE_STRING),
        't', 'i', 'n', 'y', 'u', 's', 'b', ' ', 'd', 'e', 'v', 'i', 'c', 'e' // len = 14
    },

    [3] = (uint16_t []) { // serials
        ENDIAN_BE16_FROM( STRING_LEN_UNICODE(4), TUSB_DESC_TYPE_STRING),
        '1', '2', '3', '4' // len = 4
    },

    [4] = (uint16_t []) { // CDC Interface
        ENDIAN_BE16_FROM( STRING_LEN_UNICODE(3), TUSB_DESC_TYPE_STRING),
        'c', 'd', 'c' // len = 3
    },

    [5] = (uint16_t []) { // Keyboard Interface
        ENDIAN_BE16_FROM( STRING_LEN_UNICODE(5), TUSB_DESC_TYPE_STRING),
        'm', 'o', 'u', 's', 'e' // len = 5
    },

    [6] = (uint16_t []) { // Keyboard Interface
        ENDIAN_BE16_FROM( STRING_LEN_UNICODE(8), TUSB_DESC_TYPE_STRING),
        'k', 'e', 'y', 'b', 'o', 'a', 'r', 'd' // len = 8
    },

    [7] = (uint16_t []) { // MSC Interface
        ENDIAN_BE16_FROM( STRING_LEN_UNICODE(3), TUSB_DESC_TYPE_STRING),
        'm', 's', 'c' // len = 3
    }
};

//--------------------------------------------------------------------+
// TINYUSB Descriptors Pointer (this variable is required by the stack)
//--------------------------------------------------------------------+
tusbd_descriptor_pointer_t tusbd_descriptor_pointers =
{
    .p_device              = (uint8_t const * ) &desc_device,
    .p_configuration       = (uint8_t const * ) &desc_configuration,
    .p_string_arr          = (uint8_t const **) string_descriptor_arr,

    #if TUSB_CFG_DEVICE_HID_KEYBOARD
    .p_hid_keyboard_report = (uint8_t const *) desc_keyboard_report,
    #endif

    #if TUSB_CFG_DEVICE_HID_MOUSE
    .p_hid_mouse_report    = (uint8_t const *)  desc_mouse_report,
    #endif
};

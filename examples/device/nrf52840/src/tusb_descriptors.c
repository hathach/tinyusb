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
// USB DEVICE DESCRIPTOR
//--------------------------------------------------------------------+
tusb_desc_device_t const desc_device =
{
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,

    // Use Interface Association Descriptor (IAD) for CDC
    // As required by USB Specs IAD's subclass must be common class (2) and protocol must be IAD (1)
    .bDeviceClass       = TUSB_CLASS_MISC,
    .bDeviceSubClass    = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol    = MISC_PROTOCOL_IAD,

    .bMaxPacketSize0    = CFG_TUD_ENDOINT0_SIZE,

    .idVendor           = CFG_VENDORID,
    .idProduct          = CFG_PRODUCTID,
    .bcdDevice          = 0x0100,

    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,

    .bNumConfigurations = 0x01
};

//--------------------------------------------------------------------+
// USB COFNIGURATION DESCRIPTOR
//--------------------------------------------------------------------+
app_descriptor_configuration_t const desc_configuration =
{
    .configuration =
    {
        .bLength             = sizeof(tusb_desc_configuration_t),
        .bDescriptorType     = TUSB_DESC_CONFIGURATION,

        .wTotalLength        = sizeof(app_descriptor_configuration_t),
        .bNumInterfaces      = ITF_TOTAL,

        .bConfigurationValue = 1,
        .iConfiguration      = 0x00,
        .bmAttributes        = TUSB_DESC_CONFIG_ATT_BUS_POWER,
        .bMaxPower           = TUSB_DESC_CONFIG_POWER_MA(500)
    },

    // IAD points to CDC Interfaces
    .cdc =
    {
      .iad =
      {
          .bLength           = sizeof(tusb_desc_interface_assoc_t),
          .bDescriptorType   = TUSB_DESC_INTERFACE_ASSOCIATION,

          .bFirstInterface   = ITF_NUM_CDC,
          .bInterfaceCount   = 2,

          .bFunctionClass    = TUSB_CLASS_CDC,
          .bFunctionSubClass = CDC_COMM_SUBCLASS_ABSTRACT_CONTROL_MODEL,
          .bFunctionProtocol = CDC_COMM_PROTOCOL_ATCOMMAND,
          .iFunction         = 0
      },

      //------------- CDC Communication Interface -------------//
      .comm_itf =
      {
          .bLength            = sizeof(tusb_desc_interface_t),
          .bDescriptorType    = TUSB_DESC_INTERFACE,
          .bInterfaceNumber   = ITF_NUM_CDC,
          .bAlternateSetting  = 0,
          .bNumEndpoints      = 1,
          .bInterfaceClass    = TUSB_CLASS_CDC,
          .bInterfaceSubClass = CDC_COMM_SUBCLASS_ABSTRACT_CONTROL_MODEL,
          .bInterfaceProtocol = CDC_COMM_PROTOCOL_ATCOMMAND,
          .iInterface         = 0x00
      },

      .header =
      {
          .bLength            = sizeof(cdc_desc_func_header_t),
          .bDescriptorType    = TUSB_DESC_CLASS_SPECIFIC,
          .bDescriptorSubType = CDC_FUNC_DESC_HEADER,
          .bcdCDC             = 0x0120
      },

      .call =
      {
          .bLength            = sizeof(cdc_desc_func_call_management_t),
          .bDescriptorType    = TUSB_DESC_CLASS_SPECIFIC,
          .bDescriptorSubType = CDC_FUNC_DESC_CALL_MANAGEMENT,
          .bmCapabilities     = { 0 },
          .bDataInterface     = ITF_NUM_CDC+1,
      },

      .acm =
      {
          .bLength            = sizeof(cdc_desc_func_acm_t),
          .bDescriptorType    = TUSB_DESC_CLASS_SPECIFIC,
          .bDescriptorSubType = CDC_FUNC_DESC_ABSTRACT_CONTROL_MANAGEMENT,
          .bmCapabilities     = { // 0x02
              .support_line_request = 1,
          }
      },

      .union_func =
      {
          .bLength                  = sizeof(cdc_desc_func_union_t), // plus number of
          .bDescriptorType          = TUSB_DESC_CLASS_SPECIFIC,
          .bDescriptorSubType       = CDC_FUNC_DESC_UNION,
          .bControlInterface        = ITF_NUM_CDC,
          .bSubordinateInterface    = ITF_NUM_CDC+1,
      },

      .ep_notif =
      {
          .bLength          = sizeof(tusb_desc_endpoint_t),
          .bDescriptorType  = TUSB_DESC_ENDPOINT,
          .bEndpointAddress = CDC_EDPT_NOTIF,
          .bmAttributes     = { .xfer = TUSB_XFER_INTERRUPT },
          .wMaxPacketSize   = { .size = CDC_EDPT_NOTIF_SIZE },
          .bInterval        = 0x10
      },

      //------------- CDC Data Interface -------------//
      .data_itf =
      {
          .bLength            = sizeof(tusb_desc_interface_t),
          .bDescriptorType    = TUSB_DESC_INTERFACE,
          .bInterfaceNumber   = ITF_NUM_CDC+1,
          .bAlternateSetting  = 0x00,
          .bNumEndpoints      = 2,
          .bInterfaceClass    = TUSB_CLASS_CDC_DATA,
          .bInterfaceSubClass = 0,
          .bInterfaceProtocol = 0,
          .iInterface         = 0x00
      },

      .ep_out =
      {
          .bLength          = sizeof(tusb_desc_endpoint_t),
          .bDescriptorType  = TUSB_DESC_ENDPOINT,
          .bEndpointAddress = CDC_EDPT_OUT,
          .bmAttributes     = { .xfer = TUSB_XFER_BULK },
          .wMaxPacketSize   = { .size = CDC_EDPT_SIZE },
      .bInterval        = 0
      },

      .ep_in =
      {
          .bLength          = sizeof(tusb_desc_endpoint_t),
          .bDescriptorType  = TUSB_DESC_ENDPOINT,
          .bEndpointAddress = CDC_EDPT_IN,
          .bmAttributes     = { .xfer = TUSB_XFER_BULK },
          .wMaxPacketSize   = { .size = CDC_EDPT_SIZE },
          .bInterval        = 0
      },
    },

    .msc =
    {
      .interface =
      {
          .bLength            = sizeof(tusb_desc_interface_t),
          .bDescriptorType    = TUSB_DESC_INTERFACE,
          .bInterfaceNumber   = ITF_NUM_MSC,
          .bAlternateSetting  = 0x00,
          .bNumEndpoints      = 2,
          .bInterfaceClass    = TUSB_CLASS_MSC,
          .bInterfaceSubClass = MSC_SUBCLASS_SCSI,
          .bInterfaceProtocol = MSC_PROTOCOL_BOT,
          .iInterface         = 0x07
      },

      .ep_out =
      {
          .bLength          = sizeof(tusb_desc_endpoint_t),
          .bDescriptorType  = TUSB_DESC_ENDPOINT,
          .bEndpointAddress = MSC_EDPT_OUT,
          .bmAttributes     = { .xfer = TUSB_XFER_BULK },
          .wMaxPacketSize   = { .size = MSC_EDPT_SIZE},
          .bInterval        = 1
      },

      .ep_in =
      {
          .bLength          = sizeof(tusb_desc_endpoint_t),
          .bDescriptorType  = TUSB_DESC_ENDPOINT,
          .bEndpointAddress = MSC_EDPT_IN,
          .bmAttributes     = { .xfer = TUSB_XFER_BULK },
          .wMaxPacketSize   = { .size = MSC_EDPT_SIZE},
          .bInterval        = 1
      }
    }
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
        ENDIAN_BE16_FROM( STRING_LEN_UNICODE(1), TUSB_DESC_STRING ),
        0x0409 // English
    },

    [1] = (uint16_t []) { // manufacturer
        ENDIAN_BE16_FROM( STRING_LEN_UNICODE(11), TUSB_DESC_STRING),
        't', 'i', 'n', 'y', 'u', 's', 'b', '.', 'o', 'r', 'g' // len = 11
    },

    [2] = (uint16_t []) { // product
        ENDIAN_BE16_FROM( STRING_LEN_UNICODE(14), TUSB_DESC_STRING),
        't', 'i', 'n', 'y', 'u', 's', 'b', ' ', 'd', 'e', 'v', 'i', 'c', 'e' // len = 14
    },

    [3] = (uint16_t []) { // serials
        ENDIAN_BE16_FROM( STRING_LEN_UNICODE(4), TUSB_DESC_STRING),
        '1', '2', '3', '4' // len = 4
    },

    [4] = (uint16_t []) { // CDC Interface
        ENDIAN_BE16_FROM( STRING_LEN_UNICODE(3), TUSB_DESC_STRING),
        'c', 'd', 'c' // len = 3
    },

    [5] = (uint16_t []) { // Keyboard Interface
        ENDIAN_BE16_FROM( STRING_LEN_UNICODE(5), TUSB_DESC_STRING),
        'm', 'o', 'u', 's', 'e' // len = 5
    },

    [6] = (uint16_t []) { // Keyboard Interface
        ENDIAN_BE16_FROM( STRING_LEN_UNICODE(8), TUSB_DESC_STRING),
        'k', 'e', 'y', 'b', 'o', 'a', 'r', 'd' // len = 8
    },

    [7] = (uint16_t []) { // MSC Interface
        ENDIAN_BE16_FROM( STRING_LEN_UNICODE(3), TUSB_DESC_STRING),
        'm', 's', 'c' // len = 3
    }
};


/*------------- Variable used by tud_set_descriptors -------------*/
tud_desc_init_t usb_desc_init =
{
    .device              = (uint8_t const * ) &desc_device,
    .configuration       = (uint8_t const * ) &desc_configuration,
    .string_arr          = (uint8_t const **) string_descriptor_arr,
};

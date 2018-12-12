/**************************************************************************/
/*!
    @file     descriptor_cdc.c
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
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

    This file is part of the tinyusb stack.
*/
/**************************************************************************/

#include "tusb_option.h"
#include "descriptor_cdc.h"

//--------------------------------------------------------------------+
// CDC Serials
//--------------------------------------------------------------------+
CFG_TUSB_MEM_SECTION
const cdc_configuration_desc_t cdc_config_descriptor =
{
    .configuration =
    {
        .bLength             = sizeof(tusb_desc_configuration_t),
        .bDescriptorType     = TUSB_DESC_TYPE_CONFIGURATION,

        .wTotalLength        = sizeof(cdc_configuration_desc_t),
        .bNumInterfaces      = 2,

        .bConfigurationValue = 1,
        .iConfiguration      = 0x00,
        .bmAttributes        = TUSB_DESC_CONFIG_ATT_BUS_POWER,
        .bMaxPower           = TUSB_DESC_CONFIG_POWER_MA(100)
    },

    // IAD points to CDC Interfaces
    .cdc_iad =
    {
        .bLength           = sizeof(tusb_desc_interface_assoc_t),
        .bDescriptorType   = TUSB_DESC_TYPE_INTERFACE_ASSOCIATION,

        .bFirstInterface   = 1,
        .bInterfaceCount   = 2,

        .bFunctionClass    = TUSB_CLASS_CDC,
        .bFunctionSubClass = CDC_COMM_SUBCLASS_ABSTRACT_CONTROL_MODEL,
        .bFunctionProtocol = CDC_COMM_PROTOCOL_ATCOMMAND,

        .iFunction         = 0
    },


    // USB CDC Serial Interface
    //------------- CDC Communication Interface -------------//
    .cdc_comm_interface =
    {
        .bLength            = sizeof(tusb_desc_interface_t),
        .bDescriptorType    = TUSB_DESC_TYPE_INTERFACE,
        .bInterfaceNumber   = 1,
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
        .bLength            = sizeof(cdc_desc_func_acm_t),
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
        .bControlInterface        = 1,
        .bSubordinateInterface    = 2,
    },

    .cdc_endpoint_notification =
    {
        .bLength          = sizeof(tusb_desc_endpoint_t),
        .bDescriptorType  = TUSB_DESC_TYPE_ENDPOINT,
        .bEndpointAddress = 0x81,
        .bmAttributes     = { .xfer = TUSB_XFER_INTERRUPT },
        .wMaxPacketSize   = 8,
        .bInterval        = 0x0a // lowest polling rate
    },

    //------------- CDC Data Interface -------------//
    .cdc_data_interface =
    {
        .bLength            = sizeof(tusb_desc_interface_t),
        .bDescriptorType    = TUSB_DESC_TYPE_INTERFACE,
        .bInterfaceNumber   = 2,
        .bAlternateSetting  = 0x00,
        .bNumEndpoints      = 2,
        .bInterfaceClass    = TUSB_CLASS_CDC_DATA,
        .bInterfaceSubClass = 0,
        .bInterfaceProtocol = 0,
        .iInterface         = 0x00
    },

    .cdc_endpoint_out =
    {
        .bLength          = sizeof(tusb_desc_endpoint_t),
        .bDescriptorType  = TUSB_DESC_TYPE_ENDPOINT,
        .bEndpointAddress = 2,
        .bmAttributes     = { .xfer = TUSB_XFER_BULK },
        .wMaxPacketSize   = 64,
        .bInterval        = 0
    },

    .cdc_endpoint_in =
    {
        .bLength          = sizeof(tusb_desc_endpoint_t),
        .bDescriptorType  = TUSB_DESC_TYPE_ENDPOINT,
        .bEndpointAddress = 0x82,
        .bmAttributes     = { .xfer = TUSB_XFER_BULK },
        .wMaxPacketSize   = 64,
        .bInterval        = 0
    },
};

//--------------------------------------------------------------------+
// CDC RNSID
//--------------------------------------------------------------------+

CFG_TUSB_MEM_SECTION
const cdc_configuration_desc_t rndis_config_descriptor =
{
    .configuration =
    {
        .bLength             = sizeof(tusb_desc_configuration_t),
        .bDescriptorType     = TUSB_DESC_TYPE_CONFIGURATION,

        .wTotalLength        = sizeof(cdc_configuration_desc_t),
        .bNumInterfaces      = 2,

        .bConfigurationValue = 1,
        .iConfiguration      = 0x00,
        .bmAttributes        = TUSB_DESC_CONFIG_ATT_BUS_POWER,
        .bMaxPower           = TUSB_DESC_CONFIG_POWER_MA(100)
    },

    // IAD points to CDC Interfaces
    .cdc_iad =
    {
        .bLength           = sizeof(tusb_desc_interface_assoc_t),
        .bDescriptorType   = TUSB_DESC_TYPE_INTERFACE_ASSOCIATION,

        .bFirstInterface   = 1,
        .bInterfaceCount   = 2,

        .bFunctionClass    = TUSB_CLASS_CDC,
        .bFunctionSubClass = CDC_COMM_SUBCLASS_ABSTRACT_CONTROL_MODEL,
        .bFunctionProtocol = 0,

        .iFunction         = 0
    },


    // USB CDC Serial Interface
    //------------- CDC Communication Interface -------------//
    .cdc_comm_interface =
    {
        .bLength            = sizeof(tusb_desc_interface_t),
        .bDescriptorType    = TUSB_DESC_TYPE_INTERFACE,
        .bInterfaceNumber   = 1,
        .bAlternateSetting  = 0,
        .bNumEndpoints      = 1,
        .bInterfaceClass    = TUSB_CLASS_CDC,
        .bInterfaceSubClass = CDC_COMM_SUBCLASS_ABSTRACT_CONTROL_MODEL,
        .bInterfaceProtocol = 0xff, // RNDIS is vendor specific protocol
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
        .bLength            = sizeof(cdc_desc_func_acm_t),
        .bDescriptorType    = TUSB_DESC_TYPE_INTERFACE_CLASS_SPECIFIC,
        .bDescriptorSubType = CDC_FUNC_DESC_ABSTRACT_CONTROL_MANAGEMENT,
        .bmCapabilities     = { 0 }
    },

    .cdc_union =
    {
        .bLength                  = sizeof(cdc_desc_func_union_t), // plus number of
        .bDescriptorType          = TUSB_DESC_TYPE_INTERFACE_CLASS_SPECIFIC,
        .bDescriptorSubType       = CDC_FUNC_DESC_UNION,
        .bControlInterface        = 1,
        .bSubordinateInterface    = 2,
    },

    .cdc_endpoint_notification =
    {
        .bLength          = sizeof(tusb_desc_endpoint_t),
        .bDescriptorType  = TUSB_DESC_TYPE_ENDPOINT,
        .bEndpointAddress = 0x81,
        .bmAttributes     = { .xfer = TUSB_XFER_INTERRUPT },
        .wMaxPacketSize   = 8,
        .bInterval        = 0x0a // lowest polling rate
    },

    //------------- CDC Data Interface -------------//
    .cdc_data_interface =
    {
        .bLength            = sizeof(tusb_desc_interface_t),
        .bDescriptorType    = TUSB_DESC_TYPE_INTERFACE,
        .bInterfaceNumber   = 2,
        .bAlternateSetting  = 0x00,
        .bNumEndpoints      = 2,
        .bInterfaceClass    = TUSB_CLASS_CDC_DATA,
        .bInterfaceSubClass = 0,
        .bInterfaceProtocol = 0,
        .iInterface         = 0x00
    },

    .cdc_endpoint_out =
    {
        .bLength          = sizeof(tusb_desc_endpoint_t),
        .bDescriptorType  = TUSB_DESC_TYPE_ENDPOINT,
        .bEndpointAddress = 2,
        .bmAttributes     = { .xfer = TUSB_XFER_BULK },
        .wMaxPacketSize   = 512,
        .bInterval        = 0
    },

    .cdc_endpoint_in =
    {
        .bLength          = sizeof(tusb_desc_endpoint_t),
        .bDescriptorType  = TUSB_DESC_TYPE_ENDPOINT,
        .bEndpointAddress = 0x82,
        .bmAttributes     = { .xfer = TUSB_XFER_BULK },
        .wMaxPacketSize   = 512,
        .bInterval        = 0
    },
};

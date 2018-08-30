/**************************************************************************/
/*!
    @file     usbd_auto_desc.c
    @author   hathach (tinyusb.org)

    @section LICENSE

    Software License Agreement (BSD License)

    Copyright (c) 2018, hathach (tinyusb.org)
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

#if TUSB_OPT_DEVICE_ENABLED && CFG_TUD_DESC_AUTO

#include "tusb.h"

//--------------------------------------------------------------------+
// Auto Description Default Configure & Validation
//--------------------------------------------------------------------+

// If HID Generic interface is generated
#define AUTO_DESC_HID_GENERIC    (CFG_TUD_HID && ((CFG_TUD_HID_KEYBOARD && !CFG_TUD_HID_KEYBOARD_BOOT) || \
                                                (CFG_TUD_HID_MOUSE && !CFG_TUD_HID_MOUSE_BOOT)) )
/*------------- VID/PID -------------*/
#ifndef CFG_TUD_DESC_VID
#define CFG_TUD_DESC_VID       0xCAFE
#endif

#ifndef CFG_TUD_DESC_PID

/* A combination of interfaces must have a unique product id, since PC will save device driver after the first plug.
 * Same VID/PID with different interface e.g MSC (first), then CDC (later) will possibly cause system error on PC.
 *
 * Auto ProductID layout's Bitmap:
 *   [MSB]         HID Generic | Boot Mouse | Boot Keyboard | MSC | CDC          [LSB]
 */
#define _PID_MAP(itf, n)      ( (CFG_TUD_##itf) << (n) )
#define CFG_TUD_DESC_PID      (0x4000 | _PID_MAP(CDC, 0) | _PID_MAP(MSC, 1) | _PID_MAP(HID, 2) | \
                               _PID_MAP(HID_KEYBOARD, 2) | _PID_MAP(HID_MOUSE, 3) | (AUTO_DESC_HID_GENERIC << 4) )
#endif

//--------------------------------------------------------------------+
// Interface & Endpoint mapping
//--------------------------------------------------------------------+

/*------------- Interface Numbering -------------*/
/* The order as follows: CDC, MSC, Boot Keyboard, Boot Mouse, HID Generic
 * If an interface is not enabled, the later will take its place */

enum
{
#if CFG_TUD_CDC
  ITF_NUM_CDC,
  ITF_NUM_CDC_DATA,
#endif

#if CFG_TUD_MSC
  ITF_NUM_MSC,
#endif

#if CFG_TUD_HID_KEYBOARD && CFG_TUD_HID_KEYBOARD_BOOT
  ITF_NUM_HID_BOOT_KBD,
#endif

#if CFG_TUD_HID_MOUSE && CFG_TUD_HID_MOUSE_BOOT
  ITF_NUM_HID_BOOT_MSE,
#endif

#if AUTO_DESC_HID_GENERIC
  ITF_NUM_HID_GEN,
#endif

  ITF_NUM_TOTAL
};

enum {
    ITF_STR_LANGUAGE = 0 ,
    ITF_STR_MANUFACTURER ,
    ITF_STR_PRODUCT      ,
    ITF_STR_SERIAL       ,

#if CFG_TUD_CDC
    ITF_STR_CDC          ,
#endif

#if CFG_TUD_MSC
    ITF_STR_MSC          ,
#endif

#if CFG_TUD_HID_KEYBOARD && CFG_TUD_HID_KEYBOARD_BOOT
  ITF_STR_HID_BOOT_KBD,
#endif

#if CFG_TUD_HID_MOUSE && CFG_TUD_HID_MOUSE_BOOT
  ITF_STR_HID_BOOT_MSE,
#endif

#if AUTO_DESC_HID_GENERIC
  ITF_STR_HID_GEN,
#endif
};

/*------------- Endpoint Numbering & Size -------------*/
#define _EP_IN(x)               (0x80 | (x))
#define _EP_OUT(x)              (x)

// CDC
#define EP_CDC_NOTIF            _EP_IN ( ITF_NUM_CDC+1 )
#define EP_CDC_NOTIF_SIZE       8

#define EP_CDC_OUT              _EP_OUT( ITF_NUM_CDC+2 )
#define EP_CDC_IN               _EP_IN ( ITF_NUM_CDC+2 )

// Mass Storage
#define EP_MSC_OUT              _EP_OUT( ITF_NUM_MSC+1 )
#define EP_MSC_IN               _EP_IN ( ITF_NUM_MSC+1 )


// HID Keyboard with boot protocol
#define EP_HID_KBD_BOOT         _EP_IN ( ITF_NUM_HID_BOOT_KBD+1 )
#define EP_HID_KBD_BOOT_SZ      8

// HID Mouse with boot protocol
#define EP_HID_MSE_BOOT         _EP_IN ( ITF_NUM_HID_BOOT_MSE+1 )
#define EP_HID_MSE_BOOT_SZ      8

// HID composite = keyboard + mouse + gamepad + etc ...
#define EP_HID_GEN              _EP_IN ( ITF_NUM_HID_GEN+1 )
#define EP_HID_GEN_SIZE         16


//--------------------------------------------------------------------+
// Auto generated HID Report Descriptors
//--------------------------------------------------------------------+


/*------------- Boot Protocol Report Descriptor -------------*/
#if CFG_TUD_HID_KEYBOARD && CFG_TUD_HID_KEYBOARD_BOOT
uint8_t const _desc_auto_hid_boot_kbd_report[] = { HID_REPORT_DESC_KEYBOARD() };
#endif

#if CFG_TUD_HID_MOUSE && CFG_TUD_HID_MOUSE_BOOT
uint8_t const _desc_auto_hid_boot_mse_report[] = { HID_REPORT_DESC_MOUSE() };
#endif


/*------------- Generic (composite) Descriptor -------------*/
#if AUTO_DESC_HID_GENERIC

// Report ID: 0 if there is only 1 report
// starting from 1 if there is multiple reports
#define _REPORT_ID_KBD

// TODO report ID
uint8_t const _desc_auto_hid_generic_report[] =
{
#if CFG_TUD_HID_KEYBOARD && !CFG_TUD_HID_KEYBOARD_BOOT
    HID_REPORT_DESC_KEYBOARD( HID_REPORT_ID(1), ),
#endif

#if CFG_TUD_HID_MOUSE && !CFG_TUD_HID_MOUSE_BOOT
    HID_REPORT_DESC_MOUSE( HID_REPORT_ID(2), )
#endif

};

#endif // hid generic


/*------------------------------------------------------------------*/
/* Auto generated Device & Configuration descriptor
 *------------------------------------------------------------------*/

// For highspeed device but currently in full speed mode
//tusb_desc_device_qualifier_t _device_qual =
//{
//    .bLength = sizeof(tusb_desc_device_qualifier_t),
//    .bDescriptorType = TUSB_DESC_DEVICE_QUALIFIER,
//    .bcdUSB = 0x0200,
//    .bDeviceClass =
//};

/*------------- Device Descriptor -------------*/
tusb_desc_device_t const _desc_auto_device =
{
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,

  #if CFG_TUD_CDC
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

    .bMaxPacketSize0    = CFG_TUD_ENDOINT0_SIZE,

    .idVendor           = CFG_TUD_DESC_VID,
    .idProduct          = CFG_TUD_DESC_PID,
    .bcdDevice          = 0x0100,

    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,

    .bNumConfigurations = 0x01 // TODO multiple configurations
};


/*------------- Configuration Descriptor -------------*/
typedef struct ATTR_PACKED
{
  tusb_desc_configuration_t           config;

  //------------- CDC -------------//
#if CFG_TUD_CDC
  struct ATTR_PACKED
  {
    tusb_desc_interface_assoc_t       iad;

    //CDC Control Interface
    tusb_desc_interface_t             comm_itf;
    cdc_desc_func_header_t            header;
    cdc_desc_func_call_management_t   call;
    cdc_desc_func_acm_t               acm;
    cdc_desc_func_union_t             union_func;
    tusb_desc_endpoint_t              ep_notif;

    //CDC Data Interface
    tusb_desc_interface_t             data_itf;
    tusb_desc_endpoint_t              ep_out;
    tusb_desc_endpoint_t              ep_in;
  }cdc;
#endif

  //------------- Mass Storage -------------//
#if CFG_TUD_MSC
  struct ATTR_PACKED
  {
    tusb_desc_interface_t             itf;
    tusb_desc_endpoint_t              ep_out;
    tusb_desc_endpoint_t              ep_in;
  } msc;
#endif

  //------------- HID -------------//
#if CFG_TUD_HID_KEYBOARD && CFG_TUD_HID_KEYBOARD_BOOT
  struct ATTR_PACKED
  {
    tusb_desc_interface_t             itf;
    tusb_hid_descriptor_hid_t         hid_desc;
    tusb_desc_endpoint_t              ep_in;
  } hid_kbd_boot;
#endif

#if CFG_TUD_HID_MOUSE && CFG_TUD_HID_MOUSE_BOOT
  struct ATTR_PACKED
  {
    tusb_desc_interface_t             itf;
    tusb_hid_descriptor_hid_t         hid_desc;
    tusb_desc_endpoint_t              ep_in;
  } hid_mse_boot;
#endif

#if AUTO_DESC_HID_GENERIC

  struct ATTR_PACKED
  {
    tusb_desc_interface_t             itf;
    tusb_hid_descriptor_hid_t         hid_desc;
    tusb_desc_endpoint_t              ep_in;

    #if 0 //  CFG_TUD_HID_KEYBOARD
    tusb_desc_endpoint_t              ep_out;
    #endif
  } hid_generic;

#endif

} desc_auto_cfg_t;

desc_auto_cfg_t const _desc_auto_config_struct =
{
    .config =
    {
        .bLength             = sizeof(tusb_desc_configuration_t),
        .bDescriptorType     = TUSB_DESC_CONFIGURATION,

        .wTotalLength        = sizeof(desc_auto_cfg_t),
        .bNumInterfaces      = ITF_NUM_TOTAL,

        .bConfigurationValue = 1,
        .iConfiguration      = 0x00,
        .bmAttributes        = TUSB_DESC_CONFIG_ATT_BUS_POWER,
        .bMaxPower           = TUSB_DESC_CONFIG_POWER_MA(100)
    },

#if CFG_TUD_CDC
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
          .iInterface         = 4
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
          .bEndpointAddress = EP_CDC_NOTIF,
          .bmAttributes     = { .xfer = TUSB_XFER_INTERRUPT },
          .wMaxPacketSize   = { .size = EP_CDC_NOTIF_SIZE },
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
          .bEndpointAddress = EP_CDC_OUT,
          .bmAttributes     = { .xfer = TUSB_XFER_BULK },
          .wMaxPacketSize   = { .size = CFG_TUD_CDC_EPSIZE },
          .bInterval        = 0
      },

      .ep_in =
      {
          .bLength          = sizeof(tusb_desc_endpoint_t),
          .bDescriptorType  = TUSB_DESC_ENDPOINT,
          .bEndpointAddress = EP_CDC_IN,
          .bmAttributes     = { .xfer = TUSB_XFER_BULK },
          .wMaxPacketSize   = { .size = CFG_TUD_CDC_EPSIZE },
          .bInterval        = 0
      },
    },
#endif // cdc

#if CFG_TUD_MSC
    //------------- Mass Storage-------------//
    .msc =
    {
      .itf =
      {
          .bLength            = sizeof(tusb_desc_interface_t),
          .bDescriptorType    = TUSB_DESC_INTERFACE,
          .bInterfaceNumber   = ITF_NUM_MSC,
          .bAlternateSetting  = 0x00,
          .bNumEndpoints      = 2,
          .bInterfaceClass    = TUSB_CLASS_MSC,
          .bInterfaceSubClass = MSC_SUBCLASS_SCSI,
          .bInterfaceProtocol = MSC_PROTOCOL_BOT,
          .iInterface         = 4 + CFG_TUD_CDC
      },

      .ep_out =
      {
          .bLength          = sizeof(tusb_desc_endpoint_t),
          .bDescriptorType  = TUSB_DESC_ENDPOINT,
          .bEndpointAddress = EP_MSC_OUT,
          .bmAttributes     = { .xfer = TUSB_XFER_BULK },
          .wMaxPacketSize   = { .size = CFG_TUD_MSC_EPSIZE},
          .bInterval        = 1
      },

      .ep_in =
      {
          .bLength          = sizeof(tusb_desc_endpoint_t),
          .bDescriptorType  = TUSB_DESC_ENDPOINT,
          .bEndpointAddress = EP_MSC_IN,
          .bmAttributes     = { .xfer = TUSB_XFER_BULK },
          .wMaxPacketSize   = { .size = CFG_TUD_MSC_EPSIZE},
          .bInterval        = 1
      }
    },
#endif // msc

#if CFG_TUD_HID_KEYBOARD && CFG_TUD_HID_KEYBOARD_BOOT
    .hid_kbd_boot =
    {
        .itf =
        {
          .bLength            = sizeof(tusb_desc_interface_t),
          .bDescriptorType    = TUSB_DESC_INTERFACE,
          .bInterfaceNumber   = ITF_NUM_HID_BOOT_KBD,
          .bAlternateSetting  = 0x00,
          .bNumEndpoints      = 1,
          .bInterfaceClass    = TUSB_CLASS_HID,
          .bInterfaceSubClass = HID_SUBCLASS_BOOT,
          .bInterfaceProtocol = HID_PROTOCOL_KEYBOARD,
          .iInterface         = 0 //4 + CFG_TUD_CDC + CFG_TUD_MSC
        },

        .hid_desc =
        {
          .bLength         = sizeof(tusb_hid_descriptor_hid_t),
          .bDescriptorType = HID_DESC_TYPE_HID,
          .bcdHID          = 0x0111,
          .bCountryCode    = HID_Local_NotSupported,
          .bNumDescriptors = 1,
          .bReportType     = HID_DESC_TYPE_REPORT,
          .wReportLength   = sizeof(_desc_auto_hid_boot_kbd_report)
        },

        .ep_in =
        {
          .bLength          = sizeof(tusb_desc_endpoint_t),
          .bDescriptorType  = TUSB_DESC_ENDPOINT,
          .bEndpointAddress = EP_HID_KBD_BOOT,
          .bmAttributes     = { .xfer = TUSB_XFER_INTERRUPT },
          .wMaxPacketSize   = { .size = EP_HID_KBD_BOOT_SZ },
          .bInterval        = 0x0A
        }
    },
#endif // boot keyboard

  //------------- HID Mouse -------------//
#if CFG_TUD_HID_MOUSE && CFG_TUD_HID_MOUSE_BOOT
    .hid_mse_boot =
    {
        .itf =
        {
          .bLength            = sizeof(tusb_desc_interface_t),
          .bDescriptorType    = TUSB_DESC_INTERFACE,
          .bInterfaceNumber   = ITF_NUM_HID_BOOT_MSE,
          .bAlternateSetting  = 0x00,
          .bNumEndpoints      = 1,
          .bInterfaceClass    = TUSB_CLASS_HID,
          .bInterfaceSubClass = HID_SUBCLASS_BOOT,
          .bInterfaceProtocol = HID_PROTOCOL_MOUSE,
          .iInterface         = 0 // 4 + CFG_TUD_CDC + CFG_TUD_MSC + CFG_TUD_HID_KEYBOARD
        },

        .hid_desc =
        {
          .bLength         = sizeof(tusb_hid_descriptor_hid_t),
          .bDescriptorType = HID_DESC_TYPE_HID,
          .bcdHID          = 0x0111,
          .bCountryCode    = HID_Local_NotSupported,
          .bNumDescriptors = 1,
          .bReportType     = HID_DESC_TYPE_REPORT,
          .wReportLength   = sizeof(_desc_auto_hid_boot_mse_report)
        },

        .ep_in =
        {
          .bLength          = sizeof(tusb_desc_endpoint_t),
          .bDescriptorType  = TUSB_DESC_ENDPOINT,
          .bEndpointAddress = EP_HID_MSE_BOOT,
          .bmAttributes     = { .xfer = TUSB_XFER_INTERRUPT },
          .wMaxPacketSize   = { .size = EP_HID_MSE_BOOT_SZ },
          .bInterval        = 0x0A
        },
    },

#endif // boot mouse

#if AUTO_DESC_HID_GENERIC

    //------------- HID Generic Multiple report -------------//
    .hid_generic =
    {
        .itf =
        {
            .bLength            = sizeof(tusb_desc_interface_t),
            .bDescriptorType    = TUSB_DESC_INTERFACE,
            .bInterfaceNumber   = ITF_NUM_HID_GEN,
            .bAlternateSetting  = 0x00,
            .bNumEndpoints      = 1,
            .bInterfaceClass    = TUSB_CLASS_HID,
            .bInterfaceSubClass = 0,
            .bInterfaceProtocol = 0,
            .iInterface         = 0, // 4 + CFG_TUD_CDC + CFG_TUD_MSC,
        },

        .hid_desc =
        {
            .bLength           = sizeof(tusb_hid_descriptor_hid_t),
            .bDescriptorType   = HID_DESC_TYPE_HID,
            .bcdHID            = 0x0111,
            .bCountryCode      = HID_Local_NotSupported,
            .bNumDescriptors   = 1,
            .bReportType       = HID_DESC_TYPE_REPORT,
            .wReportLength     = sizeof(_desc_auto_hid_generic_report)
        },

        .ep_in =
        {
            .bLength          = sizeof(tusb_desc_endpoint_t),
            .bDescriptorType  = TUSB_DESC_ENDPOINT,
            .bEndpointAddress = EP_HID_GEN,
            .bmAttributes     = { .xfer = TUSB_XFER_INTERRUPT },
            .wMaxPacketSize   = { .size = EP_HID_GEN_SIZE },
            .bInterval        = 0x0A
        }
    }

#endif // hid generic
};

uint8_t const * const _desc_auto_config = (uint8_t const*) &_desc_auto_config_struct;

tud_desc_set_t const _usbd_auto_desc_set =
{
    .device = &_desc_auto_device,
    .config = &_desc_auto_config_struct,

    .hid_report =
    {
#if AUTO_DESC_HID_GENERIC
        .generic = _desc_auto_hid_generic_report,
#else
        .generic = NULL,
#endif

#if CFG_TUD_HID_KEYBOARD && CFG_TUD_HID_KEYBOARD_BOOT
        .boot_keyboard = _desc_auto_hid_boot_kbd_report,
#endif

#if CFG_TUD_HID_MOUSE && CFG_TUD_HID_MOUSE_BOOT
        .boot_mouse = _desc_auto_hid_boot_mse_report
#endif
    }
};

#endif

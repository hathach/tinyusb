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

//--------------------------------------------------------------------+
// Descriptors Value (calculated by enabled Classes)
//--------------------------------------------------------------------+
#define CFG_VENDORID            0xCAFE
//#define CFG_PRODUCTID           0x4567 // use auto product id to prevent conflict with pc's driver

// each combination of interfaces need to have a unique productid, as windows will bind & remember device driver after the first plug.
// Auto ProductID layout's Bitmap: (MSB) MassStorage | Generic | Mouse | Key | CDC (LSB)
#ifndef CFG_PRODUCTID
  #define PRODUCTID_BITMAP(interface, n)  ( (CFG_TUD_##interface) << (n) )
  #define CFG_PRODUCTID                   (0x4000 | ( PRODUCTID_BITMAP(CDC, 0) | PRODUCTID_BITMAP(HID_KEYBOARD, 1) | \
                                           PRODUCTID_BITMAP(HID_MOUSE, 2) | PRODUCTID_BITMAP(HID_GENERIC, 3) | \
                                           PRODUCTID_BITMAP(MSC, 4) ) )
#endif

#define ITF_NUM_CDC          0
#define ITF_TOTAL           2

//--------------------------------------------------------------------+
// Endpoints Address & Max Packet Size
//--------------------------------------------------------------------+
#define EDPT_IN(x)    (0x80 | (x))
#define EDPT_OUT(x)   (x)

#define CDC_EDPT_NOTIF            EDPT_IN (1)
#define CDC_EDPT_NOTIFICATION_PACKETSIZE      64

#define CDC_EDPT_OUT                EDPT_OUT(2)
#define CDC_EDPT_IN                 EDPT_IN (2)
#define CDC_EDPT_SIZE              64


//--------------------------------------------------------------------+
// CONFIGURATION DESCRIPTOR
//--------------------------------------------------------------------+
typedef struct ATTR_PACKED
{
  tusb_desc_configuration_t              configuration;

  //------------- CDC -------------//
  tusb_desc_interface_assoc_t      cdc_iad;

  //CDC Control Interface
  tusb_desc_interface_t                  cdc_comm_interface;
  cdc_desc_func_header_t                       cdc_header;
  cdc_desc_func_call_management_t              cdc_call;
  cdc_desc_func_acm_t  cdc_acm;
  cdc_desc_func_union_t                        cdc_union;
  tusb_desc_endpoint_t                   cdc_endpoint_notification;

  //CDC Data Interface
  tusb_desc_interface_t                  cdc_data_interface;
  tusb_desc_endpoint_t                   cdc_endpoint_out;
  tusb_desc_endpoint_t                   cdc_endpoint_in;

} app_descriptor_configuration_t;



extern tud_desc_set_t usb_desc_init;

#endif

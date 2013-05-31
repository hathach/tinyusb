/**************************************************************************/
/*!
    @file     usbd.c
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

#if MODE_DEVICE_SUPPORTED

#define _TINY_USB_SOURCE_FILE_

//--------------------------------------------------------------------+
// INCLUDE
//--------------------------------------------------------------------+
#include "tusb.h"
#include "tusb_descriptors.h" // TODO callback include

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+
//typedef struct {
//  volatile uint8_t state;
//
//};



//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+

//--------------------------------------------------------------------+
// IMPLEMENTATION
//--------------------------------------------------------------------+
tusb_error_t usbd_init (void)
{
  ASSERT_INT( USB_STRING_LEN(sizeof(TUSB_CFG_DEVICE_STRING_MANUFACTURER)-1),
             app_tusb_desc_strings.manufacturer.bLength, TUSB_ERROR_USBD_DESCRIPTOR_STRING);

  ASSERT_INT( USB_STRING_LEN(sizeof(TUSB_CFG_DEVICE_STRING_PRODUCT)-1)     ,
             app_tusb_desc_strings.product.bLength     , TUSB_ERROR_USBD_DESCRIPTOR_STRING);

  ASSERT_INT( USB_STRING_LEN(sizeof(TUSB_CFG_DEVICE_STRING_SERIAL)-1)      ,
              app_tusb_desc_strings.serial.bLength      , TUSB_ERROR_USBD_DESCRIPTOR_STRING);

  for(uint32_t i=0; i < sizeof(TUSB_CFG_DEVICE_STRING_MANUFACTURER)-1; i++)
  {
    app_tusb_desc_strings.manufacturer.unicode_string[i] = TUSB_CFG_DEVICE_STRING_MANUFACTURER[i];
  }

  for(uint32_t i=0; i < sizeof(TUSB_CFG_DEVICE_STRING_PRODUCT)-1; i++)
  {
    app_tusb_desc_strings.product.unicode_string[i] = TUSB_CFG_DEVICE_STRING_PRODUCT[i];
  }

  for(uint32_t i=0; i < sizeof(TUSB_CFG_DEVICE_STRING_SERIAL)-1; i++)
  {
    app_tusb_desc_strings.serial.unicode_string[i] = TUSB_CFG_DEVICE_STRING_SERIAL[i];
  }

  ASSERT_STATUS ( dcd_init() );

  return TUSB_ERROR_NONE;
}

#endif

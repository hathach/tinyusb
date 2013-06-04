/**************************************************************************/
/*!
    @file     dcd_nxp_romdriver.c
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

#if MODE_DEVICE_SUPPORTED && TUSB_CFG_DEVICE_USE_ROM_DRIVER

#define _TINY_USB_SOURCE_FILE_

//--------------------------------------------------------------------+
// INCLUDE
//--------------------------------------------------------------------+
#include "dcd.h"
#include "dcd_nxp_romdriver.h"
#include "tusb_descriptors.h"

#define USB_ROM_SIZE (1024*2) // TODO dcd abstract later
uint8_t usb_RomDriver_buffer[USB_ROM_SIZE] ATTR_ALIGNED(2048) TUSB_CFG_ATTR_USBRAM;

USBD_HANDLE_T romdriver_hdl;

typedef struct {
  volatile uint8_t state;
}usbd_info_t; // TODO rename

usbd_info_t usbd_info; // TODO rename

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+


//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+


//--------------------------------------------------------------------+
// IMPLEMENTATION
//--------------------------------------------------------------------+
ErrorCode_t USB_Configure_Event (USBD_HANDLE_T hUsb)
{
  USB_CORE_CTRL_T* pCtrl = (USB_CORE_CTRL_T*)hUsb;

  if (pCtrl->config_value)
  {
    usbd_info.state = TUSB_DEVICE_STATE_CONFIGURED;

    #if DEVICE_CLASS_HID
    ASSERT( TUSB_ERROR_NONE == hidd_configured(hUsb), ERR_FAILED );
    #endif

    #if TUSB_CFG_DEVICE_CDC
    ASSERT( TUSB_ERROR_NONE == tusb_cdc_configured(hUsb), ERR_FAILED );
    #endif
  }

  return LPC_OK;
}

ErrorCode_t USB_Reset_Event (USBD_HANDLE_T hUsb)
{
  usbd_info.state = TUSB_DEVICE_STATE_UNPLUG;
  return LPC_OK;
}

ErrorCode_t USB_Interface_Event (USBD_HANDLE_T hUsb)
{
  return LPC_OK;
}

ErrorCode_t USB_Error_Event (USBD_HANDLE_T hUsb, uint32_t param1)
{
  (void) param1;
  return LPC_OK;
}

tusb_error_t dcd_init(void)
{
  USBD_API_INIT_PARAM_T usb_param =
  {
    .usb_reg_base        = NXP_ROMDRIVER_REG_BASE,
    .max_num_ep          = USB_MAX_EP_NUM,
    .mem_base            = (uint32_t) usb_RomDriver_buffer,
    .mem_size            = USB_ROM_SIZE,

    .USB_Configure_Event = USB_Configure_Event,
    .USB_Reset_Event     = USB_Reset_Event,
    .USB_Error_Event     = USB_Error_Event,
    .USB_Interface_Event = USB_Interface_Event
  };

  USB_CORE_DESCS_T desc_core =
  {
    .device_desc      = (uint8_t*) &app_tusb_desc_device,
    .string_desc      = (uint8_t*) &app_tusb_desc_strings,
    .full_speed_desc  = (uint8_t*) &app_tusb_desc_configuration,
    .high_speed_desc  = (uint8_t*) &app_tusb_desc_configuration,
    .device_qualifier = NULL
  };

  /* USB hardware core initialization */
  ASSERT_INT(LPC_OK, ROM_API->hw->Init(&romdriver_hdl, &desc_core, &usb_param), TUSB_ERROR_FAILED);

  // TODO need to confirm the mem_size is reduced by the number of byte used
//  membase += (memsize - usb_param.mem_size);
//  memsize = usb_param.mem_size;

  return TUSB_ERROR_NONE;
}

bool tusb_device_is_configured(void)
{
  return usbd_info.state == TUSB_DEVICE_STATE_CONFIGURED;
}

tusb_error_t dcd_controller_reset(uint8_t coreid)
{
//TODO merge with hcd_controller_reset
// default mode is device ?
  return TUSB_ERROR_NONE;
}

void dcd_controller_connect(uint8_t coreid)
{
  ROM_API->hw->Connect(romdriver_hdl, 1);
}

void dcd_isr(uint8_t coreid)
{
  ROM_API->hw->ISR(romdriver_hdl);
}

#endif


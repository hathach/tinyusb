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
#include "romdriver/mw_usbd_rom_api.h"

#include "tusb_descriptors.h"


#define USB_ROM_SIZE (1024*2) // TODO dcd abstract later
uint8_t usb_RomDriver_buffer[USB_ROM_SIZE] ATTR_ALIGNED(2048) TUSB_CFG_ATTR_USBRAM;
USBD_HANDLE_T g_hUsb;

typedef struct {
  volatile uint8_t state;
}usbd_info_t; // TODO rename

usbd_info_t usbd_info; // TODO rename

typedef struct {
  void (* const init) (void);
  void (* const configured) (void);
  void (* const unmounted) (void);
}device_class_driver_t;

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

    #if defined(DEVICE_CLASS_HID)
    ASSERT( TUSB_ERROR_NONE == hidd_configured(hUsb), ERR_FAILED );
    #endif

    #ifdef TUSB_CFG_DEVICE_CDC
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


tusb_error_t dcd_init(void)
{
  uint32_t membase = (uint32_t) usb_RomDriver_buffer;
  uint32_t memsize = USB_ROM_SIZE;

  USBD_API_INIT_PARAM_T usb_param =
  {
    .usb_reg_base        = DEVICE_ROM_REG_BASE,
    .max_num_ep          = USB_MAX_EP_NUM,
    .mem_base            = membase,
    .mem_size            = memsize,

    .USB_Configure_Event = USB_Configure_Event,
    .USB_Reset_Event     = USB_Reset_Event
  };

  USB_CORE_DESCS_T desc_core =
  {
    .device_desc      = (uint8_t*) &app_desc_device,
    .string_desc      = (uint8_t*) &app_desc_strings,
    .full_speed_desc  = (uint8_t*) &app_desc_configuration,
    .high_speed_desc  = (uint8_t*) &app_desc_configuration,
    .device_qualifier = NULL
  };

  /* USB hardware core initialization */
  ASSERT_INT(LPC_OK, ROM_API->hw->Init(&g_hUsb, &desc_core, &usb_param), TUSB_ERROR_FAILED);

  // TODO need to confirm the mem_size is reduced by the number of byte used
  membase += (memsize - usb_param.mem_size);
  memsize = usb_param.mem_size;


  #if TUSB_CFG_DEVICE_HID_KEYBOARD
  ASSERT_STATUS( hidd_init(g_hUsb , &app_desc_configuration.keyboard_interface,
            keyboard_report_descriptor, app_desc_configuration.keyboard_hid.wReportLength,
            &membase , &memsize) );
  #endif

  #if TUSB_CFG_DEVICE_HID_MOUSE
  ASSERT_STATUS( tusb_hid_init(g_hUsb , &USB_FsConfigDescriptor.HID_MouseInterface    ,
            HID_MouseReportDescriptor, USB_FsConfigDescriptor.HID_MouseHID.DescriptorList[0].wDescriptorLength,
            &membase , &memsize) );
  #endif

  ROM_API->hw->Connect(g_hUsb, 1);

  return TUSB_ERROR_NONE;
}

bool usb_isConfigured(void)
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
  ROM_API->hw->Connect(g_hUsb, 1);
}

void dcd_isr(uint8_t coreid)
{
  ROM_API->hw->ISR(g_hUsb);
}

#endif


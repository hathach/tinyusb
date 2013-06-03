/*
 * dcd.c
 *
 *  Created on: Nov 27, 2012
 *      Author: hathach
 */

/*
 * Software License Agreement (BSD License)
 * Copyright (c) 2013, hathach (tinyusb.org)
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
 * This file is part of the tinyUSB stack.
 */

#include "dcd.h"

#if 0

#include "descriptors.h" // TODO refractor later

#define USB_ROM_SIZE (1024*2) // TODO dcd abstract later
uint8_t usb_RomDriver_buffer[USB_ROM_SIZE] ATTR_ALIGNED(2048) TUSB_CFG_ATTR_USBRAM;
USBD_HANDLE_T romdriver_hdl;
static volatile bool isConfigured = false;

/**************************************************************************/
/*!
    @brief Handler for the USB Configure Event
*/
/**************************************************************************/
ErrorCode_t USB_Configure_Event (USBD_HANDLE_T hUsb)
{
  USB_CORE_CTRL_T* pCtrl = (USB_CORE_CTRL_T*)hUsb;
  if (pCtrl->config_value)
  {
    #if defined(DEVICE_CLASS_HID)
    ASSERT( TUSB_ERROR_NONE == tusb_hid_configured(hUsb), ERR_FAILED );
    #endif

    #ifdef TUSB_CFG_DEVICE_CDC
    ASSERT( TUSB_ERROR_NONE == tusb_cdc_configured(hUsb), ERR_FAILED );
    #endif
  }

  isConfigured = true;

  return LPC_OK;
}

/**************************************************************************/
/*!
    @brief Handler for the USB Reset Event
*/
/**************************************************************************/
ErrorCode_t USB_Reset_Event (USBD_HANDLE_T hUsb)
{
  isConfigured = false;
  return LPC_OK;
}

tusb_error_t dcd_init(uint8_t coreid)
{
#ifdef DEVICE_ROMDRIVER // TODO refractor later
  /* ROM DRIVER INIT */
  uint32_t membase = (uint32_t) usb_RomDriver_buffer;
  uint32_t memsize = USB_ROM_SIZE;

  USBD_API_INIT_PARAM_T usb_param =
  {
    .usb_reg_base        = NXP_ROMDRIVER_REG_BASE,
    .max_num_ep          = USB_MAX_EP_NUM,
    .mem_base            = membase,
    .mem_size            = memsize,

    .USB_Configure_Event = USB_Configure_Event,
    .USB_Reset_Event     = USB_Reset_Event
  };

  USB_CORE_DESCS_T DeviceDes =
  {
    .device_desc      = (uint8_t*) &USB_DeviceDescriptor,
    .string_desc      = (uint8_t*) &USB_StringDescriptor,
    .full_speed_desc  = (uint8_t*) &USB_FsConfigDescriptor,
    .high_speed_desc  = (uint8_t*) &USB_FsConfigDescriptor,
    .device_qualifier = NULL
  };

  /* USB hardware core initialization */
  ASSERT(LPC_OK == ROM_API->hw->Init(&romdriver_hdl, &DeviceDes, &usb_param), TUSB_ERROR_FAILED);

  membase += (memsize - usb_param.mem_size);
  memsize = usb_param.mem_size;

  /* Initialise the class driver(s) */
  #ifdef TUSB_CFG_DEVICE_CDC
  ASSERT_STATUS( tusb_cdc_init(romdriver_hdl, &USB_FsConfigDescriptor.CDC_CCI_Interface,
            &USB_FsConfigDescriptor.CDC_DCI_Interface, &membase, &memsize) );
  #endif

  #ifdef TUSB_CFG_DEVICE_HID_KEYBOARD
  ASSERT_STATUS( tusb_hid_init(romdriver_hdl , &USB_FsConfigDescriptor.HID_KeyboardInterface ,
            HID_KeyboardReportDescriptor, USB_FsConfigDescriptor.HID_KeyboardHID.DescriptorList[0].wDescriptorLength,
            &membase , &memsize) );
  #endif

  #ifdef TUSB_CFG_DEVICE_HID_MOUSE
  ASSERT_STATUS( tusb_hid_init(romdriver_hdl , &USB_FsConfigDescriptor.HID_MouseInterface    ,
            HID_MouseReportDescriptor, USB_FsConfigDescriptor.HID_MouseHID.DescriptorList[0].wDescriptorLength,
            &membase , &memsize) );
  #endif

  hal_interrupt_enable(); /* Enable the USB interrupt */

  /* Perform USB soft connect */
  ROM_API->hw->Connect(romdriver_hdl, 1);
#endif

  return TUSB_ERROR_NONE;
}

/**************************************************************************/
/*!
    @brief Indicates whether USB is configured or not
*/
/**************************************************************************/
bool usb_isConfigured(void)
{
  return isConfigured;
}

#endif

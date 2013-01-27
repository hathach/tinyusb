/*
 * hid_host.c
 *
 *  Created on: Dec 20, 2012
 *      Author: hathach
 */

/*
 * Software License Agreement (BSD License)
 * Copyright (c) 2013, hathach (tinyusb.net)
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

#include "common/common.h"

#if defined TUSB_CFG_HOST && defined DEVICE_CLASS_HID

#define _TINY_USB_SOURCE_FILE_

//--------------------------------------------------------------------+
// INCLUDE
//--------------------------------------------------------------------+
#include "hid_host.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+

//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+
STATIC_ class_hid_keyboard_info_t keyboard_info_pool[TUSB_CFG_HOST_DEVICE_MAX];


//--------------------------------------------------------------------+
// PUBLIC API
//--------------------------------------------------------------------+
tusb_error_t tusbh_hid_keyboard_get(tusb_handle_device_t const device_hdl, uint8_t instance_num, tusb_keyboard_report_t * const report)
{
  keyboard_interface_t *p_kbd;

  ASSERT(usbh_device_is_plugged(device_hdl), TUSB_ERROR_INVALID_PARA);
  ASSERT_PTR(report, TUSB_ERROR_INVALID_PARA);
  ASSERT(instance_num < TUSB_CFG_HOST_HID_KEYBOARD_NO_INSTANCES_PER_DEVICE, TUSB_ERROR_INVALID_PARA);

  p_kbd = &keyboard_info_pool[device_hdl].instance[instance_num];

  ASSERT(0 != p_kbd->pipe_in, TUSB_ERROR_CLASS_DEVICE_DONT_SUPPORT);

  ASSERT_INT(PIPE_STATUS_COMPLETE, usbh_pipe_status_get(p_kbd->pipe_in), TUSB_ERROR_CLASS_DATA_NOT_AVAILABLE);

  memcpy(report, p_kbd->buffer, p_kbd->report_size);

  return TUSB_ERROR_NONE;
}

uint8_t tusbh_hid_keyboard_no_instances(tusb_handle_device_t const device_hdl)
{
  ASSERT(usbh_device_is_plugged(device_hdl), 0);

  return keyboard_info_pool[device_hdl].instance_count;
}

//--------------------------------------------------------------------+
// CLASS-USBD API
//--------------------------------------------------------------------+
void class_hid_keyboard_init(void)
{
  memset(&keyboard_info_pool, 0, sizeof(class_hid_keyboard_info_t)*TUSB_CFG_HOST_DEVICE_MAX);
}

tusb_error_t class_hid_keyboard_install(uint8_t const dev_addr, uint8_t const *descriptor)
{
  keyboard_info_pool[0].instance_count++;

  return TUSB_ERROR_NONE;
}

#endif

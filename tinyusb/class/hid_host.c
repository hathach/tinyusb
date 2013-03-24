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

#include "tusb_option.h"

#if (MODE_HOST_SUPPORTED && defined HOST_CLASS_HID)

#define _TINY_USB_SOURCE_FILE_

//--------------------------------------------------------------------+
// INCLUDE
//--------------------------------------------------------------------+
#include "common/common.h"
#include "hid_host.h"

#if TUSB_CFG_HOST_HID_KEYBOARD
//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+

//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+
STATIC_ hidh_keyboard_info_t keyboard_data[TUSB_CFG_HOST_DEVICE_MAX]; // does not have addr0, index = dev_address-1


//--------------------------------------------------------------------+
// PUBLIC API (Parameter Verification is required)
//--------------------------------------------------------------------+
tusb_error_t tusbh_hid_keyboard_get(uint8_t const dev_addr, uint8_t instance_num, tusb_keyboard_report_t * const report)
{
  //------------- parameters validation -------------//
  ASSERT_INT(TUSB_DEVICE_STATE_CONFIGURED, tusbh_device_get_state(dev_addr), TUSB_ERROR_DEVICE_NOT_READY);
  ASSERT_PTR(report, TUSB_ERROR_INVALID_PARA);
  ASSERT(instance_num < TUSB_CFG_HOST_HID_KEYBOARD_NO_INSTANCES_PER_DEVICE, TUSB_ERROR_INVALID_PARA);

  keyboard_interface_t *p_kbd;
  p_kbd = &keyboard_data[dev_addr-1].instance[instance_num];

  // TODO abtract class support for device
  ASSERT(0 != p_kbd->pipe_in.dev_addr, TUSB_ERROR_CLASS_DEVICE_DONT_SUPPORT);

  // TODO abtract to use hidh service
  ASSERT_STATUS( hcd_pipe_xfer(p_kbd->pipe_in, report, p_kbd->report_size, 1) ) ;

  return TUSB_ERROR_NONE;
}

uint8_t tusbh_hid_keyboard_no_instances(uint8_t const dev_addr)
{
  ASSERT(tusbh_device_is_configured(dev_addr), 0);

  return keyboard_data[dev_addr-1].instance_count;
}

//--------------------------------------------------------------------+
// CLASS-USBD API (don't require to verify parameters)
//--------------------------------------------------------------------+
void hidh_init(void)
{
#if TUSB_CFG_HOST_HID_KEYBOARD
  hidh_keyboard_init();
#endif
}

void hidh_keyboard_init(void)
{
  memclr_(&keyboard_data, sizeof(hidh_keyboard_info_t)*TUSB_CFG_HOST_DEVICE_MAX);
}

tusb_error_t hidh_keyboard_install(uint8_t const dev_addr, uint8_t const *descriptor)
{
  keyboard_data[dev_addr-1].instance_count++;

  return TUSB_ERROR_NONE;
}
#endif

#endif

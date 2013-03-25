/*
 * hid_host_keyboard.c
 *
 *  Created on: Mar 25, 2013
 *      Author: hathach
 */

/*
 * Software License Agreement (BSD License)
 * Copyright (c) 2012, hathach (tinyusb.net)
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

#if (MODE_HOST_SUPPORTED && TUSB_CFG_HOST_HID_KEYBOARD)

#define _TINY_USB_SOURCE_FILE_

//--------------------------------------------------------------------+
// INCLUDE
//--------------------------------------------------------------------+
#include "common/common.h"
#include "hid_host.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+

//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+
STATIC_ hidh_keyboard_info_t keyboard_data[TUSB_CFG_HOST_DEVICE_MAX]; // does not have addr0, index = dev_address-1

static inline hidh_keyboard_info_t* get_kbd_data(uint8_t dev_addr) ATTR_PURE ATTR_ALWAYS_INLINE ATTR_WARN_UNUSED_RESULT;
static inline hidh_keyboard_info_t* get_kbd_data(uint8_t dev_addr)
{
  return &keyboard_data[dev_addr-1];
}

//--------------------------------------------------------------------+
// IMPLEMENTATION
//--------------------------------------------------------------------+

//--------------------------------------------------------------------+
// PUBLIC API (parameter validation required)
//--------------------------------------------------------------------+
bool  tusbh_hid_keyboard_is_supported(uint8_t dev_addr)
{
  return tusbh_device_is_configured(dev_addr) && pipehandle_is_valid(keyboard_data[dev_addr-1].pipe_hdl);
}

tusb_error_t tusbh_hid_keyboard_get(uint8_t dev_addr, uint8_t instance_num, tusb_keyboard_report_t * const report)
{
  //------------- parameters validation -------------//
  ASSERT_INT(TUSB_DEVICE_STATE_CONFIGURED, tusbh_device_get_state(dev_addr), TUSB_ERROR_DEVICE_NOT_READY);
  ASSERT_PTR(report, TUSB_ERROR_INVALID_PARA);

  (void) instance_num;

  hidh_keyboard_info_t *p_keyboard = get_kbd_data(dev_addr);

  // TODO abstract to use hidh service
  ASSERT_STATUS( hcd_pipe_xfer(p_keyboard->pipe_hdl, (uint8_t*) report, p_keyboard->report_size, true) ) ;

  return TUSB_ERROR_NONE;
}

//--------------------------------------------------------------------+
// INTERNAL API (no need for parameter validation)
//--------------------------------------------------------------------+
void hidh_keyboard_init(void)
{
  memclr_(&keyboard_data, sizeof(hidh_keyboard_info_t)*TUSB_CFG_HOST_DEVICE_MAX);
}

tusb_error_t hidh_keyboard_open_subtask(uint8_t dev_addr, uint8_t const *descriptor, uint16_t *p_length)
{
  hidh_keyboard_info_t *p_keyboard = get_kbd_data(dev_addr);
  uint8_t const *p_desc = descriptor;

  p_desc += p_desc[DESCRIPTOR_OFFSET_LENGTH]; // skip interface
  (*p_length) = p_desc - descriptor; // set ASAP, in case of error, p_length has to be not zero to prevent infinite re-open

  //------------- HID descriptor -------------//
  tusb_hid_descriptor_hid_t* const p_desc_hid = (tusb_hid_descriptor_hid_t* const) p_desc;
  ASSERT_INT(HID_DESC_HID, p_desc_hid->bDescriptorType, TUSB_ERROR_INVALID_PARA);

  p_desc += p_desc[DESCRIPTOR_OFFSET_LENGTH]; // TODO skip HID, only support std keyboard
  (*p_length) = p_desc - descriptor;

  //------------- Endpoint Descriptor -------------//
  ASSERT_INT(TUSB_DESC_ENDPOINT, p_desc[DESCRIPTOR_OFFSET_TYPE], TUSB_ERROR_INVALID_PARA);

  p_keyboard->pipe_hdl    = hcd_pipe_open(dev_addr, (tusb_descriptor_endpoint_t*) p_desc, TUSB_CLASS_HID);
  p_keyboard->report_size = ( ((tusb_descriptor_endpoint_t*) p_desc)->wMaxPacketSize & (BIT_(12)-1) );

  p_desc += p_desc[DESCRIPTOR_OFFSET_LENGTH]; // advance endpoint descriptor
  (*p_length) = p_desc - descriptor;

  ASSERT (pipehandle_is_valid(p_keyboard->pipe_hdl), TUSB_ERROR_HCD_FAILED);

  return TUSB_ERROR_NONE;
}

void hidh_keyboard_close(uint8_t dev_addr)
{

}

#endif

/*
 * hid_host_keyboard.h
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

/** \file
 *  \brief TBD
 *
 *  \note TBD
 */

/** \ingroup TBD
 *  \defgroup TBD
 *  \brief TBD
 *
 *  @{
 */

#ifndef _TUSB_HID_HOST_KEYBOARD_H_
#define _TUSB_HID_HOST_KEYBOARD_H_

#include "common/common.h"
#include "host/usbh.h" // TODO refractor
#include "hid.h"

#ifdef __cplusplus
 extern "C" {
#endif

//--------------------------------------------------------------------+
// PUBLIC API (parameter validation required)
//--------------------------------------------------------------------+
uint8_t       tusbh_hid_keyboard_no_instances(uint8_t const dev_addr) ATTR_PURE ATTR_WARN_UNUSED_RESULT;
static inline bool  tusbh_hid_keyboard_is_supported(uint8_t const dev_addr) ATTR_ALWAYS_INLINE ATTR_WARN_UNUSED_RESULT ATTR_PURE;
static inline bool  tusbh_hid_keyboard_is_supported(uint8_t const dev_addr)
{
  return tusbh_hid_keyboard_no_instances(dev_addr) > 0;
}

tusb_error_t  tusbh_hid_keyboard_get(uint8_t const dev_addr, uint8_t const instance_num, tusb_keyboard_report_t * const report) ATTR_WARN_UNUSED_RESULT;
pipe_status_t tusbh_hid_keyboard_pipe_status(uint8_t const dev_addr, uint8_t const instance_num) ATTR_WARN_UNUSED_RESULT;

//--------------------------------------------------------------------+
// INTERNAL API (no need for parameter validation)
//--------------------------------------------------------------------+
#ifdef _TINY_USB_SOURCE_FILE_

typedef struct {
  pipe_handle_t pipe_in;
  uint16_t report_size;
}keyboard_interface_t;

typedef struct {
  uint8_t instance_count;
  keyboard_interface_t instance[TUSB_CFG_HOST_HID_KEYBOARD_NO_INSTANCES_PER_DEVICE];
} hidh_keyboard_info_t;

void         hidh_keyboard_init(void);
tusb_error_t hidh_keyboard_install(uint8_t dev_addr, uint8_t const *descriptor) ATTR_WARN_UNUSED_RESULT;
tusb_error_t hidh_keyboard_open_subtask(uint8_t dev_addr, uint8_t const *descriptor, uint16_t *p_length) ATTR_WARN_UNUSED_RESULT;
void         hidh_keyboard_close(uint8_t dev_addr);

#endif

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_HID_HOST_KEYBOARD_H_ */

/** @} */

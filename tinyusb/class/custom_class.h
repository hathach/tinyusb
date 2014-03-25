/**************************************************************************/
/*!
    @file     custom_class.h
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

/** \ingroup group_class
 *  \defgroup Group_Custom Custom Class (not supported yet)
 *  @{ */

#ifndef _TUSB_CUSTOM_CLASS_H_
#define _TUSB_CUSTOM_CLASS_H_

#include "common/common.h"
#include "host/usbh.h"

#ifdef __cplusplus
 extern "C" {
#endif

typedef struct {
  pipe_handle_t pipe_in;
  pipe_handle_t pipe_out;
}custom_interface_info_t;

//--------------------------------------------------------------------+
// USBH-CLASS DRIVER API
//--------------------------------------------------------------------+
STATIC_ INLINE_ bool tusbh_custom_is_mounted(uint8_t dev_addr, uint16_t vendor_id, uint16_t product_id) ATTR_PURE ATTR_ALWAYS_INLINE ATTR_WARN_UNUSED_RESULT;
STATIC_ INLINE_ bool tusbh_custom_is_mounted(uint8_t dev_addr, uint16_t vendor_id, uint16_t product_id)
{
  (void) vendor_id; // TODO check this later
  (void) product_id;
//  return (tusbh_device_get_mounted_class_flag(dev_addr) & BIT_(TUSB_CLASS_MAPPED_INDEX_END-1) ) != 0;
  return false;
}

tusb_error_t tusbh_custom_read(uint8_t dev_addr, uint16_t vendor_id, uint16_t product_id, void * p_buffer, uint16_t length);
tusb_error_t tusbh_custom_write(uint8_t dev_addr, uint16_t vendor_id, uint16_t product_id, void const * p_data, uint16_t length);

#ifdef _TINY_USB_SOURCE_FILE_

void         cush_init(void);
tusb_error_t cush_open_subtask(uint8_t dev_addr, tusb_descriptor_interface_t const *p_interface_desc, uint16_t *p_length) ATTR_WARN_UNUSED_RESULT;
void         cush_isr(pipe_handle_t pipe_hdl, tusb_event_t event);
void         cush_close(uint8_t dev_addr);

#endif

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_CUSTOM_CLASS_H_ */

/** @} */

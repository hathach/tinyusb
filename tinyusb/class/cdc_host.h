/**************************************************************************/
/*!
    @file     cdc_host.h
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

/** \ingroup TBD
 *  \defgroup TBD
 *  \brief TBD
 *
 *  @{
 */

#ifndef _TUSB_CDC_HOST_H_
#define _TUSB_CDC_HOST_H_

#include "common/common.h"
#include "host/usbh.h"
#include "cdc.h"

#ifdef __cplusplus
 extern "C" {
#endif

//--------------------------------------------------------------------+
// APPLICATION PUBLIC API
//--------------------------------------------------------------------+

//--------------------------------------------------------------------+
// USBH-CLASS API
//--------------------------------------------------------------------+
#ifdef _TINY_USB_SOURCE_FILE_

typedef struct {
  uint8_t interface_number;
  uint8_t interface_protocol;
  cdc_fun_acm_capability_t acm_capability;

  pipe_handle_t pipe_notification, pipe_out, pipe_in;

} cdch_data_t;

extern cdch_data_t cdch_data[TUSB_CFG_HOST_DEVICE_MAX]; // TODO consider to move to cdch internal header file

void         cdch_init(void);
tusb_error_t cdch_open_subtask(uint8_t dev_addr, tusb_descriptor_interface_t const *p_interface_desc, uint16_t *p_length) ATTR_WARN_UNUSED_RESULT;
void         cdch_isr(pipe_handle_t pipe_hdl, tusb_event_t event);
void         cdch_close(uint8_t dev_addr);

#endif

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_CDC_HOST_H_ */

/** @} */

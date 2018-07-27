/**************************************************************************/
/*!
    @file     custom_device.h
    @author   hathach (tinyusb.org)

    @section LICENSE

    Software License Agreement (BSD License)

    Copyright (c) 2018, hathach (tinyusb.org)
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

#ifndef _TUSB_CUSTOM_DEVICE_H_
#define _TUSB_CUSTOM_DEVICE_H_

#include "common/tusb_common.h"
#include "device/usbd.h"

//--------------------------------------------------------------------+
// APPLICATION API (Multiple Root Ports)
// Should be used only with MCU that support more than 1 ports
//--------------------------------------------------------------------+

//--------------------------------------------------------------------+
// APPLICATION API (Single Port)
// Should be used with MCU supporting only 1 USB port for code simplicity
//--------------------------------------------------------------------+


//--------------------------------------------------------------------+
// APPLICATION CALLBACK API (WEAK is optional)
//--------------------------------------------------------------------+

//--------------------------------------------------------------------+
// USBD-CLASS DRIVER API
//--------------------------------------------------------------------+
#ifdef _TINY_USB_SOURCE_FILE_

void cusd_init(void);
tusb_error_t cusd_open(uint8_t rhport, tusb_desc_interface_t const * p_interface_desc, uint16_t *p_length);
tusb_error_t cusd_control_request_st(uint8_t rhport, tusb_control_request_t const * p_request);
tusb_error_t cusd_xfer_cb(uint8_t rhport, uint8_t edpt_addr, tusb_event_t event, uint32_t xferred_bytes);
void cusd_reset(uint8_t rhport);
#endif


#endif /* _TUSB_CUSTOM_DEVICE_H_ */

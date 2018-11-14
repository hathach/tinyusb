/**************************************************************************/
/*!
    @file     usbd.h
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

/** \ingroup group_usbd
 *  @{ */

#ifndef _TUSB_CONTROL_H_
#define _TUSB_CONTROL_H_

#ifdef __cplusplus
 extern "C" {
#endif

#include "tusb.h"

typedef enum {
    CONTROL_STAGE_SETUP, // Waiting for a setup token.
    CONTROL_STAGE_DATA, // In the process of sending or receiving data.
    CONTROL_STAGE_STATUS // In the process of transmitting the STATUS ZLP.
} control_stage_t;

typedef struct {
    control_stage_t current_stage;
    tusb_control_request_t current_request;
    uint16_t total_transferred;
    uint8_t config;
} control_t;

extern uint8_t _shared_control_buffer[64];

tusb_error_t controld_process_setup_request(uint8_t rhport, tusb_control_request_t const * const p_request);

// Callback when the configuration of the device is changed.
tusb_error_t tud_control_set_config_cb(uint8_t rhport, uint8_t config_number);

// Called when the DATA stage of a control transaction is complete.
void tud_control_interface_control_complete_cb(uint8_t rhport, uint8_t interface, tusb_control_request_t const * const p_request);

tusb_error_t tud_control_interface_control_cb(uint8_t rhport, uint8_t interface, tusb_control_request_t const * const p_request, uint16_t bytes_already_sent);

//--------------------------------------------------------------------+
// INTERNAL API
//--------------------------------------------------------------------+
tusb_error_t controld_open(uint8_t rhport, tusb_desc_interface_t const * p_interface_desc, uint16_t *p_length);

// This tracks the state of a control request.
tusb_error_t controld_process_setup_request(uint8_t rhport, tusb_control_request_t const * p_request);

// This handles the actual request and its response.
tusb_error_t controld_process_control_request(uint8_t rhport, tusb_control_request_t const * p_request, uint16_t bytes_already_sent);

tusb_error_t controld_xfer_cb(uint8_t rhport, uint8_t edpt_addr, tusb_event_t event, uint32_t xferred_bytes);
void controld_reset(uint8_t rhport);

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_CONTROL_H_ */

/** @} */

/**************************************************************************/
/*!
    @file     usbd_pvt.h
    @author   hathach

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
#ifndef USBD_PVT_H_
#define USBD_PVT_H_

#include "osal/osal.h"
#include "common/tusb_fifo.h"

#ifdef __cplusplus
 extern "C" {
#endif

// Either point to tud_desc_set or usbd_auto_desc_set depending on CFG_TUD_DESC_AUTO
extern tud_desc_set_t const* usbd_desc_set;

//--------------------------------------------------------------------+
// INTERNAL API for stack management
//--------------------------------------------------------------------+
bool usbd_init (void);
void usbd_task (void* param);


// Carry out Data and Status stage of control transfer
// - If len = 0, it is equivalent to sending status only
// - If len > wLength : it will be truncated
bool usbd_control_xfer(uint8_t rhport, tusb_control_request_t const * request, void* buffer, uint16_t len);

// Send STATUS (zero length) packet
bool usbd_control_status(uint8_t rhport, tusb_control_request_t const * request);

// Stall control endpoint until new setup packet arrived
void usbd_control_stall(uint8_t rhport);

/*------------------------------------------------------------------*/
/* Helper
 *------------------------------------------------------------------*/
// helper to parse an pair of In and Out endpoint descriptors. They must be consecutive
bool usbd_open_edpt_pair(uint8_t rhport, tusb_desc_endpoint_t const* p_desc_ep, uint8_t xfer_type, uint8_t* ep_out, uint8_t* ep_in);

void usbd_defer_func( osal_task_func_t func, void* param, bool in_isr );


#ifdef __cplusplus
 }
#endif

#endif /* USBD_PVT_H_ */

/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2018, hathach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * This file is part of the TinyUSB stack.
 */
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

// Carry out Data and Status stage of control transfer
// - If len = 0, it is equivalent to sending status only
// - If len > wLength : it will be truncated
bool usbd_control_xfer(uint8_t rhport, tusb_control_request_t const * request, void* buffer, uint16_t len);

// Send STATUS (zero length) packet
bool usbd_control_status(uint8_t rhport, tusb_control_request_t const * request);

// Stall control endpoint (both IN and OUT) until new setup packet arrived
void usbd_control_stall(uint8_t rhport);


void usbd_edpt_stall(uint8_t rhport, uint8_t ep_addr);
void usbd_edpt_clear_stall(uint8_t rhport, uint8_t ep_addr);
bool usbd_edpt_stalled(uint8_t rhport, uint8_t ep_addr);

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

/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
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

#ifndef _TUSB_CDC_NCM_DEVICE_H_
#define _TUSB_CDC_NCM_DEVICE_H_

#include "common/tusb_common.h"
#include "device/usbd.h"
#include "cdc.h"

#ifdef __cplusplus
 extern "C" {
#endif

#ifndef CFG_TUD_NCM_EP_BUFSIZE
#define CFG_TUD_NCM_EP_BUFSIZE (TUD_OPT_HIGH_SPEED ? 512 : 64)
#endif

#ifndef CFG_TUD_NCM_IN_NTB_MAX_SIZE
#define CFG_TUD_NCM_IN_NTB_MAX_SIZE 3200
#endif

#ifndef CFG_TUD_NCM_OUT_NTB_MAX_SIZE
#define CFG_TUD_NCM_OUT_NTB_MAX_SIZE 3200
#endif

#ifndef CFG_TUD_NCM_MAX_DATAGRAMS_PER_NTB
#define CFG_TUD_NCM_MAX_DATAGRAMS_PER_NTB 8
#endif

#ifndef CFG_TUD_NCM_ALIGNMENT
#define CFG_TUD_NCM_ALIGNMENT 4
#endif

/**
 * Transmit data. Returns true if successful, false if the packet was rejected (transmit buffer full,
 * interface not up).
 *
 * This is structured with a callback to avoid the need for multiple copies; some network stacks,
 * including LwIP, allow for non-contiguous buffers and provide a function to flatten them. In the
 * LwIP case, a thin wrapper around pbuf_copy_partial() can be passed here.
 */
bool tud_ncm_xmit(void *arg, uint16_t size, void (*flatten)(void *arg, uint8_t *buffer, uint16_t size));

//--------------------------------------------------------------------+
// Application Callback API
//--------------------------------------------------------------------+
void tud_ncm_receive_cb(uint8_t *data, size_t length);
void tud_ncm_link_state_cb(bool state);

//--------------------------------------------------------------------+
// INTERNAL USBD-CLASS DRIVER API
//--------------------------------------------------------------------+
void     ncmd_init             (void);
void     ncmd_reset            (uint8_t rhport);
uint16_t ncmd_open             (uint8_t rhport, tusb_desc_interface_t const * itf_desc, uint16_t max_len);
bool     ncmd_control_xfer_cb  (uint8_t rhport, uint8_t stage, tusb_control_request_t const * request);
bool     ncmd_xfer_cb          (uint8_t rhport, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes);

#ifdef __cplusplus
}
#endif

#endif /* _TUSB_CDC_NCM_DEVICE_H_ */

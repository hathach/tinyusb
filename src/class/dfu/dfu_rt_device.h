/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Sylvain Munaut <tnt@246tNt.com>
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

#ifndef _TUSB_DFU_RT_DEVICE_H_
#define _TUSB_DFU_RT_DEVICE_H_

#include "common/tusb_common.h"
#include "device/usbd.h"
#include "dfu.h"

#ifdef __cplusplus
 extern "C" {
#endif

//--------------------------------------------------------------------+
// Application Callback API (weak is optional)
//--------------------------------------------------------------------+
// Allow the application to update the status as required.
// Is set to DFU_STATUS_OK during internal initialization and USB reset
// Value is not checked to allow for custom statuses to be used
void tud_dfu_runtime_set_status(dfu_mode_device_status_t status);

// Invoked when a DFU_DETACH request is received and bitWillDetach is set
void tud_dfu_runtime_reboot_to_dfu_cb();

// Invoked when a DFU_DETACH request is received and bitWillDetach is not set
// This should start a timer for wTimeout ms
// When the timer has elapsed, the app must call tud_dfu_runtime_detach_timer_elapsed
// If a USB reset is called while the timer is running, the class will call
// tud_dfu_runtime_reboot_to_dfu_cb.
// NOTE: This callback should return immediately, and not implement the delay
// internally, as this can will hold up the USB stack
TU_ATTR_WEAK void tud_dfu_runtime_detach_start_timer_cb(uint16_t wTimeout);

// Invoke when the dfu runtime detach timer has elapsed
void tud_dfu_runtime_detach_timer_elapsed();

// Invoked when a nonstandard request is received
// Use may be vendor specific.
// Return false to stall
TU_ATTR_WEAK bool tud_dfu_runtime_req_nonstandard_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const * request);

// Invoked during initialization of the dfu driver to set attributes
// Return byte set with bitmasks:
//   DFU_FUNC_ATTR_CAN_DOWNLOAD_BITMASK
//   DFU_FUNC_ATTR_CAN_UPLOAD_BITMASK
//   DFU_FUNC_ATTR_MANIFESTATION_TOLERANT_BITMASK
//   DFU_FUNC_ATTR_WILL_DETACH_BITMASK
// Note: This should match the USB descriptor
uint8_t tud_dfu_runtime_init_attrs_cb();

// Invoked during a DFU_GETSTATUS request to get for the string index
// to the status description string table.
TU_ATTR_WEAK uint8_t tud_dfu_runtime_get_status_desc_table_index_cb();

//--------------------------------------------------------------------+
// Internal Class Driver API
//--------------------------------------------------------------------+
void     dfu_rtd_init(void);
void     dfu_rtd_reset(uint8_t rhport);
uint16_t dfu_rtd_open(uint8_t rhport, tusb_desc_interface_t const * itf_desc, uint16_t max_len);
bool     dfu_rtd_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const * request);

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_DFU_RT_DEVICE_H_ */

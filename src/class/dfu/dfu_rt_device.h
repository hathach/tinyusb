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

// Invoked when a DFU_DETACH request is received and bitWillDetatch is set
void tud_dfu_runtime_reboot_to_dfu_cb();

// Invoked when a DFU_DETACH request is received and bitWillDetatch is not set
// This should start a timer for wTimeout ms
// The application should then look for a USB reset signal to occur before
// the timeout has elapsed.  The reset signal will invoke tud_dfu_runtime_usb_reset_cb.
// If this reset signal is received before the timeout, then the application must
// switch to DFU mode.
// If the timer expires, the app does not need to do anything.
// NOTE: This callback should return immediately, and not implement the delay
// internally, as this can will hold up the USB stack
TU_ATTR_WEAK void tud_dfu_runtime_detach_start_timer_cb(uint16_t wTimeout);

// Invoked when a nonstandard request is received
// Use may be vendor specific.
// Return false to stall
TU_ATTR_WEAK bool tud_dfu_runtime_req_nonstandard_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const * request);

// Invoked when a reset is received to check if firmware is valid
bool tud_dfu_runtime_firmware_valid_check_cb();

// Invoked during initialization of the dfu driver to set attributes
// Return byte set with bitmasks:
//   DFU_FUNC_ATTR_CAN_DOWNLOAD_BITMASK
//   DFU_FUNC_ATTR_CAN_UPLOAD_BITMASK
//   DFU_FUNC_ATTR_MANIFESTATION_TOLERANT_BITMASK
//   DFU_FUNC_ATTR_WILL_DETACH_BITMASK
// Note: This should match the USB descriptor
uint8_t tud_dfu_runtime_init_attrs_cb();

// Invoked during initialization of the dfu driver to start as RT or DFU
// This will determine if the internal state machine will begin in
// APP_IDLE or DFU_IDLE
TU_ATTR_WEAK dfu_protocol_type_t tud_dfu_runtime_init_cb();

// Invoked during a DFU_GETSTATUS request to get for the string index
// to the status description string table.
TU_ATTR_WEAK uint8_t tud_dfu_runtime_get_status_desc_table_index_cb();

// Invoked during a USB reset
// Lets the app know that a USB reset has occurred so it can act accordingly,
// See tud_dfu_runtime_detach_start_timer_cb for how to handle the APP_DETACH state
// Other states will be application specific
TU_ATTR_WEAK void tud_dfu_runtime_usb_reset_cb(uint8_t rhport, dfu_mode_state_t state);

// Invoked during a DFU_GETSTATUS request to set the timeout in ms to use
// before the subsequent DFU_GETSTATUS requests.
// The purpose of this value is to allow the device to tell the host
// how long to wait between the DFU_DNLOAD and DFU_GETSTATUS checks
// to allow the device to have time to erase, write memory, etc.
// ms_timeout is a pointer to array of length 3.
// Refer to the USB DFU class specification for more details
TU_ATTR_WEAK void tud_dfu_runtime_get_poll_timeout_cb(uint8_t *ms_timeout);

// Invoked when a DFU_DNLOAD request is received
// This should start a timer for ms_timeout ms
// When the timer has elapsed, tud_dfu_runtime_poll_timeout_done must be called
// NOTE: This callback should return immediately, and not implement the delay
// internally, as this will hold up the class stack from receiving any packets
// during the DFU_DNLOAD_SYNC and DFU_DNBUSY states
void tud_dfu_runtime_start_poll_timeout_cb(uint8_t *ms_timeout);

// Must be called when the poll_timeout has elapsed
void tud_dfu_runtime_poll_timeout_done();

// Invoked when a DFU_DNLOAD request is received
// This callback takes the wBlockNum chunk of length length and provides it
// to the application at the data pointer.  This data is only valid for this
// call, so the app must use it not or copy it.
void tud_dfu_runtime_req_dnload_data_cb(uint16_t wBlockNum, uint8_t* data, uint16_t length);

// Invoked during the last DFU_DNLOAD request, signifying that the host believes
// it is done transmitting data.
// Return true if the application agrees there is no more data
// Return false if the device disagrees, which will stall the pipe, and the Host
//              should initiate a recovery procedure
bool tud_dfu_runtime_device_data_done_check_cb();

// Invoked when the Host has terminated a download or upload transfer
TU_ATTR_WEAK void tud_dfu_runtime_abort_cb();

// Invoked when a DFU_UPLOAD request is received
// This callback must populate data with up to length bytes
// Return the number of bytes to write
uint16_t tud_dfu_runtime_req_upload_data_cb(uint16_t block_num, uint8_t* data, uint16_t length);

//--------------------------------------------------------------------+
// Internal Class Driver API
//--------------------------------------------------------------------+
void     dfu_rtd_init(void);
void     dfu_rtd_reset(uint8_t rhport);
uint16_t dfu_rtd_open(uint8_t rhport, tusb_desc_interface_t const * itf_desc, uint16_t max_len);
bool     dfu_rtd_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const * request);

//--------------------------------------------------------------------+
void     dfu_mode_init(void);
void     dfu_mode_reset(uint8_t rhport);
void     dfu_mode_req_dnload_reply(uint8_t rhport, tusb_control_request_t const * request);
#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_DFU_RT_DEVICE_H_ */

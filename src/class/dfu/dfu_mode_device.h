/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2021 XMOS LIMITED
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

#ifndef _TUSB_DFU_MODE_DEVICE_H_
#define _TUSB_DFU_MODE_DEVICE_H_

#include "common/tusb_common.h"
#include "device/usbd.h"
#include "dfu.h"

#ifdef __cplusplus
  extern "C" {
#endif


//--------------------------------------------------------------------+
// Application Callback API (weak is optional)
//--------------------------------------------------------------------+
// Invoked when a reset is received to check if firmware is valid
bool tud_dfu_mode_firmware_valid_check_cb(void);

// Invoked when the device must reboot to dfu runtime mode
void tud_dfu_mode_reboot_to_rt_cb(void);

// Invoked during initialization of the dfu driver to set attributes
// Return byte set with bitmasks:
//   DFU_FUNC_ATTR_CAN_DOWNLOAD_BITMASK
//   DFU_FUNC_ATTR_CAN_UPLOAD_BITMASK
//   DFU_FUNC_ATTR_MANIFESTATION_TOLERANT_BITMASK
//   DFU_FUNC_ATTR_WILL_DETACH_BITMASK
// Note: This should match the USB descriptor
uint8_t tud_dfu_mode_init_attrs_cb(void);

// Invoked during a DFU_GETSTATUS request to get for the string index
// to the status description string table.
TU_ATTR_WEAK uint8_t tud_dfu_mode_get_status_desc_table_index_cb(void);

// Invoked during a USB reset
// Lets the app perform custom behavior on a USB reset.
// If not defined, the default behavior remains the DFU specification of
// Checking the firmware valid and changing to the error state or rebooting to runtime
// Note: If used, the application must perform the reset logic for all states.
// Changing the state to APP_IDLE will result in tud_dfu_mode_reboot_to_rt_cb being called
TU_ATTR_WEAK void tud_dfu_mode_usb_reset_cb(uint8_t rhport, dfu_mode_state_t *state);

// Invoked during a DFU_GETSTATUS request to set the timeout in ms to use
// before the subsequent DFU_GETSTATUS requests.
// The purpose of this value is to allow the device to tell the host
// how long to wait between the DFU_DNLOAD and DFU_GETSTATUS checks
// to allow the device to have time to erase, write memory, etc.
// ms_timeout is a pointer to array of length 3.
// Refer to the USB DFU class specification for more details
TU_ATTR_WEAK void tud_dfu_mode_get_poll_timeout_cb(uint8_t *ms_timeout);

// Invoked when a DFU_DNLOAD request is received
// This should start a timer for ms_timeout ms
// When the timer has elapsed, tud_dfu_runtime_poll_timeout_done must be called
// NOTE: This callback should return immediately, and not implement the delay
// internally, as this will hold up the class stack from receiving any packets
// during the DFU_DNLOAD_SYNC and DFU_DNBUSY states
void tud_dfu_mode_start_poll_timeout_cb(uint8_t *ms_timeout);

// Must be called when the poll_timeout has elapsed
void tud_dfu_mode_poll_timeout_done(void);

// Invoked when a DFU_DNLOAD request is received
// This callback takes the wBlockNum chunk of length length and provides it
// to the application at the data pointer.  This data is only valid for this
// call, so the app must use it not or copy it.
void tud_dfu_mode_req_dnload_data_cb(uint16_t wBlockNum, uint8_t* data, uint16_t length);

// Invoked during the last DFU_DNLOAD request, signifying that the host believes
// it is done transmitting data.
// Return true if the application agrees there is no more data
// Return false if the device disagrees, which will stall the pipe, and the Host
//              should initiate a recovery procedure
bool tud_dfu_mode_device_data_done_check_cb(void);

// Invoked when the Host has terminated a download or upload transfer
TU_ATTR_WEAK void tud_dfu_mode_abort_cb(void);

// Invoked when a DFU_UPLOAD request is received
// This callback must populate data with up to length bytes
// Return the number of bytes to write
uint16_t tud_dfu_mode_req_upload_data_cb(uint16_t block_num, uint8_t* data, uint16_t length);

// Invoked when a nonstandard request is received
// Use may be vendor specific.
// Return false to stall
TU_ATTR_WEAK bool tud_dfu_mode_req_nonstandard_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const * request);

// Invoked during a DFU_GETSTATUS request to get for the string index
// to the status description string table.
TU_ATTR_WEAK uint8_t tud_dfu_mode_get_status_desc_table_index_cb(void);
//--------------------------------------------------------------------+
// Internal Class Driver API
//--------------------------------------------------------------------+
void     dfu_moded_init(void);
void     dfu_moded_reset(uint8_t rhport);
uint16_t dfu_moded_open(uint8_t rhport, tusb_desc_interface_t const * itf_desc, uint16_t max_len);
bool     dfu_moded_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const * request);


#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_DFU_MODE_DEVICE_H_ */

/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 * Copyright (c) 2021 Koji KITAYAMA
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

#ifndef TUSB_VIDEO_DEVICE_H_
#define TUSB_VIDEO_DEVICE_H_

#ifdef __cplusplus
 extern "C" {
#endif

//--------------------------------------------------------------------+
// Application Callback API (weak is optional)
//--------------------------------------------------------------------+

// Invoked when GET_INFO request received
TU_ATTR_WEAK bool tud_video_get_info_cb(uint8_t rhport, tusb_control_request_t const *request, uint8_t const *itf_desc);
// Invoked when GET_INFO request received
TU_ATTR_WEAK bool tud_video_set_cur_cb(uint8_t rhport, tusb_control_request_t const *request);
// Invoked when GET_CUR request received
TU_ATTR_WEAK bool tud_video_get_info_cb(uint8_t rhport, tusb_control_request_t const *request);

//--------------------------------------------------------------------+
// INTERNAL USBD-CLASS DRIVER API
//--------------------------------------------------------------------+
void     videod_init           (void);
void     videod_reset          (uint8_t rhport);
uint16_t videod_open           (uint8_t rhport, tusb_desc_interface_t const * itf_desc, uint16_t max_len);
bool     videod_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const * request);
bool     videod_xfer_cb        (uint8_t rhport, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes);

#ifdef __cplusplus
 }
#endif

#endif

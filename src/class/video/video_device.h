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

#include "common/tusb_common.h"
#include "video.h"

#ifdef __cplusplus
extern "C" {
#endif

//--------------------------------------------------------------------+
// Application API (Multiple Ports)
// CFG_TUD_VIDEO > 1
//--------------------------------------------------------------------+
bool tud_video_n_streaming(uint8_t itf);


/* itf       instance number of streaming interface */
bool tud_video_n_frame_xfer(uint8_t itf, uint32_t pts, void *buffer, size_t bufsize);

/*------------- Optional callbacks -------------*/
/* itf       instance number of streaming interface */
TU_ATTR_WEAK int tud_video_frame_xfer_complete_cb(unsigned itf);

//--------------------------------------------------------------------+
// Application Callback API (weak is optional)
//--------------------------------------------------------------------+

/* Invoked when SET POWER_MODE request received to 
 * @return video_error_code_t */
TU_ATTR_WEAK int tud_video_power_mode_cb(unsigned itf, uint8_t power_mod);

/* @return video_error_code_t */
TU_ATTR_WEAK int tud_video_probe_set_cb(unsigned itf, video_probe_and_commit_control_t const *settings);

/* @return video_error_code_t */
TU_ATTR_WEAK int tud_video_commit_set_cb(unsigned itf, video_probe_and_commit_control_t const *settings);
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

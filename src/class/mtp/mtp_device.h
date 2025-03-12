/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2025 Ennebi Elettronica (https://ennebielettronica.com)
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

#ifndef _TUSB_MTP_DEVICE_H_
#define _TUSB_MTP_DEVICE_H_

#include "common/tusb_common.h"
#include "mtp.h"

#if (CFG_TUD_ENABLED && CFG_TUD_MTP)

#ifdef __cplusplus
 extern "C" {
#endif

//--------------------------------------------------------------------+
// Internal Class Driver API
//--------------------------------------------------------------------+
void     mtpd_init            (void);
bool     mtpd_deinit          (void);
void     mtpd_reset           (uint8_t rhport);
uint16_t mtpd_open            (uint8_t rhport, tusb_desc_interface_t const *itf_desc, uint16_t max_len);
bool     mtpd_control_xfer_cb (uint8_t rhport, uint8_t stage, tusb_control_request_t const *p_request);
bool     mtpd_xfer_cb         (uint8_t rhport, uint8_t ep_addr, xfer_result_t event, uint32_t xferred_bytes);

//--------------------------------------------------------------------+
// Helper functions
//--------------------------------------------------------------------+
// Generic container function
void mtpd_wc16cpy(uint8_t *dest, const char *src);
bool mtpd_gct_append_uint8(const uint8_t value);
bool mtpd_gct_append_object_handle(const uint32_t object_handle);
bool mtpd_gct_append_wstring(const char *s);
bool mtpd_gct_get_string(uint16_t *offset_data, char *string, const uint16_t max_size);

// Append the given array to the global context buffer
// The function returns true if the data fits in the available buffer space.
bool mtpd_gct_append_array(uint32_t array_size, const void *data, size_t type_size);

// Append an UTC date string to the global context buffer
// Required format is 'YYYYMMDDThhmmss.s' optionally added 'Z' for UTC or +/-hhmm for time zone
// This function is provided for reference and only supports UTC format without partial seconds
// The function returns true if the data fits in the available buffer space.
bool mtpd_gct_append_date(struct tm *timeinfo);

#ifdef __cplusplus
 }
#endif

#endif /* CFG_TUD_ENABLED && CFG_TUD_MTP */

#endif /* _TUSB_MTP_DEVICE_H_ */

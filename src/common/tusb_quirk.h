/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2024 HiFiPhile
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
 */

#ifndef _TUSB_QUIRK_H_
#define _TUSB_QUIRK_H_

#ifdef __cplusplus
 extern "C" {
#endif

#include "common/tusb_common.h"

//===================================== WARNING =========================================
// These quirks operate out of USB specification in order to workaround specific issues.
// They may not work on your platform.
//=======================================================================================

#ifndef CFG_TUD_QUIRK_HOST_OS_HINT
#define CFG_TUD_QUIRK_HOST_OS_HINT 0
#endif

// Host OS detection, can be used to adjust configuration to workaround host limits.
//
// Prerequisites:
// - Set USB version to at least 2.01 in Device Descriptor
// - Has a valid BOS Descriptor, refer to webusb_serial example
//
// Attention:
//   Windows detection result comes out after Configuration Descriptor request,
//   meaning it will be too late to do descriptor adjustment. It's advised to make
//   Windows as default configuration and adjust to other OS accordingly.
#if CFG_TUD_QUIRK_HOST_OS_HINT
typedef enum {
  TUD_QUIRK_OS_HINT_UNKNOWN,
  TUD_QUIRK_OS_HINT_LINUX,
  TUD_QUIRK_OS_HINT_OSX,
  TUD_QUIRK_OS_HINT_WINDOWS,
} tud_quirk_host_os_t;

// Get Host OS type
tud_quirk_host_os_t tud_quirk_host_os_hint(void);

// Internal callback
void tud_quirk_host_os_hint_desc_cb(tusb_desc_type_t desc);
#endif


#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_QUIRK_H_ */

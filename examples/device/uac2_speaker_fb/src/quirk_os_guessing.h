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

#ifndef _QUIRK_OS_GUESSING_H_
#define _QUIRK_OS_GUESSING_H_

#ifdef __cplusplus
 extern "C" {
#endif

#include "tusb.h"

//================================== !!! WARNING !!! ====================================
// This quirk operate out of USB specification in order to workaround specific issues.
// It may not work on your platform.
//=======================================================================================
//
// Prerequisites:
// - Set USB version to at least 2.01 in Device Descriptor
// - Has a valid BOS Descriptor, refer to webusb_serial example
//
// Attention:
//   Windows detection result comes out after Configuration Descriptor request,
//   meaning it will be too late to do descriptor adjustment. It's advised to make
//   Windows as default configuration and adjust to other OS accordingly.

typedef enum {
  QUIRK_OS_GUESSING_UNKNOWN,
  QUIRK_OS_GUESSING_LINUX,
  QUIRK_OS_GUESSING_OSX,
  QUIRK_OS_GUESSING_WINDOWS,
} quirk_os_guessing_t;

// Get Host OS type
quirk_os_guessing_t quirk_os_guessing_get(void);

// Place at the start of tud_descriptor_device_cb()
void quirk_os_guessing_desc_device_cb(void);

// Place at the start of tud_descriptor_configuration_cb()
void quirk_os_guessing_desc_configuration_cb(void);

// Place at the start of tud_descriptor_bos_cb()
void quirk_os_guessing_desc_bos_cb(void);

// Place at the start of tud_descriptor_string_cb()
void quirk_os_guessing_desc_string_cb(void);

#ifdef __cplusplus
 }
#endif

#endif /* _QUIRK_OS_GUESSING_H_ */

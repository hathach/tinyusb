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

#include "quirk_os_guessing.h"

static tusb_desc_type_t desc_req_buf[2];
static int desc_req_idx = 0;

// Place at the start of tud_descriptor_device_cb()
void quirk_os_guessing_desc_device_cb() {
  desc_req_idx = 0;
}

// Place at the start of tud_descriptor_configuration_cb()
void quirk_os_guessing_desc_configuration_cb() {
  // Skip redundant request
  if (desc_req_idx == 0 || (desc_req_idx == 1 && desc_req_buf[0] != TUSB_DESC_CONFIGURATION)) {
    desc_req_buf[desc_req_idx++] = TUSB_DESC_CONFIGURATION;
  }
}

// Place at the start of tud_descriptor_bos_cb()
void quirk_os_guessing_desc_bos_cb() {
  // Skip redundant request
  if (desc_req_idx == 0 || (desc_req_idx == 1 && desc_req_buf[0] != TUSB_DESC_BOS)) {
    desc_req_buf[desc_req_idx++] = TUSB_DESC_BOS;
  }
}

// Place at the start of tud_descriptor_string_cb()
void quirk_os_guessing_desc_string_cb() {
  // Skip redundant request
  if (desc_req_idx == 0 || (desc_req_idx == 1 && desc_req_buf[0] != TUSB_DESC_STRING)) {
    desc_req_buf[desc_req_idx++] = TUSB_DESC_STRING;
  }
}

// Each OS request descriptors differently:
// Windows 10 - 11
//    Device Desc
//    Config Desc
//    BOS    Desc
//    String Desc
// Linux 3.16 - 6.8
//    Device Desc
//    BOS    Desc
//    Config Desc
//    String Desc
// OS X Ventura - Sonoma
//    Device Desc
//    String Desc
//    Config Desc || BOS    Desc
//    BOS    Desc || Config Desc
quirk_os_guessing_t quirk_os_guessing_get(void) {
  if (desc_req_idx < 2) {
    return QUIRK_OS_GUESSING_UNKNOWN;
  }

  if (desc_req_buf[0] == TUSB_DESC_BOS && desc_req_buf[1] == TUSB_DESC_CONFIGURATION) {
    return QUIRK_OS_GUESSING_LINUX;
  } else if (desc_req_buf[0] == TUSB_DESC_CONFIGURATION && desc_req_buf[1] == TUSB_DESC_BOS) {
    return QUIRK_OS_GUESSING_WINDOWS;
  } else if (desc_req_buf[0] == TUSB_DESC_STRING && (desc_req_buf[1] == TUSB_DESC_BOS || desc_req_buf[1] == TUSB_DESC_CONFIGURATION)) {
    return QUIRK_OS_GUESSING_OSX;
  }

  return QUIRK_OS_GUESSING_UNKNOWN;
}

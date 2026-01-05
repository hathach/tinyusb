/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2026 Ha Thach (tinyusb.org)
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

#ifndef _TUSB_PRINTER_H_
#define _TUSB_PRINTER_H_

#include "common/tusb_common.h"

#ifdef __cplusplus
 extern "C" {
#endif

/// Printer Class Specific Control Request
typedef enum
{
  PRINTER_REQ_CONTROL_GET_DEVICE_ID   = 0x01, ///< Get device ID
  PRINTER_REQ_CONTROL_GET_PORT_STATUS = 0x02, ///< Get port status
  PRINTER_REQ_CONTROL_SOFT_RESET      = 0x03, ///< Soft reset
}printer_request_enum_t;

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_PRINTER_H__ */

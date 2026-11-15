/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2026, hathach (tinyusb.org)
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

// Shared by the MSC device driver tests: constructors for a Data-In CBW and the CSW that answers it

#ifndef MSC_DEVICE_TEST_H_
#define MSC_DEVICE_TEST_H_

#include "tusb.h"

static inline msc_cbw_t data_in_cbw(uint8_t cmd_code, uint32_t total_bytes) {
  msc_cbw_t cbw = {
    .signature   = MSC_CBW_SIGNATURE,
    .tag         = 0xCAFECAFE,
    .total_bytes = total_bytes,
    .lun         = 0,
    .dir         = TUSB_DIR_IN_MASK,
    .cmd_len     = 6,
    .command     = { cmd_code, 0, 0x3F, 0, (uint8_t) total_bytes, 0 }
  };
  return cbw;
}

static inline msc_csw_t csw_with_residue(uint32_t residue) {
  msc_csw_t csw = {
    .signature    = MSC_CSW_SIGNATURE,
    .tag          = 0xCAFECAFE,
    .data_residue = residue,
    .status       = MSC_CSW_STATUS_PASSED
  };
  return csw;
}

#endif

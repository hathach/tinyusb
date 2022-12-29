/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2022 Nathaniel Brough
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

#include "fuzz/fuzz_private.h"
#include "tusb.h"
// #include "usb_descriptors.h"

#ifndef CFG_FUZZ_MAX_STRING_LEN
#define CFG_FUZZ_MAX_STRING_LEN 1000
#endif

extern "C" {

/* TODO: Implement a fuzzed version of this.
uint8_t const *tud_descriptor_bos_cb(void) {  }
*/

/* TODO: Implement a fuzzed version of this.
uint8_t const *tud_descriptor_device_qualifier_cb(void) {}
*/

/* TODO: Implement a fuzzed version of this.
uint8_t const *tud_descriptor_other_speed_configuration_cb(uint8_t index) {}
*/

void tud_mount_cb(void) {
  // NOOP
}

void tud_umount_cb(void) {
  // NOOP
}

void tud_suspend_cb(bool remote_wakeup_en) {
  (void)remote_wakeup_en;
  // NOOP
}

void tud_resume_cb(void) {
  // NOOP
}

/* TODO: Implement a fuzzed version of this.
bool tud_vendor_control_xfer_cb(uint8_t rhport, uint8_t stage,
                                tusb_control_request_t const *request) {}
*/

/* TODO: Implement a fuzzed version of this.
uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid) {}
*/
}

/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 TinyUSB contributors
 * SPDX-License-Identifier: MIT
 */

#include <stdbool.h>
#include <stdint.h>

// Kept out of test_usbh_enum_xfer_retry.c, which includes usbh.c: there it would redefine a weak hcd_edpt_close()
// default in usbh.c, while a separate translation unit overrides it at link time.
bool hcd_edpt_close(uint8_t rhport, uint8_t daddr, uint8_t ep_addr) {
  (void) rhport;
  (void) daddr;
  (void) ep_addr;
  return true;
}

/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 TinyUSB contributors
 * SPDX-License-Identifier: MIT
 */

#include <stdbool.h>
#include <stdint.h>

#include "unity.h"

// Kept in a separate translation unit to override usbh.c's weak hcd_edpt_close(), which test_usbh_abort.c includes.
extern uint8_t hcd_abort_count;
extern uint8_t hcd_close_count;

bool hcd_edpt_close(uint8_t rhport, uint8_t daddr, uint8_t ep_addr) {
  (void) rhport;
  (void) daddr;
  (void) ep_addr;
  TEST_ASSERT_EQUAL(1, hcd_abort_count); // the abort must have reached the HCD before the close
  hcd_close_count++;
  return true;
}

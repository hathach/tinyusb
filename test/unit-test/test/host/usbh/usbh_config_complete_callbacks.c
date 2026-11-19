/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 TinyUSB contributors
 * SPDX-License-Identifier: MIT
 */

#include <stdint.h>

// Kept in a separate translation unit to override usbh.c's weak callbacks, which test_usbh_config_complete.c includes.
extern uint8_t mount_cb_count;
extern uint8_t umount_cb_count;

void tuh_mount_cb(uint8_t daddr) {
  (void) daddr;
  mount_cb_count++;
}

void tuh_umount_cb(uint8_t daddr) {
  (void) daddr;
  umount_cb_count++;
}

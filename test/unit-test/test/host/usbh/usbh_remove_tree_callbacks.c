/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 TinyUSB contributors
 * SPDX-License-Identifier: MIT
 */

#include <stdint.h>

// Kept in a separate translation unit to override usbh.c's weak callback, which test_usbh_remove_tree.c includes.
void remove_tree_umount_observer(uint8_t daddr);

void tuh_umount_cb(uint8_t daddr) {
  remove_tree_umount_observer(daddr);
}

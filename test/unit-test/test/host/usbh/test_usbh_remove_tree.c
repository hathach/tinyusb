/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 TinyUSB contributors
 * SPDX-License-Identifier: MIT
 */

#include "unity.h"

// Include the production implementation to build a device tree and drive remove_device_tree() directly.
#include "host/usbh.c"

TEST_SOURCE_FILE("tusb.c")
TEST_SOURCE_FILE("tusb_fifo.c")
TEST_SOURCE_FILE("hub.c")
TEST_SOURCE_FILE("usbh_remove_tree_callbacks.c")
TEST_SOURCE_FILE("usbh_test_hcd_stubs.c")

enum {
  RHPORT = 0,
  DEV_A  = 1,
  DEV_B  = 2,
  DEV_D  = 3,
  HUB_1  = CFG_TUH_DEVICE_MAX + 1,
  HUB_2  = CFG_TUH_DEVICE_MAX + 2,
};

TU_VERIFY_STATIC(CFG_TUH_DEVICE_MAX == 3 && CFG_TUH_HUB == 2, "tree layout needs 3 devices and 2 hubs");

static uint8_t umount_count;
static uint8_t first_umount_daddr;
static bool    connected_at_first_umount[TOTAL_DEVICES + 1];

// tuh_umount_cb() is where an application may drive tuh_task(): snapshot what it would see there
void remove_tree_umount_observer(uint8_t daddr) {
  if (umount_count == 0) {
    first_umount_daddr = daddr;
    for (uint8_t addr = 1; addr <= TOTAL_DEVICES; addr++) {
      connected_at_first_umount[addr] = tuh_connected(addr);
    }
  }
  umount_count++;
}

//--------------------------------------------------------------------+
// HCD stubs
//--------------------------------------------------------------------+
bool hcd_setup_send(uint8_t rhport, uint8_t daddr, const uint8_t setup_packet[8]) {
  (void) rhport;
  (void) daddr;
  (void) setup_packet;
  return true;
}

//--------------------------------------------------------------------+
// Helpers
//--------------------------------------------------------------------+
static void attach(uint8_t daddr, uint8_t hub_addr, uint8_t hub_port) {
  usbh_device_t *dev     = get_device(daddr);
  dev->bus_info.rhport   = RHPORT;
  dev->bus_info.hub_addr = hub_addr;
  dev->bus_info.hub_port = hub_port;
  dev->connected         = 1;
  dev->addressed         = 1;
  dev->configured        = 1;
  dev->mounted           = 1;
}

static void assert_removed(uint8_t daddr) {
  const usbh_device_t *dev = get_device(daddr);
  TEST_ASSERT_FALSE(dev->connected);
  TEST_ASSERT_FALSE(dev->removing);
}

//--------------------------------------------------------------------+
// Setup
//--------------------------------------------------------------------+
void setUp(void) {
  tu_memclr(&_usbh_data, sizeof(_usbh_data));
  for (uint8_t i = 0; i < TOTAL_DEVICES; i++) {
    clear_device(&_usbh_devices[i]);
  }
  _app_driver       = NULL;
  _app_driver_count = 0;
  _usbh_data.enumerating_daddr = TUSB_INDEX_INVALID_8;

  umount_count       = 0;
  first_umount_daddr = 0;
  tu_memclr(connected_at_first_umount, sizeof(connected_at_first_umount));
}

void tearDown(void) {
}

//--------------------------------------------------------------------+
// Tests
//--------------------------------------------------------------------+
// unplugging a root hub: B is torn down after A, but must already read unplugged in A's unmount callback
void test_remove_root_hub_marks_siblings_before_umount_cb(void) {
  attach(HUB_1, 0, 0);
  attach(DEV_A, HUB_1, 1);
  attach(DEV_B, HUB_1, 2);

  remove_device_tree(RHPORT, 0, 0);

  TEST_ASSERT_EQUAL(2, umount_count);
  TEST_ASSERT_EQUAL(DEV_A, first_umount_daddr);
  TEST_ASSERT_FALSE(connected_at_first_umount[DEV_B]);
  TEST_ASSERT_FALSE(connected_at_first_umount[HUB_1]);

  assert_removed(DEV_A);
  assert_removed(DEV_B);
  assert_removed(HUB_1);
}

// unplugging a downstream hub: the devices behind it are reached on a later pass, so they must be marked up front,
// while the rest of the tree stays connected
void test_remove_nested_hub_marks_subtree_only(void) {
  attach(HUB_1, 0, 0);
  attach(HUB_2, HUB_1, 1);
  attach(DEV_D, HUB_1, 2);
  attach(DEV_A, HUB_2, 1);
  attach(DEV_B, HUB_2, 2);

  remove_device_tree(RHPORT, HUB_1, 1);

  TEST_ASSERT_EQUAL(2, umount_count);
  TEST_ASSERT_EQUAL(DEV_A, first_umount_daddr);
  TEST_ASSERT_FALSE(connected_at_first_umount[DEV_B]);
  TEST_ASSERT_FALSE(connected_at_first_umount[HUB_2]);
  TEST_ASSERT_TRUE(connected_at_first_umount[DEV_D]);
  TEST_ASSERT_TRUE(connected_at_first_umount[HUB_1]);

  assert_removed(DEV_A);
  assert_removed(DEV_B);
  assert_removed(HUB_2);
  TEST_ASSERT_TRUE(tuh_connected(DEV_D));
  TEST_ASSERT_TRUE(tuh_connected(HUB_1));
}

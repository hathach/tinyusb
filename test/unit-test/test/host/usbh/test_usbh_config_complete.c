/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 TinyUSB contributors
 * SPDX-License-Identifier: MIT
 */

#include "unity.h"

// Include the production implementation to set enumeration state directly.
#include "host/usbh.c"

TEST_SOURCE_FILE("tusb.c")
TEST_SOURCE_FILE("tusb_fifo.c")
TEST_SOURCE_FILE("usbh_config_complete_callbacks.c")

enum {
  RHPORT     = 0,
  DADDR      = 1,
  OTHER_ADDR = 2,
  ITF_COUNT  = 2,
};

uint8_t mount_cb_count;
uint8_t umount_cb_count;

static uint8_t set_config_count;
static uint8_t close_count;
static void (*set_config_hook)(void);

//--------------------------------------------------------------------+
// HCD stubs
//--------------------------------------------------------------------+
uint32_t tusb_time_millis_api(void) {
  return 0;
}

bool hcd_init(uint8_t rhport, const tusb_rhport_init_t *rh_init) {
  (void) rhport;
  (void) rh_init;
  return true;
}

void hcd_int_handler(uint8_t rhport, bool in_isr) {
  (void) rhport;
  (void) in_isr;
}

void hcd_int_enable(uint8_t rhport) {
  (void) rhport;
}

void hcd_int_disable(uint8_t rhport) {
  (void) rhport;
}

bool hcd_port_connect_status(uint8_t rhport) {
  (void) rhport;
  return true;
}

void hcd_port_reset(uint8_t rhport) {
  (void) rhport;
}

void hcd_port_reset_end(uint8_t rhport) {
  (void) rhport;
}

tusb_speed_t hcd_port_speed_get(uint8_t rhport) {
  (void) rhport;
  return TUSB_SPEED_FULL;
}

void hcd_device_close(uint8_t rhport, uint8_t daddr) {
  (void) rhport;
  (void) daddr;
}

bool hcd_setup_send(uint8_t rhport, uint8_t daddr, const uint8_t setup_packet[8]) {
  (void) rhport;
  (void) daddr;
  (void) setup_packet;
  return true;
}

bool hcd_edpt_open(uint8_t rhport, uint8_t daddr, const tusb_desc_endpoint_t *ep_desc) {
  (void) rhport;
  (void) daddr;
  (void) ep_desc;
  return true;
}

bool hcd_edpt_close(uint8_t rhport, uint8_t daddr, uint8_t ep_addr) {
  (void) rhport;
  (void) daddr;
  (void) ep_addr;
  return true;
}

bool hcd_edpt_xfer(uint8_t rhport, uint8_t daddr, uint8_t ep_addr, uint8_t *buffer, uint16_t buflen) {
  (void) rhport;
  (void) daddr;
  (void) ep_addr;
  (void) buffer;
  (void) buflen;
  return true;
}

bool hcd_edpt_abort_xfer(uint8_t rhport, uint8_t daddr, uint8_t ep_addr) {
  (void) rhport;
  (void) daddr;
  (void) ep_addr;
  return true;
}

//--------------------------------------------------------------------+
// Fake class driver: its set_config runs the hook, standing in for the application's mount callback, before it
// reports completion as the class drivers do
//--------------------------------------------------------------------+
static bool fake_set_config(uint8_t dev_addr, uint8_t itf_num) {
  set_config_count++;
  if (set_config_hook != NULL) {
    set_config_hook();
    set_config_hook = NULL;
  }
  usbh_driver_set_config_complete(dev_addr, itf_num);
  return true;
}

static void fake_close(uint8_t dev_addr) {
  (void) dev_addr;
  close_count++;
}

static const usbh_class_driver_t fake_driver = {
  .name       = "FAKE",
  .set_config = fake_set_config,
  .close      = fake_close,
};

//--------------------------------------------------------------------+
// Hooks
//--------------------------------------------------------------------+
static void remove_device(void) {
  remove_device_tree(RHPORT, 0, 0);
}

// a second device attached meanwhile: it waits at address 0 for its debounce delay
static void remove_device_and_attach_another(void) {
  remove_device();
  _usbh_data.enumerating_daddr = 0;
  _usbh_data.root_retry_count  = 2;
  usbh_defer_func_ms_async(ENUM_DEBOUNCING_DELAY_MS, enum_delay_async, ENUM_AFTER_DEBOUNCING_DELAY);
}

//--------------------------------------------------------------------+
// Setup
//--------------------------------------------------------------------+
void setUp(void) {
  tu_memclr(&_usbh_data, sizeof(_usbh_data));
  for (uint8_t i = 0; i < TOTAL_DEVICES; i++) {
    clear_device(&_usbh_devices[i]);
  }
  _app_driver                  = &fake_driver;
  _app_driver_count            = 1;

  mount_cb_count   = 0;
  umount_cb_count  = 0;
  set_config_count = 0;
  close_count      = 0;
  set_config_hook  = NULL;

  // DADDR is being enumerated and has reached the SET_CONFIGURATION stage
  usbh_device_t *dev    = get_device(DADDR);
  dev->bus_info.rhport  = RHPORT;
  dev->connected        = 1;
  dev->addressed        = 1;
  dev->configured       = 1;
  for (uint8_t i = 0; i < ITF_COUNT; i++) {
    dev->itf2drv[i] = 0;
  }
  _usbh_data.enumerating_daddr = DADDR;
}

void tearDown(void) {
}

//--------------------------------------------------------------------+
// Tests
//--------------------------------------------------------------------+
void test_config_complete_mounts_once(void) {
  usbh_driver_set_config_complete(DADDR, TUSB_INDEX_INVALID_8);

  TEST_ASSERT_EQUAL(ITF_COUNT, set_config_count);
  TEST_ASSERT_EQUAL(1, mount_cb_count);
  TEST_ASSERT_TRUE(get_device(DADDR)->mounted);
  TEST_ASSERT_EQUAL(TUSB_INDEX_INVALID_8, _usbh_data.enumerating_daddr);
}

void test_removed_during_mount_callback_not_mounted(void) {
  set_config_hook = remove_device;

  usbh_driver_set_config_complete(DADDR, TUSB_INDEX_INVALID_8);

  TEST_ASSERT_EQUAL(1, set_config_count);
  TEST_ASSERT_EQUAL(1, close_count);
  TEST_ASSERT_EQUAL(0, mount_cb_count);
  TEST_ASSERT_EQUAL(0, umount_cb_count);
  TEST_ASSERT_FALSE(get_device(DADDR)->mounted);
}

// The second enumeration's state is installed by the hook, not reached through the deferred attach queue.
void test_removed_during_mount_callback_keeps_next_enumeration(void) {
  set_config_hook = remove_device_and_attach_another;

  usbh_driver_set_config_complete(DADDR, TUSB_INDEX_INVALID_8);

  TEST_ASSERT_EQUAL(0, mount_cb_count);
  TEST_ASSERT_EQUAL(0, _usbh_data.enumerating_daddr);
  TEST_ASSERT_EQUAL_PTR(enum_delay_async, _usbh_data.call_after.func);
  TEST_ASSERT_EQUAL(2, _usbh_data.root_retry_count);
}

void test_config_complete_ignored_when_not_enumerating(void) {
  _usbh_data.enumerating_daddr = OTHER_ADDR;

  usbh_driver_set_config_complete(DADDR, 0);

  TEST_ASSERT_EQUAL(0, set_config_count);
  TEST_ASSERT_EQUAL(0, mount_cb_count);
  TEST_ASSERT_EQUAL(OTHER_ADDR, _usbh_data.enumerating_daddr);
}

// a replacement given the same address that has not reached SET_CONFIGURATION yet
void test_config_complete_ignored_when_not_configured(void) {
  get_device(DADDR)->configured = 0;

  usbh_driver_set_config_complete(DADDR, 0);

  TEST_ASSERT_EQUAL(0, set_config_count);
  TEST_ASSERT_EQUAL(0, mount_cb_count);
  TEST_ASSERT_EQUAL(DADDR, _usbh_data.enumerating_daddr);
}

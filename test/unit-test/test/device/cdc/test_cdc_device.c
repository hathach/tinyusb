/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 Ha Thach (tinyusb.org)
 * SPDX-License-Identifier: MIT
 *
 * This file is part of the TinyUSB stack.
 */

// cdcd_open() must never read a descriptor's bDescriptorType past desc_end. A final 1-byte descriptor
// passes tu_desc_in_bounds(); the bytes after desc_end form the descriptor it would be mistaken for, so a
// read past the end changes the result instead of passing silently.

#include "unity.h"

#include "osal/osal.h"
#include "tusb_fifo.h"
#include "tusb.h"
#include "usbd.h"
TEST_SOURCE_FILE("cdc_device.c")

#include "mock_dcd.h"

#if !TUD_OPT_SUPER_SPEED
#error "test_cdc_device needs a SuperSpeed build (no TUD_TEST_NO_SUPER_SPEED)"
#endif

enum { RHPORT = 0, EP_NOTIF = 0x81, EP_OUT = 0x02, EP_IN = 0x82 };

#define CTRL_ITF_DESC \
  9, TUSB_DESC_INTERFACE, 0, 0, 1, TUSB_CLASS_CDC, CDC_COMM_SUBCLASS_ABSTRACT_CONTROL_MODEL, CDC_COMM_PROTOCOL_NONE, 0
#define DATA_ITF_DESC(_ep_count) 9, TUSB_DESC_INTERFACE, 1, 0, _ep_count, TUSB_CLASS_CDC_DATA, 0, 0, 0

uint32_t tusb_time_millis_api(void) { return 0; }
const uint8_t* tud_descriptor_device_cb(void) { return NULL; }
const uint8_t* tud_descriptor_configuration_cb(uint8_t index) { (void) index; return NULL; }
const uint16_t* tud_descriptor_string_cb(uint8_t index, uint16_t langid) { (void) index; (void) langid; return NULL; }

static uint16_t open_itf(const uint8_t* desc, uint16_t len) {
  return cdcd_open(RHPORT, (const tusb_desc_interface_t*) desc, len);
}

void setUp(void) {
  dcd_int_disable_Ignore();
  dcd_int_enable_Ignore();
  if (!tud_inited()) {
    tusb_rhport_init_t dev_init = {.role = TUSB_ROLE_DEVICE, .speed = TUSB_SPEED_AUTO};
    dcd_init_ExpectAndReturn(RHPORT, &dev_init, true);
    tusb_init(RHPORT, &dev_init);
  }
  cdcd_init();
}

void tearDown(void) {}

void test_open_full_interface(void) {
  const uint8_t desc[] = { TUD_CDC_DESCRIPTOR(0, 0, EP_NOTIF, 8, EP_OUT, EP_IN, 64) };
  const uint8_t* itf = desc + 8; // after the IAD
  const uint16_t len = (uint16_t) (sizeof(desc) - 8);

  dcd_edpt_open_ExpectAndReturn(RHPORT, (const tusb_desc_endpoint_t*) (itf + 28), true);
  dcd_edpt_open_ExpectAndReturn(RHPORT, (const tusb_desc_endpoint_t*) (itf + 44), true);
  dcd_edpt_open_ExpectAndReturn(RHPORT, (const tusb_desc_endpoint_t*) (itf + 51), true);
  dcd_edpt_xfer_IgnoreAndReturn(true);
  dcd_edpt_xfer_fifo_IgnoreAndReturn(true);
  TEST_ASSERT_EQUAL_UINT16(len, open_itf(itf, len));
}

void test_open_ignores_one_byte_trailer_before_notification(void) {
  const uint8_t buf[] = { CTRL_ITF_DESC, 1, TUSB_DESC_ENDPOINT, 0x83, TUSB_XFER_INTERRUPT, 8, 0, 16 };
  TEST_ASSERT_EQUAL_UINT16(9, open_itf(buf, 10));
}

void test_open_ignores_one_byte_trailer_before_data_interface(void) {
  const uint8_t buf[] = { CTRL_ITF_DESC, 1, TUSB_DESC_INTERFACE, 1, 0, 0, TUSB_CLASS_CDC_DATA, 0, 0, 0 };
  TEST_ASSERT_EQUAL_UINT16(9, open_itf(buf, 10));
}

void test_open_rejects_one_byte_bulk_endpoint(void) {
  const uint8_t buf[] = { CTRL_ITF_DESC, DATA_ITF_DESC(1), 1, TUSB_DESC_ENDPOINT, EP_IN, TUSB_XFER_BULK, 64, 0, 0 };
  TEST_ASSERT_EQUAL_UINT16(0, open_itf(buf, 19));
}

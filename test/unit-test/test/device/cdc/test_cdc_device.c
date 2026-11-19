/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 TinyUSB contributors
 * SPDX-License-Identifier: MIT
 */

#include "unity.h"

#include "tusb_option.h"
#include "osal/osal.h"
#include "tusb_fifo.h"
#include "tusb.h"
#include "device/usbd.h"
#include "device/usbd_pvt.h"
TEST_SOURCE_FILE("usbd.c")
TEST_SOURCE_FILE("usbd_control.c")
TEST_SOURCE_FILE("cdc_device.c")

#include "mock_dcd.h"

uint32_t tusb_time_millis_api(void) { return 0; }
const uint8_t* tud_descriptor_device_cb(void) { return NULL; }
const uint8_t* tud_descriptor_configuration_cb(uint8_t index) { (void) index; return NULL; }
const uint16_t* tud_descriptor_string_cb(uint8_t index, uint16_t langid) { (void) index; (void) langid; return NULL; }

// CDC ACM communication interface without endpoints, as the start of a descriptor whose walk ends after it
#define CDC_COMM_ITF \
  9, TUSB_DESC_INTERFACE, 0, 0, 0, TUSB_CLASS_CDC, CDC_COMM_SUBCLASS_ABSTRACT_CONTROL_MODEL, CDC_COMM_PROTOCOL_NONE, 0

void setUp(void) {}
void tearDown(void) {}

//--------------------------------------------------------------------+
// Class-specific descriptor skip loop (#4387)
// The byte past max_len is a CS_INTERFACE guard: an ENDPOINT or INTERFACE guard would stop the loop and hide the
// over-read.
//--------------------------------------------------------------------+

// A trailing bLength 1 descriptor is not a class-specific descriptor to skip: its bDescriptorType would be the guard
// byte past max_len. The interface ends before it.
void test_cdcd_open_trailing_blength1(void) {
  const uint8_t desc[] = { CDC_COMM_ITF, 1, TUSB_DESC_CS_INTERFACE, TUSB_DESC_CS_INTERFACE };
  TEST_ASSERT_EQUAL_UINT16(9, cdcd_open(0, (const tusb_desc_interface_t*) desc, 10));
}

// A bLength 0 CS_INTERFACE descriptor would make the skip loop spin in place
void test_cdcd_open_blength0_cs_interface(void) {
  const uint8_t desc[] = { CDC_COMM_ITF, 0, TUSB_DESC_CS_INTERFACE, TUSB_DESC_CS_INTERFACE };
  TEST_ASSERT_EQUAL_UINT16(9, cdcd_open(0, (const tusb_desc_interface_t*) desc, 11));
}

// The interface ends exactly at max_len: nothing past it is read as an endpoint or data interface
void test_cdcd_open_ends_at_max_len(void) {
  const uint8_t desc[] = { CDC_COMM_ITF, 2, TUSB_DESC_ENDPOINT };
  TEST_ASSERT_EQUAL_UINT16(9, cdcd_open(0, (const tusb_desc_interface_t*) desc, 9));
}

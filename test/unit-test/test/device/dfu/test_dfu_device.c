/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 TinyUSB contributors
 * SPDX-License-Identifier: MIT
 */

#include <string.h>
#include "unity.h"

#include "osal/osal.h"
#include "tusb_fifo.h"
#include "tusb.h"
#include "usbd.h"
TEST_SOURCE_FILE("dfu_device.c")

#include "mock_dcd.h"

//--------------------------------------------------------------------+
// Fixture: one DFU mode interface with two alternate settings, as in examples/device/dfu
//--------------------------------------------------------------------+
enum {
  RHPORT      = 0,
  ITF_NUM_DFU = 0,
};

#define ALT_COUNT  2
#define FUNC_ATTRS (DFU_ATTR_CAN_UPLOAD | DFU_ATTR_CAN_DOWNLOAD | DFU_ATTR_MANIFESTATION_TOLERANT)

static const uint8_t desc_configuration[] = {
  TUD_CONFIG_DESCRIPTOR(1, 1, 0, TUD_CONFIG_DESC_LEN + TUD_DFU_DESC_LEN(ALT_COUNT), 0x00, 100),
  TUD_DFU_DESCRIPTOR(ITF_NUM_DFU, ALT_COUNT, 4, FUNC_ATTRS, 1000, CFG_TUD_DFU_XFER_BUFSIZE),
};

// The most alternate settings an interface can have: bAlternateSetting 0..255, built at run time
#define ALT_COUNT_MAX 256
static uint8_t desc_configuration_max[TUD_CONFIG_DESC_LEN + TUD_DFU_DESC_LEN(ALT_COUNT_MAX)];

static const uint8_t *desc_config = desc_configuration;

uint32_t tusb_time_millis_api(void) {
  return 0;
}

const uint8_t *tud_descriptor_device_cb(void) {
  return NULL;
}

const uint8_t *tud_descriptor_configuration_cb(uint8_t index) {
  (void)index;
  return desc_config;
}

const uint16_t *tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
  (void)index;
  (void)langid;
  return NULL;
}

//--------------------------------------------------------------------+
// DFU callbacks
//--------------------------------------------------------------------+
static int upload_alt; // alt passed to the last tud_dfu_upload_cb(), -1 if not invoked

uint32_t tud_dfu_get_timeout_cb(uint8_t alt, uint8_t state) {
  (void)alt;
  (void)state;
  return 0;
}

void tud_dfu_download_cb(uint8_t alt, uint16_t block_num, const uint8_t *data, uint16_t length) {
  (void)alt;
  (void)block_num;
  (void)data;
  (void)length;
  tud_dfu_finish_flashing(DFU_STATUS_OK);
}

void tud_dfu_manifest_cb(uint8_t alt) {
  (void)alt;
  tud_dfu_finish_flashing(DFU_STATUS_OK);
}

uint16_t tud_dfu_upload_cb(uint8_t alt, uint16_t block_num, uint8_t *data, uint16_t length) {
  (void)block_num;
  (void)length;
  upload_alt = alt;
  data[0]    = alt;
  return 1;
}

//--------------------------------------------------------------------+
// EP0: the last transfer queued by the stack, and whether it stalled
//--------------------------------------------------------------------+
static bool     ep0_pending;
static bool     ep0_stalled;
static uint8_t  ep0_addr;
static uint16_t ep0_len;

static bool stub_edpt_xfer(uint8_t rhport, uint8_t ep_addr, uint8_t *buffer, uint16_t total_bytes, bool is_isr, int n) {
  (void)rhport;
  (void)buffer;
  (void)is_isr;
  (void)n;
  ep0_pending = true;
  ep0_addr    = ep_addr;
  ep0_len     = total_bytes;
  return true;
}

static void stub_edpt_stall(uint8_t rhport, uint8_t ep_addr, int n) {
  (void)rhport;
  (void)n;
  if (tu_edpt_number(ep_addr) == 0) {
    ep0_stalled = true;
  }
}

// Drive a control request through its data and status stages; false if the device stalled it
static bool host_control(const tusb_control_request_t *req) {
  ep0_pending = false;
  ep0_stalled = false;
  dcd_event_setup_received(RHPORT, (const uint8_t *)req, false);
  tud_task();
  while (ep0_pending && !ep0_stalled) {
    ep0_pending = false;
    dcd_event_xfer_complete(RHPORT, ep0_addr, ep0_len, XFER_RESULT_SUCCESS, false);
    tud_task();
  }
  return !ep0_stalled;
}

static bool set_interface(uint16_t alt) {
  const tusb_control_request_t req = {.bmRequestType = 0x01,
                                      .bRequest      = TUSB_REQ_SET_INTERFACE,
                                      .wValue        = alt,
                                      .wIndex        = ITF_NUM_DFU};
  return host_control(&req);
}

static bool dfu_upload(void) {
  const tusb_control_request_t req = {.bmRequestType = 0xA1,
                                      .bRequest      = DFU_REQUEST_UPLOAD,
                                      .wIndex        = ITF_NUM_DFU,
                                      .wLength       = CFG_TUD_DFU_XFER_BUFSIZE};
  return host_control(&req);
}

// Bus reset then SET_CONFIGURATION(1) with the given configuration descriptor
static void configure(const uint8_t *desc) {
  desc_config = desc;
  dcd_event_bus_reset(RHPORT, TUSB_SPEED_HIGH, false);
  tud_task();

  const tusb_control_request_t set_config = {.bmRequestType = 0x00,
                                             .bRequest      = TUSB_REQ_SET_CONFIGURATION,
                                             .wValue        = 1};
  TEST_ASSERT_TRUE(host_control(&set_config));
  upload_alt = -1;
}

void setUp(void) {
  dcd_int_disable_Ignore();
  dcd_int_enable_Ignore();
  dcd_edpt_close_all_Ignore();
  dcd_edpt0_status_complete_Ignore();
  dcd_edpt0_setup_begin_IgnoreAndReturn(true);
  dcd_edpt_xfer_Stub(stub_edpt_xfer);
  dcd_edpt_stall_Stub(stub_edpt_stall);

  if (!tud_inited()) {
    tusb_rhport_init_t dev_init = {.role = TUSB_ROLE_DEVICE, .speed = TUSB_SPEED_AUTO};
    dcd_init_ExpectAndReturn(RHPORT, &dev_init, true);
    tusb_init(RHPORT, &dev_init);
  }

  configure(desc_configuration);
}

void tearDown(void) {
}

//--------------------------------------------------------------------+
// Tests
//--------------------------------------------------------------------+
void test_set_interface_selects_alt(void) {
  TEST_ASSERT_TRUE(set_interface(1));
  TEST_ASSERT_TRUE(dfu_upload());
  TEST_ASSERT_EQUAL(1, upload_alt);
}

// alt is the application's partition index: one past the descriptor's alternate settings must
// stall (USB 2.0 9.4.10) and leave the selected alt unchanged
void test_set_interface_nonexistent_alt_stalls(void) {
  TEST_ASSERT_TRUE(set_interface(1));
  TEST_ASSERT_FALSE(set_interface(ALT_COUNT));
  TEST_ASSERT_FALSE(set_interface(0xFF));
  TEST_ASSERT_FALSE(set_interface(0x0101)); // high byte must not be dropped

  TEST_ASSERT_TRUE(dfu_upload());
  TEST_ASSERT_EQUAL(1, upload_alt);
}

// 256 alternate settings: the last one (255) is selectable, one past it (low byte 1) stalls
void test_set_interface_max_alt_count(void) {
  const uint8_t config_head[] = {
    TUD_CONFIG_DESCRIPTOR(1, 1, 0, sizeof(desc_configuration_max), 0x00, 100),
  };
  const uint8_t func_desc[] = {
    9, DFU_DESC_FUNCTIONAL, FUNC_ATTRS, U16_TO_U8S_LE(1000), U16_TO_U8S_LE(CFG_TUD_DFU_XFER_BUFSIZE), U16_TO_U8S_LE(0x0110),
  };
  uint8_t *p = desc_configuration_max;
  memcpy(p, config_head, sizeof(config_head));
  p += sizeof(config_head);
  for (unsigned alt = 0; alt < ALT_COUNT_MAX; alt++) {
    const uint8_t alt_desc[] = {TUD_DFU_ALT(ITF_NUM_DFU, (uint8_t)alt, 0)};
    memcpy(p, alt_desc, sizeof(alt_desc));
    p += sizeof(alt_desc);
  }
  memcpy(p, func_desc, sizeof(func_desc));

  configure(desc_configuration_max);

  TEST_ASSERT_TRUE(set_interface(255));
  TEST_ASSERT_FALSE(set_interface(257));

  TEST_ASSERT_TRUE(dfu_upload());
  TEST_ASSERT_EQUAL(255, upload_alt);
}

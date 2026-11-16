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
#include "usbd_ep0_test.h"

//--------------------------------------------------------------------+
// Fixture: one DFU mode interface with two alternate settings, as in examples/device/dfu
//--------------------------------------------------------------------+
enum {
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

static tusb_control_request_t set_interface_req(uint16_t alt) {
  const tusb_control_request_t req = {.bmRequestType = 0x01,
                                      .bRequest      = TUSB_REQ_SET_INTERFACE,
                                      .wValue        = alt,
                                      .wIndex        = ITF_NUM_DFU};
  return req;
}

static void set_interface(uint16_t alt) {
  const tusb_control_request_t req = set_interface_req(alt);
  host_no_data(&req);
}

static void set_interface_stalled(uint16_t alt) {
  const tusb_control_request_t req = set_interface_req(alt);
  host_setup_stalled(&req);
}

// UPLOAD of one block: tud_dfu_upload_cb() returns its alt as the single data byte
static void dfu_upload(uint8_t expected_alt) {
  const tusb_control_request_t req = {.bmRequestType = 0xA1,
                                      .bRequest      = DFU_REQUEST_UPLOAD,
                                      .wIndex        = ITF_NUM_DFU,
                                      .wLength       = CFG_TUD_DFU_XFER_BUFSIZE};
  host_setup_ok(&req);
  const ep0_xfer_t *x = host_complete_ep0(EP_CTRL_IN, 1);
  TEST_ASSERT_EQUAL_HEX8(expected_alt, x->data[0]);
  host_complete_ep0(EP_CTRL_OUT, 0);
  TEST_ASSERT_EQUAL(expected_alt, upload_alt);
}

// Bus reset then SET_CONFIGURATION(1) with the given configuration descriptor
static void configure(const uint8_t *desc) {
  desc_config = desc;
  host_bus_reset();
  host_set_config(1);
  upload_alt = -1;
}

void setUp(void) {
  ep0_test_init();
  configure(desc_configuration);
}

void tearDown(void) {
}

//--------------------------------------------------------------------+
// Tests
//--------------------------------------------------------------------+
void test_set_interface_selects_alt(void) {
  set_interface(1);
  dfu_upload(1);
}

// alt is the application's partition index: one past the descriptor's alternate settings must
// stall (USB 2.0 9.4.10) and leave the selected alt unchanged
void test_set_interface_nonexistent_alt_stalls(void) {
  set_interface(1);
  set_interface_stalled(ALT_COUNT);
  set_interface_stalled(0xFF);
  set_interface_stalled(0x0101); // high byte must not be dropped

  dfu_upload(1);
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

  set_interface(255);
  set_interface_stalled(257);

  dfu_upload(255);
}

//--------------------------------------------------------------------+
// Malformed descriptors: dfu_moded_open() reads nothing past max_len and rejects the interface
//--------------------------------------------------------------------+
#define DFU_ITF_LEN (TUD_DFU_DESC_LEN(ALT_COUNT))

static uint8_t desc_dfu[DFU_ITF_LEN];

static uint16_t open_dfu(uint16_t max_len) {
  return dfu_moded_open(RHPORT, (const tusb_desc_interface_t *) desc_dfu, max_len);
}

static void copy_dfu_itf(void) {
  memcpy(desc_dfu, desc_configuration + TUD_CONFIG_DESC_LEN, DFU_ITF_LEN);
}

void test_open_accepts_whole_interface(void) {
  copy_dfu_itf();
  TEST_ASSERT_EQUAL(DFU_ITF_LEN, open_dfu(DFU_ITF_LEN));
}

void test_open_rejects_truncated_descriptors(void) {
  copy_dfu_itf();
  TEST_ASSERT_EQUAL(0, open_dfu(DFU_ITF_LEN - 1));              // functional descriptor cut short
  TEST_ASSERT_EQUAL(0, open_dfu(2 * sizeof(tusb_desc_interface_t))); // functional descriptor missing
  TEST_ASSERT_EQUAL(0, open_dfu(sizeof(tusb_desc_interface_t) - 1)); // interface cut short
}

// The second alternate is not an interface descriptor, though its bytes would match a DFU alternate
void test_open_rejects_wrong_type_alternate(void) {
  copy_dfu_itf();
  desc_dfu[sizeof(tusb_desc_interface_t) + 1] = TUSB_DESC_CS_INTERFACE;
  TEST_ASSERT_EQUAL(0, open_dfu(DFU_ITF_LEN));
}

void test_open_rejects_short_functional_descriptor(void) {
  copy_dfu_itf();
  desc_dfu[2 * sizeof(tusb_desc_interface_t)] = sizeof(tusb_desc_dfu_functional_t) - 1;
  TEST_ASSERT_EQUAL(0, open_dfu(DFU_ITF_LEN));
}

// A zero bLength would never move the walk forward
void test_open_rejects_zero_length_descriptors(void) {
  copy_dfu_itf();
  desc_dfu[sizeof(tusb_desc_interface_t)] = 0; // second alternate
  TEST_ASSERT_EQUAL(0, open_dfu(DFU_ITF_LEN));

  copy_dfu_itf();
  desc_dfu[0] = 0; // first interface
  TEST_ASSERT_EQUAL(0, open_dfu(DFU_ITF_LEN));
}

// GETSTATUS is device-to-host (DFU 1.1 Table 3.1) and its reply is a stack local: an OUT one must stall rather than
// arm a data stage that writes host data into it after the handler returned
void test_getstatus_out_stalls(void) {
  tusb_control_request_t req = {.bmRequestType = 0x21,
                                .bRequest      = DFU_REQUEST_GETSTATUS,
                                .wIndex        = ITF_NUM_DFU,
                                .wLength       = sizeof(dfu_status_response_t)};
  host_setup_stalled(&req);

  req.bmRequestType = 0xA1;
  host_setup_ok(&req);
  host_complete_ep0(EP_CTRL_IN, sizeof(dfu_status_response_t));
  host_complete_ep0(EP_CTRL_OUT, 0);
}

// A tail too short to hold bDescriptorType after an interface: nothing past max_len is read
static uint8_t desc_dfu_tail[sizeof(tusb_desc_interface_t) + 1];

void test_open_rejects_one_byte_tail(void) {
  for (uint8_t tail_len = 0; tail_len < 2; tail_len++) {
    memcpy(desc_dfu_tail, desc_configuration + TUD_CONFIG_DESC_LEN, sizeof(tusb_desc_interface_t));
    desc_dfu_tail[sizeof(tusb_desc_interface_t)] = tail_len;
    TEST_ASSERT_EQUAL(0, dfu_moded_open(RHPORT, (const tusb_desc_interface_t *) desc_dfu_tail, sizeof(desc_dfu_tail)));
  }
}

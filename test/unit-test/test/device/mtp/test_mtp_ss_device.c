/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 Ha Thach (tinyusb.org)
 * SPDX-License-Identifier: MIT
 *
 * This file is part of the TinyUSB stack.
 */

// MTP over SuperSpeed: the interface opens with endpoint companion descriptors and a data IN
// phase ends with a ZLP on whole 1024-byte packets only. test_mtp_device.c covers FS/HS.

#include <string.h>
#include "unity.h"

#include "osal/osal.h"
#include "tusb_fifo.h"
#include "tusb.h"
#include "usbd.h"
#include "device/usbd_pvt.h"
TEST_SOURCE_FILE("usbd_control.c")
TEST_SOURCE_FILE("mtp_device.c")

#include "mock_dcd.h"

#if !TUD_OPT_SUPER_SPEED || CFG_TUD_MTP_EP_BUFSIZE < 2 * TUSB_EPSIZE_BULK_SS
#error "test_mtp_ss_device needs the SuperSpeed shape and tusb_config_mtp_ss.h (project.yml :defines:)"
#endif

enum {
  EP_CTRL_IN = 0x80,
  EP_EVT_IN  = 0x81,
  EP_OUT     = 0x02,
  EP_IN      = 0x82,
};

enum { RHPORT = 0, BUFSIZE = CFG_TUD_MTP_EP_BUFSIZE, HDR = sizeof(mtp_container_header_t) };

static const uint8_t desc_config_ss[] = {
  TUD_CONFIG_DESCRIPTOR(1, 1, 0, TUD_CONFIG_DESC_LEN + TUD_MTP_SS_DESC_LEN, 0, 100),
  TUD_MTP_SS_DESCRIPTOR(0, 0, EP_EVT_IN, 8, 10, EP_OUT, EP_IN, 0)
};

uint32_t tusb_time_millis_api(void) { return 0; }
const uint8_t* tud_descriptor_device_cb(void) { return NULL; }
const uint8_t* tud_descriptor_configuration_cb(uint8_t index) { (void) index; return desc_config_ss; }
const uint16_t* tud_descriptor_string_cb(uint8_t index, uint16_t langid) { (void) index; (void) langid; return NULL; }

//--------------------------------------------------------------------+
// dcd stubs: transfers outside EP0 are logged; one outstanding transfer per endpoint
//--------------------------------------------------------------------+
typedef struct {
  uint8_t ep;
  uint16_t len;
  uint8_t* buf;
} xfer_call_t;

static xfer_call_t log_calls[16];
static uint32_t log_count, log_cursor;
static uint16_t opened_mps[2][16]; // [dir][epnum]
static bool ep_stalled;

static bool stub_edpt_open(uint8_t rhport, const tusb_desc_endpoint_t* desc, int n) {
  (void) rhport; (void) n;
  opened_mps[tu_edpt_dir(desc->bEndpointAddress)][tu_edpt_number(desc->bEndpointAddress)] = tu_edpt_packet_size(desc);
  return true;
}
static bool stub_edpt_xfer(uint8_t rhport, uint8_t ep, uint8_t* buf, uint16_t len, bool is_isr, int n) {
  (void) rhport; (void) is_isr; (void) n;
  if (tu_edpt_number(ep) != 0) {
    TEST_ASSERT_LESS_THAN(TU_ARRAY_SIZE(log_calls), log_count);
    log_calls[log_count++] = (xfer_call_t){ ep, len, buf };
  }
  return true;
}
static void stub_edpt_stall(uint8_t rhport, uint8_t ep, int n) {
  (void) rhport; (void) ep; (void) n;
  ep_stalled = true;
}

static const xfer_call_t* expect_xfer(uint8_t ep, uint16_t len) {
  TEST_ASSERT_MESSAGE(log_cursor < log_count, "expected a transfer, got none");
  const xfer_call_t* c = &log_calls[log_cursor++];
  TEST_ASSERT_EQUAL_HEX8_MESSAGE(ep, c->ep, "transfer endpoint");
  TEST_ASSERT_EQUAL_MESSAGE(len, c->len, "transfer length");
  return c;
}
static void expect_no_more_xfers(void) {
  TEST_ASSERT_EQUAL_MESSAGE(log_count, log_cursor, "unexpected transfer");
  TEST_ASSERT_FALSE_MESSAGE(ep_stalled, "endpoint stalled");
}
static void host_complete(uint8_t ep, uint32_t len) {
  dcd_event_xfer_complete(RHPORT, ep, len, XFER_RESULT_SUCCESS, false);
  tud_task();
}

//--------------------------------------------------------------------+
// Application callbacks: a data IN of app_len payload bytes, answered OK
//--------------------------------------------------------------------+
static uint32_t app_len, complete_calls;

int32_t tud_mtp_command_received_cb(tud_mtp_cb_data_t* cb) {
  mtp_container_info_t* io = &cb->io_container;
  memset(io->payload, 0xA5, app_len);
  io->header->len = HDR + app_len;
  TEST_ASSERT_TRUE(tud_mtp_data_send(io));
  return 0;
}

int32_t tud_mtp_data_complete_cb(tud_mtp_cb_data_t* cb) {
  complete_calls++;
  cb->io_container.header->code = MTP_RESP_OK;
  TEST_ASSERT_TRUE(tud_mtp_response_send(&cb->io_container));
  return 0;
}

//--------------------------------------------------------------------+
// Setup
//--------------------------------------------------------------------+
// SuperSpeed bus reset, then SET_CONFIGURATION through its status stage
static void open_device_ss(void) {
  dcd_event_bus_reset(RHPORT, TUSB_SPEED_SUPER, false);
  tud_task();
  const tusb_control_request_t set_config = { .bmRequestType = 0x00, .bRequest = TUSB_REQ_SET_CONFIGURATION, .wValue = 1 };
  dcd_event_setup_received(RHPORT, (const uint8_t*) &set_config, false);
  tud_task();
  host_complete(EP_CTRL_IN, 0);
  TEST_ASSERT_TRUE(tud_mounted());
}

void setUp(void) {
  dcd_int_disable_Ignore();
  dcd_int_enable_Ignore();
  dcd_edpt_close_all_Ignore();
  dcd_set_address_Ignore();
  dcd_edpt0_status_complete_Ignore();
  dcd_edpt0_setup_begin_IgnoreAndReturn(true);
  dcd_edpt_open_Stub(stub_edpt_open);
  dcd_edpt_xfer_Stub(stub_edpt_xfer);
  dcd_edpt_stall_Stub(stub_edpt_stall);
  log_count = log_cursor = 0;
  memset(opened_mps, 0, sizeof(opened_mps));
  ep_stalled = false;
  complete_calls = 0;

  if (!tud_inited()) {
    tusb_rhport_init_t dev_init = { .role = TUSB_ROLE_DEVICE, .speed = TUSB_SPEED_AUTO };
    dcd_init_ExpectAndReturn(RHPORT, &dev_init, true);
    tusb_init(RHPORT, &dev_init);
  }
}

void tearDown(void) {}

//--------------------------------------------------------------------+
// Tests
//--------------------------------------------------------------------+
void test_open_skips_ep_companions(void) {
  open_device_ss();
  TEST_ASSERT_EQUAL(8, opened_mps[TUSB_DIR_IN][tu_edpt_number(EP_EVT_IN)]);
  TEST_ASSERT_EQUAL(TUSB_EPSIZE_BULK_SS, opened_mps[TUSB_DIR_OUT][tu_edpt_number(EP_OUT)]);
  TEST_ASSERT_EQUAL(TUSB_EPSIZE_BULK_SS, opened_mps[TUSB_DIR_IN][tu_edpt_number(EP_IN)]);
  expect_xfer(EP_OUT, BUFSIZE); // first command read
  expect_no_more_xfers();
}

// one data IN transfer of `container_len` bytes, then the ZLP when `zlp`, then the response
static void check_data_in_ss(uint16_t container_len, bool zlp) {
  open_device_ss();
  const xfer_call_t* cmd_read = expect_xfer(EP_OUT, BUFSIZE);
  app_len = container_len - HDR;
  const mtp_container_command_t cmd = {
    .header = { .len = HDR, .type = MTP_CONTAINER_TYPE_COMMAND_BLOCK, .code = MTP_OP_GET_DEVICE_INFO, .transaction_id = 1 }
  };
  memcpy(cmd_read->buf, &cmd, HDR);
  host_complete(EP_OUT, HDR);

  expect_xfer(EP_IN, container_len);
  expect_no_more_xfers();
  host_complete(EP_IN, container_len);
  if (zlp) {
    expect_xfer(EP_IN, 0);
    expect_no_more_xfers();
    TEST_ASSERT_EQUAL(0, complete_calls);
    host_complete(EP_IN, 0);
  }
  TEST_ASSERT_EQUAL(1, complete_calls);
  expect_xfer(EP_IN, HDR); // response
  expect_no_more_xfers();
  host_complete(EP_IN, HDR);
  expect_xfer(EP_OUT, BUFSIZE); // next command read
  expect_no_more_xfers();
}

void test_data_in_ss_packet_multiple_zlp(void) { check_data_in_ss(2 * TUSB_EPSIZE_BULK_SS, true); }
// a whole number of 512-byte (HS) packets ends short on a 1024-byte link
void test_data_in_ss_hs_packet_multiple_no_zlp(void) { check_data_in_ss(3 * 512, false); }

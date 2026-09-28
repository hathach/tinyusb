/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 TinyUSB contributors
 * SPDX-License-Identifier: MIT
 */

#include "unity.h"
#include "tusb_option.h"
#include "host/usbh.h"
#include "host/usbh_pvt.h"
#include "class/msc/msc_host.h"

TEST_SOURCE_FILE("msc_host.c")

enum {
  DADDR     = 1,
  EP_OUT    = 0x01,
  EP_IN     = 0x81,
  MAX_XFERS = 8,
};

static uint8_t  xfer_ep[MAX_XFERS];
static uint8_t *xfer_buf[MAX_XFERS];
static uint16_t xfer_len[MAX_XFERS];
static uint8_t  xfer_count;
static uint8_t  xfer_fail_index; // xfer_count value whose submission fails
static uint8_t  complete_count;
static uint8_t  set_config_count;
static msc_csw_t complete_csw;
static bool     retry_submitted;
static tuh_xfer_cb_t ctrl_complete_cb;
static uint8_t  enum_buf[64];
static uint8_t  data[98304];

bool tuh_edpt_open(uint8_t daddr, const tusb_desc_endpoint_t *desc_ep) {
  (void) daddr;
  (void) desc_ep;
  return true;
}

bool tuh_control_xfer(tuh_xfer_t *xfer) {
  ctrl_complete_cb = xfer->complete_cb;
  return true;
}

uint8_t *usbh_get_enum_buf(void) {
  return enum_buf;
}

void usbh_driver_set_config_complete(uint8_t dev_addr, uint8_t itf_num) {
  (void) dev_addr;
  (void) itf_num;
  set_config_count++;
}

bool usbh_edpt_claim(uint8_t dev_addr, uint8_t ep_addr) {
  (void) dev_addr;
  (void) ep_addr;
  return true;
}

bool usbh_edpt_release(uint8_t dev_addr, uint8_t ep_addr) {
  (void) dev_addr;
  (void) ep_addr;
  return true;
}

bool usbh_edpt_busy(uint8_t dev_addr, uint8_t ep_addr) {
  (void) dev_addr;
  (void) ep_addr;
  return false;
}

bool usbh_edpt_xfer_with_callback(uint8_t dev_addr, uint8_t ep_addr, uint8_t *buffer, uint16_t total_bytes,
                                  tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  (void) dev_addr;
  (void) complete_cb;
  (void) user_data;
  TEST_ASSERT_LESS_THAN(MAX_XFERS, xfer_count);
  xfer_ep[xfer_count]  = ep_addr;
  xfer_buf[xfer_count] = buffer;
  xfer_len[xfer_count] = total_bytes;
  return xfer_count++ != xfer_fail_index;
}

static bool record_complete(uint8_t daddr, const tuh_msc_complete_data_t *cb_data) {
  (void) daddr;
  complete_count++;
  complete_csw = *cb_data->csw;
  return true;
}

// like the enumeration and msc_file_explorer, issue the next command straight from the callback
static bool retry_complete(uint8_t daddr, const tuh_msc_complete_data_t *cb_data) {
  (void) record_complete(daddr, cb_data);
  retry_submitted = tuh_msc_test_unit_ready(daddr, 0, record_complete, 0);
  return true;
}

static void mount_bot_interface(void) {
  struct TU_ATTR_PACKED {
    tusb_desc_interface_t itf;
    tusb_desc_endpoint_t  ep_out;
    tusb_desc_endpoint_t  ep_in;
  } const desc = {
    .itf    = {sizeof(tusb_desc_interface_t), TUSB_DESC_INTERFACE, 0, 0, 2, TUSB_CLASS_MSC, MSC_SUBCLASS_SCSI,
               MSC_PROTOCOL_BOT, 0},
    .ep_out = {sizeof(tusb_desc_endpoint_t), TUSB_DESC_ENDPOINT, EP_OUT, {.xfer = TUSB_XFER_BULK}, 512, 0},
    .ep_in  = {sizeof(tusb_desc_endpoint_t), TUSB_DESC_ENDPOINT, EP_IN, {.xfer = TUSB_XFER_BULK}, 512, 0},
  };

  TEST_ASSERT_EQUAL(sizeof(desc), msch_open(0, DADDR, &desc.itf, sizeof(desc)));
  TEST_ASSERT_TRUE(msch_set_config(DADDR, 0));
}

static void reply_csw(uint8_t status) {
  const msc_csw_t csw = {.signature = MSC_CSW_SIGNATURE, .status = status};
  memcpy(xfer_buf[xfer_count - 1], &csw, sizeof(csw));
  TEST_ASSERT_TRUE(msch_xfer_cb(DADDR, EP_IN, XFER_RESULT_SUCCESS, sizeof(msc_csw_t)));
}

static void reply_csw_passed(void) {
  reply_csw(MSC_CSW_STATUS_PASSED);
}

// Get Max LUN STALLs (one LUN): Test Unit Ready follows
static void get_max_lun_stalled(void) {
  tuh_xfer_t ctrl = {.daddr = DADDR, .result = XFER_RESULT_STALLED};
  ctrl_complete_cb(&ctrl);
}

// run Get Max LUN, Test Unit Ready and Read Capacity 10 to completion
static void enumerate(void) {
  get_max_lun_stalled();
  TEST_ASSERT_TRUE(msch_xfer_cb(DADDR, EP_OUT, XFER_RESULT_SUCCESS, sizeof(msc_cbw_t)));
  reply_csw_passed();

  TEST_ASSERT_TRUE(msch_xfer_cb(DADDR, EP_OUT, XFER_RESULT_SUCCESS, sizeof(msc_cbw_t)));
  scsi_read_capacity10_resp_t cap;
  cap.last_lba   = tu_htonl(1023);
  cap.block_size = tu_htonl(512);
  memcpy(xfer_buf[xfer_count - 1], &cap, sizeof(cap));
  TEST_ASSERT_TRUE(msch_xfer_cb(DADDR, EP_IN, XFER_RESULT_SUCCESS, sizeof(cap)));
  reply_csw_passed();

  TEST_ASSERT_TRUE(tuh_msc_mounted(DADDR));
  TEST_ASSERT_EQUAL(1, set_config_count);
  xfer_count = 0;
}

void setUp(void) {
  xfer_count      = 0;
  xfer_fail_index = UINT8_MAX;
  complete_count  = 0;
  set_config_count = 0;
  retry_submitted = false;
  msch_init();
  mount_bot_interface();
}

void tearDown(void) {}

static void start_data_in_cb(tuh_msc_complete_cb_t complete_cb) {
  const msc_cbw_t cbw = {
    .signature = MSC_CBW_SIGNATURE, .tag = 0x1234, .total_bytes = sizeof(data), .dir = TUSB_DIR_IN_MASK};
  TEST_ASSERT_TRUE(tuh_msc_scsi_command(DADDR, &cbw, data, complete_cb, 0));
  TEST_ASSERT_TRUE(msch_xfer_cb(DADDR, EP_OUT, XFER_RESULT_SUCCESS, sizeof(msc_cbw_t)));
}

static void start_data_in(void) {
  start_data_in_cb(record_complete);
}

// usbh_edpt_xfer() takes a 16-bit length, so a data stage of 64 KiB or more must be split into
// packet-aligned transfers rather than narrowed (98304 would otherwise become 32768).
void test_msc_host_data_stage_over_64k(void) {
  start_data_in();
  TEST_ASSERT_EQUAL(2, xfer_count);
  TEST_ASSERT_EQUAL_HEX8(EP_IN, xfer_ep[1]);
  TEST_ASSERT_EQUAL_PTR(data, xfer_buf[1]);
  const uint16_t first = xfer_len[1];
  TEST_ASSERT_TRUE(first > sizeof(data) - UINT16_MAX);
  TEST_ASSERT_EQUAL(0, first % 512);

  TEST_ASSERT_TRUE(msch_xfer_cb(DADDR, EP_IN, XFER_RESULT_SUCCESS, first));
  TEST_ASSERT_EQUAL(3, xfer_count);
  TEST_ASSERT_EQUAL_HEX8(EP_IN, xfer_ep[2]);
  TEST_ASSERT_EQUAL_PTR(data + first, xfer_buf[2]);
  TEST_ASSERT_EQUAL(sizeof(data) - first, xfer_len[2]);

  // last data transfer done: status stage reads the CSW
  TEST_ASSERT_TRUE(msch_xfer_cb(DADDR, EP_IN, XFER_RESULT_SUCCESS, xfer_len[2]));
  TEST_ASSERT_EQUAL(4, xfer_count);
  TEST_ASSERT_EQUAL_HEX8(EP_IN, xfer_ep[3]);
  TEST_ASSERT_EQUAL(sizeof(msc_csw_t), xfer_len[3]);
}

// A short transfer ends the data stage early: go straight to the CSW
void test_msc_host_data_stage_short_ends_early(void) {
  start_data_in();
  TEST_ASSERT_TRUE(msch_xfer_cb(DADDR, EP_IN, XFER_RESULT_SUCCESS, 512));

  TEST_ASSERT_EQUAL(3, xfer_count);
  TEST_ASSERT_EQUAL(sizeof(msc_csw_t), xfer_len[2]);
}

// A stage that fails to submit leaves no transfer pending: the command must still complete, and only once.
// The device is then left mid-command, so no further CBW may reach it until it is re-enumerated.
static void assert_submit_fail_completed(uint8_t submitted, uint32_t residue) {
  TEST_ASSERT_EQUAL(submitted, xfer_count);
  TEST_ASSERT_EQUAL(1, complete_count);
  TEST_ASSERT_EQUAL_HEX32(MSC_CSW_SIGNATURE, complete_csw.signature);
  TEST_ASSERT_EQUAL_HEX32(0x1234, complete_csw.tag);
  TEST_ASSERT_EQUAL(MSC_CSW_STATUS_PHASE_ERROR, complete_csw.status);
  TEST_ASSERT_EQUAL(residue, complete_csw.data_residue);

  // refused from the completion callback and afterwards, with nothing submitted
  TEST_ASSERT_FALSE(retry_submitted);
  TEST_ASSERT_FALSE(tuh_msc_ready(DADDR));
  const msc_cbw_t cbw = {.signature = MSC_CBW_SIGNATURE, .tag = 0x5678};
  TEST_ASSERT_FALSE(tuh_msc_scsi_command(DADDR, &cbw, NULL, record_complete, 0));
  TEST_ASSERT_FALSE(tuh_msc_read10(DADDR, 0, data, 0, 1, record_complete, 0));
  TEST_ASSERT_EQUAL(submitted, xfer_count);
  TEST_ASSERT_EQUAL(1, complete_count);
}

void test_msc_host_data_stage_first_chunk_submit_fail_completes(void) {
  enumerate();
  TEST_ASSERT_TRUE(tuh_msc_ready(DADDR));

  xfer_fail_index = 1; // CBW, then the first chunk fails
  start_data_in_cb(retry_complete);

  assert_submit_fail_completed(2, sizeof(data));
}

void test_msc_host_no_data_status_submit_fail_completes(void) {
  enumerate();
  TEST_ASSERT_TRUE(tuh_msc_ready(DADDR));

  xfer_fail_index = 1; // CBW, then the CSW fails
  const msc_cbw_t cbw = {.signature = MSC_CBW_SIGNATURE, .tag = 0x1234};
  TEST_ASSERT_TRUE(tuh_msc_scsi_command(DADDR, &cbw, NULL, retry_complete, 0));
  TEST_ASSERT_TRUE(msch_xfer_cb(DADDR, EP_OUT, XFER_RESULT_SUCCESS, sizeof(msc_cbw_t)));

  assert_submit_fail_completed(2, 0);
}

void test_msc_host_data_stage_status_submit_fail_completes(void) {
  enumerate();
  TEST_ASSERT_TRUE(tuh_msc_ready(DADDR));

  xfer_fail_index = 2; // CBW, a short first chunk, then the CSW fails
  start_data_in_cb(retry_complete);
  TEST_ASSERT_TRUE(msch_xfer_cb(DADDR, EP_IN, XFER_RESULT_SUCCESS, 512));

  TEST_ASSERT_EQUAL(sizeof(msc_csw_t), xfer_len[2]);
  assert_submit_fail_completed(3, sizeof(data) - 512);
}

void test_msc_host_data_stage_chunk_submit_fail_completes(void) {
  enumerate();
  TEST_ASSERT_TRUE(tuh_msc_ready(DADDR));

  xfer_fail_index = 2; // CBW, first chunk, then the second chunk fails
  start_data_in_cb(retry_complete);
  (void) msch_xfer_cb(DADDR, EP_IN, XFER_RESULT_SUCCESS, xfer_len[1]);

  assert_submit_fail_completed(3, sizeof(data) - xfer_len[1]);
  const msc_cbw_t cbw = {.signature = MSC_CBW_SIGNATURE, .tag = 0x5678};

  // re-enumeration clears it: the next command runs through all its stages
  msch_close(DADDR);
  xfer_fail_index  = UINT8_MAX;
  set_config_count = 0;
  mount_bot_interface();
  enumerate();
  TEST_ASSERT_TRUE(tuh_msc_ready(DADDR));

  TEST_ASSERT_TRUE(tuh_msc_scsi_command(DADDR, &cbw, NULL, record_complete, 0));
  TEST_ASSERT_EQUAL(1, xfer_count);
  TEST_ASSERT_EQUAL_HEX8(EP_OUT, xfer_ep[0]);
  TEST_ASSERT_TRUE(msch_xfer_cb(DADDR, EP_OUT, XFER_RESULT_SUCCESS, sizeof(msc_cbw_t)));
  TEST_ASSERT_EQUAL(2, xfer_count);
  TEST_ASSERT_EQUAL_HEX8(EP_IN, xfer_ep[1]);

  const msc_csw_t csw = {.signature = MSC_CSW_SIGNATURE, .tag = 0x5678, .status = MSC_CSW_STATUS_PASSED};
  memcpy(xfer_buf[1], &csw, sizeof(csw));
  TEST_ASSERT_TRUE(msch_xfer_cb(DADDR, EP_IN, XFER_RESULT_SUCCESS, sizeof(msc_csw_t)));
  TEST_ASSERT_EQUAL(2, complete_count);
  TEST_ASSERT_EQUAL(MSC_CSW_STATUS_PASSED, complete_csw.status);
  TEST_ASSERT_EQUAL_HEX32(0x5678, complete_csw.tag);
}

// A failed enumeration command leaves the interface unmounted, but usbh must still be told its configuration
// is done: until then it holds the enumeration, blocking this device's other interfaces and every later device.
static void assert_enum_ended_unmounted(uint8_t submitted) {
  TEST_ASSERT_EQUAL(submitted, xfer_count);
  TEST_ASSERT_EQUAL(1, set_config_count);
  TEST_ASSERT_FALSE(tuh_msc_mounted(DADDR));
  TEST_ASSERT_FALSE(tuh_msc_ready(DADDR));
}

void test_msc_host_enum_tur_cbw_submit_fail(void) {
  xfer_fail_index = 0;
  get_max_lun_stalled();

  assert_enum_ended_unmounted(1);
}

// the synthesized PHASE_ERROR fails Test Unit Ready, and its Request Sense retry is refused
void test_msc_host_enum_tur_status_submit_fail(void) {
  xfer_fail_index = 1; // TUR CBW, then its CSW fails
  get_max_lun_stalled();
  TEST_ASSERT_TRUE(msch_xfer_cb(DADDR, EP_OUT, XFER_RESULT_SUCCESS, sizeof(msc_cbw_t)));

  assert_enum_ended_unmounted(2);
}

void test_msc_host_enum_request_sense_submit_fail(void) {
  xfer_fail_index = 3; // TUR CBW and CSW, Request Sense CBW, then its data stage fails
  get_max_lun_stalled();
  TEST_ASSERT_TRUE(msch_xfer_cb(DADDR, EP_OUT, XFER_RESULT_SUCCESS, sizeof(msc_cbw_t)));
  reply_csw(MSC_CSW_STATUS_FAILED);
  TEST_ASSERT_TRUE(msch_xfer_cb(DADDR, EP_OUT, XFER_RESULT_SUCCESS, sizeof(msc_cbw_t)));

  assert_enum_ended_unmounted(4);
}

void test_msc_host_enum_read_capacity_submit_fail(void) {
  xfer_fail_index = 3; // TUR CBW and CSW, Read Capacity CBW, then its data stage fails
  get_max_lun_stalled();
  TEST_ASSERT_TRUE(msch_xfer_cb(DADDR, EP_OUT, XFER_RESULT_SUCCESS, sizeof(msc_cbw_t)));
  reply_csw_passed();
  TEST_ASSERT_TRUE(msch_xfer_cb(DADDR, EP_OUT, XFER_RESULT_SUCCESS, sizeof(msc_cbw_t)));

  assert_enum_ended_unmounted(4);
}

// usbh closes the driver before failing the in-flight Get Max LUN, and has already ended the enumeration
void test_msc_host_enum_unplug_during_get_max_lun(void) {
  msch_close(DADDR);
  tuh_xfer_t ctrl = {.daddr = DADDR, .result = XFER_RESULT_FAILED};
  ctrl_complete_cb(&ctrl);

  TEST_ASSERT_EQUAL(0, xfer_count);
  TEST_ASSERT_EQUAL(0, set_config_count);
  TEST_ASSERT_FALSE(tuh_msc_mounted(DADDR));
}

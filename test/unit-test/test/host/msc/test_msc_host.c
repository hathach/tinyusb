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
static msc_csw_t complete_csw;
static uint8_t  enum_buf[64];
static uint8_t  max_lun_resp; // GET_MAX_LUN response: highest LUN index
static xfer_result_t max_lun_result;
static uint8_t  data[98304];

bool tuh_edpt_open(uint8_t daddr, const tusb_desc_endpoint_t *desc_ep) {
  (void) daddr;
  (void) desc_ep;
  return true;
}

// completes GET_MAX_LUN at once, as if the device answered max_lun_resp (or STALLed)
bool tuh_control_xfer(tuh_xfer_t *xfer) {
  TEST_ASSERT_EQUAL(MSC_REQ_GET_MAX_LUN, xfer->setup->bRequest);
  enum_buf[0]  = max_lun_resp;
  xfer->result = max_lun_result;
  xfer->complete_cb(xfer);
  return true;
}

uint8_t *usbh_get_enum_buf(void) {
  return enum_buf;
}

void usbh_driver_set_config_complete(uint8_t dev_addr, uint8_t itf_num) {
  (void) dev_addr;
  (void) itf_num;
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

void setUp(void) {
  max_lun_resp    = 0;
  max_lun_result  = XFER_RESULT_SUCCESS;
  xfer_count      = 0;
  xfer_fail_index = UINT8_MAX;
  complete_count  = 0;
  msch_init();
  mount_bot_interface();
  xfer_count = 0; // drop the TEST UNIT READY the configuration sent
}

void tearDown(void) {}

static void check_open(uint8_t itf_len, uint8_t num_ep, uint8_t out_len, uint8_t in_len, uint16_t expected) {
  struct TU_ATTR_PACKED {
    tusb_desc_interface_t itf;
    tusb_desc_endpoint_t  ep_out;
    tusb_desc_endpoint_t  ep_in;
  } const desc = {
    .itf    = {itf_len, TUSB_DESC_INTERFACE, 0, 0, num_ep, TUSB_CLASS_MSC, MSC_SUBCLASS_SCSI, MSC_PROTOCOL_BOT, 0},
    .ep_out = {out_len, TUSB_DESC_ENDPOINT, EP_OUT, {.xfer = TUSB_XFER_BULK}, 512, 0},
    .ep_in  = {in_len, TUSB_DESC_ENDPOINT, EP_IN, {.xfer = TUSB_XFER_BULK}, 512, 0},
  };
  TEST_ASSERT_EQUAL(expected, msch_open(0, DADDR, &desc.itf, sizeof(desc)));
}

void test_msc_host_open_valid(void) {
  check_open(9, 2, 7, 7, 9 + 2 * 7);
}

void test_msc_host_open_rejects_bad_num_endpoints(void) {
  check_open(9, 1, 7, 7, 0);
  check_open(9, 3, 7, 7, 0);
}

void test_msc_host_open_rejects_bad_interface_length(void) {
  check_open(23, 2, 7, 7, 0);
  check_open(8, 2, 7, 7, 0);
}

void test_msc_host_open_rejects_bad_endpoint_length(void) {
  check_open(9, 2, 6, 7, 0);
  check_open(9, 2, 7, 8, 0);
  check_open(9, 2, 0, 7, 0);
}

static void start_data_in(void) {
  const msc_cbw_t cbw = {
    .signature = MSC_CBW_SIGNATURE, .tag = 0x1234, .total_bytes = sizeof(data), .dir = TUSB_DIR_IN_MASK};
  TEST_ASSERT_TRUE(tuh_msc_scsi_command(DADDR, &cbw, data, record_complete, 0));
  TEST_ASSERT_TRUE(msch_xfer_cb(DADDR, EP_OUT, XFER_RESULT_SUCCESS, sizeof(msc_cbw_t)));
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

// The LUN count is the GET_MAX_LUN response plus one, clamped to CFG_TUH_MSC_MAXLUN; a STALL means one LUN
void test_msc_host_max_lun_count(void) {
  const struct {
    uint8_t       resp;
    xfer_result_t result;
    uint8_t       count;
  } cases[] = {
    {0x00, XFER_RESULT_SUCCESS, 1},
    {0x0A, XFER_RESULT_SUCCESS, CFG_TUH_MSC_MAXLUN},
    {0x00, XFER_RESULT_STALLED, 1},
  };
  for (size_t i = 0; i < TU_ARRAY_SIZE(cases); i++) {
    msch_close(DADDR);
    max_lun_resp   = cases[i].resp;
    max_lun_result = cases[i].result;
    mount_bot_interface();
    TEST_ASSERT_EQUAL(cases[i].count, tuh_msc_get_maxlun(DADDR));
  }
}

// A device reporting more LUNs than CFG_TUH_MSC_MAXLUN is clamped, and LUNs past it are refused
void test_msc_host_max_lun_clamped(void) {
  msch_close(DADDR);
  xfer_count   = 0;
  max_lun_resp = 255; // 256 LUNs, wraps to 0 in a uint8_t count
  mount_bot_interface();
  TEST_ASSERT_EQUAL(CFG_TUH_MSC_MAXLUN, tuh_msc_get_maxlun(DADDR));

  TEST_ASSERT_EQUAL(0, tuh_msc_get_block_count(DADDR, CFG_TUH_MSC_MAXLUN));
  TEST_ASSERT_EQUAL(0, tuh_msc_get_block_size(DADDR, CFG_TUH_MSC_MAXLUN));
  xfer_count = 0;
  const msc_cbw_t cbw = {.signature = MSC_CBW_SIGNATURE, .lun = CFG_TUH_MSC_MAXLUN};
  TEST_ASSERT_FALSE(tuh_msc_scsi_command(DADDR, &cbw, NULL, NULL, 0));
  TEST_ASSERT_EQUAL(0, xfer_count);
}

void test_msc_host_lun_past_reported_count_refused(void) {
  TEST_ASSERT_EQUAL(1, tuh_msc_get_maxlun(DADDR));
  const msc_cbw_t cbw = {.signature = MSC_CBW_SIGNATURE, .lun = 1};
  TEST_ASSERT_FALSE(tuh_msc_scsi_command(DADDR, &cbw, NULL, NULL, 0));
  TEST_ASSERT_EQUAL(0, xfer_count);
}

// Drives the enumeration's TEST UNIT READY and READ CAPACITY. The READ CAPACITY CBW is patched to `lun` so its
// geometry lands in that capacity[] slot, which enumeration (LUN 0 only) cannot otherwise reach.
static void complete_enumeration_capacity_as_lun(uint8_t lun, uint32_t block_count, uint32_t block_size) {
  TEST_ASSERT_TRUE(msch_xfer_cb(DADDR, EP_OUT, XFER_RESULT_SUCCESS, sizeof(msc_cbw_t))); // TUR CBW
  TEST_ASSERT_TRUE(msch_xfer_cb(DADDR, EP_IN, XFER_RESULT_SUCCESS, sizeof(msc_csw_t)));  // TUR CSW: passed
  TEST_ASSERT_EQUAL(2, xfer_count);
  TEST_ASSERT_EQUAL_HEX8(EP_OUT, xfer_ep[1]);
  ((msc_cbw_t*) xfer_buf[1])->lun = lun;
  TEST_ASSERT_TRUE(msch_xfer_cb(DADDR, EP_OUT, XFER_RESULT_SUCCESS, sizeof(msc_cbw_t)));

  scsi_read_capacity10_resp_t* resp = (scsi_read_capacity10_resp_t*) (uintptr_t) enum_buf;
  resp->last_lba   = tu_htonl(block_count - 1);
  resp->block_size = tu_htonl(block_size);
  TEST_ASSERT_TRUE(msch_xfer_cb(DADDR, EP_IN, XFER_RESULT_SUCCESS, sizeof(scsi_read_capacity10_resp_t)));
  TEST_ASSERT_TRUE(msch_xfer_cb(DADDR, EP_IN, XFER_RESULT_SUCCESS, sizeof(msc_csw_t)));
  TEST_ASSERT_TRUE(tuh_msc_mounted(DADDR));
}

void test_msc_host_geometry_of_lun_0(void) {
  complete_enumeration_capacity_as_lun(0, 1000, 512);
  TEST_ASSERT_EQUAL(1000, tuh_msc_get_block_count(DADDR, 0));
  TEST_ASSERT_EQUAL(512, tuh_msc_get_block_size(DADDR, 0));
}

// capacity[1] holds geometry, but LUN 1 is past the one LUN the device reported
void test_msc_host_geometry_past_reported_count_is_zero(void) {
  complete_enumeration_capacity_as_lun(1, 1000, 512);
  TEST_ASSERT_EQUAL(1, tuh_msc_get_maxlun(DADDR));
  TEST_ASSERT_EQUAL(0, tuh_msc_get_block_count(DADDR, 1));
  TEST_ASSERT_EQUAL(0, tuh_msc_get_block_size(DADDR, 1));

  xfer_count = 0;
  TEST_ASSERT_FALSE(tuh_msc_read10(DADDR, 1, data, 0, 1, NULL, 0));
  TEST_ASSERT_FALSE(tuh_msc_write10(DADDR, 1, data, 0, 1, NULL, 0));
  TEST_ASSERT_EQUAL(0, xfer_count);
}

// No transfer is pending once a later chunk fails to submit: the command must still complete, and only once
void test_msc_host_data_stage_chunk_submit_fail_completes(void) {
  xfer_fail_index = 2; // CBW, first chunk, then the second chunk fails
  start_data_in();
  (void) msch_xfer_cb(DADDR, EP_IN, XFER_RESULT_SUCCESS, xfer_len[1]);

  TEST_ASSERT_EQUAL(3, xfer_count);
  TEST_ASSERT_EQUAL(1, complete_count);
  TEST_ASSERT_EQUAL_HEX32(MSC_CSW_SIGNATURE, complete_csw.signature);
  TEST_ASSERT_EQUAL_HEX32(0x1234, complete_csw.tag);
  TEST_ASSERT_EQUAL(MSC_CSW_STATUS_PHASE_ERROR, complete_csw.status);
  TEST_ASSERT_EQUAL(sizeof(data) - xfer_len[1], complete_csw.data_residue);

  // the next command runs through all its stages
  const msc_cbw_t cbw = {.signature = MSC_CBW_SIGNATURE, .tag = 0x5678};
  TEST_ASSERT_TRUE(tuh_msc_scsi_command(DADDR, &cbw, NULL, record_complete, 0));
  TEST_ASSERT_EQUAL(4, xfer_count);
  TEST_ASSERT_EQUAL_HEX8(EP_OUT, xfer_ep[3]);
  TEST_ASSERT_TRUE(msch_xfer_cb(DADDR, EP_OUT, XFER_RESULT_SUCCESS, sizeof(msc_cbw_t)));
  TEST_ASSERT_EQUAL(5, xfer_count);
  TEST_ASSERT_EQUAL_HEX8(EP_IN, xfer_ep[4]);

  const msc_csw_t csw = {.signature = MSC_CSW_SIGNATURE, .tag = 0x5678, .status = MSC_CSW_STATUS_PASSED};
  memcpy(xfer_buf[4], &csw, sizeof(csw));
  TEST_ASSERT_TRUE(msch_xfer_cb(DADDR, EP_IN, XFER_RESULT_SUCCESS, sizeof(msc_csw_t)));
  TEST_ASSERT_EQUAL(2, complete_count);
  TEST_ASSERT_EQUAL(MSC_CSW_STATUS_PASSED, complete_csw.status);
  TEST_ASSERT_EQUAL_HEX32(0x5678, complete_csw.tag);
}

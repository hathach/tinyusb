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
  ITF_NUM   = 2, // not 0, so an interface number the driver loses or zeroes shows
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
static uint8_t  set_config_daddr;
static uint8_t  set_config_itf;
static msc_csw_t complete_csw;
static bool     retry_submitted;
static bool     ctrl_xfer_fail;
static uint8_t  enum_buf[64];
static uint8_t  data[98304];
static uint8_t  edpt_open_count;

bool tuh_edpt_open(uint8_t daddr, const tusb_desc_endpoint_t *desc_ep) {
  (void) daddr;
  (void) desc_ep;
  edpt_open_count++;
  return true;
}

static tuh_xfer_t ctrl_xfer;

bool tuh_control_xfer(tuh_xfer_t *xfer) {
  ctrl_xfer = *xfer;
  return !ctrl_xfer_fail;
}

uint8_t *usbh_get_enum_buf(void) {
  return enum_buf;
}

void usbh_driver_set_config_complete(uint8_t dev_addr, uint8_t itf_num) {
  set_config_daddr = dev_addr;
  set_config_itf   = itf_num;
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

static void open_bot_interface(void) {
  struct TU_ATTR_PACKED {
    tusb_desc_interface_t itf;
    tusb_desc_endpoint_t  ep_out;
    tusb_desc_endpoint_t  ep_in;
  } const desc = {
    .itf    = {sizeof(tusb_desc_interface_t), TUSB_DESC_INTERFACE, ITF_NUM, 0, 2, TUSB_CLASS_MSC, MSC_SUBCLASS_SCSI,
               MSC_PROTOCOL_BOT, 0},
    .ep_out = {sizeof(tusb_desc_endpoint_t), TUSB_DESC_ENDPOINT, EP_OUT, {.xfer = TUSB_XFER_BULK}, 512, 0},
    .ep_in  = {sizeof(tusb_desc_endpoint_t), TUSB_DESC_ENDPOINT, EP_IN, {.xfer = TUSB_XFER_BULK}, 512, 0},
  };

  TEST_ASSERT_EQUAL(sizeof(desc), msch_open(0, DADDR, &desc.itf, sizeof(desc)));
}

// usbh resumes enumeration from the interface it is told completed
static void assert_config_completed(void) {
  TEST_ASSERT_EQUAL(1, set_config_count);
  TEST_ASSERT_EQUAL(DADDR, set_config_daddr);
  TEST_ASSERT_EQUAL(ITF_NUM, set_config_itf);
}

static void mount_bot_interface(void) {
  open_bot_interface();
  TEST_ASSERT_TRUE(msch_set_config(DADDR, ITF_NUM));
}

static void reply_csw(uint8_t status) {
  const msc_csw_t csw = {.signature = MSC_CSW_SIGNATURE, .status = status};
  memcpy(xfer_buf[xfer_count - 1], &csw, sizeof(csw));
  TEST_ASSERT_TRUE(msch_xfer_cb(DADDR, EP_IN, XFER_RESULT_SUCCESS, sizeof(msc_csw_t)));
}

static void reply_csw_passed(void) {
  reply_csw(MSC_CSW_STATUS_PASSED);
}

// msch_set_config() queues Get Max LUN; complete it with a chosen response byte / result. Test Unit Ready follows.
static void complete_get_maxlun(uint8_t resp_byte, xfer_result_t result) {
  enum_buf[0] = resp_byte;
  tuh_xfer_t ctrl = ctrl_xfer;
  ctrl.result = result;
  ctrl_xfer.complete_cb(&ctrl);
}

// Get Max LUN STALLs (one LUN)
static void get_max_lun_stalled(void) {
  complete_get_maxlun(0x00, XFER_RESULT_STALLED);
}

// Get Max LUN reports one LUN; drop the Test Unit Ready that follows
static void get_max_lun_one(void) {
  complete_get_maxlun(0x00, XFER_RESULT_SUCCESS);
  xfer_count = 0;
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
  assert_config_completed();
  xfer_count = 0;
}

void setUp(void) {
  xfer_count      = 0;
  xfer_fail_index = UINT8_MAX;
  complete_count  = 0;
  set_config_count = 0;
  retry_submitted = false;
  ctrl_xfer_fail  = false;
  msch_init();
  mount_bot_interface();
}

void tearDown(void) {}

static void check_open_at(uint8_t daddr, uint8_t itf_len, uint8_t num_ep, uint8_t out_len, uint8_t in_len,
                          uint16_t expected) {
  struct TU_ATTR_PACKED {
    tusb_desc_interface_t itf;
    tusb_desc_endpoint_t  ep_out;
    tusb_desc_endpoint_t  ep_in;
  } const desc = {
    .itf    = {itf_len, TUSB_DESC_INTERFACE, 0, 0, num_ep, TUSB_CLASS_MSC, MSC_SUBCLASS_SCSI, MSC_PROTOCOL_BOT, 0},
    .ep_out = {out_len, TUSB_DESC_ENDPOINT, EP_OUT, {.xfer = TUSB_XFER_BULK}, 512, 0},
    .ep_in  = {in_len, TUSB_DESC_ENDPOINT, EP_IN, {.xfer = TUSB_XFER_BULK}, 512, 0},
  };
  TEST_ASSERT_EQUAL(expected, msch_open(0, daddr, &desc.itf, sizeof(desc)));
}

static void check_open(uint8_t itf_len, uint8_t num_ep, uint8_t out_len, uint8_t in_len, uint16_t expected) {
  check_open_at(DADDR, itf_len, num_ep, out_len, in_len, expected);
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

// usbh addresses a hub-class device above CFG_TUH_DEVICE_MAX and still offers its interfaces to every driver
void test_msc_host_open_rejects_hub_address(void) {
  edpt_open_count = 0;
  check_open_at(CFG_TUH_DEVICE_MAX + 1, 9, 2, 7, 7, 0);
  TEST_ASSERT_EQUAL(0, edpt_open_count);
}

static void start_data_in_cb(tuh_msc_complete_cb_t complete_cb) {
  const msc_cbw_t cbw = {
    .signature = MSC_CBW_SIGNATURE, .tag = 0x1234, .total_bytes = sizeof(data), .dir = TUSB_DIR_IN_MASK};
  TEST_ASSERT_TRUE(tuh_msc_scsi_command(DADDR, &cbw, data, complete_cb, 0));
  TEST_ASSERT_TRUE(msch_xfer_cb(DADDR, EP_OUT, XFER_RESULT_SUCCESS, sizeof(msc_cbw_t)));
}

static void start_data_in(void) {
  start_data_in_cb(record_complete);
}

static void start_no_data_cb(tuh_msc_complete_cb_t complete_cb) {
  const msc_cbw_t cbw = {.signature = MSC_CBW_SIGNATURE, .tag = 0x1234};
  TEST_ASSERT_TRUE(tuh_msc_scsi_command(DADDR, &cbw, NULL, complete_cb, 0));
  TEST_ASSERT_TRUE(msch_xfer_cb(DADDR, EP_OUT, XFER_RESULT_SUCCESS, sizeof(msc_cbw_t)));
}

// usbh_edpt_xfer() takes a 16-bit length, so a data stage of 64 KiB or more must be split into
// packet-aligned transfers rather than narrowed (98304 would otherwise become 32768).
void test_msc_host_data_stage_over_64k(void) {
  get_max_lun_one();
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
  get_max_lun_one();
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
    mount_bot_interface();
    complete_get_maxlun(cases[i].resp, cases[i].result);
    TEST_ASSERT_EQUAL(cases[i].count, tuh_msc_get_maxlun(DADDR));
  }
}

// A device reporting more LUNs than CFG_TUH_MSC_MAXLUN is clamped, and LUNs past it are refused
void test_msc_host_max_lun_clamped(void) {
  complete_get_maxlun(255, XFER_RESULT_SUCCESS); // 256 LUNs, wraps to 0 in a uint8_t count
  TEST_ASSERT_EQUAL(CFG_TUH_MSC_MAXLUN, tuh_msc_get_maxlun(DADDR));

  TEST_ASSERT_EQUAL(0, tuh_msc_get_block_count(DADDR, CFG_TUH_MSC_MAXLUN));
  TEST_ASSERT_EQUAL(0, tuh_msc_get_block_size(DADDR, CFG_TUH_MSC_MAXLUN));
  xfer_count = 0;
  const msc_cbw_t cbw = {.signature = MSC_CBW_SIGNATURE, .lun = CFG_TUH_MSC_MAXLUN};
  TEST_ASSERT_FALSE(tuh_msc_scsi_command(DADDR, &cbw, NULL, NULL, 0));
  TEST_ASSERT_EQUAL(0, xfer_count);
}

void test_msc_host_lun_past_reported_count_refused(void) {
  get_max_lun_one();
  TEST_ASSERT_EQUAL(1, tuh_msc_get_maxlun(DADDR));
  const msc_cbw_t cbw = {.signature = MSC_CBW_SIGNATURE, .lun = 1};
  TEST_ASSERT_FALSE(tuh_msc_scsi_command(DADDR, &cbw, NULL, NULL, 0));
  TEST_ASSERT_EQUAL(0, xfer_count);
}

// Drives the enumeration's TEST UNIT READY and READ CAPACITY. The READ CAPACITY CBW is patched to `lun` so its
// geometry lands in that capacity[] slot, which enumeration (LUN 0 only) cannot otherwise reach.
static void complete_enumeration_capacity_as_lun(uint8_t lun, uint32_t block_count, uint32_t block_size) {
  get_max_lun_one();
  TEST_ASSERT_TRUE(msch_xfer_cb(DADDR, EP_OUT, XFER_RESULT_SUCCESS, sizeof(msc_cbw_t))); // TUR CBW
  reply_csw_passed();
  TEST_ASSERT_EQUAL(2, xfer_count);
  TEST_ASSERT_EQUAL_HEX8(EP_OUT, xfer_ep[1]);
  ((msc_cbw_t*) xfer_buf[1])->lun = lun;
  TEST_ASSERT_TRUE(msch_xfer_cb(DADDR, EP_OUT, XFER_RESULT_SUCCESS, sizeof(msc_cbw_t)));

  scsi_read_capacity10_resp_t* resp = (scsi_read_capacity10_resp_t*) (uintptr_t) enum_buf;
  resp->last_lba   = tu_htonl(block_count - 1);
  resp->block_size = tu_htonl(block_size);
  TEST_ASSERT_TRUE(msch_xfer_cb(DADDR, EP_IN, XFER_RESULT_SUCCESS, sizeof(scsi_read_capacity10_resp_t)));
  reply_csw_passed();
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

// refused from the completion callback and afterwards, with nothing submitted
static void assert_fenced(uint8_t submitted, uint8_t completed) {
  TEST_ASSERT_FALSE(retry_submitted);
  TEST_ASSERT_FALSE(tuh_msc_ready(DADDR));
  const msc_cbw_t cbw = {.signature = MSC_CBW_SIGNATURE, .tag = 0x5678};
  TEST_ASSERT_FALSE(tuh_msc_scsi_command(DADDR, &cbw, NULL, record_complete, 0));
  TEST_ASSERT_FALSE(tuh_msc_read10(DADDR, 0, data, 0, 1, record_complete, 0));
  TEST_ASSERT_EQUAL(submitted, xfer_count);
  TEST_ASSERT_EQUAL(completed, complete_count);
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
  assert_fenced(submitted, 1);
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
  start_no_data_cb(retry_complete);

  assert_submit_fail_completed(2, 0);
}

// without a completion callback the failed command still fences the interface
void test_msc_host_submit_fail_without_callback(void) {
  enumerate();

  xfer_fail_index = 1; // CBW, then the CSW fails
  start_no_data_cb(NULL);

  assert_fenced(2, 0);
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
  TEST_ASSERT_TRUE(msch_xfer_cb(DADDR, EP_IN, XFER_RESULT_SUCCESS, xfer_len[1]));

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
  assert_config_completed();
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

// Test Unit Ready fails and its Request Sense passes: enumeration retries Test Unit Ready (5 transfers before it)
static void enum_tur_failed_request_sense_passed(void) {
  get_max_lun_stalled();
  TEST_ASSERT_TRUE(msch_xfer_cb(DADDR, EP_OUT, XFER_RESULT_SUCCESS, sizeof(msc_cbw_t)));
  reply_csw(MSC_CSW_STATUS_FAILED);
  TEST_ASSERT_TRUE(msch_xfer_cb(DADDR, EP_OUT, XFER_RESULT_SUCCESS, sizeof(msc_cbw_t)));
  TEST_ASSERT_TRUE(msch_xfer_cb(DADDR, EP_IN, XFER_RESULT_SUCCESS, 18));
  reply_csw_passed();
}

void test_msc_host_enum_request_sense_passed_retries_tur(void) {
  enum_tur_failed_request_sense_passed();

  TEST_ASSERT_EQUAL(6, xfer_count);
  TEST_ASSERT_EQUAL_HEX8(EP_OUT, xfer_ep[5]);
  TEST_ASSERT_EQUAL(sizeof(msc_cbw_t), xfer_len[5]);
  TEST_ASSERT_EQUAL_HEX8(SCSI_CMD_TEST_UNIT_READY, ((msc_cbw_t*) xfer_buf[5])->command[0]);
  TEST_ASSERT_EQUAL(0, set_config_count);
}

void test_msc_host_enum_request_sense_passed_tur_submit_fail(void) {
  xfer_fail_index = 5; // TUR, Request Sense, then the retried TUR CBW fails
  enum_tur_failed_request_sense_passed();

  assert_enum_ended_unmounted(6);
}

void test_msc_host_enum_read_capacity_submit_fail(void) {
  xfer_fail_index = 3; // TUR CBW and CSW, Read Capacity CBW, then its data stage fails
  get_max_lun_stalled();
  TEST_ASSERT_TRUE(msch_xfer_cb(DADDR, EP_OUT, XFER_RESULT_SUCCESS, sizeof(msc_cbw_t)));
  reply_csw_passed();
  TEST_ASSERT_TRUE(msch_xfer_cb(DADDR, EP_OUT, XFER_RESULT_SUCCESS, sizeof(msc_cbw_t)));

  assert_enum_ended_unmounted(4);
}

// usbh ignores set_config's result, so a Get Max LUN that fails to submit must end the configuration itself
void test_msc_host_enum_get_max_lun_submit_fail(void) {
  msch_close(DADDR);
  set_config_count = 0;
  ctrl_xfer_fail   = true;
  open_bot_interface();
  TEST_ASSERT_TRUE(msch_set_config(DADDR, ITF_NUM));

  assert_enum_ended_unmounted(0);
}

// usbh closes the driver before failing the in-flight Get Max LUN, and has already ended the enumeration
void test_msc_host_enum_unplug_during_get_max_lun(void) {
  msch_close(DADDR);
  tuh_xfer_t ctrl = ctrl_xfer; // closing keeps the tagid, so only the configured check rejects it
  ctrl.result = XFER_RESULT_FAILED;
  ctrl.complete_cb(&ctrl);

  TEST_ASSERT_EQUAL(0, xfer_count);
  TEST_ASSERT_EQUAL(0, set_config_count);
  TEST_ASSERT_FALSE(tuh_msc_mounted(DADDR));
}

// usbh fails a Get Max LUN still queued when its device left, even once the address is re-enumerated:
// it belongs to the earlier open and must not touch the reopened interface or end its configuration
void test_msc_host_enum_stale_get_max_lun_after_reopen(void) {
  tuh_xfer_t stale = ctrl_xfer;
  msch_close(DADDR);
  set_config_count = 0;
  mount_bot_interface();

  stale.result = XFER_RESULT_FAILED;
  stale.complete_cb(&stale);

  TEST_ASSERT_EQUAL(0, xfer_count);
  TEST_ASSERT_EQUAL(0, set_config_count);
  TEST_ASSERT_EQUAL(0, tuh_msc_get_maxlun(DADDR));

  // the reopened interface's own Get Max LUN still enumerates it
  enumerate();
}

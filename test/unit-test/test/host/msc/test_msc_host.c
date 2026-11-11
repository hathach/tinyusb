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
  MAX_CTRL  = 8,
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
static tusb_control_request_t ctrl_req[MAX_CTRL]; // the caller's setup is on its stack
static uint8_t ctrl_count;                        // submissions attempted
static uint8_t ctrl_fail_index;                   // ctrl_count value whose submission fails

bool tuh_control_xfer(tuh_xfer_t *xfer) {
  TEST_ASSERT_LESS_THAN(MAX_CTRL, ctrl_count);
  const uint8_t index = ctrl_count++;
  ctrl_req[index]     = *xfer->setup;
  ctrl_xfer           = *xfer;
  ctrl_xfer.setup     = &ctrl_req[index];
  return index != ctrl_fail_index;
}

// complete the last control transfer; its callback may submit the next one
static void complete_ctrl(xfer_result_t result) {
  tuh_xfer_t ctrl = ctrl_xfer;
  ctrl.result = result;
  ctrl.complete_cb(&ctrl);
}

static uint8_t clear_stall_ep[MAX_CTRL];
static uint8_t clear_stall_count;
static uint8_t clear_stall_fail_ep;

bool usbh_edpt_clear_stall(uint8_t dev_addr, uint8_t ep_addr) {
  (void) dev_addr;
  TEST_ASSERT_LESS_THAN(TU_ARRAY_SIZE(clear_stall_ep), clear_stall_count);
  clear_stall_ep[clear_stall_count++] = ep_addr;
  return ep_addr != clear_stall_fail_ep;
}

uint8_t *usbh_get_enum_buf(void) {
  return enum_buf;
}

void usbh_driver_set_config_complete(uint8_t dev_addr, uint8_t itf_num) {
  set_config_daddr = dev_addr;
  set_config_itf   = itf_num;
  set_config_count++;
}

static uint8_t busy_ep;
static uint8_t claimed_ep;              // claimed and not yet submitted or released, 0 for none
static void (*claim_preempt)(void);     // runs once inside the next claim, as another task would

bool usbh_edpt_claim(uint8_t dev_addr, uint8_t ep_addr) {
  (void) dev_addr;
  void (*preempt)(void) = claim_preempt;
  claim_preempt = NULL;
  if (preempt != NULL) {
    preempt();
  }
  if (ep_addr == claimed_ep || ep_addr == busy_ep) {
    return false;
  }
  claimed_ep = ep_addr;
  return true;
}

bool usbh_edpt_release(uint8_t dev_addr, uint8_t ep_addr) {
  (void) dev_addr;
  if (ep_addr != claimed_ep) {
    return false;
  }
  claimed_ep = 0;
  return true;
}

bool usbh_edpt_busy(uint8_t dev_addr, uint8_t ep_addr) {
  (void) dev_addr;
  return ep_addr == busy_ep;
}

bool usbh_edpt_xfer_with_callback(uint8_t dev_addr, uint8_t ep_addr, uint8_t *buffer, uint16_t total_bytes,
                                  tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  (void) dev_addr;
  (void) complete_cb;
  (void) user_data;
  TEST_ASSERT_LESS_THAN(MAX_XFERS, xfer_count);
  if (ep_addr == claimed_ep) {
    claimed_ep = 0; // usbh drops it once the transfer ends, which these tests drive themselves
  }
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

static void (*umount_hook)(void);

void tuh_msc_umount_cb(uint8_t dev_addr) {
  (void) dev_addr;
  if (umount_hook != NULL) {
    umount_hook();
  }
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
  complete_ctrl(result);
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
  ctrl_count      = 0;
  ctrl_fail_index = UINT8_MAX;
  clear_stall_count   = 0;
  clear_stall_fail_ep = 0;
  busy_ep         = 0;
  claimed_ep      = 0;
  claim_preempt   = NULL;
  umount_hook     = NULL;
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
  ctrl_fail_index  = ctrl_count;
  open_bot_interface();
  TEST_ASSERT_TRUE(msch_set_config(DADDR, ITF_NUM));

  assert_enum_ended_unmounted(0);
}

// usbh closes the driver before failing the in-flight Get Max LUN, and has already ended the enumeration
void test_msc_host_enum_unplug_during_get_max_lun(void) {
  msch_close(DADDR);
  tuh_xfer_t ctrl = ctrl_xfer;
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

//--------------------------------------------------------------------+
// Reset Recovery
//--------------------------------------------------------------------+
enum {
  RESET_ARG = 0x55,
};

static uint8_t reset_cb_count;
static bool    reset_success;
static bool    reset_retry_started;

static void record_reset(uint8_t daddr, bool success, uintptr_t arg) {
  TEST_ASSERT_EQUAL(DADDR, daddr);
  TEST_ASSERT_EQUAL(RESET_ARG, arg);
  reset_cb_count++;
  reset_success = success;
}

// a command straight from the success callback
static void reset_then_command(uint8_t daddr, bool success, uintptr_t arg) {
  record_reset(daddr, success, arg);
  retry_submitted = tuh_msc_test_unit_ready(daddr, 0, record_complete, 0);
}

// another attempt straight from the failure callback
static void reset_then_retry(uint8_t daddr, bool success, uintptr_t arg) {
  record_reset(daddr, success, arg);
  reset_retry_started = tuh_msc_reset(daddr, record_reset, RESET_ARG);
}

static void reset_counters(void) {
  xfer_count        = 0;
  complete_count    = 0;
  ctrl_count        = 0;
  clear_stall_count = 0;
  reset_cb_count    = 0;
  retry_submitted   = false;
  reset_retry_started = false;
}

static void enumerate_and_fence(void);

// usbh closes the driver, then clears the device's endpoint states
static void unplug(void) {
  msch_close(DADDR);
  claimed_ep = 0;
}

static void remount_and_fence(void) {
  msch_close(DADDR);
  set_config_count = 0;
  mount_bot_interface();
  enumerate_and_fence();
}

static void enumerate_and_fence(void) {
  enumerate();
  xfer_fail_index = 1; // CBW, then the CSW fails
  start_no_data_cb(record_complete);
  xfer_fail_index = UINT8_MAX;
  TEST_ASSERT_FALSE(tuh_msc_ready(DADDR));
  reset_counters();
}

static void assert_request(uint8_t index, uint8_t bm_request_type, uint8_t b_request, uint16_t w_value,
                           uint16_t w_index) {
  TEST_ASSERT_EQUAL_HEX8(bm_request_type, ctrl_req[index].bmRequestType);
  TEST_ASSERT_EQUAL_HEX8(b_request, ctrl_req[index].bRequest);
  TEST_ASSERT_EQUAL_HEX16(w_value, ctrl_req[index].wValue);
  TEST_ASSERT_EQUAL_HEX16(w_index, ctrl_req[index].wIndex);
  TEST_ASSERT_EQUAL(0, ctrl_req[index].wLength);
}

// the interface takes a command again and runs it from its CBW
static void assert_unfenced(void) {
  TEST_ASSERT_TRUE(tuh_msc_ready(DADDR));
  const uint8_t submitted = xfer_count;
  TEST_ASSERT_TRUE(tuh_msc_test_unit_ready(DADDR, 0, record_complete, 0));
  TEST_ASSERT_EQUAL(submitted + 1, xfer_count);
  TEST_ASSERT_EQUAL_HEX8(EP_OUT, xfer_ep[submitted]);
}

static void assert_reset_failed(void) {
  TEST_ASSERT_EQUAL(1, reset_cb_count);
  TEST_ASSERT_FALSE(reset_success);
  assert_fenced(0, 0);
}

static void complete_recovery(void) {
  complete_ctrl(XFER_RESULT_SUCCESS); // Bulk-Only Mass Storage Reset
  complete_ctrl(XFER_RESULT_SUCCESS); // Clear HALT Bulk-In
  complete_ctrl(XFER_RESULT_SUCCESS); // Clear HALT Bulk-Out
}

// BOT 1.0 §5.3.4 order, each Clear HALT followed by its host toggle reset; the fence holds until the last one
void test_msc_host_reset_recovers_fenced_interface(void) {
  enumerate_and_fence();

  TEST_ASSERT_TRUE(tuh_msc_reset(DADDR, record_reset, RESET_ARG));
  TEST_ASSERT_EQUAL(1, ctrl_count);
  TEST_ASSERT_EQUAL_PTR(NULL, ctrl_xfer.buffer);
  assert_request(0, 0x21, MSC_REQ_RESET, 0, ITF_NUM);
  TEST_ASSERT_FALSE(tuh_msc_ready(DADDR));

  complete_ctrl(XFER_RESULT_SUCCESS);
  TEST_ASSERT_EQUAL(2, ctrl_count);
  assert_request(1, 0x02, TUSB_REQ_CLEAR_FEATURE, TUSB_REQ_FEATURE_EDPT_HALT, EP_IN);
  TEST_ASSERT_EQUAL(0, clear_stall_count);
  TEST_ASSERT_FALSE(tuh_msc_ready(DADDR));

  complete_ctrl(XFER_RESULT_SUCCESS);
  TEST_ASSERT_EQUAL(3, ctrl_count);
  assert_request(2, 0x02, TUSB_REQ_CLEAR_FEATURE, TUSB_REQ_FEATURE_EDPT_HALT, EP_OUT);
  TEST_ASSERT_EQUAL(1, clear_stall_count);
  TEST_ASSERT_EQUAL_HEX8(EP_IN, clear_stall_ep[0]);
  TEST_ASSERT_FALSE(tuh_msc_ready(DADDR));

  complete_ctrl(XFER_RESULT_SUCCESS);
  TEST_ASSERT_EQUAL(3, ctrl_count);
  TEST_ASSERT_EQUAL(2, clear_stall_count);
  TEST_ASSERT_EQUAL_HEX8(EP_OUT, clear_stall_ep[1]);
  TEST_ASSERT_EQUAL(1, reset_cb_count);
  TEST_ASSERT_TRUE(reset_success);
  TEST_ASSERT_EQUAL(0, xfer_count);
  TEST_ASSERT_TRUE(tuh_msc_mounted(DADDR));
  TEST_ASSERT_EQUAL(1024, tuh_msc_get_block_count(DADDR, 0));
  assert_unfenced();
}

void test_msc_host_reset_idle_interface_then_command_from_callback(void) {
  enumerate();
  reset_counters();

  TEST_ASSERT_TRUE(tuh_msc_reset(DADDR, reset_then_command, RESET_ARG));
  TEST_ASSERT_FALSE(tuh_msc_ready(DADDR)); // fenced while it runs
  complete_recovery();

  TEST_ASSERT_TRUE(reset_success);
  TEST_ASSERT_TRUE(retry_submitted);
  TEST_ASSERT_EQUAL(1, xfer_count);
  TEST_ASSERT_EQUAL_HEX8(EP_OUT, xfer_ep[0]);
}

void test_msc_host_reset_without_callback(void) {
  enumerate_and_fence();

  TEST_ASSERT_TRUE(tuh_msc_reset(DADDR, NULL, 0));
  complete_recovery();

  assert_unfenced();
}

// any failed step ends the attempt fenced, with no further request
typedef struct {
  uint8_t       ok_steps;     // requests completed successfully first
  xfer_result_t result;       // how the next request completes; XFER_RESULT_INVALID: it fails to submit
  uint8_t       toggle_fail;  // endpoint whose host toggle reset fails, 0 for none
  uint8_t       requests;     // submissions attempted
  uint8_t       toggle_resets;
} reset_failure_t;

static void check_reset_failure(const reset_failure_t* tc) {
  remount_and_fence();
  clear_stall_fail_ep = tc->toggle_fail;
  if (tc->result == XFER_RESULT_INVALID) {
    ctrl_fail_index = tc->ok_steps; // the request submitted after them
  }

  TEST_ASSERT_TRUE(tuh_msc_reset(DADDR, record_reset, RESET_ARG));
  for (uint8_t i = 0; i < tc->ok_steps; i++) {
    complete_ctrl(XFER_RESULT_SUCCESS);
  }
  if (tc->result != XFER_RESULT_INVALID) {
    complete_ctrl(tc->result);
  }

  ctrl_fail_index     = UINT8_MAX;
  clear_stall_fail_ep = 0;
  TEST_ASSERT_EQUAL(tc->requests, ctrl_count);
  TEST_ASSERT_EQUAL(tc->toggle_resets, clear_stall_count);
  assert_reset_failed();
}

void test_msc_host_reset_request_failure_keeps_fence(void) {
  const reset_failure_t cases[] = {
    {0, XFER_RESULT_STALLED, 0, 1, 0}, // Bulk-Only Mass Storage Reset
    {0, XFER_RESULT_FAILED,  0, 1, 0},
    {0, XFER_RESULT_TIMEOUT, 0, 1, 0},
    {1, XFER_RESULT_STALLED, 0, 2, 0}, // Clear HALT Bulk-In
    {1, XFER_RESULT_FAILED,  0, 2, 0},
    {1, XFER_RESULT_TIMEOUT, 0, 2, 0},
    {2, XFER_RESULT_STALLED, 0, 3, 1}, // Clear HALT Bulk-Out
    {2, XFER_RESULT_FAILED,  0, 3, 1},
    {2, XFER_RESULT_TIMEOUT, 0, 3, 1},
  };
  for (size_t i = 0; i < TU_ARRAY_SIZE(cases); i++) {
    check_reset_failure(&cases[i]);
  }
}

void test_msc_host_reset_submit_failure_keeps_fence(void) {
  const reset_failure_t cases[] = {
    {1, XFER_RESULT_INVALID, 0, 2, 0}, // Clear HALT Bulk-In not submitted
    {2, XFER_RESULT_INVALID, 0, 3, 1}, // Clear HALT Bulk-Out not submitted
  };
  for (size_t i = 0; i < TU_ARRAY_SIZE(cases); i++) {
    check_reset_failure(&cases[i]);
  }
}

// a host controller that cannot reset the toggle fails the attempt, even after the device took the request
void test_msc_host_reset_in_toggle_reset_failure_keeps_fence(void) {
  const reset_failure_t tc = {1, XFER_RESULT_SUCCESS, EP_IN, 2, 1};
  check_reset_failure(&tc);
}

void test_msc_host_reset_out_toggle_reset_failure_keeps_fence(void) {
  const reset_failure_t tc = {2, XFER_RESULT_SUCCESS, EP_OUT, 3, 2};
  check_reset_failure(&tc);
}

// a healthy interface fenced by an accepted attempt that then fails
void test_msc_host_reset_idle_interface_failure_fences(void) {
  enumerate();
  reset_counters();

  TEST_ASSERT_TRUE(tuh_msc_reset(DADDR, record_reset, RESET_ARG));
  complete_ctrl(XFER_RESULT_STALLED);

  assert_reset_failed();
}

void test_msc_host_reset_first_submit_rejected_keeps_state(void) {
  enumerate();
  reset_counters();
  ctrl_fail_index = 0;

  TEST_ASSERT_FALSE(tuh_msc_reset(DADDR, record_reset, RESET_ARG));
  TEST_ASSERT_EQUAL(0, reset_cb_count);
  ctrl_fail_index = UINT8_MAX;
  assert_unfenced(); // still healthy

  remount_and_fence();
  ctrl_fail_index = 0;
  TEST_ASSERT_FALSE(tuh_msc_reset(DADDR, record_reset, RESET_ARG));
  ctrl_fail_index = UINT8_MAX;
  TEST_ASSERT_EQUAL(0, reset_cb_count);
  assert_fenced(0, 0);

  // and may be retried
  TEST_ASSERT_TRUE(tuh_msc_reset(DADDR, record_reset, RESET_ARG));
  complete_recovery();
  TEST_ASSERT_TRUE(reset_success);
}

// a failed attempt can be retried, also straight from its callback
void test_msc_host_reset_retry_from_failure_callback(void) {
  enumerate_and_fence();

  TEST_ASSERT_TRUE(tuh_msc_reset(DADDR, reset_then_retry, RESET_ARG));
  complete_ctrl(XFER_RESULT_STALLED);
  TEST_ASSERT_EQUAL(1, reset_cb_count);
  TEST_ASSERT_FALSE(reset_success);
  TEST_ASSERT_TRUE(reset_retry_started);
  TEST_ASSERT_EQUAL(2, ctrl_count);
  assert_request(1, 0x21, MSC_REQ_RESET, 0, ITF_NUM);

  complete_recovery();
  TEST_ASSERT_EQUAL(2, reset_cb_count);
  TEST_ASSERT_TRUE(reset_success);
  assert_unfenced();
}

void test_msc_host_reset_refused(void) {
  TEST_ASSERT_FALSE(tuh_msc_reset(DADDR, record_reset, RESET_ARG)); // configured, not yet mounted
  TEST_ASSERT_EQUAL(1, ctrl_count);                                  // Get Max LUN only

  enumerate();
  reset_counters();
  TEST_ASSERT_FALSE(tuh_msc_reset(0, record_reset, RESET_ARG));
  TEST_ASSERT_FALSE(tuh_msc_reset(CFG_TUH_DEVICE_MAX + 1, record_reset, RESET_ARG));

  const uint8_t eps[] = {EP_IN, EP_OUT};
  for (size_t i = 0; i < TU_ARRAY_SIZE(eps); i++) {
    busy_ep = eps[i];
    TEST_ASSERT_FALSE(tuh_msc_reset(DADDR, record_reset, RESET_ARG));
  }
  busy_ep = 0;

  // a command in flight
  start_no_data_cb(record_complete);
  TEST_ASSERT_FALSE(tuh_msc_reset(DADDR, record_reset, RESET_ARG));
  reply_csw_passed();

  // a recovery in flight
  TEST_ASSERT_TRUE(tuh_msc_reset(DADDR, record_reset, RESET_ARG));
  TEST_ASSERT_FALSE(tuh_msc_reset(DADDR, record_reset, RESET_ARG));
  TEST_ASSERT_EQUAL(1, ctrl_count);
  TEST_ASSERT_EQUAL(0, reset_cb_count);
}

// a CBW that fails to submit leaves the interface idle, so a recovery may still start
void test_msc_host_reset_after_cbw_submit_fail(void) {
  enumerate();
  reset_counters();
  xfer_fail_index = 0;
  const msc_cbw_t cbw = {.signature = MSC_CBW_SIGNATURE};
  TEST_ASSERT_FALSE(tuh_msc_scsi_command(DADDR, &cbw, NULL, record_complete, 0));
  xfer_fail_index = UINT8_MAX;

  TEST_ASSERT_TRUE(tuh_msc_reset(DADDR, record_reset, RESET_ARG));
}

// unplugged mid-recovery: usbh fails the request after closing the driver; no callback, the umount callback reports it
void test_msc_host_reset_unplug_discards(void) {
  enumerate_and_fence();
  TEST_ASSERT_TRUE(tuh_msc_reset(DADDR, record_reset, RESET_ARG));
  complete_ctrl(XFER_RESULT_SUCCESS);

  unplug();
  complete_ctrl(XFER_RESULT_FAILED);

  TEST_ASSERT_EQUAL(0, reset_cb_count);
  TEST_ASSERT_EQUAL(2, ctrl_count);
  TEST_ASSERT_EQUAL(0, clear_stall_count);
}

// a request of the earlier open completing after the address was re-enumerated must not advance the new
// interface's own recovery
void test_msc_host_reset_stale_completion_after_reopen(void) {
  enumerate_and_fence();
  TEST_ASSERT_TRUE(tuh_msc_reset(DADDR, record_reset, RESET_ARG));
  tuh_xfer_t stale = ctrl_xfer;

  unplug();
  set_config_count = 0;
  mount_bot_interface();
  enumerate();
  reset_counters();
  TEST_ASSERT_TRUE(tuh_msc_reset(DADDR, record_reset, RESET_ARG));

  stale.result = XFER_RESULT_SUCCESS;
  stale.complete_cb(&stale);
  TEST_ASSERT_EQUAL(0, reset_cb_count);
  TEST_ASSERT_EQUAL(1, ctrl_count);

  complete_recovery();
  TEST_ASSERT_EQUAL(1, reset_cb_count);
  TEST_ASSERT_TRUE(reset_success);
  assert_unfenced();
}

static void deliver_ctrl_success(void) {
  complete_ctrl(XFER_RESULT_SUCCESS);
}

// an application driving tuh_task() from its umount callback delivers the recovery's queued last request: it must
// neither call back nor let the callback start a command on the closing interface
void test_msc_host_reset_completion_from_umount_callback_discarded(void) {
  enumerate_and_fence();
  TEST_ASSERT_TRUE(tuh_msc_reset(DADDR, reset_then_command, RESET_ARG));
  complete_ctrl(XFER_RESULT_SUCCESS); // Bulk-Only Mass Storage Reset
  complete_ctrl(XFER_RESULT_SUCCESS); // Clear HALT Bulk-In

  umount_hook = deliver_ctrl_success;
  unplug();

  TEST_ASSERT_EQUAL(0, reset_cb_count);
  TEST_ASSERT_FALSE(retry_submitted);
  TEST_ASSERT_EQUAL(0, xfer_count);
  TEST_ASSERT_EQUAL(1, clear_stall_count);
}

// A command and a recovery are each admitted only with the Bulk-Out claim. The hooks below run inside the other
// one's claim, as a task preempting it right after its own checks would.
static void preempt_reset(void) {
  TEST_ASSERT_TRUE(tuh_msc_reset(DADDR, record_reset, RESET_ARG));
}

static void preempt_command(void) {
  TEST_ASSERT_TRUE(tuh_msc_test_unit_ready(DADDR, 0, record_complete, 0));
}

static void unexpected_reset(uint8_t daddr, bool success, uintptr_t arg) {
  (void) daddr;
  (void) success;
  (void) arg;
  TEST_FAIL_MESSAGE("a refused recovery called back");
}

// the command is refused; the recovery keeps its stage and callback and lifts the fence
void test_msc_host_reset_preempts_command_admission(void) {
  enumerate();
  reset_counters();

  claim_preempt = preempt_reset;
  TEST_ASSERT_FALSE(tuh_msc_test_unit_ready(DADDR, 0, record_complete, 0));
  TEST_ASSERT_EQUAL(0, xfer_count);
  TEST_ASSERT_EQUAL(1, ctrl_count);

  complete_recovery();
  TEST_ASSERT_EQUAL(1, reset_cb_count);
  TEST_ASSERT_TRUE(reset_success);
  TEST_ASSERT_EQUAL(0, complete_count);
  assert_unfenced();
}

// the later recovery is refused and cannot replace the running one's callback or argument
void test_msc_host_reset_preempts_reset_admission(void) {
  enumerate_and_fence();

  claim_preempt = preempt_reset;
  TEST_ASSERT_FALSE(tuh_msc_reset(DADDR, unexpected_reset, 0));
  TEST_ASSERT_EQUAL(1, ctrl_count);

  complete_recovery();
  TEST_ASSERT_EQUAL(1, reset_cb_count);
  TEST_ASSERT_TRUE(reset_success);
  assert_unfenced();
}

// the recovery is refused and frees the claim: the command completes, and a recovery may start after it
void test_msc_host_command_preempts_reset_admission(void) {
  enumerate();
  reset_counters();

  claim_preempt = preempt_command;
  TEST_ASSERT_FALSE(tuh_msc_reset(DADDR, unexpected_reset, 0));
  TEST_ASSERT_EQUAL(0, ctrl_count);
  TEST_ASSERT_EQUAL(1, xfer_count);

  TEST_ASSERT_TRUE(msch_xfer_cb(DADDR, EP_OUT, XFER_RESULT_SUCCESS, sizeof(msc_cbw_t)));
  reply_csw_passed();
  TEST_ASSERT_EQUAL(1, complete_count);
  TEST_ASSERT_TRUE(tuh_msc_reset(DADDR, record_reset, RESET_ARG));
}

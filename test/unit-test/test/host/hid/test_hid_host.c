/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 TinyUSB contributors
 * SPDX-License-Identifier: MIT
 */

#include "unity.h"
#include "hid_host_test.h"

TEST_SOURCE_FILE("hid_host.c")

enum {
  HID_DEV_ADDR = 1,
  HID_ITF_NUM  = 0,
  HID_EP_IN    = 0x81,
  HID_EP_SIZE  = 8,
};

#define TEST_HID_ITF_DESC                                                                                    \
  9, TUSB_DESC_INTERFACE, HID_ITF_NUM, 0, 1, TUSB_CLASS_HID, HID_SUBCLASS_BOOT, HID_ITF_PROTOCOL_KEYBOARD, 0

#define TEST_HID_DESC       9, HID_DESC_TYPE_HID, U16_TO_U8S_LE(0x0111), 0, 1, HID_DESC_TYPE_REPORT, U16_TO_U8S_LE(63)

#define TEST_HID_EP_IN_DESC 7, TUSB_DESC_ENDPOINT, HID_EP_IN, TUSB_XFER_INTERRUPT, U16_TO_U8S_LE(HID_EP_SIZE), 10

// Boot keyboard: interface, HID descriptor and one interrupt IN endpoint
static const uint8_t hid_itf_desc[] = {TEST_HID_ITF_DESC, TEST_HID_DESC, TEST_HID_EP_IN_DESC};

static uint8_t *edpt_xfer_buffer; // buffer the driver queued on the IN endpoint

static uint8_t        report_cb_count;
static uint8_t        report_cb_daddr;
static uint8_t        report_cb_idx;
static const uint8_t *report_cb_report;
static uint16_t       report_cb_len;

static uint8_t       failed_cb_count;
static uint8_t       failed_cb_daddr;
static uint8_t       failed_cb_idx;
static xfer_result_t failed_cb_result;

// last control request the driver queued, to complete it by hand
static tusb_control_request_t ctrl_setup;
static tuh_xfer_cb_t          ctrl_complete_cb;
static uintptr_t              ctrl_user_data;

static uint8_t        mount_cb_count;
static const uint8_t *mount_cb_desc;
static uint16_t       mount_cb_len;

static uint8_t config_complete_count;
static uint8_t config_complete_itf;

static bool ctrl_xfer_reject; // control requests fail to queue, as when usbh's pending queue is full
static uint8_t enum_buf[64];

//--------------------------------------------------------------------+
// Host stack functions called by hid_host.c
//--------------------------------------------------------------------+
bool tuh_edpt_open(uint8_t daddr, const tusb_desc_endpoint_t *desc_ep) {
  (void)daddr;
  (void)desc_ep;
  return true;
}

bool tuh_edpt_abort_xfer(uint8_t daddr, uint8_t ep_addr) {
  (void)daddr;
  (void)ep_addr;
  return true;
}

bool tuh_control_xfer(tuh_xfer_t *xfer) {
  if (ctrl_xfer_reject) {
    return false;
  }
  ctrl_setup       = *xfer->setup;
  ctrl_complete_cb = xfer->complete_cb;
  ctrl_user_data   = xfer->user_data;
  return true;
}

bool tuh_descriptor_get_hid_report(uint8_t daddr, uint8_t itf_num, uint8_t desc_type, uint8_t index, void *buffer,
                                   uint16_t len, tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  TEST_ASSERT_EQUAL(HID_DEV_ADDR, daddr);
  TEST_ASSERT_EQUAL_PTR(usbh_get_enum_buf(), buffer);
  if (ctrl_xfer_reject) {
    return false;
  }
  ctrl_setup = (tusb_control_request_t){
    .bmRequestType = 0x81, // IN, standard, interface
    .bRequest      = TUSB_REQ_GET_DESCRIPTOR,
    .wValue        = tu_htole16(TU_U16(desc_type, index)),
    .wIndex        = tu_htole16(itf_num),
    .wLength       = tu_htole16(len)};
  ctrl_complete_cb = complete_cb;
  ctrl_user_data   = user_data;
  return true;
}

uint8_t *usbh_get_enum_buf(void) {
  return enum_buf;
}

void usbh_driver_set_config_complete(uint8_t dev_addr, uint8_t itf_num) {
  TEST_ASSERT_EQUAL(HID_DEV_ADDR, dev_addr);
  config_complete_count++;
  config_complete_itf = itf_num;
}

bool usbh_edpt_claim(uint8_t dev_addr, uint8_t ep_addr) {
  (void)dev_addr;
  (void)ep_addr;
  return true;
}

bool usbh_edpt_release(uint8_t dev_addr, uint8_t ep_addr) {
  (void)dev_addr;
  (void)ep_addr;
  return true;
}

bool usbh_edpt_busy(uint8_t dev_addr, uint8_t ep_addr) {
  (void)dev_addr;
  (void)ep_addr;
  return false;
}

bool usbh_edpt_xfer_with_callback(uint8_t dev_addr, uint8_t ep_addr, uint8_t *buffer, uint16_t total_bytes,
                                  tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  (void)dev_addr;
  (void)total_bytes;
  (void)complete_cb;
  (void)user_data;
  if (ep_addr == HID_EP_IN) {
    edpt_xfer_buffer = buffer;
  }
  return true;
}

//--------------------------------------------------------------------+
// Application callback
//--------------------------------------------------------------------+
void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t idx, const uint8_t *report_desc, uint16_t desc_len) {
  TEST_ASSERT_EQUAL(HID_DEV_ADDR, dev_addr);
  TEST_ASSERT_EQUAL(0, idx);
  mount_cb_count++;
  mount_cb_desc = report_desc;
  mount_cb_len  = desc_len;
}

void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t idx, const uint8_t *report, uint16_t len) {
  report_cb_count++;
  report_cb_daddr  = dev_addr;
  report_cb_idx    = idx;
  report_cb_report = report;
  report_cb_len    = len;
}

void tuh_hid_report_failed_cb(uint8_t dev_addr, uint8_t idx, xfer_result_t result) {
  failed_cb_count++;
  failed_cb_daddr  = dev_addr;
  failed_cb_idx    = idx;
  failed_cb_result = result;
}

//--------------------------------------------------------------------+
// Tests
//--------------------------------------------------------------------+
void setUp(void) {
  edpt_xfer_buffer = NULL;
  report_cb_count  = 0;
  report_cb_daddr  = 0;
  report_cb_idx    = TUSB_INDEX_INVALID_8;
  report_cb_report = NULL;
  report_cb_len    = 0xFFFF;
  failed_cb_count  = 0;
  failed_cb_daddr  = 0;
  failed_cb_idx    = TUSB_INDEX_INVALID_8;
  failed_cb_result = XFER_RESULT_SUCCESS;
  ctrl_complete_cb = NULL;
  ctrl_user_data   = 0;
  mount_cb_count   = 0;
  mount_cb_desc    = NULL;
  mount_cb_len     = 0xFFFF;
  config_complete_count = 0;
  config_complete_itf   = TUSB_INDEX_INVALID_8;
  ctrl_xfer_reject      = false;

  TEST_ASSERT_TRUE(hidh_init());
  TEST_ASSERT_EQUAL(sizeof(hid_itf_desc),
                    hidh_open(0, HID_DEV_ADDR, (const tusb_desc_interface_t *)hid_itf_desc, sizeof(hid_itf_desc)));

  // queue the IN transfer the way an application does
  TEST_ASSERT_TRUE(tuh_hid_receive_report(HID_DEV_ADDR, 0));
  TEST_ASSERT_NOT_NULL(edpt_xfer_buffer);
}

void tearDown(void) {
  hidh_close(HID_DEV_ADDR);
}

void test_hidh_report_received(void) {
  const uint8_t report[HID_EP_SIZE] = {0x02, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00};
  memcpy(edpt_xfer_buffer, report, sizeof(report)); // what the HCD writes

  TEST_ASSERT_TRUE(hidh_xfer_cb(HID_DEV_ADDR, HID_EP_IN, XFER_RESULT_SUCCESS, sizeof(report)));

  TEST_ASSERT_EQUAL(1, report_cb_count);
  TEST_ASSERT_EQUAL(0, failed_cb_count);
  TEST_ASSERT_EQUAL(HID_DEV_ADDR, report_cb_daddr);
  TEST_ASSERT_EQUAL(0, report_cb_idx);
  TEST_ASSERT_EQUAL_PTR(edpt_xfer_buffer, report_cb_report);
  TEST_ASSERT_EQUAL(sizeof(report), report_cb_len);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(report, report_cb_report, sizeof(report));
}

// Some devices send zero-length reports: a successful transfer of 0 bytes still passes the buffer
void test_hidh_report_received_zero_length(void) {
  TEST_ASSERT_TRUE(hidh_xfer_cb(HID_DEV_ADDR, HID_EP_IN, XFER_RESULT_SUCCESS, 0));

  TEST_ASSERT_EQUAL(1, report_cb_count);
  TEST_ASSERT_EQUAL_PTR(edpt_xfer_buffer, report_cb_report);
  TEST_ASSERT_EQUAL(0, report_cb_len);
}

// A failed transfer is no report: tuh_hid_report_failed_cb() gets its result, tuh_hid_report_received_cb() nothing,
// so neither a stale buffer nor a zero length reaches the application as data
void test_hidh_report_failed(void) {
  const xfer_result_t results[] = {XFER_RESULT_FAILED, XFER_RESULT_STALLED, XFER_RESULT_TIMEOUT, XFER_RESULT_ABORTED};

  for (uint8_t i = 0; i < TU_ARRAY_SIZE(results); i++) {
    TEST_ASSERT_TRUE(hidh_xfer_cb(HID_DEV_ADDR, HID_EP_IN, results[i], 0));

    TEST_ASSERT_EQUAL(0, report_cb_count);
    TEST_ASSERT_EQUAL(i + 1, failed_cb_count);
    TEST_ASSERT_EQUAL(HID_DEV_ADDR, failed_cb_daddr);
    TEST_ASSERT_EQUAL(0, failed_cb_idx);
    TEST_ASSERT_EQUAL(results[i], failed_cb_result);
  }
}

// Complete the control request the driver queued last with the given result
static void complete_ctrl_xfer(xfer_result_t result) {
  TEST_ASSERT_NOT_NULL(ctrl_complete_cb);
  tuh_xfer_t xfer = {
    .daddr     = HID_DEV_ADDR,
    .ep_addr   = 0,
    .result    = result,
    .setup     = &ctrl_setup,
    .user_data = ctrl_user_data,
  };
  ctrl_complete_cb(&xfer);
}

// Run set_config through SET_IDLE and SET_PROTOCOL up to the queued report descriptor request
static void set_config_to_report_desc(void) {
  hidh_set_config(HID_DEV_ADDR, HID_ITF_NUM);
  TEST_ASSERT_EQUAL(HID_REQ_CONTROL_SET_IDLE, ctrl_setup.bRequest);
  complete_ctrl_xfer(XFER_RESULT_STALLED); // SET_IDLE is optional
  TEST_ASSERT_EQUAL(HID_REQ_CONTROL_SET_PROTOCOL, ctrl_setup.bRequest);
  complete_ctrl_xfer(XFER_RESULT_SUCCESS);
  TEST_ASSERT_EQUAL(TUSB_REQ_GET_DESCRIPTOR, ctrl_setup.bRequest);
  TEST_ASSERT_EQUAL(0, config_complete_count);
}

void test_hidh_set_config_report_desc(void) {
  set_config_to_report_desc();
  complete_ctrl_xfer(XFER_RESULT_SUCCESS);

  TEST_ASSERT_EQUAL(1, mount_cb_count);
  TEST_ASSERT_NOT_NULL(mount_cb_desc);
  TEST_ASSERT_EQUAL_PTR(usbh_get_enum_buf(), mount_cb_desc);
  TEST_ASSERT_EQUAL(63, mount_cb_len);
  TEST_ASSERT_EQUAL(1, config_complete_count);
  TEST_ASSERT_EQUAL(HID_ITF_NUM, config_complete_itf);
}

// A failed report descriptor request mounts the interface without a descriptor, so enumeration still completes
void test_hidh_set_config_report_desc_failed(void) {
  const xfer_result_t results[] = {XFER_RESULT_FAILED, XFER_RESULT_STALLED, XFER_RESULT_TIMEOUT};

  for (uint8_t i = 0; i < TU_ARRAY_SIZE(results); i++) {
    set_config_to_report_desc();
    complete_ctrl_xfer(results[i]);

    TEST_ASSERT_EQUAL(1, mount_cb_count);
    TEST_ASSERT_NULL(mount_cb_desc);
    TEST_ASSERT_EQUAL(0, mount_cb_len);
    TEST_ASSERT_EQUAL(1, config_complete_count);
    TEST_ASSERT_EQUAL(HID_ITF_NUM, config_complete_itf);

    mount_cb_count        = 0;
    config_complete_count = 0;
  }
}

// A request that fails to queue gets no completion callback: the interface mounts without a descriptor right away
void test_hidh_set_config_report_desc_rejected(void) {
  hidh_set_config(HID_DEV_ADDR, HID_ITF_NUM);
  complete_ctrl_xfer(XFER_RESULT_STALLED); // SET_IDLE
  TEST_ASSERT_EQUAL(HID_REQ_CONTROL_SET_PROTOCOL, ctrl_setup.bRequest);
  ctrl_xfer_reject = true;
  complete_ctrl_xfer(XFER_RESULT_SUCCESS);

  TEST_ASSERT_EQUAL(1, mount_cb_count);
  TEST_ASSERT_NULL(mount_cb_desc);
  TEST_ASSERT_EQUAL(0, mount_cb_len);
  TEST_ASSERT_EQUAL(1, config_complete_count);
  TEST_ASSERT_EQUAL(HID_ITF_NUM, config_complete_itf);
}

void test_hidh_set_config_set_protocol_rejected(void) {
  hidh_set_config(HID_DEV_ADDR, HID_ITF_NUM);
  TEST_ASSERT_EQUAL(HID_REQ_CONTROL_SET_IDLE, ctrl_setup.bRequest);
  ctrl_xfer_reject = true;
  complete_ctrl_xfer(XFER_RESULT_STALLED); // SET_IDLE completes, SET_PROTOCOL fails to queue

  TEST_ASSERT_EQUAL(1, mount_cb_count);
  TEST_ASSERT_NULL(mount_cb_desc);
  TEST_ASSERT_EQUAL(0, mount_cb_len);
  TEST_ASSERT_EQUAL(1, config_complete_count);
  TEST_ASSERT_EQUAL(HID_ITF_NUM, config_complete_itf);
}

void test_hidh_set_config_set_idle_rejected(void) {
  ctrl_xfer_reject = true;
  hidh_set_config(HID_DEV_ADDR, HID_ITF_NUM);

  TEST_ASSERT_EQUAL(1, mount_cb_count);
  TEST_ASSERT_NULL(mount_cb_desc);
  TEST_ASSERT_EQUAL(1, config_complete_count);
  TEST_ASSERT_EQUAL(HID_ITF_NUM, config_complete_itf);
}

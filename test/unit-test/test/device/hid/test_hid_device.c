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
TEST_SOURCE_FILE("hid_device.c")

#include "mock_dcd.h"
#include "usbd_ep0_test.h"

enum { EP_HID_IN = 0x81, EP_HID_OUT = 0x01 };
enum { REPORT_ID = 3 };
enum { OUT_MPS = 15 }; // not a multiple of 4: the stub DCD refuses a multi-packet transfer, as wch usbhs does

static const uint8_t desc_report[] = { TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(REPORT_ID)) };

#define CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN)
static const uint8_t desc_config[] = {
  TUD_CONFIG_DESCRIPTOR(1, 1, 0, CONFIG_TOTAL_LEN, 0, 100),
  TUD_HID_DESCRIPTOR(0, 0, HID_ITF_PROTOCOL_NONE, sizeof(desc_report), EP_HID_IN, CFG_TUD_HID_EP_BUFSIZE, 10)
};

#define CONFIG_INOUT_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_HID_INOUT_DESC_LEN)
static const uint8_t desc_config_inout[] = {
  TUD_CONFIG_DESCRIPTOR(1, 1, 0, CONFIG_INOUT_TOTAL_LEN, 0, 100),
  TUD_HID_INOUT_DESCRIPTOR(0, 0, HID_ITF_PROTOCOL_NONE, sizeof(desc_report), EP_HID_OUT, EP_HID_IN, OUT_MPS, 10)
};

static const uint8_t* desc_config_active;

uint32_t tusb_time_millis_api(void) { return 0; }
const uint8_t* tud_descriptor_device_cb(void) { return NULL; }
const uint8_t* tud_descriptor_configuration_cb(uint8_t index) { (void) index; return desc_config_active; }
const uint16_t* tud_descriptor_string_cb(uint8_t index, uint16_t langid) { (void) index; (void) langid; return NULL; }
const uint8_t* tud_hid_descriptor_report_cb(uint8_t instance) { (void) instance; return desc_report; }

static const uint8_t report_payload[] = { 0x11, 0x22, 0x33, 0x44 };
static uint16_t get_report_ret;
static uint8_t get_report_id;
static uint16_t get_report_reqlen;

uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer,
                               uint16_t reqlen) {
  (void) instance; (void) report_type;
  get_report_id = report_id;
  get_report_reqlen = reqlen;
  memcpy(buffer, report_payload, get_report_ret);
  return get_report_ret;
}

static unsigned set_report_count;
static hid_report_type_t set_report_type;
static uint16_t set_report_len;
static uint8_t set_report_data[CFG_TUD_HID_EP_BUFSIZE];

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer,
                           uint16_t bufsize) {
  (void) instance; (void) report_id;
  set_report_count++;
  set_report_type = report_type;
  set_report_len = bufsize;
  memcpy(set_report_data, buffer, bufsize);
}

// the OUT transfer the class armed last
static uint8_t* epout_buf;
static uint16_t epout_len;
static uint16_t epout_sent_len; // the armed length of the packet the host sent last
static bool epout_refuse_all;
static bool epout_accept_multi; // a DCD that takes the full-size transfer

static bool edpt_xfer_stub(uint8_t rhport, uint8_t ep, uint8_t* buf, uint16_t len, bool is_isr, int n) {
  if (tu_edpt_number(ep) == 0) {
    return ep0_test_edpt_xfer(rhport, ep, buf, len, is_isr, n);
  }
  TEST_ASSERT_EQUAL_HEX8(EP_HID_OUT, ep);
  if (epout_refuse_all || (len > OUT_MPS && !epout_accept_multi)) {
    return false;
  }
  TEST_ASSERT_EQUAL_MESSAGE(0, (uintptr_t) buf & 3u, "OUT packet buffer not 4-byte aligned");
  epout_buf = buf;
  epout_len = len;
  return true;
}

// the host sends one OUT packet into the armed transfer
static void host_out_packet(const uint8_t* data, uint16_t len) {
  TEST_ASSERT_NOT_NULL_MESSAGE(epout_buf, "no OUT transfer armed");
  TEST_ASSERT_LESS_OR_EQUAL(epout_len, len);
  memcpy(epout_buf, data, len);
  epout_buf = NULL;
  epout_sent_len = epout_len;
  dcd_event_xfer_complete(RHPORT, EP_HID_OUT, len, XFER_RESULT_SUCCESS, false);
  tud_task();
}

static void host_set_config_inout(void) {
  desc_config_active = desc_config_inout;
  host_bus_reset();
  host_set_config(1);
}

static void host_out_report(const uint8_t* report, uint16_t len) {
  for (uint16_t sent = 0; sent < len;) {
    const uint16_t pkt = tu_min16(OUT_MPS, (uint16_t) (len - sent));
    TEST_ASSERT_EQUAL(0, set_report_count);
    host_out_packet(report + sent, pkt);
    sent += pkt;
  }
}

static const tusb_control_request_t req_get_report = {
  .bmRequestType = 0xA1, .bRequest = HID_REQ_CONTROL_GET_REPORT,
  .wValue = (HID_REPORT_TYPE_INPUT << 8) | REPORT_ID, .wIndex = 0, .wLength = 16 };

void setUp(void) {
  ep0_test_init();
  dcd_edpt_xfer_Stub(edpt_xfer_stub);
  get_report_ret = 0;
  get_report_id = 0;
  get_report_reqlen = 0;
  set_report_count = 0;
  set_report_len = 0;
  epout_buf = NULL;
  epout_len = 0;
  epout_refuse_all = false;
  epout_accept_multi = false;
  desc_config_active = desc_config;
  host_bus_reset();
  host_set_config(1);
}

void tearDown(void) {}

// a zero-length report stalls rather than send the report ID byte alone
void test_get_report_zero_length_stalls(void) {
  get_report_ret = 0;
  const unsigned queued = ep0_count;
  host_setup_stalled(&req_get_report);
  TEST_ASSERT_EQUAL(REPORT_ID, get_report_id);
  TEST_ASSERT_EQUAL(queued, ep0_count); // no data stage
}

void test_get_report_prefixes_report_id(void) {
  get_report_ret = sizeof(report_payload);
  host_setup_ok(&req_get_report);
  TEST_ASSERT_EQUAL(REPORT_ID, get_report_id);
  TEST_ASSERT_EQUAL(req_get_report.wLength - 1, get_report_reqlen);

  const ep0_xfer_t* x = host_complete_ep0(EP_CTRL_IN, 1 + sizeof(report_payload));
  TEST_ASSERT_EQUAL_HEX8(REPORT_ID, x->data[0]);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(report_payload, x->data + 1, sizeof(report_payload));
  host_complete_ep0(EP_CTRL_OUT, 0);
}

// a report longer than the OUT packet is gathered and delivered once, ended by a short packet
void test_out_report_gathered_until_short_packet(void) {
  host_set_config_inout();
  uint8_t report[20];
  for (unsigned i = 0; i < sizeof(report); i++) report[i] = (uint8_t) (0xA0 + i);

  host_out_report(report, sizeof(report));
  TEST_ASSERT_EQUAL(1, set_report_count);
  TEST_ASSERT_EQUAL(HID_REPORT_TYPE_OUTPUT, set_report_type);
  TEST_ASSERT_EQUAL(sizeof(report), set_report_len);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(report, set_report_data, sizeof(report));

  // the next report starts afresh
  set_report_count = 0;
  const uint8_t small[] = { 0x55, 0x66 };
  host_out_report(small, sizeof(small));
  TEST_ASSERT_EQUAL(1, set_report_count);
  TEST_ASSERT_EQUAL(sizeof(small), set_report_len);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(small, set_report_data, sizeof(small));
}

// a report that fills the buffer ends without a short packet, the last packet armed to the space left
void test_out_report_gathered_to_full_buffer(void) {
  host_set_config_inout();
  uint8_t report[CFG_TUD_HID_EP_BUFSIZE];
  for (unsigned i = 0; i < sizeof(report); i++) report[i] = (uint8_t) i;

  host_out_report(report, sizeof(report));
  TEST_ASSERT_EQUAL(CFG_TUD_HID_EP_BUFSIZE % OUT_MPS, epout_sent_len);
  TEST_ASSERT_EQUAL(1, set_report_count);
  TEST_ASSERT_EQUAL(sizeof(report), set_report_len);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(report, set_report_data, sizeof(report));
  TEST_ASSERT_EQUAL(OUT_MPS, epout_len); // re-armed for the next report
}

// a DCD that accepts the full-size transfer keeps one transfer per report, armed at the buffer start
void test_out_report_full_size_transfer_unchanged(void) {
  epout_accept_multi = true;
  host_set_config_inout();
  TEST_ASSERT_EQUAL(CFG_TUD_HID_EP_BUFSIZE, epout_len);
  uint8_t* const epout_base = epout_buf;

  uint8_t report[CFG_TUD_HID_EP_BUFSIZE];
  for (unsigned i = 0; i < sizeof(report); i++) report[i] = (uint8_t) (0x30 + i);

  host_out_packet(report, 20);
  TEST_ASSERT_EQUAL(1, set_report_count);
  TEST_ASSERT_EQUAL(20, set_report_len);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(report, set_report_data, 20);
  TEST_ASSERT_EQUAL_PTR(epout_base, epout_buf);
  TEST_ASSERT_EQUAL(CFG_TUD_HID_EP_BUFSIZE, epout_len);

  host_out_packet(report, sizeof(report));
  TEST_ASSERT_EQUAL(2, set_report_count);
  TEST_ASSERT_EQUAL(sizeof(report), set_report_len);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(report, set_report_data, sizeof(report));
  TEST_ASSERT_EQUAL_PTR(epout_base, epout_buf);
  TEST_ASSERT_EQUAL(CFG_TUD_HID_EP_BUFSIZE, epout_len);
}

// an OUT endpoint the DCD cannot arm at all fails the configuration rather than leave it deaf
void test_set_config_stalls_when_epout_cannot_arm(void) {
  epout_refuse_all = true;
  desc_config_active = desc_config_inout;
  host_bus_reset();
  const tusb_control_request_t req = { .bmRequestType = 0x00, .bRequest = TUSB_REQ_SET_CONFIGURATION, .wValue = 1 };
  host_setup_stalled(&req);
  TEST_ASSERT_FALSE(tud_mounted());
}

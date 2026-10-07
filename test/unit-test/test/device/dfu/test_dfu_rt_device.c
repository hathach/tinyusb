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
TEST_SOURCE_FILE("dfu_rt_device.c")

#include "mock_dcd.h"

enum { RHPORT = 0, EP_CTRL_OUT = 0x00, EP_CTRL_IN = 0x80 };

#define CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_DFU_RT_DESC_LEN)
#define DESC_CONFIG(_attr) { \
  TUD_CONFIG_DESCRIPTOR(1, 1, 0, CONFIG_TOTAL_LEN, 0, 100), \
  TUD_DFU_RT_DESCRIPTOR(0, 0, _attr, 1000, 64) }

static const uint8_t desc_config_will_detach[] = DESC_CONFIG(DFU_ATTR_CAN_DOWNLOAD | DFU_ATTR_WILL_DETACH);
static const uint8_t desc_config_host_reset[]  = DESC_CONFIG(DFU_ATTR_CAN_DOWNLOAD);
static const uint8_t* desc_config;

uint32_t tusb_time_millis_api(void) { return 0; }
const uint8_t* tud_descriptor_device_cb(void) { return NULL; }
const uint8_t* tud_descriptor_configuration_cb(uint8_t index) { (void) index; return desc_config; }
const uint16_t* tud_descriptor_string_cb(uint8_t index, uint16_t langid) { (void) index; (void) langid; return NULL; }

static unsigned reboot_count;
void tud_dfu_runtime_reboot_to_dfu_cb(void) { reboot_count++; }

// EP0 transfers the stack queued, with IN data snapshotted at submission
typedef struct {
  uint8_t ep;
  uint16_t len;
  uint8_t data[CFG_TUD_ENDPOINT0_SIZE];
} ep0_xfer_t;
static ep0_xfer_t ep0_log[16];
static unsigned ep0_count;
static unsigned ep0_cursor;
static bool ep0_stalled;

static bool stub_edpt_xfer(uint8_t rhport, uint8_t ep, uint8_t* buf, uint16_t len, bool is_isr, int n) {
  (void) rhport; (void) is_isr; (void) n;
  TEST_ASSERT_EQUAL(0, tu_edpt_number(ep));
  TEST_ASSERT_LESS_THAN(TU_ARRAY_SIZE(ep0_log), ep0_count);
  ep0_xfer_t* x = &ep0_log[ep0_count++];
  x->ep = ep;
  x->len = len;
  if (tu_edpt_dir(ep) == TUSB_DIR_IN && len) memcpy(x->data, buf, len);
  return true;
}
static void stub_edpt_stall(uint8_t rhport, uint8_t ep, int n) {
  (void) rhport; (void) ep; (void) n;
  ep0_stalled = true;
}

// the next queued EP0 transfer must be `ep`/`len`; the host completes it
static const ep0_xfer_t* host_complete_ep0(uint8_t ep, uint16_t len) {
  TEST_ASSERT_LESS_THAN_MESSAGE(ep0_count, ep0_cursor, "no EP0 transfer queued");
  const ep0_xfer_t* x = &ep0_log[ep0_cursor++];
  TEST_ASSERT_EQUAL_HEX8(ep, x->ep);
  TEST_ASSERT_EQUAL(len, x->len);
  dcd_event_xfer_complete(RHPORT, ep, len, XFER_RESULT_SUCCESS, false);
  tud_task();
  return x;
}

static void host_setup(const tusb_control_request_t* req) {
  dcd_event_setup_received(RHPORT, (const uint8_t*) req, false);
  tud_task();
  TEST_ASSERT_FALSE_MESSAGE(ep0_stalled, "request stalled");
}

// no-data request: SETUP, then the IN status stage
static void host_no_data(const tusb_control_request_t* req) {
  host_setup(req);
  host_complete_ep0(EP_CTRL_IN, 0);
}

static const tusb_control_request_t req_getstate = { .bmRequestType = 0xA1, .bRequest = DFU_REQUEST_GETSTATE, .wLength = 1 };

static uint8_t host_get_state(void) {
  host_setup(&req_getstate);
  const uint8_t state = host_complete_ep0(EP_CTRL_IN, 1)->data[0];
  host_complete_ep0(EP_CTRL_OUT, 0);
  return state;
}

static uint8_t host_get_status_state(void) {
  const tusb_control_request_t req = { .bmRequestType = 0xA1, .bRequest = DFU_REQUEST_GETSTATUS,
                                       .wLength = sizeof(dfu_status_response_t) };
  host_setup(&req);
  const ep0_xfer_t* x = host_complete_ep0(EP_CTRL_IN, sizeof(dfu_status_response_t));
  const uint8_t state = ((const dfu_status_response_t*) x->data)->bState;
  host_complete_ep0(EP_CTRL_OUT, 0);
  return state;
}

static const tusb_control_request_t req_detach = { .bmRequestType = 0x21, .bRequest = DFU_REQUEST_DETACH, .wValue = 1000 };

static void host_set_config(uint8_t cfg) {
  const tusb_control_request_t req = { .bmRequestType = 0x00, .bRequest = TUSB_REQ_SET_CONFIGURATION, .wValue = cfg };
  host_no_data(&req);
}

static void bus_reset(void) {
  dcd_event_bus_reset(RHPORT, TUSB_SPEED_FULL, false);
  tud_task();
}

// queued only: the caller runs tud_task()
static void queue_unplug(void) {
  const dcd_event_t evt = { .rhport = RHPORT, .event_id = DCD_EVENT_UNPLUGGED };
  dcd_event_handler(&evt, false);
}

static void open_device(const uint8_t* config) {
  desc_config = config;
  bus_reset();
  host_set_config(1);
  reboot_count = 0;
}

void setUp(void) {
  dcd_int_disable_Ignore();
  dcd_int_enable_Ignore();
  dcd_edpt_close_all_Ignore();
  dcd_set_address_Ignore();
  dcd_edpt0_status_complete_Ignore();
  dcd_sof_enable_Ignore();
  dcd_edpt_xfer_Stub(stub_edpt_xfer);
  dcd_edpt_stall_Stub(stub_edpt_stall);
  ep0_count = ep0_cursor = 0;
  ep0_stalled = false;

  if (!tud_inited()) {
    tusb_rhport_init_t dev_init = { .role = TUSB_ROLE_DEVICE, .speed = TUSB_SPEED_AUTO };
    dcd_init_ExpectAndReturn(RHPORT, &dev_init, true);
    tusb_init(RHPORT, &dev_init);
  }
}

void tearDown(void) {}

void test_will_detach_invokes_after_status_stage(void) {
  open_device(desc_config_will_detach);
  TEST_ASSERT_EQUAL(APP_IDLE, host_get_status_state());

  host_setup(&req_detach);
  TEST_ASSERT_EQUAL(0, reboot_count); // status stage not done yet
  host_complete_ep0(EP_CTRL_IN, 0);
  TEST_ASSERT_EQUAL(1, reboot_count);
  TEST_ASSERT_EQUAL(APP_DETACH, host_get_status_state());

  bus_reset();
  TEST_ASSERT_EQUAL(1, reboot_count);
}

void test_host_reset_invokes_on_bus_reset_once(void) {
  open_device(desc_config_host_reset);
  host_no_data(&req_detach);
  TEST_ASSERT_EQUAL(0, reboot_count);
  TEST_ASSERT_EQUAL(APP_DETACH, host_get_state());
  TEST_ASSERT_EQUAL(APP_DETACH, host_get_status_state());

  // a DCD reporting both reset edges
  dcd_event_t evt = { .rhport = RHPORT, .event_id = DCD_EVENT_BUS_RESET_START };
  dcd_event_handler(&evt, false);
  dcd_event_bus_reset(RHPORT, TUSB_SPEED_FULL, false);
  tud_task();
  TEST_ASSERT_EQUAL(1, reboot_count);

  host_set_config(1);
  TEST_ASSERT_EQUAL(APP_IDLE, host_get_state());
}

void test_bus_reset_without_detach_does_nothing(void) {
  open_device(desc_config_host_reset);
  bus_reset();
  TEST_ASSERT_EQUAL(0, reboot_count);
}

void test_unplug_after_detach_does_nothing(void) {
  open_device(desc_config_host_reset);
  host_no_data(&req_detach);
  queue_unplug();
  tud_task();
  bus_reset();
  TEST_ASSERT_EQUAL(0, reboot_count);
}

// a SETUP queued ahead of the unplug marks the device connected again when processed
void test_unplug_behind_queued_setup_does_nothing(void) {
  open_device(desc_config_host_reset);
  host_no_data(&req_detach);
  dcd_event_setup_received(RHPORT, (const uint8_t*) &req_getstate, false);
  queue_unplug();
  tud_task();
  TEST_ASSERT_EQUAL(0, reboot_count);
}

void test_deconfigure_after_detach_does_nothing(void) {
  open_device(desc_config_host_reset);
  host_no_data(&req_detach);
  host_set_config(0);
  TEST_ASSERT_EQUAL(0, reboot_count);
  bus_reset();
  TEST_ASSERT_EQUAL(0, reboot_count);
}

// GETSTATE sent as OUT must stall rather than let the host write the state
void test_getstate_out_stalls(void) {
  open_device(desc_config_host_reset);
  const tusb_control_request_t req = { .bmRequestType = 0x21, .bRequest = DFU_REQUEST_GETSTATE, .wLength = 1 };
  dcd_event_setup_received(RHPORT, (const uint8_t*) &req, false);
  tud_task();
  TEST_ASSERT_TRUE(ep0_stalled);
  ep0_stalled = false;
  bus_reset();
  TEST_ASSERT_EQUAL(0, reboot_count);
}

void test_other_request_cancels_detach(void) {
  open_device(desc_config_host_reset);
  host_no_data(&req_detach);
  const tusb_control_request_t abort = { .bmRequestType = 0x21, .bRequest = DFU_REQUEST_ABORT };
  dcd_event_setup_received(RHPORT, (const uint8_t*) &abort, false);
  tud_task();
  TEST_ASSERT_TRUE(ep0_stalled);
  ep0_stalled = false;
  TEST_ASSERT_EQUAL(APP_IDLE, host_get_state());
  bus_reset();
  TEST_ASSERT_EQUAL(0, reboot_count);
}

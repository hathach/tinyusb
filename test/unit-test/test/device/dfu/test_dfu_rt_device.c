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
#include "usbd_ep0_test.h"

#define CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_DFU_RT_DESC_LEN)
#define DESC_CONFIG(_attr) { \
  TUD_CONFIG_DESCRIPTOR(1, 1, 0, CONFIG_TOTAL_LEN, 0, 100), \
  TUD_DFU_RT_DESCRIPTOR(0, 0, _attr, 1000, 64) }

static const uint8_t desc_config_will_detach[] = DESC_CONFIG(DFU_ATTR_CAN_DOWNLOAD | DFU_ATTR_WILL_DETACH);
static const uint8_t desc_config_host_reset[]  = DESC_CONFIG(DFU_ATTR_CAN_DOWNLOAD);
// non-compliant: a DFU runtime interface without its functional descriptor
static const uint8_t desc_config_no_functional[] = {
  TUD_CONFIG_DESCRIPTOR(1, 1, 0, TUD_CONFIG_DESC_LEN + 9, 0, 100),
  9, TUSB_DESC_INTERFACE, 0, 0, 0, TUD_DFU_APP_CLASS, TUD_DFU_APP_SUBCLASS, DFU_PROTOCOL_RT, 0 };
static const uint8_t* desc_config;

uint32_t tusb_time_millis_api(void) { return 0; }
const uint8_t* tud_descriptor_device_cb(void) { return NULL; }
const uint8_t* tud_descriptor_configuration_cb(uint8_t index) { (void) index; return desc_config; }
const uint16_t* tud_descriptor_string_cb(uint8_t index, uint16_t langid) { (void) index; (void) langid; return NULL; }

static unsigned reboot_count;
static bool reboot_reenters; // the callback runs tud_task() with another bus reset queued
void tud_dfu_runtime_reboot_to_dfu_cb(void) {
  reboot_count++;
  if (reboot_reenters) {
    reboot_reenters = false;
    host_bus_reset();
  }
}

static const tusb_control_request_t req_getstate = { .bmRequestType = 0xA1, .bRequest = DFU_REQUEST_GETSTATE, .wLength = 1 };

static uint8_t host_get_state(void) {
  host_setup_ok(&req_getstate);
  const uint8_t state = host_complete_ep0(EP_CTRL_IN, 1)->data[0];
  host_complete_ep0(EP_CTRL_OUT, 0);
  return state;
}

static uint8_t host_get_status_state(void) {
  const tusb_control_request_t req = { .bmRequestType = 0xA1, .bRequest = DFU_REQUEST_GETSTATUS,
                                       .wLength = sizeof(dfu_status_response_t) };
  host_setup_ok(&req);
  const ep0_xfer_t* x = host_complete_ep0(EP_CTRL_IN, sizeof(dfu_status_response_t));
  const uint8_t state = ((const dfu_status_response_t*) x->data)->bState;
  host_complete_ep0(EP_CTRL_OUT, 0);
  return state;
}

static const tusb_control_request_t req_detach = { .bmRequestType = 0x21, .bRequest = DFU_REQUEST_DETACH, .wValue = 1000 };

// queued only: the caller runs tud_task()
static void queue_unplug(void) {
  const dcd_event_t evt = { .rhport = RHPORT, .event_id = DCD_EVENT_UNPLUGGED };
  dcd_event_handler(&evt, false);
}

static void open_device(const uint8_t* config) {
  desc_config = config;
  host_bus_reset();
  host_set_config(1);
  reboot_count = 0;
}

void setUp(void) {
  ep0_test_init();
  reboot_reenters = false;
}

void tearDown(void) {}

void test_will_detach_invokes_after_status_stage(void) {
  open_device(desc_config_will_detach);
  TEST_ASSERT_EQUAL(APP_IDLE, host_get_status_state());

  host_setup_ok(&req_detach);
  TEST_ASSERT_EQUAL(0, reboot_count); // status stage not done yet
  host_complete_ep0(EP_CTRL_IN, 0);
  TEST_ASSERT_EQUAL(1, reboot_count);
  TEST_ASSERT_EQUAL(APP_DETACH, host_get_status_state());

  host_bus_reset();
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

// the detach is consumed before the callback, so a callback that runs tud_task() over another reset is not re-entered
void test_host_reset_callback_running_tud_task_invokes_once(void) {
  open_device(desc_config_host_reset);
  host_no_data(&req_detach);
  reboot_reenters = true;
  host_bus_reset();
  TEST_ASSERT_FALSE(reboot_reenters);
  TEST_ASSERT_EQUAL(1, reboot_count);
}

// The detach is armed by the status stage's XFER_COMPLETE, so it must be dequeued before the BUS_RESET: a DCD that
// queues the reset first loses the detach (usbd_reset() drops the pending status stage)
void test_host_reset_status_complete_queued_before_bus_reset(void) {
  open_device(desc_config_host_reset);
  host_setup_ok(&req_detach);
  dcd_event_xfer_complete(RHPORT, EP_CTRL_IN, 0, XFER_RESULT_SUCCESS, false);
  dcd_event_bus_reset(RHPORT, TUSB_SPEED_FULL, false);
  tud_task();
  TEST_ASSERT_EQUAL(1, reboot_count);
}

// the host resets before the status stage: DFU_DETACH never completed
void test_host_reset_bus_reset_before_status_stage_does_nothing(void) {
  open_device(desc_config_host_reset);
  host_setup_ok(&req_detach);
  host_bus_reset();
  TEST_ASSERT_EQUAL(0, reboot_count);
}

// a repeated DFU_DETACH re-arms the detach rather than stalling (a documented deviation from DFU 1.1 A.2.2)
void test_repeated_detach_rearms(void) {
  open_device(desc_config_will_detach);
  host_no_data(&req_detach);
  host_no_data(&req_detach);
  TEST_ASSERT_EQUAL(2, reboot_count);
  TEST_ASSERT_EQUAL(APP_DETACH, host_get_state());

  open_device(desc_config_host_reset);
  host_no_data(&req_detach);
  host_no_data(&req_detach);
  TEST_ASSERT_EQUAL(APP_DETACH, host_get_state());
  host_bus_reset();
  TEST_ASSERT_EQUAL(1, reboot_count);
}

// a configuration whose DFU runtime interface has no functional descriptor must not keep the previous bitWillDetach
void test_attrs_not_inherited_across_configurations(void) {
  open_device(desc_config_will_detach);
  open_device(desc_config_no_functional);
  host_no_data(&req_detach);
  TEST_ASSERT_EQUAL(0, reboot_count);
  host_bus_reset();
  TEST_ASSERT_EQUAL(1, reboot_count);
}

void test_bus_reset_without_detach_does_nothing(void) {
  open_device(desc_config_host_reset);
  host_bus_reset();
  TEST_ASSERT_EQUAL(0, reboot_count);
}

void test_unplug_after_detach_does_nothing(void) {
  open_device(desc_config_host_reset);
  host_no_data(&req_detach);
  queue_unplug();
  tud_task();
  host_bus_reset();
  TEST_ASSERT_EQUAL(0, reboot_count);
}

// a SETUP queued ahead of the unplug marks the device connected again when processed; the unplug still cancels
void test_unplug_behind_queued_setup_does_nothing(void) {
  open_device(desc_config_host_reset);
  host_no_data(&req_detach);
  dcd_event_setup_received(RHPORT, (const uint8_t*) &req_getstate, false);
  queue_unplug();
  tud_task();
  host_bus_reset();
  TEST_ASSERT_EQUAL(0, reboot_count);
}

void test_deconfigure_after_detach_does_nothing(void) {
  open_device(desc_config_host_reset);
  host_no_data(&req_detach);
  host_set_config(0);
  TEST_ASSERT_EQUAL(0, reboot_count);
  host_bus_reset();
  TEST_ASSERT_EQUAL(0, reboot_count);
}

// DFU 1.1 A.2.2: in appDETACH any DFU request other than an IN GETSTATUS or GETSTATE (or, here, an OUT DETACH) stalls
// and returns to appIDLE, cancelling the detach
void test_other_request_cancels_detach(void) {
  open_device(desc_config_host_reset);
  const tusb_control_request_t requests[] = {
    { .bmRequestType = 0x21, .bRequest = DFU_REQUEST_ABORT },
    { .bmRequestType = 0x21, .bRequest = DFU_REQUEST_GETSTATUS },
    { .bmRequestType = 0x21, .bRequest = DFU_REQUEST_GETSTATE },
    { .bmRequestType = 0xA1, .bRequest = DFU_REQUEST_DETACH },
  };
  for (size_t i = 0; i < TU_ARRAY_SIZE(requests); i++) {
    host_no_data(&req_detach);
    TEST_ASSERT_EQUAL(APP_DETACH, host_get_state());
    host_setup_stalled(&requests[i]);
    TEST_ASSERT_EQUAL(APP_IDLE, host_get_state());
  }
  host_bus_reset();
  TEST_ASSERT_EQUAL(0, reboot_count);
}

/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 TinyUSB contributors
 * SPDX-License-Identifier: MIT
 */

#include "unity.h"

// Include the production implementation to drive the enumeration state machine directly.
#include "host/usbh.c"

TEST_SOURCE_FILE("tusb.c")
TEST_SOURCE_FILE("tusb_fifo.c")

enum {
  RHPORT = 0,
};

static uint32_t now_ms;
static bool     port_connected;
static uint32_t port_reset_count;
static uint32_t setup_send_count;
static uint8_t  setup_send_daddr;

//--------------------------------------------------------------------+
// HCD stubs
//--------------------------------------------------------------------+
uint32_t tusb_time_millis_api(void) {
  return now_ms;
}

bool hcd_init(uint8_t rhport, const tusb_rhport_init_t *rh_init) {
  (void) rhport;
  (void) rh_init;
  return true;
}

void hcd_int_handler(uint8_t rhport, bool in_isr) {
  (void) rhport;
  (void) in_isr;
}

void hcd_int_enable(uint8_t rhport) {
  (void) rhport;
}

void hcd_int_disable(uint8_t rhport) {
  (void) rhport;
}

bool hcd_port_connect_status(uint8_t rhport) {
  return rhport == RHPORT && port_connected;
}

void hcd_port_reset(uint8_t rhport) {
  (void) rhport;
  port_reset_count++;
}

void hcd_port_reset_end(uint8_t rhport) {
  (void) rhport;
}

tusb_speed_t hcd_port_speed_get(uint8_t rhport) {
  (void) rhport;
  return TUSB_SPEED_FULL;
}

void hcd_device_close(uint8_t rhport, uint8_t daddr) {
  (void) rhport;
  (void) daddr;
}

bool hcd_setup_send(uint8_t rhport, uint8_t daddr, const uint8_t setup_packet[8]) {
  (void) rhport;
  (void) setup_packet;
  setup_send_count++;
  setup_send_daddr = daddr;
  return true;
}

bool hcd_edpt_open(uint8_t rhport, uint8_t daddr, const tusb_desc_endpoint_t *ep_desc) {
  (void) rhport;
  (void) daddr;
  (void) ep_desc;
  return true;
}

bool hcd_edpt_close(uint8_t rhport, uint8_t daddr, uint8_t ep_addr) {
  (void) rhport;
  (void) daddr;
  (void) ep_addr;
  return true;
}

bool hcd_edpt_xfer(uint8_t rhport, uint8_t daddr, uint8_t ep_addr, uint8_t *buffer, uint16_t buflen) {
  (void) rhport;
  (void) daddr;
  (void) ep_addr;
  (void) buffer;
  (void) buflen;
  return true;
}

bool hcd_edpt_abort_xfer(uint8_t rhport, uint8_t daddr, uint8_t ep_addr) {
  (void) rhport;
  (void) daddr;
  (void) ep_addr;
  return true;
}

bool hcd_edpt_clear_stall(uint8_t rhport, uint8_t daddr, uint8_t ep_addr) {
  (void) rhport;
  (void) daddr;
  (void) ep_addr;
  return true;
}

//--------------------------------------------------------------------+
// Helpers
//--------------------------------------------------------------------+
// Length in ms of the pending enumeration delay
static uint32_t pending_delay_ms(void) {
  TEST_ASSERT_EQUAL_PTR(enum_delay_async, _usbh_data.call_after.func);
  return _usbh_data.call_after.at_ms - now_ms - 1;
}

// Run the pending enumeration delay as tuh_task_ext() does once it is due
static void fire_delay(void) {
  const tusb_defer_func_t func = _usbh_data.call_after.func;
  TEST_ASSERT_NOT_NULL(func);
  now_ms                     = _usbh_data.call_after.at_ms;
  _usbh_data.call_after.func = NULL;
  func(_usbh_data.call_after.arg);
}

// Handle the next queued host event as tuh_task_ext() does
static void process_next_event(void) {
  hcd_event_t event;
  TEST_ASSERT_TRUE(osal_queue_receive(_usbh_q, &event, 0));
  TEST_ASSERT_TRUE(usbh_process_event(&event, false));
}

// Attach a device to the root port and run debouncing and the port reset up to the reset recovery
static void attach_to_reset_recovery(void) {
  port_connected = true;
  hcd_event_device_attach(RHPORT, false);
  process_next_event();
  while (_usbh_data.call_after.arg != ENUM_AFTER_RESET_RECOVERY_DELAY) {
    fire_delay();
  }
}

// Send the first request: GET_DESCRIPTOR(device, 8) to address 0
static void send_first_request(void) {
  attach_to_reset_recovery();
  fire_delay();
  TEST_ASSERT_EQUAL(CONTROL_STAGE_SETUP, _usbh_data.ctrl_xfer_info.stage);
  TEST_ASSERT_EQUAL_UINT8(TUSB_REQ_GET_DESCRIPTOR, _usbh_epbuf.request.bRequest);
  TEST_ASSERT_EQUAL_UINT16(8, _usbh_epbuf.request.wLength);
}

// The device does not answer the request in flight: the hcd fails it, usbh resends it at once until
// USBH_CONTROL_RETRY_MAX, then completes it as FAILED
static void request_fails(void) {
  const uint8_t daddr = _usbh_data.ctrl_xfer_info.daddr;
  setup_send_count    = 0;
  while (_usbh_data.ctrl_xfer_info.stage != CONTROL_STAGE_IDLE) {
    usbh_control_xfer_cb(daddr, 0, XFER_RESULT_FAILED, 0);
  }
  TEST_ASSERT_EQUAL_UINT32(USBH_CONTROL_RETRY_MAX, setup_send_count);
}

// The device answers the request in flight with the given data
static void request_succeeds(const void *data, uint16_t len) {
  const uint8_t daddr = _usbh_data.ctrl_xfer_info.daddr;
  const bool    in    = _usbh_epbuf.request.bmRequestType_bit.direction == TUSB_DIR_IN;
  usbh_control_xfer_cb(daddr, 0, XFER_RESULT_SUCCESS, 0); // SETUP
  if (_usbh_epbuf.request.wLength > 0) {
    if (in) {
      memcpy(_usbh_data.ctrl_xfer_info.buffer, data, len);
    }
    usbh_control_xfer_cb(daddr, in ? 0x80 : 0x00, XFER_RESULT_SUCCESS, len); // DATA
  }
  usbh_control_xfer_cb(daddr, in ? 0x00 : 0x80, XFER_RESULT_SUCCESS, 0); // STATUS
}

static const tusb_desc_device_t desc_device = {
  .bLength            = sizeof(tusb_desc_device_t),
  .bDescriptorType    = TUSB_DESC_DEVICE,
  .bcdUSB             = 0x0200,
  .bDeviceClass       = 0,
  .bDeviceSubClass    = 0,
  .bDeviceProtocol    = 0,
  .bMaxPacketSize0    = 64,
  .idVendor           = 0xCafe,
  .idProduct          = 0x4000,
  .bcdDevice          = 0x0100,
  .iManufacturer      = 0,
  .iProduct           = 0,
  .iSerialNumber      = 0,
  .bNumConfigurations = 1,
};

//--------------------------------------------------------------------+
// Setup
//--------------------------------------------------------------------+
void setUp(void) {
  tu_memclr(&_usbh_data, sizeof(_usbh_data));
  tu_memclr(_usbh_devices, sizeof(_usbh_devices));
  now_ms                       = 1000;
  port_connected               = false;
  port_reset_count             = 0;
  setup_send_count             = 0;
  setup_send_daddr             = TUSB_INDEX_INVALID_8;
  _usbh_data.enumerating_daddr = TUSB_INDEX_INVALID_8;
  _usbh_q                      = osal_queue_create(&_usbh_qdef);
  tu_fifo_clear(&_usbh_pending_ctrl_q);
}

void tearDown(void) {
}

//--------------------------------------------------------------------+
// Tests
//--------------------------------------------------------------------+
// The first request waits 50 ms after the port reset, as Linux, not the 10 ms USB 2.0 minimum (#3876)
void test_reset_recovery_is_50ms(void) {
  attach_to_reset_recovery();
  TEST_ASSERT_EQUAL_UINT32(1, port_reset_count);
  TEST_ASSERT_EQUAL_UINT32(50, pending_delay_ms());
}

// A failed request is sent again after the retry gap, with no port reset in between: the device keeps recovering from
// the reset it already had
void test_failed_request_is_sent_again_without_port_reset(void) {
  send_first_request();
  const tusb_control_request_t request = _usbh_epbuf.request;

  request_fails();
  TEST_ASSERT_EQUAL(ENUM_AFTER_XFER_RETRY_DELAY, _usbh_data.call_after.arg);
  TEST_ASSERT_EQUAL_UINT32(ENUM_XFER_RETRY_DELAY_MS, pending_delay_ms());

  fire_delay();
  TEST_ASSERT_EQUAL(CONTROL_STAGE_SETUP, _usbh_data.ctrl_xfer_info.stage);
  TEST_ASSERT_EQUAL_UINT8(0, setup_send_daddr);
  TEST_ASSERT_EQUAL_MEMORY(&request, &_usbh_epbuf.request, sizeof(request));
  TEST_ASSERT_EQUAL_UINT32(1, port_reset_count);
  TEST_ASSERT_EQUAL_UINT8(0, _usbh_data.enum_attempt);
}

// A device silent for up to 250 ms after the reset answers within the first attempt: tries at 50, 150 and 250 ms
void test_tries_span_250ms_after_reset(void) {
  attach_to_reset_recovery();
  const uint32_t reset_done_ms = now_ms;
  fire_delay();
  TEST_ASSERT_EQUAL_UINT32(50, now_ms - reset_done_ms - 1);

  for (uint32_t i = 1; i < ENUM_XFER_ATTEMPT_MAX; i++) {
    request_fails();
    fire_delay();
    TEST_ASSERT_EQUAL(CONTROL_STAGE_SETUP, _usbh_data.ctrl_xfer_info.stage);
  }
  TEST_ASSERT_EQUAL_UINT32(50 + 1 + (ENUM_XFER_ATTEMPT_MAX - 1) * (ENUM_XFER_RETRY_DELAY_MS + 1),
                           now_ms - reset_done_ms);
  TEST_ASSERT_EQUAL_UINT32(1, port_reset_count);
}

// Once the request's tries are used up, the attempt fails and the next one starts with a port reset
void test_last_try_failing_resets_port(void) {
  send_first_request();
  for (uint32_t i = 1; i < ENUM_XFER_ATTEMPT_MAX; i++) {
    request_fails();
    fire_delay();
  }
  request_fails();

  TEST_ASSERT_EQUAL(ENUM_AFTER_DEBOUNCING_DELAY, _usbh_data.call_after.arg);
  TEST_ASSERT_EQUAL_UINT8(1, _usbh_data.enum_attempt);
  TEST_ASSERT_EQUAL_UINT8(0, _usbh_data.enum_xfer_retry);

  // the next attempt's request gets all its tries again
  while (_usbh_data.call_after.arg != ENUM_AFTER_RESET_RECOVERY_DELAY) {
    fire_delay();
  }
  TEST_ASSERT_EQUAL_UINT32(2, port_reset_count);
  fire_delay();
  request_fails();
  TEST_ASSERT_EQUAL(ENUM_AFTER_XFER_RETRY_DELAY, _usbh_data.call_after.arg);
}

// A timeout is not sent again: the device already NAKed it for CFG_TUH_CONTROL_TIMEOUT_MS
void test_timeout_is_not_sent_again(void) {
  send_first_request();
  now_ms += CFG_TUH_CONTROL_TIMEOUT_MS + 1;
  control_xfer_timeout_expired();

  TEST_ASSERT_EQUAL(ENUM_AFTER_DEBOUNCING_DELAY, _usbh_data.call_after.arg);
  TEST_ASSERT_EQUAL_UINT8(1, _usbh_data.enum_attempt);
}

// A request the device answers gives the next request its own tries, and SET_ADDRESS waits 10 ms to settle, as Linux
void test_answer_restores_tries_and_set_address_recovery_is_10ms(void) {
  send_first_request();
  request_fails();
  fire_delay();
  TEST_ASSERT_EQUAL_UINT8(1, _usbh_data.enum_xfer_retry);

  request_succeeds(&desc_device, 8);
  TEST_ASSERT_EQUAL_UINT8(0, _usbh_data.enum_xfer_retry);
  TEST_ASSERT_EQUAL(CONTROL_STAGE_SETUP, _usbh_data.ctrl_xfer_info.stage);
  TEST_ASSERT_EQUAL_UINT8(TUSB_REQ_SET_ADDRESS, _usbh_epbuf.request.bRequest);
  const uint8_t new_addr = (uint8_t) tu_le16toh(_usbh_epbuf.request.wValue);

  // SET_ADDRESS fails too: it gets its own tries
  request_fails();
  TEST_ASSERT_EQUAL(ENUM_AFTER_XFER_RETRY_DELAY, _usbh_data.call_after.arg);
  fire_delay();
  TEST_ASSERT_EQUAL_UINT8(TUSB_REQ_SET_ADDRESS, _usbh_epbuf.request.bRequest);
  TEST_ASSERT_EQUAL_UINT16(new_addr, tu_le16toh(_usbh_epbuf.request.wValue));

  request_succeeds(NULL, 0);
  TEST_ASSERT_EQUAL(ENUM_AFTER_SET_ADDRESS_RECOVERY_DELAY, _usbh_data.call_after.arg);
  TEST_ASSERT_EQUAL_UINT32(10, pending_delay_ms());
  TEST_ASSERT_EQUAL_UINT8(new_addr, _usbh_data.enumerating_daddr);

  // a request to the new address is sent again to that address
  fire_delay();
  TEST_ASSERT_EQUAL_UINT8(new_addr, setup_send_daddr);
  request_fails();
  fire_delay();
  TEST_ASSERT_EQUAL_UINT8(new_addr, setup_send_daddr);
  TEST_ASSERT_EQUAL_UINT8(TUSB_REQ_GET_DESCRIPTOR, _usbh_epbuf.request.bRequest);
  TEST_ASSERT_EQUAL_UINT16(sizeof(tusb_desc_device_t), _usbh_epbuf.request.wLength);
}

// An unplug during the retry gap cancels the resend
void test_unplug_during_gap_cancels_resend(void) {
  send_first_request();
  request_fails();
  TEST_ASSERT_EQUAL(ENUM_AFTER_XFER_RETRY_DELAY, _usbh_data.call_after.arg);

  port_connected = false;
  hcd_event_device_remove(RHPORT, false);
  process_next_event();

  TEST_ASSERT_NULL(_usbh_data.call_after.func);
  TEST_ASSERT_EQUAL_UINT8(TUSB_INDEX_INVALID_8, _usbh_data.enumerating_daddr);
  TEST_ASSERT_TRUE(osal_queue_empty(_usbh_q));
}

// A request failed by an unplug is not sent again
void test_request_failed_by_unplug_is_not_sent_again(void) {
  send_first_request();

  port_connected = false;
  hcd_event_device_remove(RHPORT, false);
  process_next_event(); // the unplug fails the request in flight

  TEST_ASSERT_NULL(_usbh_data.call_after.func);
  TEST_ASSERT_EQUAL(CONTROL_STAGE_IDLE, _usbh_data.ctrl_xfer_info.stage);
  TEST_ASSERT_TRUE(osal_queue_empty(_usbh_q));
}

// Another device's control transfer holds the slot when the gap ends: the resend is queued and sent once it is free
void test_resend_waits_for_busy_control_slot(void) {
  send_first_request();
  const tusb_control_request_t request = _usbh_epbuf.request;
  request_fails();

  // another device's transfer takes the slot during the gap
  _usbh_data.ctrl_xfer_info.stage = CONTROL_STAGE_DATA;
  _usbh_data.ctrl_xfer_info.daddr = 1;
  setup_send_count                = 0;
  fire_delay();
  TEST_ASSERT_EQUAL_UINT32(0, setup_send_count);
  TEST_ASSERT_EQUAL_UINT8(0, _usbh_data.enumerating_daddr);

  _usbh_data.ctrl_xfer_info.stage = CONTROL_STAGE_IDLE;
  control_xfer_dispatch_pending();
  TEST_ASSERT_EQUAL_UINT32(1, setup_send_count);
  TEST_ASSERT_EQUAL_UINT8(0, setup_send_daddr);
  TEST_ASSERT_EQUAL_MEMORY(&request, &_usbh_epbuf.request, sizeof(request));
  TEST_ASSERT_EQUAL_PTR(process_enumeration, _usbh_data.ctrl_xfer_info.complete_cb);
}

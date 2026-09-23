/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 Ha Thach (tinyusb.org)
 * SPDX-License-Identifier: MIT
 *
 * This file is part of the TinyUSB stack.
 */

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

//--------------------------------------------------------------------+
// Fixture: one MTP interface, dcd calls recorded in a log, app callbacks driven by knobs
//--------------------------------------------------------------------+
enum {
  EP_CTRL_OUT = 0x00,
  EP_CTRL_IN  = 0x80,
  EP_EVT_IN   = 0x81,
  EP_OUT      = 0x02,
  EP_IN       = 0x82,
};

enum { RHPORT = 0, BUFSIZE = CFG_TUD_MTP_EP_BUFSIZE, HDR = sizeof(mtp_container_header_t) };

#define CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_MTP_DESC_LEN)
#define DESC_CONFIG(_epsize) { \
  TUD_CONFIG_DESCRIPTOR(1, 1, 0, CONFIG_TOTAL_LEN, 0, 100), \
  TUD_MTP_DESCRIPTOR(0, 0, EP_EVT_IN, 8, 10, EP_OUT, EP_IN, _epsize) }

static const uint8_t desc_config_fs[] = DESC_CONFIG(64);
static const uint8_t desc_config_hs[] = DESC_CONFIG(512);
static const uint8_t* desc_config;
static uint16_t ep_mps;

uint32_t tusb_time_millis_api(void) { return 0; }
const uint8_t* tud_descriptor_device_cb(void) { return NULL; }
const uint8_t* tud_descriptor_configuration_cb(uint8_t index) { (void) index; return desc_config; }
const uint16_t* tud_descriptor_string_cb(uint8_t index, uint16_t langid) { (void) index; (void) langid; return NULL; }

// dcd call log. An IN transfer's bytes are snapshotted at submission: a dcd may copy them right
// away, so the driver must not fix the buffer up afterwards.
typedef enum { DCD_XFER, DCD_STALL, DCD_CLEAR_STALL } dcd_call_type_t;
typedef struct {
  dcd_call_type_t type;
  uint8_t ep;
  bool ep0_done; // EP0 transfer already driven by host_control(), skipped by expect_call()
  uint16_t len;
  uint8_t* buf;
  uint8_t data[BUFSIZE];
} dcd_call_t;

static dcd_call_t log_calls[64];
static uint32_t log_count;
static uint32_t log_cursor; // calls before it were already checked

// one outstanding transfer per endpoint, retired by its completion or a stall
typedef struct {
  bool active;
  uint8_t* buf;
  uint16_t len;
  const dcd_call_t* call;
} xfer_t;
static xfer_t xfers[2][16]; // [dir][epnum]

static xfer_t* xfer_of(uint8_t ep) {
  return &xfers[tu_edpt_dir(ep)][tu_edpt_number(ep)];
}

static bool stub_edpt_xfer(uint8_t rhport, uint8_t ep, uint8_t* buf, uint16_t len, bool is_isr, int n) {
  (void) rhport; (void) is_isr; (void) n;
  TEST_ASSERT_LESS_THAN(TU_ARRAY_SIZE(log_calls), log_count);
  TEST_ASSERT_LESS_OR_EQUAL_MESSAGE(BUFSIZE, len, "transfer longer than the endpoint buffer");
  dcd_call_t* c = &log_calls[log_count++];
  *c = (dcd_call_t){ .type = DCD_XFER, .ep = ep, .len = len, .buf = buf };
  if (tu_edpt_dir(ep) == TUSB_DIR_IN && len) memcpy(c->data, buf, len);
  xfer_t* x = xfer_of(ep);
  TEST_ASSERT_FALSE_MESSAGE(x->active, "transfer queued on a busy endpoint");
  *x = (xfer_t){ true, buf, len, c };
  return true;
}
static void stub_edpt_stall(uint8_t rhport, uint8_t ep, int n) {
  (void) rhport; (void) n;
  log_calls[log_count++] = (dcd_call_t){ .type = DCD_STALL, .ep = ep };
  xfer_of(ep)->active = false; // a dcd disables the endpoint, dropping its transfer
}
static void stub_edpt_clear_stall(uint8_t rhport, uint8_t ep, int n) {
  (void) rhport; (void) n;
  log_calls[log_count++] = (dcd_call_t){ .type = DCD_CLEAR_STALL, .ep = ep };
}
static bool stub_edpt_open(uint8_t rhport, const tusb_desc_endpoint_t* desc, int n) {
  (void) rhport; (void) desc; (void) n;
  return true;
}

// next unchecked call must be `type` on `ep` (and `len` for a transfer); returns it
static const dcd_call_t* expect_call(dcd_call_type_t type, uint8_t ep, uint16_t len) {
  while (log_cursor < log_count && log_calls[log_cursor].ep0_done) log_cursor++;
  TEST_ASSERT_MESSAGE(log_cursor < log_count, "expected a dcd call, got none");
  const dcd_call_t* c = &log_calls[log_cursor++];
  TEST_ASSERT_EQUAL_MESSAGE(type, c->type, "dcd call type");
  TEST_ASSERT_EQUAL_HEX8_MESSAGE(ep, c->ep, "dcd call endpoint");
  if (type == DCD_XFER) {
    TEST_ASSERT_EQUAL_MESSAGE(len, c->len, "dcd transfer length");
  }
  return c;
}
static void expect_no_more_calls(void) {
  while (log_cursor < log_count && log_calls[log_cursor].ep0_done) log_cursor++;
  TEST_ASSERT_EQUAL_MESSAGE(log_count, log_cursor, "unexpected dcd call");
}
static void expect_stall_both(void) {
  expect_call(DCD_STALL, EP_OUT, 0);
  expect_call(DCD_STALL, EP_IN, 0);
}
static void expect_command_read(void) {
  expect_call(DCD_XFER, EP_OUT, BUFSIZE);
}

// the host completes the outstanding transfer on `ep`: fills an OUT read with `len` bytes of
// `data`, or retires an IN transfer after checking its buffer was left alone while queued
static void host_complete(uint8_t ep, const void* data, uint32_t len, xfer_result_t result) {
  xfer_t* x = xfer_of(ep);
  TEST_ASSERT_TRUE_MESSAGE(x->active, "completion for an endpoint with no transfer queued");
  TEST_ASSERT_LESS_OR_EQUAL_MESSAGE(x->len, len, "completion longer than the queued transfer");
  if (tu_edpt_dir(ep) == TUSB_DIR_OUT) {
    if (len) memcpy(x->buf, data, len);
  } else {
    TEST_ASSERT_EQUAL_MESSAGE(x->len, len, "IN transfer completed short");
    if (len) TEST_ASSERT_EQUAL_MEMORY_MESSAGE(x->call->data, x->buf, len, "IN buffer changed while queued");
  }
  x->active = false;
  dcd_event_xfer_complete(RHPORT, ep, len, result, false);
  tud_task();
}
static void host_out(const void* data, uint32_t len) {
  host_complete(EP_OUT, data, len, XFER_RESULT_SUCCESS);
}
static void host_in_done(uint32_t len) {
  host_complete(EP_IN, NULL, len, XFER_RESULT_SUCCESS);
}

static uint32_t next_tid = 1;
static uint32_t host_command(uint16_t code, const uint32_t* params, uint8_t nparams) {
  mtp_container_command_t cmd = { .header = { .len = HDR + 4u * nparams, .type = MTP_CONTAINER_TYPE_COMMAND_BLOCK,
                                              .code = code, .transaction_id = next_tid++ } };
  if (nparams) memcpy(cmd.params, params, 4u * nparams);
  host_out(&cmd, cmd.header.len);
  return cmd.header.transaction_id;
}

// Drive a control request through every stage, completing each EP0 transfer the stack queues
// until its status stage is done. Other endpoints' calls stay in the log for expect_call().
// `data_out` fills the data stage read, `data_in` receives what the device sends; returns the
// data stage length, or -1 if the request was stalled instead of completed.
static int host_control(const tusb_control_request_t* req, const void* data_out, uint8_t* data_in) {
  int data_len = 0;
  bool status_done = false;
  // the status stage runs opposite to the data stage; IN when there is no data stage
  const uint8_t status_ep = (req->wLength && req->bmRequestType_bit.direction == TUSB_DIR_IN) ? EP_CTRL_OUT : EP_CTRL_IN;
  uint32_t scan = log_count;
  dcd_event_setup_received(RHPORT, (const uint8_t*) req, false);
  tud_task();
  while (!status_done) {
    bool progressed = false;
    for (; scan < log_count; scan++) {
      dcd_call_t* c = &log_calls[scan];
      if (tu_edpt_number(c->ep) != 0) continue;
      c->ep0_done = true;
      if (c->type == DCD_STALL) return -1;
      if (c->type != DCD_XFER) continue;
      if (c->ep == status_ep && c->len == 0) {
        status_done = true;
      } else if (c->ep == EP_CTRL_IN) {
        if (data_in) memcpy(data_in + data_len, c->buf, c->len);
        data_len += c->len;
      } else {
        memcpy(c->buf, (const uint8_t*) data_out + data_len, c->len);
        data_len += c->len;
      }
      xfer_of(c->ep)->active = false;
      dcd_event_xfer_complete(RHPORT, c->ep, c->len, XFER_RESULT_SUCCESS, false);
      tud_task();
      progressed = true;
      scan++;
      break; // the completion may have queued more calls: rescan from here
    }
    TEST_ASSERT_TRUE_MESSAGE(progressed, "control request never reached its status stage");
  }
  return data_len;
}

//--------------------------------------------------------------------+
// Application callbacks
//--------------------------------------------------------------------+
typedef enum { APP_NO_DATA, APP_SEND, APP_RECEIVE } app_data_mode_t;

static struct {
  app_data_mode_t data_mode;
  uint32_t data_len;      // bytes to send (APP_SEND) or expected (APP_RECEIVE)
  int32_t cmd_ret, xfer_ret, complete_ret;
  bool defer_response;    // complete_cb returns without responding
  uint16_t resp_code;
  uint8_t resp_nparams;

  uint32_t cmd_calls, xfer_calls, complete_calls, resp_complete_calls, cancel_calls, reset_calls, status_calls;
  uint8_t complete_phase;
  uint32_t received; // bytes received in APP_RECEIVE
  uint8_t rx[4 * BUFSIZE];
} app;

static uint8_t pattern(uint32_t i) { return (uint8_t) (i * 7 + 3); }

static void app_send_next(tud_mtp_cb_data_t* cb) {
  mtp_container_info_t* io = &cb->io_container;
  const uint32_t offset = (cb->phase == MTP_PHASE_COMMAND) ? 0 : cb->total_xferred_bytes - HDR;
  const uint32_t n = tu_min32(app.data_len - offset, io->payload_bytes);
  for (uint32_t i = 0; i < n; i++) io->payload[i] = pattern(offset + i);
  if (cb->phase == MTP_PHASE_COMMAND) {
    io->header->len = HDR + app.data_len;
    tud_mtp_data_send(io);
  } else if (n > 0) {
    tud_mtp_data_send(io);
  }
}

int32_t tud_mtp_command_received_cb(tud_mtp_cb_data_t* cb) {
  app.cmd_calls++;
  if (app.cmd_ret < 0) return app.cmd_ret;
  switch (app.data_mode) {
    case APP_SEND: app_send_next(cb); break;
    case APP_RECEIVE: tud_mtp_data_receive(&cb->io_container); break;
    default:
      cb->io_container.header->code = app.resp_code;
      for (uint8_t i = 0; i < app.resp_nparams; i++) mtp_container_add_uint32(&cb->io_container, 0x1000 + i);
      tud_mtp_response_send(&cb->io_container);
      break;
  }
  return 0;
}

int32_t tud_mtp_data_xfer_cb(tud_mtp_cb_data_t* cb) {
  app.xfer_calls++;
  if (app.xfer_ret < 0) return app.xfer_ret;
  if (app.data_mode == APP_SEND) {
    app_send_next(cb);
  } else if (app.data_mode == APP_RECEIVE) {
    mtp_container_info_t* io = &cb->io_container;
    TEST_ASSERT_LESS_OR_EQUAL(sizeof(app.rx), app.received + io->payload_bytes);
    memcpy(app.rx + app.received, io->payload, io->payload_bytes);
    app.received += io->payload_bytes;
    if (cb->total_xferred_bytes < io->header->len) tud_mtp_data_receive(io);
  }
  return 0;
}

int32_t tud_mtp_data_complete_cb(tud_mtp_cb_data_t* cb) {
  app.complete_calls++;
  app.complete_phase = cb->phase;
  if (app.complete_ret < 0) return app.complete_ret;
  if (!app.defer_response) {
    cb->io_container.header->code = app.resp_code;
    for (uint8_t i = 0; i < app.resp_nparams; i++) mtp_container_add_uint32(&cb->io_container, 0x1000 + i);
    tud_mtp_response_send(&cb->io_container);
  }
  return 0;
}

int32_t tud_mtp_response_complete_cb(tud_mtp_cb_data_t* cb) { (void) cb; app.resp_complete_calls++; return 0; }
bool tud_mtp_request_cancel_cb(tud_mtp_request_cb_data_t* cb) { (void) cb; app.cancel_calls++; return true; }
bool tud_mtp_request_device_reset_cb(tud_mtp_request_cb_data_t* cb) { (void) cb; app.reset_calls++; return true; }
int32_t tud_mtp_request_get_device_status_cb(tud_mtp_request_cb_data_t* cb) {
  app.status_calls++;
  uint16_t* buf16 = (uint16_t*)(uintptr_t) cb->buf;
  buf16[0] = 4;
  buf16[1] = MTP_RESP_OK;
  return 4;
}

//--------------------------------------------------------------------+
// Setup
//--------------------------------------------------------------------+
static void log_reset(void) {
  log_count = log_cursor = 0;
}

// bus reset at `speed`, SET_CONFIGURATION, then the driver arms its first command read
static void open_device(tusb_speed_t speed) {
  desc_config = (speed == TUSB_SPEED_HIGH) ? desc_config_hs : desc_config_fs;
  ep_mps = (speed == TUSB_SPEED_HIGH) ? 512 : 64;
  dcd_event_bus_reset(RHPORT, speed, false);
  tud_task();
  log_reset();

  const tusb_control_request_t set_config = { .bmRequestType = 0x00, .bRequest = TUSB_REQ_SET_CONFIGURATION, .wValue = 1 };
  TEST_ASSERT_EQUAL(0, host_control(&set_config, NULL, NULL));
  expect_command_read();
  expect_no_more_calls();
  memset(&app, 0, sizeof(app));
  app.resp_code = MTP_RESP_OK;
}

void setUp(void) {
  dcd_int_disable_Ignore();
  dcd_int_enable_Ignore();
  dcd_edpt_close_all_Ignore();
  dcd_set_address_Ignore();
  dcd_edpt0_status_complete_Ignore();
  dcd_edpt_open_Stub(stub_edpt_open);
  dcd_edpt_xfer_Stub(stub_edpt_xfer);
  dcd_edpt_stall_Stub(stub_edpt_stall);
  dcd_edpt_clear_stall_Stub(stub_edpt_clear_stall);
  memset(xfers, 0, sizeof(xfers));
  log_reset();

  if (!tud_inited()) {
    tusb_rhport_init_t dev_init = { .role = TUSB_ROLE_DEVICE, .speed = TUSB_SPEED_AUTO };
    dcd_init_ExpectAndReturn(RHPORT, &dev_init, true);
    tusb_init(RHPORT, &dev_init);
  }
}

void tearDown(void) {}

//--------------------------------------------------------------------+
// Helpers for the common transaction shapes
//--------------------------------------------------------------------+
// A response of `len` bytes is queued on IN; check its header, retire it, expect the next command read.
static void expect_response(uint32_t tid, uint16_t code, uint8_t nparams) {
  const dcd_call_t* c = expect_call(DCD_XFER, EP_IN, HDR + 4u * nparams);
  const mtp_container_command_t* resp = (const mtp_container_command_t*) c->data;
  TEST_ASSERT_EQUAL(HDR + 4u * nparams, resp->header.len);
  TEST_ASSERT_EQUAL(MTP_CONTAINER_TYPE_RESPONSE_BLOCK, resp->header.type);
  TEST_ASSERT_EQUAL_HEX16(code, resp->header.code);
  TEST_ASSERT_EQUAL(tid, resp->header.transaction_id);
  for (uint8_t i = 0; i < nparams; i++) TEST_ASSERT_EQUAL_HEX32(0x1000 + i, resp->params[i]);
  expect_no_more_calls();
  host_in_done(c->len);
  expect_command_read();
  expect_no_more_calls();
}

//--------------------------------------------------------------------+
// Tests
//--------------------------------------------------------------------+
static void check_no_data_command(tusb_speed_t speed) {
  open_device(speed);
  app.resp_nparams = 1;
  const uint32_t p[] = { 0xAABBCCDD };
  const uint32_t tid = host_command(MTP_OP_OPEN_SESSION, p, 1);
  TEST_ASSERT_EQUAL(1, app.cmd_calls);
  expect_response(tid, MTP_RESP_OK, 1);
  TEST_ASSERT_EQUAL(1, app.resp_complete_calls);
}

void test_no_data_command_fs(void) { check_no_data_command(TUSB_SPEED_FULL); }
void test_no_data_command_hs(void) { check_no_data_command(TUSB_SPEED_HIGH); }

// data IN: 700 bytes = 512 + 200, no ZLP since the last packet is short
static void check_data_in(tusb_speed_t speed) {
  open_device(speed);
  app.data_mode = APP_SEND;
  app.data_len = 700;
  const uint32_t tid = host_command(MTP_OP_GET_DEVICE_INFO, NULL, 0);

  const dcd_call_t* c = expect_call(DCD_XFER, EP_IN, BUFSIZE);
  const mtp_generic_container_t* d = (const mtp_generic_container_t*) c->data;
  TEST_ASSERT_EQUAL(HDR + 700, d->header.len);
  TEST_ASSERT_EQUAL(MTP_CONTAINER_TYPE_DATA_BLOCK, d->header.type);
  TEST_ASSERT_EQUAL(tid, d->header.transaction_id);
  for (uint32_t i = 0; i < BUFSIZE - HDR; i++) TEST_ASSERT_EQUAL_HEX8(pattern(i), d->payload[i]);
  expect_no_more_calls();
  host_in_done(BUFSIZE);

  c = expect_call(DCD_XFER, EP_IN, 700 + HDR - BUFSIZE);
  for (uint32_t i = 0; i < c->len; i++) TEST_ASSERT_EQUAL_HEX8(pattern(BUFSIZE - HDR + i), c->data[i]);
  expect_no_more_calls();
  host_in_done(c->len);

  TEST_ASSERT_EQUAL(1, app.complete_calls);
  TEST_ASSERT_EQUAL(MTP_PHASE_DATA_COMPLETE, app.complete_phase);
  expect_response(tid, MTP_RESP_OK, 0);
}

void test_data_in_fs(void) { check_data_in(TUSB_SPEED_FULL); }
void test_data_in_hs(void) { check_data_in(TUSB_SPEED_HIGH); }

// data OUT: the host declares 700 bytes in its container header; the app only asked for a read
static void check_data_out(tusb_speed_t speed) {
  open_device(speed);
  app.data_mode = APP_RECEIVE;
  const uint32_t tid = host_command(MTP_OP_SEND_OBJECT_INFO, NULL, 0);
  expect_call(DCD_XFER, EP_OUT, BUFSIZE);
  expect_no_more_calls();

  uint8_t pkt[BUFSIZE];
  mtp_container_header_t* h = (mtp_container_header_t*) pkt;
  *h = (mtp_container_header_t){ .len = HDR + 700, .type = MTP_CONTAINER_TYPE_DATA_BLOCK, .code = MTP_OP_SEND_OBJECT_INFO, .transaction_id = tid };
  for (uint32_t i = 0; i < 700; i++) {
    const uint32_t pos = HDR + i;
    if (pos < BUFSIZE) pkt[pos] = pattern(i);
  }
  host_out(pkt, BUFSIZE);
  TEST_ASSERT_EQUAL(1, app.xfer_calls);
  TEST_ASSERT_EQUAL(BUFSIZE - HDR, app.received);
  expect_call(DCD_XFER, EP_OUT, BUFSIZE);
  expect_no_more_calls();

  for (uint32_t i = 0; i < 700 + HDR - BUFSIZE; i++) pkt[i] = pattern(BUFSIZE - HDR + i);
  host_out(pkt, 700 + HDR - BUFSIZE);
  TEST_ASSERT_EQUAL(2, app.xfer_calls);
  TEST_ASSERT_EQUAL(700, app.received);
  for (uint32_t i = 0; i < 700; i++) TEST_ASSERT_EQUAL_HEX8(pattern(i), app.rx[i]);
  TEST_ASSERT_EQUAL(1, app.complete_calls);
  TEST_ASSERT_EQUAL(MTP_PHASE_DATA_COMPLETE, app.complete_phase);
  expect_response(tid, MTP_RESP_OK, 0);
}

void test_data_out_fs(void) { check_data_out(TUSB_SPEED_FULL); }
void test_data_out_hs(void) { check_data_out(TUSB_SPEED_HIGH); }

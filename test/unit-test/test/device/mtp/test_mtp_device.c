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
  uint32_t app_received; // app.received at submission: what a read was armed over
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

static int dcd_refuse_ep; // the dcd refuses the next transfer on this endpoint, once; -1: none

static uint32_t app_received_now(void);

static xfer_t* xfer_of(uint8_t ep) {
  return &xfers[tu_edpt_dir(ep)][tu_edpt_number(ep)];
}

static bool stub_edpt_xfer(uint8_t rhport, uint8_t ep, uint8_t* buf, uint16_t len, bool is_isr, int n) {
  (void) rhport; (void) is_isr; (void) n;
  if (ep == dcd_refuse_ep) {
    dcd_refuse_ep = -1;
    return false;
  }
  TEST_ASSERT_LESS_THAN(TU_ARRAY_SIZE(log_calls), log_count);
  TEST_ASSERT_LESS_OR_EQUAL_MESSAGE(BUFSIZE, len, "transfer longer than the endpoint buffer");
  dcd_call_t* c = &log_calls[log_count++];
  *c = (dcd_call_t){ .type = DCD_XFER, .ep = ep, .len = len, .buf = buf, .app_received = app_received_now() };
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
static void expect_bulk_aborted(void) {
  expect_call(DCD_STALL, EP_OUT, 0);
  expect_call(DCD_CLEAR_STALL, EP_OUT, 0);
  expect_call(DCD_STALL, EP_IN, 0);
  expect_call(DCD_CLEAR_STALL, EP_IN, 0);
  expect_command_read();
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
  uint32_t respond_at_xfer; // data_xfer_cb call number that responds instead of continuing (0: never)
  bool queue_twice;       // the first data_send/receive is repeated: the second must be refused
  bool silent;            // command_received_cb does nothing: the test drives the API itself
  bool receive_after_last; // data_xfer_cb arms a read after the last payload too
  uint16_t resp_code;
  uint8_t resp_nparams;
  bool resp_refused_once; // the dcd refuses the first response, which the app retries unchanged

  uint32_t cmd_calls, xfer_calls, complete_calls, resp_complete_calls, cancel_calls, reset_calls, status_calls;
  uint8_t complete_phase;
  uint32_t received; // bytes received in APP_RECEIVE
  uint8_t rx[4 * BUFSIZE];
} app;

static uint8_t pattern(uint32_t i) { return (uint8_t) (i * 7 + 3); }
static uint32_t app_received_now(void) { return app.received; }

static void app_respond(tud_mtp_cb_data_t* cb) {
  if (cb->phase == MTP_PHASE_DATA) {
    cb->io_container.header->len = HDR; // early response: the header still describes the data
  } else {
    TEST_ASSERT_EQUAL_MESSAGE(HDR, cb->io_container.header->len, "driver must hand over a bare header");
  }
  cb->io_container.header->code = app.resp_code;
  for (uint8_t i = 0; i < app.resp_nparams; i++) mtp_container_add_uint32(&cb->io_container, 0x1000 + i);
  if (app.resp_refused_once) {
    app.resp_refused_once = false;
    dcd_refuse_ep = EP_IN;
    TEST_ASSERT_FALSE(tud_mtp_response_send(&cb->io_container));
    TEST_ASSERT_EQUAL_MESSAGE(-1, dcd_refuse_ep, "response never attempted");
  }
  TEST_ASSERT_TRUE(tud_mtp_response_send(&cb->io_container));
}

static void app_send_next(tud_mtp_cb_data_t* cb) {
  mtp_container_info_t* io = &cb->io_container;
  const uint32_t offset = (cb->phase == MTP_PHASE_COMMAND) ? 0 : cb->total_xferred_bytes - HDR;
  const uint32_t n = tu_min32(app.data_len - offset, io->payload_bytes);
  for (uint32_t i = 0; i < n; i++) io->payload[i] = pattern(offset + i);
  if (cb->phase == MTP_PHASE_COMMAND) {
    io->header->len = HDR + app.data_len;
    TEST_ASSERT_TRUE(tud_mtp_data_send(io));
    if (app.queue_twice) TEST_ASSERT_FALSE(tud_mtp_data_send(io));
  } else if (n > 0) {
    TEST_ASSERT_TRUE(tud_mtp_data_send(io));
  }
}

const mtp_container_command_t* last_command;
int32_t tud_mtp_command_received_cb(tud_mtp_cb_data_t* cb) {
  app.cmd_calls++;
  last_command = cb->command_container;
  if (app.cmd_ret < 0) return app.cmd_ret;
  if (app.silent) return 0;
  switch (app.data_mode) {
    case APP_SEND: app_send_next(cb); break;
    case APP_RECEIVE:
      TEST_ASSERT_TRUE(tud_mtp_data_receive(&cb->io_container));
      if (app.queue_twice) TEST_ASSERT_FALSE(tud_mtp_data_receive(&cb->io_container));
      break;
    default: app_respond(cb); break;
  }
  return 0;
}

int32_t tud_mtp_data_xfer_cb(tud_mtp_cb_data_t* cb) {
  app.xfer_calls++;
  if (app.xfer_ret < 0) return app.xfer_ret;
  if (app.respond_at_xfer == app.xfer_calls) {
    app_respond(cb);
    return 0;
  }
  if (app.data_mode == APP_SEND) {
    app_send_next(cb);
  } else if (app.data_mode == APP_RECEIVE) {
    mtp_container_info_t* io = &cb->io_container;
    TEST_ASSERT_LESS_OR_EQUAL(sizeof(app.rx), app.received + io->payload_bytes);
    memcpy(app.rx + app.received, io->payload, io->payload_bytes);
    app.received += io->payload_bytes;
    if (app.receive_after_last || cb->total_xferred_bytes < io->header->len) tud_mtp_data_receive(io);
  }
  return 0;
}

int32_t tud_mtp_data_complete_cb(tud_mtp_cb_data_t* cb) {
  app.complete_calls++;
  app.complete_phase = cb->phase;
  if (app.complete_ret < 0) return app.complete_ret;
  if (!app.defer_response) app_respond(cb);
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
  dcd_refuse_ep = -1;
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
// the response container queued by `c`, carrying the `nparams` params app_respond() adds
static void check_response(const dcd_call_t* c, uint32_t tid, uint16_t code, uint8_t nparams) {
  const mtp_container_command_t* resp = (const mtp_container_command_t*) c->data;
  TEST_ASSERT_EQUAL(HDR + 4u * nparams, resp->header.len);
  TEST_ASSERT_EQUAL(MTP_CONTAINER_TYPE_RESPONSE_BLOCK, resp->header.type);
  TEST_ASSERT_EQUAL_HEX16(code, resp->header.code);
  TEST_ASSERT_EQUAL(tid, resp->header.transaction_id);
  for (uint8_t i = 0; i < nparams; i++) TEST_ASSERT_EQUAL_HEX32(0x1000 + i, resp->params[i]);
}

// A response is queued on IN; check it, retire it, expect the next command read.
static void expect_response(uint32_t tid, uint16_t code, uint8_t nparams) {
  const dcd_call_t* c = expect_call(DCD_XFER, EP_IN, HDR + 4u * nparams);
  check_response(c, tid, code, nparams);
  expect_no_more_calls();
  host_in_done(c->len);
  expect_command_read();
  expect_no_more_calls();
}

// a data-OUT transaction the host declares `total` payload bytes for; returns the tid
static uint32_t start_data_out(uint32_t total, uint8_t* pkt) {
  app.data_mode = APP_RECEIVE;
  const uint32_t tid = host_command(MTP_OP_SEND_OBJECT_INFO, NULL, 0);
  expect_call(DCD_XFER, EP_OUT, BUFSIZE);
  expect_no_more_calls();
  mtp_container_header_t* h = (mtp_container_header_t*) pkt;
  *h = (mtp_container_header_t){ .len = HDR + total, .type = MTP_CONTAINER_TYPE_DATA_BLOCK, .code = MTP_OP_SEND_OBJECT_INFO, .transaction_id = tid };
  for (uint32_t i = HDR; i < BUFSIZE; i++) pkt[i] = pattern(i - HDR);
  return tid;
}

static mtp_container_info_t headered_io(void) {
  mtp_container_header_t* h = (mtp_container_header_t*) xfer_of(EP_OUT)->buf;
  return (mtp_container_info_t){ .header = h, .payload = (uint8_t*) h + HDR, .payload_bytes = BUFSIZE - HDR };
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
  uint8_t pkt[BUFSIZE];
  const uint32_t tid = start_data_out(700, pkt);
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

//--------------------------------------------------------------------+
// Class and standard requests
//--------------------------------------------------------------------+
static const tusb_control_request_t req_cancel = { .bmRequestType = 0x21, .bRequest = MTP_REQ_CANCEL, .wLength = 6 };
static const tusb_control_request_t req_reset = { .bmRequestType = 0x21, .bRequest = MTP_REQ_RESET };
static const tusb_control_request_t req_status = { .bmRequestType = 0xA1, .bRequest = MTP_REQ_GET_DEVICE_STATUS, .wLength = 16 };

static void host_cancel(uint32_t tid) {
  const uint16_t data[3] = { 0x4001, (uint16_t) tid, (uint16_t) (tid >> 16) };
  TEST_ASSERT_EQUAL(6, host_control(&req_cancel, data, NULL));
}
static void host_clear_halt(uint8_t ep) {
  const tusb_control_request_t req = { .bmRequestType = 0x02, .bRequest = TUSB_REQ_CLEAR_FEATURE,
                                       .wValue = TUSB_REQ_FEATURE_EDPT_HALT, .wIndex = ep };
  TEST_ASSERT_EQUAL(0, host_control(&req, NULL, NULL));
  expect_call(DCD_CLEAR_STALL, ep, 0);
}
// Get Device Status: returns the status code, and the halted endpoints in ep[] when 8 bytes came back
static uint16_t host_device_status(uint8_t ep[2]) {
  uint8_t buf[16] = { 0 };
  const int len = host_control(&req_status, NULL, buf);
  uint16_t buf16[4];
  memcpy(buf16, buf, sizeof(buf16));
  TEST_ASSERT_EQUAL(buf16[0], len);
  if (len == 8) {
    ep[0] = (uint8_t) buf16[2];
    ep[1] = (uint8_t) buf16[3];
  } else {
    TEST_ASSERT_EQUAL(4, len);
  }
  return buf16[1];
}
static void expect_status_ok(void) {
  const uint32_t before = app.status_calls;
  TEST_ASSERT_EQUAL_HEX16(MTP_RESP_OK, host_device_status(NULL));
  TEST_ASSERT_EQUAL(before + 1, app.status_calls);
}
static void expect_status_cancelled(void) {
  const uint32_t before = app.status_calls;
  uint8_t ep[2];
  TEST_ASSERT_EQUAL_HEX16(MTP_RESP_TRANSACTION_CANCELLED, host_device_status(ep));
  TEST_ASSERT_EQUAL_HEX8(EP_IN, ep[0]);
  TEST_ASSERT_EQUAL_HEX8(EP_OUT, ep[1]);
  TEST_ASSERT_EQUAL_MESSAGE(before, app.status_calls, "driver must answer the error phase itself");
}

// Both bulk endpoints are stalled and nothing else is queued: the host clears them in `first`,
// `second` order; the command read is armed only once both are clear.
static void recover_from_error(uint8_t first, uint8_t second) {
  expect_stall_both();
  expect_no_more_calls();
  expect_status_cancelled();
  host_clear_halt(first);
  expect_no_more_calls();
  expect_status_cancelled();
  host_clear_halt(second);
  expect_command_read();
  expect_no_more_calls();
  expect_status_ok();
}

//--------------------------------------------------------------------+
// Command phase
//--------------------------------------------------------------------+
void test_absent_params_are_zeroed(void) {
  open_device(TUSB_SPEED_HIGH);
  const uint32_t five[5] = { 1, 2, 3, 4, 5 };
  uint32_t tid = host_command(MTP_OP_GET_OBJECT_HANDLES, five, 5);
  expect_response(tid, MTP_RESP_OK, 0);
  const uint32_t one[1] = { 0xAA };
  tid = host_command(MTP_OP_GET_OBJECT, one, 1);
  TEST_ASSERT_EQUAL_HEX32(0xAA, last_command->params[0]);
  for (int i = 1; i < 5; i++) TEST_ASSERT_EQUAL_HEX32(0, last_command->params[i]);
  expect_response(tid, MTP_RESP_OK, 0);
}

static void check_bad_command(const void* data, uint32_t len) {
  open_device(TUSB_SPEED_HIGH);
  host_out(data, len);
  TEST_ASSERT_EQUAL(0, app.cmd_calls);
  recover_from_error(EP_OUT, EP_IN);
}
void test_runt_command_stalls(void) {
  const uint8_t runt[8] = { 8, 0, 0, 0, 1, 0, 1, 0x10 };
  check_bad_command(runt, sizeof(runt));
}
void test_command_with_short_header_len_stalls(void) {
  const mtp_container_command_t cmd = { .header = { .len = 4, .type = MTP_CONTAINER_TYPE_COMMAND_BLOCK, .code = MTP_OP_OPEN_SESSION, .transaction_id = 9 } };
  check_bad_command(&cmd, HDR);
}
void test_non_command_container_stalls(void) {
  const mtp_container_command_t cmd = { .header = { .len = HDR, .type = MTP_CONTAINER_TYPE_DATA_BLOCK, .code = MTP_OP_OPEN_SESSION, .transaction_id = 9 } };
  check_bad_command(&cmd, HDR);
}
// declares 5 params but only its header arrived: not dispatched with zeros in their place
void test_command_shorter_than_declared_stalls(void) {
  const mtp_container_command_t cmd = { .header = { .len = sizeof(mtp_container_command_t), .type = MTP_CONTAINER_TYPE_COMMAND_BLOCK, .code = MTP_OP_GET_OBJECT_HANDLES, .transaction_id = 9 } };
  check_bad_command(&cmd, HDR);
}
void test_command_with_six_params_stalls(void) {
  uint32_t cmd[(HDR + 24) / 4] = { 0 };
  const mtp_container_header_t h = { .len = sizeof(cmd), .type = MTP_CONTAINER_TYPE_COMMAND_BLOCK, .code = MTP_OP_GET_OBJECT_HANDLES, .transaction_id = 9 };
  memcpy(cmd, &h, HDR);
  check_bad_command(cmd, sizeof(cmd));
}
void test_command_with_partial_param_stalls(void) {
  const mtp_container_command_t cmd = { .header = { .len = HDR + 2, .type = MTP_CONTAINER_TYPE_COMMAND_BLOCK, .code = MTP_OP_GET_OBJECT, .transaction_id = 9 }, .params = { 0xAA } };
  check_bad_command(&cmd, HDR + 2);
}
void test_command_callback_negative_stalls(void) {
  open_device(TUSB_SPEED_HIGH);
  app.cmd_ret = -1;
  host_command(MTP_OP_OPEN_SESSION, NULL, 0);
  recover_from_error(EP_IN, EP_OUT);
}
void test_leftover_zlp_in_command_phase_is_absorbed(void) {
  open_device(TUSB_SPEED_HIGH);
  host_out(NULL, 0);
  TEST_ASSERT_EQUAL(0, app.cmd_calls);
  expect_command_read();
  expect_no_more_calls();
  const uint32_t tid = host_command(MTP_OP_OPEN_SESSION, NULL, 0);
  expect_response(tid, MTP_RESP_OK, 0);
}

// the dcd refuses the command read after a leftover ZLP: halt rather than idle with nothing armed
void test_leftover_zlp_read_refused_stalls(void) {
  open_device(TUSB_SPEED_HIGH);
  dcd_refuse_ep = EP_OUT;
  host_out(NULL, 0);
  TEST_ASSERT_EQUAL_MESSAGE(-1, dcd_refuse_ep, "command read never attempted");
  TEST_ASSERT_EQUAL(0, app.cmd_calls);
  recover_from_error(EP_OUT, EP_IN);
  const uint32_t tid = host_command(MTP_OP_OPEN_SESSION, NULL, 0);
  expect_response(tid, MTP_RESP_OK, 0);
}

// the dcd refuses the command read once the response is sent: halt rather than idle with nothing armed
void test_response_complete_read_refused_stalls(void) {
  open_device(TUSB_SPEED_HIGH);
  host_command(MTP_OP_OPEN_SESSION, NULL, 0);
  const uint16_t resp_len = expect_call(DCD_XFER, EP_IN, HDR)->len;
  expect_no_more_calls();
  dcd_refuse_ep = EP_OUT;
  host_in_done(resp_len);
  TEST_ASSERT_EQUAL_MESSAGE(-1, dcd_refuse_ep, "command read never attempted");
  TEST_ASSERT_EQUAL(1, app.resp_complete_calls);
  recover_from_error(EP_IN, EP_OUT);
  const uint32_t tid = host_command(MTP_OP_OPEN_SESSION, NULL, 0);
  expect_response(tid, MTP_RESP_OK, 0);
}

//--------------------------------------------------------------------+
// Data OUT
//--------------------------------------------------------------------+
// the first packet is a full buffer and the host declared more: the phase does not end early
void test_data_out_short_packet_does_not_finish(void) {
  open_device(TUSB_SPEED_HIGH);
  uint8_t pkt[BUFSIZE];
  const uint32_t tid = start_data_out(700, pkt);
  host_out(pkt, 300); // a short packet, but the host still owes 412 bytes
  TEST_ASSERT_EQUAL(1, app.xfer_calls);
  TEST_ASSERT_EQUAL(0, app.complete_calls);
  expect_call(DCD_XFER, EP_OUT, BUFSIZE);
  expect_no_more_calls();
  host_out(pkt, 700 + HDR - 300);
  TEST_ASSERT_EQUAL(1, app.complete_calls);
  expect_response(tid, MTP_RESP_OK, 0);
}

// exactly 2 full buffers: the last payload is delivered before the ZLP read is armed, and
// data_complete_cb runs once, on the ZLP
void test_data_out_full_buffer_boundary_then_zlp(void) {
  open_device(TUSB_SPEED_HIGH);
  uint8_t pkt[BUFSIZE];
  const uint32_t tid = start_data_out(2 * BUFSIZE - HDR, pkt);
  host_out(pkt, BUFSIZE);
  expect_call(DCD_XFER, EP_OUT, BUFSIZE);
  expect_no_more_calls();
  host_out(pkt, BUFSIZE);
  TEST_ASSERT_EQUAL(2, app.xfer_calls);
  TEST_ASSERT_EQUAL(2 * BUFSIZE - HDR, app.received);
  TEST_ASSERT_EQUAL_MESSAGE(0, app.complete_calls, "complete before the terminating ZLP");
  const dcd_call_t* zlp_read = expect_call(DCD_XFER, EP_OUT, BUFSIZE); // the ZLP read, full-sized
  TEST_ASSERT_EQUAL_MESSAGE(2 * BUFSIZE - HDR, zlp_read->app_received, "ZLP read armed over the undelivered final payload");
  for (uint32_t i = 0; i < BUFSIZE - HDR; i++) TEST_ASSERT_EQUAL_HEX8(pattern(i), app.rx[i]);
  for (uint32_t i = 0; i < BUFSIZE; i++) TEST_ASSERT_EQUAL_HEX8(pkt[i], app.rx[BUFSIZE - HDR + i]);
  expect_no_more_calls();
  host_out(NULL, 0);
  TEST_ASSERT_EQUAL(1, app.complete_calls);
  TEST_ASSERT_EQUAL(2, app.xfer_calls);
  expect_response(tid, MTP_RESP_OK, 0);
}

// the dcd refuses the terminating ZLP's read: halt rather than wait in DATA with nothing armed
void test_data_out_zlp_read_refused_stalls(void) {
  open_device(TUSB_SPEED_HIGH);
  uint8_t pkt[BUFSIZE];
  start_data_out(BUFSIZE - HDR, pkt);
  dcd_refuse_ep = EP_OUT;
  host_out(pkt, BUFSIZE);
  TEST_ASSERT_EQUAL_MESSAGE(-1, dcd_refuse_ep, "ZLP read never attempted");
  TEST_ASSERT_EQUAL(1, app.xfer_calls);
  TEST_ASSERT_EQUAL(0, app.complete_calls);
  recover_from_error(EP_OUT, EP_IN);
}

// the app armed its own read on the last full buffer: that read takes the ZLP, no halt
void test_data_out_app_read_takes_zlp(void) {
  open_device(TUSB_SPEED_HIGH);
  uint8_t pkt[BUFSIZE];
  const uint32_t tid = start_data_out(BUFSIZE - HDR, pkt);
  app.receive_after_last = true;
  host_out(pkt, BUFSIZE);
  expect_call(DCD_XFER, EP_OUT, BUFSIZE);
  expect_no_more_calls();
  host_out(NULL, 0);
  TEST_ASSERT_EQUAL(1, app.complete_calls);
  expect_response(tid, MTP_RESP_OK, 0);
}

void test_data_out_runt_first_packet_stalls(void) {
  open_device(TUSB_SPEED_HIGH);
  uint8_t pkt[BUFSIZE];
  start_data_out(100, pkt);
  host_out(pkt, 8);
  TEST_ASSERT_EQUAL(0, app.xfer_calls);
  recover_from_error(EP_OUT, EP_IN);
}

// a first data-OUT container that is not this transaction's data block never reaches the application
static void check_data_out_foreign_header(uint16_t type, uint16_t code, uint32_t tid_delta) {
  open_device(TUSB_SPEED_HIGH);
  uint8_t pkt[BUFSIZE];
  const uint32_t tid = start_data_out(100, pkt);
  mtp_container_header_t* h = (mtp_container_header_t*) pkt;
  h->type = type;
  h->code = code;
  h->transaction_id = tid + tid_delta;
  host_out(pkt, HDR + 100);
  TEST_ASSERT_EQUAL(0, app.xfer_calls);
  recover_from_error(EP_OUT, EP_IN);
}

void test_data_out_non_data_container_stalls(void) {
  check_data_out_foreign_header(MTP_CONTAINER_TYPE_COMMAND_BLOCK, MTP_OP_SEND_OBJECT_INFO, 0);
}
void test_data_out_other_operation_stalls(void) {
  check_data_out_foreign_header(MTP_CONTAINER_TYPE_DATA_BLOCK, MTP_OP_SEND_OBJECT, 0);
}
void test_data_out_other_transaction_stalls(void) {
  check_data_out_foreign_header(MTP_CONTAINER_TYPE_DATA_BLOCK, MTP_OP_SEND_OBJECT_INFO, 1);
}

void test_data_out_xfer_callback_negative_stalls(void) {
  open_device(TUSB_SPEED_HIGH);
  uint8_t pkt[BUFSIZE];
  start_data_out(1000, pkt);
  app.xfer_ret = -1;
  host_out(pkt, BUFSIZE);
  TEST_ASSERT_EQUAL(0, app.complete_calls);
  recover_from_error(EP_OUT, EP_IN);
}

void test_data_out_complete_callback_negative_stalls(void) {
  open_device(TUSB_SPEED_HIGH);
  uint8_t pkt[BUFSIZE];
  start_data_out(100, pkt);
  app.complete_ret = -1;
  host_out(pkt, HDR + 100);
  TEST_ASSERT_EQUAL(1, app.complete_calls);
  recover_from_error(EP_IN, EP_OUT);
}

void test_data_out_failed_transfer_stalls(void) {
  open_device(TUSB_SPEED_HIGH);
  uint8_t pkt[BUFSIZE];
  start_data_out(1000, pkt);
  host_out(pkt, BUFSIZE);
  expect_call(DCD_XFER, EP_OUT, BUFSIZE);
  host_complete(EP_OUT, NULL, 0, XFER_RESULT_FAILED); // a 0-byte failure is not a ZLP
  TEST_ASSERT_EQUAL(1, app.xfer_calls);
  TEST_ASSERT_EQUAL(0, app.complete_calls);
  recover_from_error(EP_OUT, EP_IN);
}

// the app answers from the 1st packet while the host still owes data: device-initiated cancel
void test_early_response_mid_data_out_is_a_cancel(void) {
  open_device(TUSB_SPEED_HIGH);
  uint8_t pkt[BUFSIZE];
  start_data_out(1000, pkt);
  app.respond_at_xfer = 1;
  app.resp_code = MTP_RESP_INVALID_PARAMETER;
  host_out(pkt, BUFSIZE);
  TEST_ASSERT_EQUAL(0, app.complete_calls);
  expect_call(DCD_XFER, EP_IN, HDR); // the response was queued, then retired by the stall
  recover_from_error(EP_OUT, EP_IN);
}

// the app answers from the last packet: the response goes out and the host's ZLP is absorbed
void test_early_response_on_complete_data_out(void) {
  open_device(TUSB_SPEED_HIGH);
  uint8_t pkt[BUFSIZE];
  const uint32_t tid = start_data_out(BUFSIZE - HDR, pkt);
  app.respond_at_xfer = 1;
  host_out(pkt, BUFSIZE);
  TEST_ASSERT_EQUAL(0, app.complete_calls);
  const dcd_call_t* resp = expect_call(DCD_XFER, EP_IN, HDR);
  expect_call(DCD_XFER, EP_OUT, BUFSIZE); // ZLP read
  expect_no_more_calls();
  host_out(NULL, 0);                      // the ZLP lands in RESPONSE and is absorbed
  expect_no_more_calls();
  host_in_done(resp->len);
  expect_command_read();
  expect_no_more_calls();
  TEST_ASSERT_EQUAL(1, app.resp_complete_calls);
  app.data_mode = APP_NO_DATA;
  app.respond_at_xfer = 0;
  const uint32_t next = host_command(MTP_OP_OPEN_SESSION, NULL, 0);
  TEST_ASSERT_EQUAL(tid + 1, next);
  expect_response(next, MTP_RESP_OK, 0);
}

// same, with the response retiring before the ZLP arrives
void test_early_response_on_complete_data_out_response_first(void) {
  open_device(TUSB_SPEED_HIGH);
  uint8_t pkt[BUFSIZE];
  start_data_out(BUFSIZE - HDR, pkt);
  app.respond_at_xfer = 1;
  host_out(pkt, BUFSIZE);
  const dcd_call_t* resp = expect_call(DCD_XFER, EP_IN, HDR);
  expect_call(DCD_XFER, EP_OUT, BUFSIZE);
  host_in_done(resp->len);
  expect_no_more_calls(); // the ZLP read is still out: no second read over it
  host_out(NULL, 0);
  expect_command_read();
  expect_no_more_calls();
}

void test_nonzero_out_in_response_phase_stalls(void) {
  open_device(TUSB_SPEED_HIGH);
  uint8_t pkt[BUFSIZE];
  start_data_out(BUFSIZE - HDR, pkt);
  app.respond_at_xfer = 1;
  host_out(pkt, BUFSIZE);
  expect_call(DCD_XFER, EP_IN, HDR);
  expect_call(DCD_XFER, EP_OUT, BUFSIZE);
  host_out(pkt, 16); // not the terminating ZLP
  recover_from_error(EP_OUT, EP_IN);
}

// response from a 2nd+ packet: the headerless view's params sit where the header goes
void test_response_from_headerless_packet_keeps_params(void) {
  open_device(TUSB_SPEED_HIGH);
  uint8_t pkt[BUFSIZE];
  const uint32_t tid = start_data_out(2 * BUFSIZE - HDR, pkt);
  host_out(pkt, BUFSIZE);
  expect_call(DCD_XFER, EP_OUT, BUFSIZE);
  app.respond_at_xfer = 2;
  app.resp_nparams = 2;
  host_out(pkt, BUFSIZE);
  check_response(expect_call(DCD_XFER, EP_IN, HDR + 8), tid, MTP_RESP_OK, 2);
  expect_call(DCD_XFER, EP_OUT, BUFSIZE); // ZLP read
  expect_no_more_calls();
}

// same, with the dcd refusing the first attempt: the retry must find the headerless view intact
void test_response_from_headerless_packet_retried_after_refusal(void) {
  open_device(TUSB_SPEED_HIGH);
  uint8_t pkt[BUFSIZE];
  const uint32_t tid = start_data_out(2 * BUFSIZE - HDR, pkt);
  host_out(pkt, BUFSIZE);
  expect_call(DCD_XFER, EP_OUT, BUFSIZE);
  app.respond_at_xfer = 2;
  app.resp_nparams = 2;
  app.resp_refused_once = true;
  host_out(pkt, BUFSIZE);
  check_response(expect_call(DCD_XFER, EP_IN, HDR + 8), tid, MTP_RESP_OK, 2);
  expect_call(DCD_XFER, EP_OUT, BUFSIZE); // ZLP read
  expect_no_more_calls();
}

void test_deferred_response_after_data_complete(void) {
  open_device(TUSB_SPEED_HIGH);
  uint8_t pkt[BUFSIZE];
  const uint32_t tid = start_data_out(100, pkt);
  app.defer_response = true;
  host_out(pkt, HDR + 100);
  TEST_ASSERT_EQUAL(1, app.complete_calls);
  expect_no_more_calls(); // nothing armed while the app decides
  // respond later, from task context, with the headered container
  mtp_container_info_t io = headered_io();
  io.header->len = HDR;
  io.header->code = MTP_RESP_OK;
  TEST_ASSERT_TRUE(tud_mtp_response_send(&io));
  expect_response(tid, MTP_RESP_OK, 0);
}

void test_stray_completion_in_data_complete_stalls(void) {
  open_device(TUSB_SPEED_HIGH);
  uint8_t pkt[BUFSIZE];
  start_data_out(100, pkt);
  app.defer_response = true;
  host_out(pkt, HDR + 100);
  expect_no_more_calls();
  // the app queued its own transfer on ep_in instead of responding
  TEST_ASSERT_TRUE(usbd_edpt_claim(RHPORT, EP_IN));
  TEST_ASSERT_TRUE(usbd_edpt_xfer(RHPORT, EP_IN, pkt, 4, false));
  expect_call(DCD_XFER, EP_IN, 4);
  host_in_done(4);
  recover_from_error(EP_IN, EP_OUT);
}

void test_data_receive_refused_while_read_outstanding(void) {
  open_device(TUSB_SPEED_HIGH);
  app.queue_twice = true;
  uint8_t pkt[BUFSIZE];
  const uint32_t tid = start_data_out(100, pkt); // asserts the 2nd receive returned false
  host_out(pkt, HDR + 100);
  expect_response(tid, MTP_RESP_OK, 0);
}

//--------------------------------------------------------------------+
// Data IN
//--------------------------------------------------------------------+
static void check_data_in_len(tusb_speed_t speed, uint32_t len, bool zlp) {
  open_device(speed);
  app.data_mode = APP_SEND;
  app.data_len = len;
  const uint32_t tid = host_command(MTP_OP_GET_DEVICE_INFO, NULL, 0);
  uint32_t left = HDR + len;
  while (left) {
    const uint16_t n = (uint16_t) tu_min32(left, BUFSIZE);
    expect_call(DCD_XFER, EP_IN, n);
    expect_no_more_calls();
    host_in_done(n);
    left -= n;
  }
  if (zlp) {
    expect_call(DCD_XFER, EP_IN, 0);
    expect_no_more_calls();
    TEST_ASSERT_EQUAL(0, app.complete_calls);
    host_in_done(0);
  }
  TEST_ASSERT_EQUAL(1, app.complete_calls);
  expect_response(tid, MTP_RESP_OK, 0);
}
void test_data_in_fs_short_no_zlp(void) { check_data_in_len(TUSB_SPEED_FULL, 100, false); }
void test_data_in_fs_packet_multiple_zlp(void) { check_data_in_len(TUSB_SPEED_FULL, 64 - HDR, true); }
void test_data_in_hs_buffer_multiple_zlp(void) { check_data_in_len(TUSB_SPEED_HIGH, 2 * BUFSIZE - HDR, true); }
void test_data_in_hs_fs_packet_multiple_no_zlp(void) { check_data_in_len(TUSB_SPEED_HIGH, 64 - HDR, false); }

void test_data_in_xfer_callback_negative_stalls(void) {
  open_device(TUSB_SPEED_HIGH);
  app.data_mode = APP_SEND;
  app.data_len = 1000;
  host_command(MTP_OP_GET_DEVICE_INFO, NULL, 0);
  expect_call(DCD_XFER, EP_IN, BUFSIZE);
  app.xfer_ret = -1;
  host_in_done(BUFSIZE);
  recover_from_error(EP_IN, EP_OUT);
}

void test_data_in_complete_callback_negative_stalls(void) {
  open_device(TUSB_SPEED_HIGH);
  app.data_mode = APP_SEND;
  app.data_len = 100;
  app.complete_ret = -1;
  host_command(MTP_OP_GET_DEVICE_INFO, NULL, 0);
  expect_call(DCD_XFER, EP_IN, HDR + 100);
  host_in_done(HDR + 100);
  TEST_ASSERT_EQUAL(1, app.complete_calls);
  recover_from_error(EP_OUT, EP_IN);
}

void test_data_in_failed_transfer_stalls(void) {
  open_device(TUSB_SPEED_HIGH);
  app.data_mode = APP_SEND;
  app.data_len = 100;
  host_command(MTP_OP_GET_DEVICE_INFO, NULL, 0);
  expect_call(DCD_XFER, EP_IN, HDR + 100);
  host_complete(EP_IN, NULL, HDR + 100, XFER_RESULT_FAILED);
  TEST_ASSERT_EQUAL(0, app.complete_calls);
  recover_from_error(EP_OUT, EP_IN);
}

void test_data_send_refused_while_transfer_outstanding(void) {
  open_device(TUSB_SPEED_HIGH);
  app.data_mode = APP_SEND;
  app.data_len = 100;
  app.queue_twice = true;
  const uint32_t tid = host_command(MTP_OP_GET_DEVICE_INFO, NULL, 0);
  expect_call(DCD_XFER, EP_IN, HDR + 100);
  expect_no_more_calls();
  host_in_done(HDR + 100);
  expect_response(tid, MTP_RESP_OK, 0);
}

//--------------------------------------------------------------------+
// Cancel, Device Reset, Get Device Status, halt
//--------------------------------------------------------------------+
void test_cancel_in_data_out_rearms_read(void) {
  open_device(TUSB_SPEED_HIGH);
  uint8_t pkt[BUFSIZE];
  const uint32_t tid = start_data_out(1000, pkt);
  host_out(pkt, BUFSIZE);
  expect_call(DCD_XFER, EP_OUT, BUFSIZE); // the app's read for the rest is outstanding
  host_cancel(tid);
  TEST_ASSERT_EQUAL(1, app.cancel_calls);
  expect_no_more_calls(); // that read receives the next command
  app.data_mode = APP_NO_DATA;
  const uint32_t next = host_command(MTP_OP_OPEN_SESSION, NULL, 0);
  TEST_ASSERT_EQUAL(2, app.cmd_calls);
  expect_response(next, MTP_RESP_OK, 0);
}

// the host abandoned a ZLP it owed: the leftover read absorbs the ZLP if it still comes
void test_cancel_then_leftover_zlp(void) {
  open_device(TUSB_SPEED_HIGH);
  uint8_t pkt[BUFSIZE];
  const uint32_t tid = start_data_out(BUFSIZE - HDR, pkt);
  host_out(pkt, BUFSIZE);
  expect_call(DCD_XFER, EP_OUT, BUFSIZE); // ZLP read
  host_cancel(tid);
  expect_no_more_calls();
  host_out(NULL, 0);
  expect_command_read();
  expect_no_more_calls();
  TEST_ASSERT_EQUAL_MESSAGE(0, app.complete_calls, "the abandoned transaction must not complete");
}

void test_cancel_in_data_in_defers_read_until_in_completes(void) {
  open_device(TUSB_SPEED_HIGH);
  app.data_mode = APP_SEND;
  app.data_len = 1000;
  const uint32_t tid = host_command(MTP_OP_GET_DEVICE_INFO, NULL, 0);
  expect_call(DCD_XFER, EP_IN, BUFSIZE);
  host_cancel(tid);
  TEST_ASSERT_EQUAL(1, app.cancel_calls);
  expect_no_more_calls(); // the IN is still sending out of the shared buffer: no read yet
  // documented current behaviour: the read waits for that IN to complete
  host_in_done(BUFSIZE);
  TEST_ASSERT_EQUAL_MESSAGE(0, app.xfer_calls, "abandoned data IN must not continue");
  expect_command_read();
  expect_no_more_calls();
}

// the dcd refuses the Cancel-deferred read once the abandoned data IN completes: halt
void test_cancel_in_data_in_deferred_read_refused_stalls(void) {
  open_device(TUSB_SPEED_HIGH);
  app.data_mode = APP_SEND;
  app.data_len = 1000;
  const uint32_t tid = host_command(MTP_OP_GET_DEVICE_INFO, NULL, 0);
  expect_call(DCD_XFER, EP_IN, BUFSIZE);
  host_cancel(tid);
  expect_no_more_calls();
  dcd_refuse_ep = EP_OUT;
  host_in_done(BUFSIZE);
  TEST_ASSERT_EQUAL_MESSAGE(-1, dcd_refuse_ep, "command read never attempted");
  TEST_ASSERT_EQUAL(0, app.xfer_calls);
  recover_from_error(EP_OUT, EP_IN);
  app.data_mode = APP_NO_DATA;
  const uint32_t next = host_command(MTP_OP_OPEN_SESSION, NULL, 0);
  expect_response(next, MTP_RESP_OK, 0);
}

// libmtp 1.1.22 (ptp_read_cancel_func) polls Get Device Status for as long as it reads Device
// Busy and only then drains the Bulk-in pipe: the undrained IN must not hold the status at Busy
void test_cancel_in_data_in_status_polled_before_drain(void) {
  open_device(TUSB_SPEED_HIGH);
  app.data_mode = APP_SEND;
  app.data_len = 1000;
  const uint32_t tid = host_command(MTP_OP_GET_DEVICE_INFO, NULL, 0);
  expect_call(DCD_XFER, EP_IN, BUFSIZE);
  host_cancel(tid);
  expect_status_ok();
  expect_no_more_calls();
  host_in_done(BUFSIZE); // the drain
  expect_command_read();
  expect_no_more_calls();
  expect_status_ok();
  app.data_mode = APP_NO_DATA;
  const uint32_t next = host_command(MTP_OP_OPEN_SESSION, NULL, 0);
  expect_response(next, MTP_RESP_OK, 0);
}

void test_cancel_in_data_complete_returns_to_command(void) {
  open_device(TUSB_SPEED_HIGH);
  uint8_t pkt[BUFSIZE];
  const uint32_t tid = start_data_out(100, pkt);
  app.defer_response = true;
  host_out(pkt, HDR + 100);
  expect_no_more_calls();
  host_cancel(tid);
  expect_command_read();
  expect_no_more_calls();
}

// the dcd refuses the command read Cancel arms: halt rather than idle with nothing armed
void test_cancel_read_refused_stalls(void) {
  open_device(TUSB_SPEED_HIGH);
  uint8_t pkt[BUFSIZE];
  const uint32_t tid = start_data_out(100, pkt);
  app.defer_response = true;
  host_out(pkt, HDR + 100);
  expect_no_more_calls();
  dcd_refuse_ep = EP_OUT;
  host_cancel(tid);
  TEST_ASSERT_EQUAL_MESSAGE(-1, dcd_refuse_ep, "command read never attempted");
  TEST_ASSERT_EQUAL(1, app.cancel_calls);
  recover_from_error(EP_OUT, EP_IN);
  app.data_mode = APP_NO_DATA;
  const uint32_t next = host_command(MTP_OP_OPEN_SESSION, NULL, 0);
  expect_response(next, MTP_RESP_OK, 0);
}

void test_cancel_in_command_and_response_changes_nothing(void) {
  open_device(TUSB_SPEED_HIGH);
  host_cancel(0);
  TEST_ASSERT_EQUAL(1, app.cancel_calls);
  expect_no_more_calls(); // the command read stays armed
  const uint32_t tid = host_command(MTP_OP_OPEN_SESSION, NULL, 0);
  const dcd_call_t* resp = expect_call(DCD_XFER, EP_IN, HDR);
  host_cancel(tid);
  expect_no_more_calls(); // the response stays queued
  host_in_done(resp->len);
  expect_command_read();
  expect_no_more_calls();
}

void test_device_reset_in_data_phase(void) {
  open_device(TUSB_SPEED_HIGH);
  app.data_mode = APP_SEND;
  app.data_len = 1000;
  host_command(MTP_OP_GET_DEVICE_INFO, NULL, 0);
  expect_call(DCD_XFER, EP_IN, BUFSIZE); // busy, never drained by the host
  TEST_ASSERT_EQUAL(0, host_control(&req_reset, NULL, NULL));
  TEST_ASSERT_EQUAL(1, app.reset_calls);
  expect_bulk_aborted();
  expect_no_more_calls();
  expect_status_ok();
  app.data_mode = APP_NO_DATA;
  const uint32_t tid = host_command(MTP_OP_OPEN_SESSION, NULL, 0);
  expect_response(tid, MTP_RESP_OK, 0);
}

void test_device_reset_in_error_phase(void) {
  open_device(TUSB_SPEED_HIGH);
  app.cmd_ret = -1;
  host_command(MTP_OP_OPEN_SESSION, NULL, 0);
  expect_stall_both();
  expect_status_cancelled();
  TEST_ASSERT_EQUAL(0, host_control(&req_reset, NULL, NULL));
  expect_bulk_aborted();
  expect_no_more_calls();
  expect_status_ok();
}

// the dcd refuses the command read after Device Reset: halt rather than idle with nothing armed
void test_device_reset_read_refused_stalls(void) {
  open_device(TUSB_SPEED_HIGH);
  dcd_refuse_ep = EP_OUT;
  TEST_ASSERT_EQUAL(0, host_control(&req_reset, NULL, NULL));
  TEST_ASSERT_EQUAL_MESSAGE(-1, dcd_refuse_ep, "command read never attempted");
  TEST_ASSERT_EQUAL(1, app.reset_calls);
  expect_call(DCD_STALL, EP_OUT, 0);
  expect_call(DCD_CLEAR_STALL, EP_OUT, 0);
  expect_call(DCD_STALL, EP_IN, 0);
  expect_call(DCD_CLEAR_STALL, EP_IN, 0);
  recover_from_error(EP_IN, EP_OUT);
  const uint32_t tid = host_command(MTP_OP_OPEN_SESSION, NULL, 0);
  expect_response(tid, MTP_RESP_OK, 0);
}

// the dcd refuses the command read once the host has cleared both halts: halt again
void test_clear_halt_read_refused_stalls(void) {
  open_device(TUSB_SPEED_HIGH);
  app.cmd_ret = -1;
  host_command(MTP_OP_OPEN_SESSION, NULL, 0);
  expect_stall_both();
  host_clear_halt(EP_IN);
  expect_no_more_calls();
  dcd_refuse_ep = EP_OUT;
  host_clear_halt(EP_OUT);
  TEST_ASSERT_EQUAL_MESSAGE(-1, dcd_refuse_ep, "command read never attempted");
  recover_from_error(EP_IN, EP_OUT);
  app.cmd_ret = 0;
  const uint32_t tid = host_command(MTP_OP_OPEN_SESSION, NULL, 0);
  expect_response(tid, MTP_RESP_OK, 0);
}

void test_clear_halt_outside_error_phase_does_not_rearm(void) {
  open_device(TUSB_SPEED_HIGH);
  host_command(MTP_OP_OPEN_SESSION, NULL, 0);
  const dcd_call_t* resp = expect_call(DCD_XFER, EP_IN, HDR);
  host_clear_halt(EP_OUT); // a toggle reset while the response is queued
  expect_no_more_calls();
  host_in_done(resp->len);
  expect_command_read();
  expect_no_more_calls();
}

void test_interface_standard_requests_reach_usbd(void) {
  open_device(TUSB_SPEED_HIGH);
  uint8_t st[2] = { 0xFF, 0xFF };
  const tusb_control_request_t get_status_itf = { .bmRequestType = 0x81, .bRequest = TUSB_REQ_GET_STATUS, .wIndex = 0, .wLength = 2 };
  TEST_ASSERT_EQUAL(2, host_control(&get_status_itf, NULL, st));
  TEST_ASSERT_EQUAL_HEX8(0, st[0]);
  TEST_ASSERT_EQUAL_HEX8(0, st[1]);
  const tusb_control_request_t get_itf = { .bmRequestType = 0x81, .bRequest = TUSB_REQ_GET_INTERFACE, .wIndex = 0, .wLength = 1 };
  TEST_ASSERT_EQUAL(1, host_control(&get_itf, NULL, st));
  TEST_ASSERT_EQUAL_HEX8(0, st[0]);
  // endpoint GET_STATUS reports the halt bit usbd tracks
  app.cmd_ret = -1;
  host_command(MTP_OP_OPEN_SESSION, NULL, 0);
  expect_stall_both();
  const tusb_control_request_t get_status_ep = { .bmRequestType = 0x82, .bRequest = TUSB_REQ_GET_STATUS, .wIndex = EP_IN, .wLength = 2 };
  TEST_ASSERT_EQUAL(2, host_control(&get_status_ep, NULL, st));
  TEST_ASSERT_EQUAL_HEX8(1, st[0]);
  expect_no_more_calls();
}

//--------------------------------------------------------------------+
// mtp_container_get_string()
//--------------------------------------------------------------------+
static void check_get_string(uint8_t count, size_t cap, size_t expect_copied) {
  uint8_t wire[1 + 2 * 255];
  wire[0] = count;
  for (uint32_t i = 0; i < count; i++) {
    wire[1 + 2 * i] = (uint8_t) ('a' + i % 26);
    wire[2 + 2 * i] = (uint8_t) (i + 1); // never a NUL on the wire
  }
  uint16_t dst[256 + 1];
  memset(dst, 0xEE, sizeof(dst)); // 0xEEEE canary past cap
  TEST_ASSERT_EQUAL(1 + 2 * count, mtp_container_get_string(wire, dst, cap));
  for (size_t i = 0; i < expect_copied; i++) {
    TEST_ASSERT_EQUAL_HEX16(((i + 1) << 8) | (uint8_t) ('a' + i % 26), dst[i]);
  }
  if (cap > 0) TEST_ASSERT_EQUAL_HEX16(0, dst[expect_copied]);
  for (size_t i = cap; i < TU_ARRAY_SIZE(dst); i++) TEST_ASSERT_EQUAL_HEX16(0xEEEE, dst[i]);
}
void test_get_string_fits(void) { check_get_string(5, 16, 5); }
void test_get_string_exact_capacity_truncates_one(void) { check_get_string(16, 16, 15); }
void test_get_string_max_count_into_small_buffer(void) { check_get_string(255, 8, 7); }
void test_get_string_count_zero(void) { check_get_string(0, 8, 0); }
void test_get_string_capacity_one(void) { check_get_string(5, 1, 0); }
void test_get_string_capacity_zero_writes_nothing(void) { check_get_string(5, 0, 0); }

//--------------------------------------------------------------------+
// Refused API calls leave the transaction where it was
//--------------------------------------------------------------------+
// still in the command phase after a refusal: a data IN started now is a first block, with
// the header's length (a phase already in RESPONSE would send nothing)
static void expect_still_in_command_phase(mtp_container_info_t* io, uint32_t tid, uint32_t payload_len) {
  io->header->len = HDR + payload_len;
  TEST_ASSERT_TRUE(tud_mtp_data_send(io));
  const dcd_call_t* c = expect_call(DCD_XFER, EP_IN, HDR + payload_len);
  const mtp_generic_container_t* d = (const mtp_generic_container_t*) c->data;
  TEST_ASSERT_EQUAL(HDR + payload_len, d->header.len);
  TEST_ASSERT_EQUAL(MTP_CONTAINER_TYPE_DATA_BLOCK, d->header.type);
  TEST_ASSERT_EQUAL(tid, d->header.transaction_id);
  expect_no_more_calls();
  host_in_done(HDR + payload_len);
  TEST_ASSERT_EQUAL(1, app.complete_calls);
  expect_response(tid, MTP_RESP_OK, 0);
}

// ep_in is held by someone else when the app starts its data IN: refused, and the retry is
// still treated as the first block, taking its length from the header (a phase left in DATA
// would keep the refused call's total_len)
void test_data_send_refused_from_command_keeps_phase(void) {
  open_device(TUSB_SPEED_HIGH);
  app.silent = true;
  const uint32_t tid = host_command(MTP_OP_GET_DEVICE_INFO, NULL, 0);
  expect_no_more_calls();
  TEST_ASSERT_TRUE(usbd_edpt_claim(RHPORT, EP_IN));

  mtp_container_info_t io = headered_io();
  io.header->len = HDR + 100;
  for (uint32_t i = 0; i < 100; i++) io.payload[i] = pattern(i);
  TEST_ASSERT_FALSE(tud_mtp_data_send(&io));
  expect_no_more_calls();

  TEST_ASSERT_TRUE(usbd_edpt_release(RHPORT, EP_IN));
  expect_still_in_command_phase(&io, tid, 200);
}

// same, with the dcd refusing the transfer after the claim succeeded
void test_data_send_refused_by_dcd_keeps_phase(void) {
  open_device(TUSB_SPEED_HIGH);
  app.silent = true;
  const uint32_t tid = host_command(MTP_OP_GET_DEVICE_INFO, NULL, 0);
  mtp_container_info_t io = headered_io();
  io.header->len = HDR + 100;
  dcd_refuse_ep = EP_IN;
  TEST_ASSERT_FALSE(tud_mtp_data_send(&io));
  TEST_ASSERT_EQUAL_MESSAGE(-1, dcd_refuse_ep, "data IN never attempted");
  expect_no_more_calls();
  expect_still_in_command_phase(&io, tid, 200);
}

void test_data_receive_refused_from_command_keeps_phase(void) {
  open_device(TUSB_SPEED_HIGH);
  app.silent = true;
  const uint32_t tid = host_command(MTP_OP_SEND_OBJECT_INFO, NULL, 0);
  expect_no_more_calls();
  usbd_edpt_rx_consume(RHPORT, EP_OUT);
  TEST_ASSERT_TRUE(usbd_edpt_claim(RHPORT, EP_OUT));

  mtp_container_info_t io = headered_io();
  TEST_ASSERT_FALSE(tud_mtp_data_receive(&io));
  expect_no_more_calls();
  // still in the command phase: a Cancel has no data phase to end, so nothing is re-armed
  host_cancel(tid);
  expect_no_more_calls();

  TEST_ASSERT_TRUE(usbd_edpt_release(RHPORT, EP_OUT));
  app.silent = false;
  app.data_mode = APP_RECEIVE;
  TEST_ASSERT_TRUE(tud_mtp_data_receive(&io));
  expect_call(DCD_XFER, EP_OUT, BUFSIZE);
  expect_no_more_calls();
  uint8_t pkt[BUFSIZE];
  mtp_container_header_t* h = (mtp_container_header_t*) pkt;
  *h = (mtp_container_header_t){ .len = HDR + 100, .type = MTP_CONTAINER_TYPE_DATA_BLOCK, .code = MTP_OP_SEND_OBJECT_INFO, .transaction_id = tid };
  host_out(pkt, HDR + 100);
  TEST_ASSERT_EQUAL(1, app.xfer_calls); // the 1st packet is parsed as such: the header set total_len
  TEST_ASSERT_EQUAL(100, app.received);
  TEST_ASSERT_EQUAL(1, app.complete_calls);
  expect_response(tid, MTP_RESP_OK, 0);
}

void test_response_refused_while_in_busy_leaves_buffer(void) {
  open_device(TUSB_SPEED_HIGH);
  app.silent = true;
  const uint32_t tid = host_command(MTP_OP_OPEN_SESSION, NULL, 0);
  TEST_ASSERT_TRUE(usbd_edpt_claim(RHPORT, EP_IN));
  mtp_container_info_t io = headered_io();
  io.header->len = HDR;
  io.header->code = MTP_RESP_OK;
  uint8_t before[HDR];
  memcpy(before, io.header, HDR);
  TEST_ASSERT_FALSE(tud_mtp_response_send(&io));
  TEST_ASSERT_EQUAL_MEMORY(before, io.header, HDR);
  expect_no_more_calls();
  TEST_ASSERT_TRUE(usbd_edpt_release(RHPORT, EP_IN));
  expect_still_in_command_phase(&io, tid, 100);
}

// the dcd refuses the response transfer: nothing was queued, so the phase stays in command
void test_response_refused_by_dcd_keeps_phase(void) {
  open_device(TUSB_SPEED_HIGH);
  app.silent = true;
  const uint32_t tid = host_command(MTP_OP_OPEN_SESSION, NULL, 0);
  mtp_container_info_t io = headered_io();
  io.header->len = HDR;
  io.header->code = MTP_RESP_OK;
  dcd_refuse_ep = EP_IN;
  TEST_ASSERT_FALSE(tud_mtp_response_send(&io));
  TEST_ASSERT_EQUAL_MESSAGE(-1, dcd_refuse_ep, "response never attempted");
  expect_no_more_calls();
  expect_still_in_command_phase(&io, tid, 100);
}

void test_response_refused_for_bad_length_leaves_buffer(void) {
  open_device(TUSB_SPEED_HIGH);
  app.silent = true;
  const uint32_t tid = host_command(MTP_OP_OPEN_SESSION, NULL, 0);
  mtp_container_info_t io = headered_io();
  io.header->code = MTP_RESP_OK;
  const uint32_t bad[] = { HDR - 1, 0, BUFSIZE + 1 };
  for (size_t i = 0; i < TU_ARRAY_SIZE(bad); i++) {
    io.header->len = bad[i];
    uint8_t before[HDR];
    memcpy(before, io.header, HDR);
    TEST_ASSERT_FALSE(tud_mtp_response_send(&io));
    TEST_ASSERT_EQUAL_MEMORY(before, io.header, HDR);
    expect_no_more_calls();
  }
  expect_still_in_command_phase(&io, tid, 100);
}

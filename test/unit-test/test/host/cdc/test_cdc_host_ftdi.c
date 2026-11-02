/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 TinyUSB contributors
 * SPDX-License-Identifier: MIT
 */

#include <string.h>

#include "unity.h"
#include "cdc_host_test.h"

TEST_SOURCE_FILE("cdc_host.c")
TEST_SOURCE_FILE("tusb_fifo.c")

enum { DEV_ADDR = 1, EP_OUT = 0x02, EP_IN = 0x81, EP_SIZE = 64 };

static tusb_desc_device_t device_desc;
static tusb_control_request_t pending_setup;
static tuh_xfer_t pending_xfer;
static bool pending;
static unsigned request_count;
static unsigned callback_count;
static unsigned configured_count;
static uint8_t closed_ep[2];
static unsigned closed_count;

//--------------------------------------------------------------------+
// Host stack and bulk stream stubs. Control requests use the real CDC driver.
//--------------------------------------------------------------------+
bool tuh_vid_pid_get(uint8_t daddr, uint16_t *vid, uint16_t *pid) {
  TEST_ASSERT_EQUAL(DEV_ADDR, daddr);
  *vid = device_desc.idVendor;
  *pid = device_desc.idProduct;
  return true;
}

bool tuh_descriptor_get_device_local(uint8_t daddr, tusb_desc_device_t *desc) {
  TEST_ASSERT_EQUAL(DEV_ADDR, daddr);
  *desc = device_desc;
  return true;
}

bool tuh_edpt_open(uint8_t daddr, const tusb_desc_endpoint_t *desc_ep) {
  TEST_ASSERT_EQUAL(DEV_ADDR, daddr);
  TEST_ASSERT_TRUE(desc_ep->bEndpointAddress == EP_IN || desc_ep->bEndpointAddress == EP_OUT);
  return true;
}

bool tuh_edpt_close(uint8_t daddr, uint8_t ep_addr) {
  TEST_ASSERT_EQUAL(DEV_ADDR, daddr);
  TEST_ASSERT_LESS_THAN(TU_ARRAY_SIZE(closed_ep), closed_count);
  closed_ep[closed_count++] = ep_addr;
  return true;
}

bool tuh_control_xfer(tuh_xfer_t *xfer) {
  TEST_ASSERT_FALSE(pending);
  TEST_ASSERT_EQUAL(DEV_ADDR, xfer->daddr);
  TEST_ASSERT_EQUAL(0, xfer->ep_addr);
  TEST_ASSERT_NULL(xfer->buffer);
  TEST_ASSERT_NOT_NULL(xfer->complete_cb);
  pending_setup = *xfer->setup;
  pending_xfer = *xfer;
  pending_xfer.setup = &pending_setup;
  pending = true;
  request_count++;
  return true;
}

void usbh_driver_set_config_complete(uint8_t daddr, uint8_t itf_num) {
  TEST_ASSERT_EQUAL(DEV_ADDR, daddr);
  (void) itf_num;
  configured_count++;
}

void usbd_edpt_rx_consume(uint8_t rhport, uint8_t ep_addr) {
  (void) rhport;
  (void) ep_addr;
}

bool tu_edpt_stream_init(tu_edpt_stream_t *s, bool is_host, bool is_tx, bool overwritable, void *ff_buf,
                        uint16_t ff_bufsize, uint8_t *ep_buf) {
  s->is_host = is_host;
  s->ep_buf = ep_buf;
  (void) is_tx;
  return tu_fifo_config(&s->ff, ff_buf, ff_bufsize, overwritable);
}

uint32_t tu_edpt_stream_write(tu_edpt_stream_t *s, const void *buffer, uint32_t bufsize) {
  (void) s;
  (void) buffer;
  (void) bufsize;
  return 0;
}

uint32_t tu_edpt_stream_write_xfer(tu_edpt_stream_t *s) {
  (void) s;
  return 0;
}

bool tu_edpt_stream_write_zlp_if_needed(tu_edpt_stream_t *s, uint32_t last_xferred_bytes) {
  (void) s;
  (void) last_xferred_bytes;
  return false;
}

uint32_t tu_edpt_stream_write_available(tu_edpt_stream_t *s) {
  (void) s;
  return 0;
}

uint32_t tu_edpt_stream_read(tu_edpt_stream_t *s, void *buffer, uint32_t bufsize) {
  return tu_fifo_read_n(&s->ff, buffer, (uint16_t) bufsize);
}

uint32_t tu_edpt_stream_read_xfer(tu_edpt_stream_t *s) {
  (void) s;
  return 0;
}

//--------------------------------------------------------------------+
// Helpers
//--------------------------------------------------------------------+
static void complete_request(void) {
  TEST_ASSERT_TRUE(pending);
  // A completion may queue another request, so keep this request on the stack.
  tusb_control_request_t setup = pending_setup;
  tuh_xfer_t xfer = pending_xfer;
  xfer.setup = &setup;
  xfer.result = XFER_RESULT_SUCCESS;
  xfer.actual_len = 0;
  pending = false;
  xfer.complete_cb(&xfer);
}

static uint8_t open_ftdi(uint16_t version, uint8_t itf_num) {
  device_desc.idVendor = 0x0403;
  device_desc.idProduct = (version == 0x0500) ? 0x6010 : ((version == 0x1000) ? 0x6015 : 0x6001);
  device_desc.bcdDevice = version;
  const uint8_t desc[] = {
    9, TUSB_DESC_INTERFACE, itf_num, 0, 2, TUSB_CLASS_VENDOR_SPECIFIC, 0xff, 0xff, 0,
    7, TUSB_DESC_ENDPOINT, EP_OUT, TUSB_XFER_BULK, U16_TO_U8S_LE(EP_SIZE), 0,
    7, TUSB_DESC_ENDPOINT, EP_IN, TUSB_XFER_BULK, U16_TO_U8S_LE(EP_SIZE), 0,
  };
  TEST_ASSERT_EQUAL(sizeof(desc), cdch_open(0, DEV_ADDR, (const tusb_desc_interface_t *) desc, sizeof(desc)));
  TEST_ASSERT_TRUE(cdch_set_config(DEV_ADDR, itf_num));
  complete_request(); // RESET -> disable flow control
  complete_request(); // disable flow control -> mounted
  TEST_ASSERT_FALSE(pending);
  TEST_ASSERT_EQUAL(1, configured_count);
  const uint8_t idx = tuh_cdc_itf_get_index(DEV_ADDR, itf_num);
  TEST_ASSERT_TRUE(tuh_cdc_mounted(idx));
  request_count = 0;
  return idx;
}

static void baudrate_complete(tuh_xfer_t *xfer) {
  TEST_ASSERT_EQUAL(XFER_RESULT_SUCCESS, xfer->result);
  TEST_ASSERT_EQUAL_HEX32(0x1234, xfer->user_data);
  callback_count++;
}

static void check_baudrate(uint8_t idx, uint32_t baud, uint16_t value, uint16_t index) {
  TEST_ASSERT_TRUE(tuh_cdc_set_baudrate(idx, baud, baudrate_complete, 0x1234));
  TEST_ASSERT_TRUE(pending);
  TEST_ASSERT_EQUAL(1, request_count);
  TEST_ASSERT_EQUAL_HEX8(0x40, pending_setup.bmRequestType); // vendor, device, OUT
  TEST_ASSERT_EQUAL_HEX8(3, pending_setup.bRequest); // FTDI SET_BAUDRATE
  TEST_ASSERT_EQUAL_HEX16(value, tu_le16toh(pending_setup.wValue));
  TEST_ASSERT_EQUAL_HEX16(index, tu_le16toh(pending_setup.wIndex));
  TEST_ASSERT_EQUAL(0, pending_setup.wLength);
  TEST_ASSERT_EQUAL(0, callback_count);
  complete_request();
  TEST_ASSERT_EQUAL(1, callback_count);
  cdc_line_coding_t coding;
  TEST_ASSERT_TRUE(tuh_cdc_get_line_coding_local(idx, &coding));
  TEST_ASSERT_EQUAL_UINT32(baud, coding.bit_rate);
}

void setUp(void) {
  memset(&device_desc, 0, sizeof(device_desc));
  pending = false;
  request_count = callback_count = configured_count = 0;
  closed_count = 0;
  TEST_ASSERT_TRUE(cdch_init());
}

void tearDown(void) {
  cdch_close(DEV_ADDR);
  TEST_ASSERT_TRUE(cdch_deinit());
}

// An IN endpoint larger than CFG_TUH_CDC_RX_EPSIZE is rejected and the endpoints the pair opened are closed. The
// stubbed tuh_edpt_open() takes any size, as a real one does when the build lowers CFG_TUH_CDC_RX_EPSIZE.
void test_open_rejects_in_over_rx_epsize_and_closes_pair(void) {
  device_desc.idVendor = 0x0403;
  device_desc.idProduct = 0x6001;
  device_desc.bcdDevice = 0x0600;
  const uint8_t desc[] = {
    9, TUSB_DESC_INTERFACE, 0, 0, 2, TUSB_CLASS_VENDOR_SPECIFIC, 0xff, 0xff, 0,
    7, TUSB_DESC_ENDPOINT, EP_OUT, TUSB_XFER_BULK, U16_TO_U8S_LE(EP_SIZE), 0,
    7, TUSB_DESC_ENDPOINT, EP_IN, TUSB_XFER_BULK, U16_TO_U8S_LE(CFG_TUH_CDC_RX_EPSIZE + 1), 0,
  };
  TEST_ASSERT_EQUAL(0, cdch_open(0, DEV_ADDR, (const tusb_desc_interface_t *) desc, sizeof(desc)));
  TEST_ASSERT_EQUAL(2, closed_count);
  TEST_ASSERT_EQUAL_HEX8(EP_IN, closed_ep[0]);
  TEST_ASSERT_EQUAL_HEX8(EP_OUT, closed_ep[1]);
}

// Each 232BM-type chip encodes 3 Mbaud with divisor zero; channel encoding differs.
void test_ft232b_3mbaud_sends_zero_divisor(void) {
  check_baudrate(open_ftdi(0x0400, 0), 3000000, 0, 0);
}

void test_ft2232c_3mbaud_keeps_channel_b_with_zero_divisor(void) {
  check_baudrate(open_ftdi(0x0500, 1), 3000000, 0, 2);
}

void test_ft232r_3mbaud_sends_zero_divisor(void) {
  check_baudrate(open_ftdi(0x0600, 0), 3000000, 0, 0);
}

void test_ftx_3mbaud_keeps_channel_a_with_zero_divisor(void) {
  check_baudrate(open_ftdi(0x1000, 0), 3000000, 0, 1);
}

void test_sio_300baud_sends_zero_divisor(void) {
  check_baudrate(open_ftdi(0x0100, 0), 300, 0, 0);
}

void test_ft232b_rate_rounding_to_3mbaud_sends_zero_divisor(void) {
  check_baudrate(open_ftdi(0x0400, 0), 2900000, 0, 0);
}

void test_ft232b_nonzero_divisor_still_works(void) {
  check_baudrate(open_ftdi(0x0400, 0), 1000000, 3, 0);
}

void test_ft232b_invalid_rates_send_no_request(void) {
  const uint8_t idx = open_ftdi(0x0400, 0);
  TEST_ASSERT_FALSE(tuh_cdc_set_baudrate(idx, 0, baudrate_complete, 0x1234));
  TEST_ASSERT_FALSE(tuh_cdc_set_baudrate(idx, 3000001, baudrate_complete, 0x1234));
  TEST_ASSERT_EQUAL(0, request_count);
  TEST_ASSERT_FALSE(pending);
  TEST_ASSERT_EQUAL(0, callback_count);
}

void test_sio_unsupported_rate_sends_no_request(void) {
  const uint8_t idx = open_ftdi(0x0100, 0);
  TEST_ASSERT_FALSE(tuh_cdc_set_baudrate(idx, 12345, baudrate_complete, 0x1234));
  TEST_ASSERT_EQUAL(0, request_count);
  TEST_ASSERT_FALSE(pending);
  TEST_ASSERT_EQUAL(0, callback_count);
}

/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 TinyUSB contributors
 * SPDX-License-Identifier: MIT
 */

// vendord_open() in buffered mode on a DCD with a dedicated HW FIFO: there is no RX endpoint buffer, the RX stream
// receives straight into its software FIFO, so a bulk OUT larger than CFG_TUD_VENDOR_RX_EPSIZE is valid.

#include "unity.h"

#include "device/usbd_pvt.h"
#include "class/vendor/vendor_device.h"
TEST_SOURCE_FILE("vendor_device.c")
TEST_SOURCE_FILE("tusb.c")
TEST_SOURCE_FILE("tusb_fifo.c")

TU_VERIFY_STATIC(CFG_TUD_VENDOR_RX_EPSIZE == 64 && CFG_TUD_VENDOR_TXRX_BUFFERED && CFG_TUD_EDPT_DEDICATED_HWFIFO,
                 "test configuration");

enum { EP_OUT = 0x01, EP_IN = 0x81 };

// tusb.c, linked for the endpoint stream, references the rest of the stack
uint32_t tusb_time_millis_api(void) {
  return 0;
}

bool tud_rhport_init(uint8_t rhport, const tusb_rhport_init_t* rh_init) {
  (void) rhport;
  (void) rh_init;
  return true;
}

bool tud_inited(void) {
  return true;
}

bool tud_deinit(uint8_t rhport) {
  (void) rhport;
  return true;
}

void dcd_int_handler(uint8_t rhport) {
  (void) rhport;
}

static uint8_t  opened_count;
static uint16_t fifo_xfer_len; // largest OUT transfer armed into the FIFO

bool usbd_edpt_open(uint8_t rhport, const tusb_desc_endpoint_t* desc_ep) {
  (void) rhport;
  (void) desc_ep;
  opened_count++;
  return true;
}

void usbd_edpt_close(uint8_t rhport, uint8_t ep_addr) {
  (void) rhport;
  (void) ep_addr;
}

bool usbd_edpt_xfer(uint8_t rhport, uint8_t ep_addr, uint8_t* buffer, uint16_t total_bytes, bool is_isr) {
  (void) rhport;
  (void) ep_addr;
  (void) buffer;
  (void) total_bytes;
  (void) is_isr;
  TEST_FAIL_MESSAGE("no endpoint buffer in dedicated HW FIFO buffered mode");
  return false;
}

bool usbd_edpt_xfer_fifo(uint8_t rhport, uint8_t ep_addr, tu_fifo_t* ff, uint16_t total_bytes, bool is_isr) {
  (void) rhport;
  (void) ff;
  (void) is_isr;
  if (ep_addr == EP_OUT) {
    fifo_xfer_len = tu_max16(fifo_xfer_len, total_bytes);
  }
  return true;
}

void usbd_edpt_rx_consume(uint8_t rhport, uint8_t ep_addr) {
  (void) rhport;
  (void) ep_addr;
}

bool usbd_edpt_claim(uint8_t rhport, uint8_t ep_addr) {
  (void) rhport;
  (void) ep_addr;
  return true;
}

bool usbd_edpt_release(uint8_t rhport, uint8_t ep_addr) {
  (void) rhport;
  (void) ep_addr;
  return true;
}

bool usbd_edpt_busy(uint8_t rhport, uint8_t ep_addr) {
  (void) rhport;
  (void) ep_addr;
  return false;
}

void usbd_edpt_stall(uint8_t rhport, uint8_t ep_addr) {
  (void) rhport;
  (void) ep_addr;
}

void usbd_edpt_clear_stall(uint8_t rhport, uint8_t ep_addr) {
  (void) rhport;
  (void) ep_addr;
}

bool tud_control_status(uint8_t rhport, const tusb_control_request_t* request) {
  (void) rhport;
  (void) request;
  return true;
}

bool tud_control_xfer(uint8_t rhport, const tusb_control_request_t* request, void* buffer, uint16_t len) {
  (void) rhport;
  (void) request;
  (void) buffer;
  (void) len;
  return true;
}

bool tud_vendor_control_xfer_cb(uint8_t rhport, uint8_t stage, const tusb_control_request_t* request) {
  (void) rhport;
  (void) stage;
  (void) request;
  return false;
}

void setUp(void) {
  opened_count  = 0;
  fifo_xfer_len = 0;
  vendord_init();
}

void tearDown(void) {
  vendord_reset(0);
}

void test_vendor_open_accepts_out_mps_over_rx_epsize(void) {
  const uint8_t desc[] = {
    9, TUSB_DESC_INTERFACE, 0, 0, 2, TUSB_CLASS_VENDOR_SPECIFIC, 0, 0, 0,
    7, TUSB_DESC_ENDPOINT, EP_OUT, TUSB_XFER_BULK, U16_TO_U8S_LE(512), 0,
    7, TUSB_DESC_ENDPOINT, EP_IN, TUSB_XFER_BULK, U16_TO_U8S_LE(512), 0,
  };
  TEST_ASSERT_EQUAL(sizeof(desc), vendord_open(0, (const tusb_desc_interface_t*) desc, sizeof(desc)));
  TEST_ASSERT_EQUAL(2, opened_count);
  TEST_ASSERT_EQUAL(512, fifo_xfer_len);
}

/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 TinyUSB contributors
 * SPDX-License-Identifier: MIT
 */

// vendord_open() with a 64-byte RX endpoint buffer (CFG_TUD_VENDOR_RX_EPSIZE): every bulk OUT transfer is armed
// into that buffer, so a larger OUT max packet size is rejected before the endpoint is opened. Built twice, with and
// without CFG_TUD_VENDOR_ALT_SETTINGS (test_vendor_device_alt).

#include <string.h>

#include "unity.h"

#include "device/usbd_pvt.h"
#include "class/vendor/vendor_device.h"
TEST_SOURCE_FILE("vendor_device.c")

TU_VERIFY_STATIC(CFG_TUD_VENDOR_RX_EPSIZE == 64 && !CFG_TUD_VENDOR_TXRX_BUFFERED, "test configuration");

enum { EP_OUT = 0x01, EP_IN = 0x81 };

static uint8_t  opened_count;
static uint16_t xfer_len; // largest OUT transfer armed

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
  (void) buffer;
  (void) is_isr;
  if (ep_addr == EP_OUT) {
    xfer_len = tu_max16(xfer_len, total_bytes);
  }
  return true;
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

// one vendor interface with a bulk OUT of `out_mps` and a bulk IN of 512 bytes
static uint16_t open_vendor(uint16_t out_mps) {
  const uint8_t desc[] = {
    9, TUSB_DESC_INTERFACE, 0, 0, 2, TUSB_CLASS_VENDOR_SPECIFIC, 0, 0, 0,
    7, TUSB_DESC_ENDPOINT, EP_OUT, TUSB_XFER_BULK, U16_TO_U8S_LE(out_mps), 0,
    7, TUSB_DESC_ENDPOINT, EP_IN, TUSB_XFER_BULK, U16_TO_U8S_LE(512), 0,
  };
  return vendord_open(0, (const tusb_desc_interface_t*) desc, sizeof(desc));
}

void setUp(void) {
  opened_count = 0;
  xfer_len     = 0;
  vendord_init();
}

void tearDown(void) {
  vendord_reset(0);
}

void test_vendor_open_rejects_out_mps_over_rx_buffer(void) {
  TEST_ASSERT_EQUAL(0, open_vendor(512));
  TEST_ASSERT_EQUAL(0, xfer_len);
}

void test_vendor_open_accepts_out_mps_within_rx_buffer(void) {
  TEST_ASSERT_EQUAL(23, open_vendor(64));
  TEST_ASSERT_EQUAL(2, opened_count);
  TEST_ASSERT_LESS_OR_EQUAL(CFG_TUD_VENDOR_RX_EPSIZE, xfer_len);
}

void test_vendor_rearm_out_within_rx_buffer_after_in_endpoint(void) {
  TEST_ASSERT_EQUAL(23, open_vendor(64));
  TEST_ASSERT_TRUE(vendord_xfer_cb(0, EP_OUT, XFER_RESULT_SUCCESS, 64));
  TEST_ASSERT_LESS_OR_EQUAL(CFG_TUD_VENDOR_RX_EPSIZE, xfer_len);
}

/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 TinyUSB contributors
 * SPDX-License-Identifier: MIT
 */
#ifndef TUSB_VENDOR_DEVICE_TEST_H_
#define TUSB_VENDOR_DEVICE_TEST_H_

#include "device/usbd_pvt.h"
#include "class/vendor/vendor_device.h"

// The usbd stubs shared by the vendor device tests; each suite keeps its own usbd_edpt_xfer(). Each test file defines
// VENDOR_DEVICE_TEST_FIXTURE before including this header; the generated runner includes it without and gets only the
// declarations above.
#ifdef VENDOR_DEVICE_TEST_FIXTURE

enum { EP_OUT = 0x01, EP_IN = 0x81 };

static uint8_t opened_count;

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

#endif // VENDOR_DEVICE_TEST_FIXTURE

#endif

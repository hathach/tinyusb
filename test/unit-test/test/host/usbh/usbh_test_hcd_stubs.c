/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 TinyUSB contributors
 * SPDX-License-Identifier: MIT
 */

#include "tusb.h"
#include "host/hcd.h"

// Stateless time/HCD stubs shared by usbh suites; each suite defines its own hcd_setup_send().
uint32_t tusb_time_millis_api(void) {
  return 0;
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
  (void) rhport;
  return true;
}

void hcd_port_reset(uint8_t rhport) {
  (void) rhport;
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

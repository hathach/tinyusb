/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 Ha Thach (tinyusb.org)
 * SPDX-License-Identifier: MIT
 *
 * This file is part of the TinyUSB stack.
 */

// Focused fuzz harness for the NCM receive path. It feeds a raw NCM Transfer
// Block (NTB), i.e. host-controlled bytes off the OUT endpoint, straight into
// recv_validate_datagram() which is the function that decides whether an
// incoming NTB is well formed before the driver walks its datagram array.
//
// recv_validate_datagram() is static, so the driver is pulled in by #include so
// the harness can reach it. The referenced usbd/glue symbols are stubbed below;
// none of them are exercised by the validation path.
//
// The seed in net_ncm_seed_corpus.zip crafts an NTB whose first NDP carries a
// wLength of 0xfff0 while the transfer itself is only 64 bytes. Before the
// wNdpIndex + wLength bound was added, max_ndx was derived from that wLength and
// the ndp16_datagram[] walk ran tens of kB past ntb->data (a 3200 byte buffer);
// this reproduces that out-of-bounds read under -fsanitize=address.

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "class/net/ncm_device.c"

//--------------------------------------------------------------------+
// Stubs: referenced by the NCM driver, unused by recv_validate_datagram
//--------------------------------------------------------------------+
bool usbd_edpt_xfer(uint8_t rhport, uint8_t ep_addr, uint8_t *buffer, uint16_t total_bytes, bool is_isr) {
  (void) rhport; (void) ep_addr; (void) buffer; (void) total_bytes; (void) is_isr;
  return true;
}
bool usbd_edpt_busy(uint8_t rhport, uint8_t ep_addr) {
  (void) rhport; (void) ep_addr;
  return false;
}
bool usbd_edpt_open(uint8_t rhport, tusb_desc_endpoint_t const *desc_ep) {
  (void) rhport; (void) desc_ep;
  return true;
}
bool usbd_open_edpt_pair(uint8_t rhport, uint8_t const *p_desc, uint8_t ep_count, uint8_t xfer_type,
                         uint8_t *ep_out, uint8_t *ep_in) {
  (void) rhport; (void) p_desc; (void) ep_count; (void) xfer_type; (void) ep_out; (void) ep_in;
  return true;
}
void usbd_defer_func(osal_task_func_t func, void *param, bool in_isr) {
  (void) func; (void) param; (void) in_isr;
}
bool tud_control_xfer(uint8_t rhport, tusb_control_request_t const *request, void *buffer, uint16_t len) {
  (void) rhport; (void) request; (void) buffer; (void) len;
  return true;
}
bool tud_control_status(uint8_t rhport, tusb_control_request_t const *request) {
  (void) rhport; (void) request;
  return true;
}
tusb_speed_t tud_speed_get(void) {
  return TUSB_SPEED_FULL;
}
bool tud_network_recv_cb(const uint8_t *src, uint16_t size) {
  (void) src; (void) size;
  return true;
}
uint16_t tud_network_xmit_cb(uint8_t *dst, void *ref, uint16_t arg) {
  (void) dst; (void) ref; (void) arg;
  return 0;
}

//--------------------------------------------------------------------+
// Fuzz entry
//--------------------------------------------------------------------+
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  // Give the NTB a heap allocation of the exact receive-buffer size so any read
  // past ntb->data is caught by AddressSanitizer.
  recv_ntb_t *ntb = (recv_ntb_t *) malloc(sizeof(recv_ntb_t));
  if (ntb == NULL) {
    return 0;
  }
  size_t n = size < sizeof(ntb->data) ? size : sizeof(ntb->data);
  memset(ntb->data, 0, sizeof(ntb->data));
  memcpy(ntb->data, data, n);
  recv_validate_datagram(ntb, (uint32_t) n);
  free(ntb);
  return 0;
}

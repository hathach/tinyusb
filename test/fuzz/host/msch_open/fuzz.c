/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 Ha Thach (tinyusb.org)
 * SPDX-License-Identifier: MIT
 *
 * This file is part of the TinyUSB stack.
 */

// Fuzz harness for msch_open(), which walks a device-supplied interface and endpoint descriptor span by bLength. The
// whole input is the span (max_len = input size), copied into a heap buffer of exactly max_len bytes so
// AddressSanitizer catches any read past it whichever engine supplies the input.

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "class/msc/msc_host.c"

#define TEST_DADDR 1u

// Unlike assert(), these checks remain enabled when NDEBUG is defined.
#define CHECK(condition)                                                                \
  do {                                                                                  \
    if (!(condition)) {                                                                 \
      fprintf(stderr, "msch_open check failed at line %d: %s\n", __LINE__, #condition); \
      abort();                                                                          \
    }                                                                                   \
  } while (0)

static const uint8_t *span_begin;
static const uint8_t *span_end;
static uint8_t        open_count;
static uint8_t        opened_addr[2];

//--------------------------------------------------------------------+
// Stubs: msch_open() reaches only tuh_edpt_open(); the others resolve the driver's remaining references
//--------------------------------------------------------------------+
bool tuh_edpt_open(uint8_t daddr, tusb_desc_endpoint_t const *desc_ep) {
  const uint8_t *p = (const uint8_t *) desc_ep;
  CHECK(daddr == TEST_DADDR);
  CHECK(open_count < 2);
  CHECK(p >= span_begin && p + sizeof(tusb_desc_endpoint_t) <= span_end);
  opened_addr[open_count++] = desc_ep->bEndpointAddress;
  return true;
}

bool usbh_edpt_xfer_with_callback(uint8_t daddr, uint8_t ep_addr, uint8_t *buffer, uint16_t total_bytes,
                                  tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  (void) daddr; (void) ep_addr; (void) buffer; (void) total_bytes; (void) complete_cb; (void) user_data;
  return false;
}

bool usbh_edpt_busy(uint8_t daddr, uint8_t ep_addr) {
  (void) daddr; (void) ep_addr;
  return true;
}

bool usbh_edpt_claim(uint8_t daddr, uint8_t ep_addr) {
  (void) daddr; (void) ep_addr;
  return false;
}

bool usbh_edpt_release(uint8_t daddr, uint8_t ep_addr) {
  (void) daddr; (void) ep_addr;
  return true;
}

bool tuh_control_xfer(tuh_xfer_t *xfer) {
  (void) xfer;
  return false;
}

bool usbh_edpt_clear_stall(uint8_t daddr, uint8_t ep_addr) {
  (void) daddr; (void) ep_addr;
  return false;
}

bool tuh_connected(uint8_t daddr) {
  (void) daddr;
  return false;
}

void usbh_driver_set_config_complete(uint8_t dev_addr, uint8_t itf_num) {
  (void) dev_addr; (void) itf_num;
}

uint8_t *usbh_get_enum_buf(void) {
  return NULL;
}

uint32_t tusb_time_millis_api(void) {
  return 0;
}

//--------------------------------------------------------------------+
// Fuzz entry
//--------------------------------------------------------------------+
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  if (size > UINT16_MAX) {
    return 0;
  }
  const uint16_t max_len = (uint16_t) size;

  // malloc(0) still yields a distinct chunk, so an empty span is checked too
  uint8_t *desc = (uint8_t *) malloc(max_len);
  if (desc == NULL) {
    return 0;
  }
  memcpy(desc, data, max_len);

  span_begin = desc;
  span_end   = desc + max_len;
  open_count = 0;
  msch_init();

  const uint16_t ret = msch_open(0, TEST_DADDR, (const tusb_desc_interface_t *) desc, max_len);

  if (ret == 0) {
    // every rejection is decided before the first endpoint is opened
    CHECK(open_count == 0);
  } else {
    CHECK(ret <= max_len);
    CHECK(open_count == 2);

    // the claimed length is the interface plus both endpoints, each by its own bLength
    const uint8_t *ep1 = desc + desc[0];
    const uint8_t *ep2 = ep1 + ep1[0];
    CHECK(ret == (uint16_t) (desc[0] + ep1[0] + ep2[0]));

    const msch_interface_t *p_msc = get_itf(TEST_DADDR);
    CHECK(tu_edpt_dir(p_msc->ep_in) == TUSB_DIR_IN);
    CHECK(tu_edpt_dir(p_msc->ep_out) == TUSB_DIR_OUT);
    // opened in descriptor order, and they are the endpoints the interface records
    const uint8_t addr1 = ((const tusb_desc_endpoint_t *) ep1)->bEndpointAddress;
    const uint8_t addr2 = ((const tusb_desc_endpoint_t *) ep2)->bEndpointAddress;
    CHECK(opened_addr[0] == addr1 && opened_addr[1] == addr2);
    CHECK((addr1 == p_msc->ep_in && addr2 == p_msc->ep_out) || (addr1 == p_msc->ep_out && addr2 == p_msc->ep_in));
    CHECK(p_msc->itf_num == ((const tusb_desc_interface_t *) desc)->bInterfaceNumber);
  }

  free(desc);
  return 0;
}

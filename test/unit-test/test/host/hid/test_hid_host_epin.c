/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 TinyUSB contributors
 * SPDX-License-Identifier: MIT
 */

#include "unity.h"
#include "tusb_option.h"
#include "host/usbh.h"
#include "host/usbh_pvt.h"
#include "class/hid/hid_host.h"

TEST_SOURCE_FILE("hid_host.c")

enum {
  DADDR  = 1,
  EP_IN  = 0x81,
  EP_OUT = 0x02,
};

static uint8_t epin_open_count;
static uint8_t epout_open_count;
static uint16_t xfer_total_bytes;

bool tuh_edpt_open(uint8_t daddr, const tusb_desc_endpoint_t *desc_ep) {
  (void) daddr;
  if (tu_edpt_dir(desc_ep->bEndpointAddress) == TUSB_DIR_IN) {
    epin_open_count++;
  } else {
    epout_open_count++;
  }
  return true;
}

// only opening an interface and receiving a report touch usbh; the rest of the driver links against these
bool tuh_control_xfer(tuh_xfer_t *xfer) { (void) xfer; TEST_FAIL(); return false; }
bool tuh_edpt_abort_xfer(uint8_t daddr, uint8_t ep_addr) { (void) daddr; (void) ep_addr; TEST_FAIL(); return false; }
bool tuh_descriptor_get_hid_report(uint8_t daddr, uint8_t itf_num, uint8_t desc_type, uint8_t index, void *buffer,
                                   uint16_t len, tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  (void) daddr; (void) itf_num; (void) desc_type; (void) index; (void) buffer; (void) len;
  (void) complete_cb; (void) user_data;
  TEST_FAIL();
  return false;
}
uint8_t *usbh_get_enum_buf(void) { TEST_FAIL(); return NULL; }
void usbh_driver_set_config_complete(uint8_t dev_addr, uint8_t itf_num) { (void) dev_addr; (void) itf_num; TEST_FAIL(); }
bool usbh_edpt_claim(uint8_t dev_addr, uint8_t ep_addr) { (void) dev_addr; (void) ep_addr; return true; }
bool usbh_edpt_release(uint8_t dev_addr, uint8_t ep_addr) { (void) dev_addr; (void) ep_addr; TEST_FAIL(); return false; }
bool usbh_edpt_busy(uint8_t dev_addr, uint8_t ep_addr) { (void) dev_addr; (void) ep_addr; TEST_FAIL(); return false; }
bool usbh_edpt_xfer_with_callback(uint8_t dev_addr, uint8_t ep_addr, uint8_t *buffer, uint16_t total_bytes,
                                  tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  (void) dev_addr; (void) ep_addr; (void) buffer; (void) complete_cb; (void) user_data;
  xfer_total_bytes = total_bytes;
  return true;
}

typedef struct TU_ATTR_PACKED {
  tusb_desc_interface_t      itf;
  tusb_hid_descriptor_hid_t  hid;
  tusb_desc_endpoint_t       ep_in;
  tusb_desc_endpoint_t       ep_out;
} hid_desc_t;

static hid_desc_t make_desc(uint8_t itf_num, uint16_t in_mps) {
  hid_desc_t desc = {
    .itf    = { .bLength = sizeof(tusb_desc_interface_t), .bDescriptorType = TUSB_DESC_INTERFACE,
                .bInterfaceNumber = itf_num, .bNumEndpoints = 2, .bInterfaceClass = TUSB_CLASS_HID },
    .hid    = { .bLength = sizeof(tusb_hid_descriptor_hid_t), .bDescriptorType = HID_DESC_TYPE_HID,
                .bNumDescriptors = 1, .bReportType = HID_DESC_TYPE_REPORT, .wReportLength = 32 },
    .ep_in  = { .bLength = sizeof(tusb_desc_endpoint_t), .bDescriptorType = TUSB_DESC_ENDPOINT,
                .bEndpointAddress = EP_IN, .bmAttributes = { .xfer = TUSB_XFER_INTERRUPT },
                .wMaxPacketSize = in_mps, .bInterval = 1 },
    .ep_out = { .bLength = sizeof(tusb_desc_endpoint_t), .bDescriptorType = TUSB_DESC_ENDPOINT,
                .bEndpointAddress = EP_OUT, .bmAttributes = { .xfer = TUSB_XFER_INTERRUPT },
                .wMaxPacketSize = 64, .bInterval = 1 },
  };
  return desc;
}

void setUp(void) {
  hidh_init();
  epin_open_count  = 0;
  epout_open_count = 0;
  xfer_total_bytes = 0;
}

void tearDown(void) {
}

void test_hid_host_skips_in_mps_over_bufsize(void) {
  const hid_desc_t desc = make_desc(0, CFG_TUH_HID_EPIN_BUFSIZE + 1);
  TEST_ASSERT_EQUAL(sizeof(desc), hidh_open(0, DADDR, &desc.itf, sizeof(desc)));
  TEST_ASSERT_EQUAL(0, epin_open_count);
  TEST_ASSERT_EQUAL(1, epout_open_count);
  TEST_ASSERT_EQUAL(1, tuh_hid_itf_get_count(DADDR));

  // the unopened IN endpoint is refused before reaching usbh
  TEST_ASSERT_FALSE(tuh_hid_receive_report(DADDR, 0));
  TEST_ASSERT_FALSE(tuh_hid_receive_ready(DADDR, 0));
  TEST_ASSERT_FALSE(tuh_hid_receive_abort(DADDR, 0));
}

void test_hid_host_opens_in_mps_at_bufsize(void) {
  const hid_desc_t desc = make_desc(0, CFG_TUH_HID_EPIN_BUFSIZE);
  TEST_ASSERT_EQUAL(sizeof(desc), hidh_open(0, DADDR, &desc.itf, sizeof(desc)));
  TEST_ASSERT_EQUAL(1, epin_open_count);
  TEST_ASSERT_EQUAL(1, epout_open_count);
  TEST_ASSERT_EQUAL(1, tuh_hid_itf_get_count(DADDR));
}

void test_hid_host_receives_in_mps(void) {
  const hid_desc_t desc = make_desc(0, 8);
  TEST_ASSERT_EQUAL(sizeof(desc), hidh_open(0, DADDR, &desc.itf, sizeof(desc)));
  TEST_ASSERT_EQUAL(1, epin_open_count);

  TEST_ASSERT_TRUE(tuh_hid_receive_report(DADDR, 0));
  TEST_ASSERT_EQUAL(8, xfer_total_bytes);
}

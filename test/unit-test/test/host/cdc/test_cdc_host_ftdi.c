/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 TinyUSB contributors
 * SPDX-License-Identifier: MIT
 */

#include <string.h>

#include "unity.h"
#include "cdc_host_test.h"

TEST_SOURCE_FILE("cdc_host.c")
TEST_SOURCE_FILE("tusb_fifo.c")

enum { DEV_ADDR = 1, EP_OUT = 0x02, EP_IN = 0x81, EP_NOTIF = 0x83, EP_SIZE = 64 };

static tusb_desc_device_t device_desc;
static tusb_control_request_t pending_setup;
static tuh_xfer_t pending_xfer;
static bool pending;
static unsigned request_count;
static unsigned callback_count;
static unsigned configured_count;
static unsigned opened_count;
static unsigned umount_count;
static uint8_t  edpt_open_fail_addr; // tuh_edpt_open() fails for this endpoint, 0 for none

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
  TEST_ASSERT_TRUE(desc_ep->bEndpointAddress == EP_IN || desc_ep->bEndpointAddress == EP_OUT ||
                   desc_ep->bEndpointAddress == EP_NOTIF);
  if (desc_ep->bEndpointAddress == edpt_open_fail_addr) {
    return false;
  }
  opened_count++;
  return true;
}

void tuh_cdc_umount_cb(uint8_t idx) {
  (void) idx;
  umount_count++;
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
  opened_count = umount_count = 0;
  edpt_open_fail_addr = 0;
  TEST_ASSERT_TRUE(cdch_init());
}

void tearDown(void) {
  cdch_close(DEV_ADDR);
  TEST_ASSERT_TRUE(cdch_deinit());
}

// usbh does not close an interface it did not bind: a failed open releases the slot it reserved (CFG_TUH_CDC = 1)
void test_failed_open_releases_slot(void) {
  device_desc.idVendor = 0x0403;
  device_desc.idProduct = 0x6001;
  device_desc.bcdDevice = 0x0600;
  const uint8_t desc[] = {
    9, TUSB_DESC_INTERFACE, 0, 0, 2, TUSB_CLASS_VENDOR_SPECIFIC, 0xff, 0xff, 0,
    7, TUSB_DESC_ENDPOINT, EP_OUT, TUSB_XFER_BULK, U16_TO_U8S_LE(EP_SIZE), 0,
    7, TUSB_DESC_ENDPOINT, EP_IN, TUSB_XFER_BULK, U16_TO_U8S_LE(EP_SIZE), 0,
  };
  edpt_open_fail_addr = EP_IN;
  TEST_ASSERT_EQUAL(0, cdch_open(0, DEV_ADDR, (const tusb_desc_interface_t *) desc, sizeof(desc)));
  TEST_ASSERT_EQUAL(TUSB_INDEX_INVALID_8, tuh_cdc_itf_get_index(DEV_ADDR, 0));

  // the freed slot takes the next adapter
  edpt_open_fail_addr = 0;
  TEST_ASSERT_EQUAL(sizeof(desc), cdch_open(0, DEV_ADDR, (const tusb_desc_interface_t *) desc, sizeof(desc)));
  TEST_ASSERT_EQUAL(0, tuh_cdc_itf_get_index(DEV_ADDR, 0));
}

// usbh offers alternate setting 1 after setting 0 is open: rejecting it before reserving a slot must not release the
// slot setting 0 holds, since configuration starts only after every interface is parsed
void test_failed_alt_open_keeps_open_interface(void) {
  device_desc.idVendor = 0x0403;
  device_desc.idProduct = 0x6001;
  device_desc.bcdDevice = 0x0600;
  const uint8_t alt0[] = {
    9, TUSB_DESC_INTERFACE, 0, 0, 2, TUSB_CLASS_VENDOR_SPECIFIC, 0xff, 0xff, 0,
    7, TUSB_DESC_ENDPOINT, EP_OUT, TUSB_XFER_BULK, U16_TO_U8S_LE(EP_SIZE), 0,
    7, TUSB_DESC_ENDPOINT, EP_IN, TUSB_XFER_BULK, U16_TO_U8S_LE(EP_SIZE), 0,
  };
  const uint8_t alt1[] = {9, TUSB_DESC_INTERFACE, 0, 1, 0, TUSB_CLASS_VENDOR_SPECIFIC, 0xff, 0xff, 0};

  TEST_ASSERT_EQUAL(sizeof(alt0), cdch_open(0, DEV_ADDR, (const tusb_desc_interface_t *) alt0, sizeof(alt0)));
  TEST_ASSERT_EQUAL(0, cdch_open(0, DEV_ADDR, (const tusb_desc_interface_t *) alt1, sizeof(alt1)));
  TEST_ASSERT_EQUAL(0, tuh_cdc_itf_get_index(DEV_ADDR, 0));
}

// USB 2.0 9.5: a descriptor longer than defined is valid, its extra bytes skipped. ACM with a 9-byte notification
// endpoint, a 10-byte data interface and 9-byte (audio-style) bulk endpoints, then the next function's interface.
static const uint8_t acm_extended[] = {
  9, TUSB_DESC_INTERFACE, 0, 0, 1, TUSB_CLASS_CDC, CDC_COMM_SUBCLASS_ABSTRACT_CONTROL_MODEL, 0, 0,
  5, TUSB_DESC_CS_INTERFACE, CDC_FUNC_DESC_HEADER, U16_TO_U8S_LE(0x0120),
  4, TUSB_DESC_CS_INTERFACE, CDC_FUNC_DESC_ABSTRACT_CONTROL_MANAGEMENT, 0x02,
  9, TUSB_DESC_ENDPOINT, EP_NOTIF, TUSB_XFER_INTERRUPT, U16_TO_U8S_LE(8), 16, 0xA5, 0xA5,
  10, TUSB_DESC_INTERFACE, 1, 0, 2, TUSB_CLASS_CDC_DATA, 0, 0, 0, 0xA5,
  9, TUSB_DESC_ENDPOINT, EP_OUT, TUSB_XFER_BULK, U16_TO_U8S_LE(EP_SIZE), 0, 0xA5, 0xA5,
  9, TUSB_DESC_ENDPOINT, EP_IN, TUSB_XFER_BULK, U16_TO_U8S_LE(EP_SIZE), 0, 0xA5, 0xA5,
};
#define ACM_EXTENDED_LEN (sizeof(acm_extended))

void test_acm_open_walks_extended_descriptors(void) {
  // trailing bytes of another function are not consumed
  uint8_t desc[ACM_EXTENDED_LEN + 9];
  memcpy(desc, acm_extended, ACM_EXTENDED_LEN);
  const uint8_t next_itf[9] = {9, TUSB_DESC_INTERFACE, 2, 0, 0, TUSB_CLASS_VENDOR_SPECIFIC, 0, 0, 0};
  memcpy(desc + ACM_EXTENDED_LEN, next_itf, sizeof(next_itf));
  TEST_ASSERT_EQUAL(ACM_EXTENDED_LEN, cdch_open(0, DEV_ADDR, (const tusb_desc_interface_t *) desc, sizeof(desc)));
  TEST_ASSERT_EQUAL(3, opened_count);
}

// A descriptor whose bLength runs past the span is not opened, wherever it sits
void test_acm_open_rejects_truncated_descriptors(void) {
  const uint16_t cut[] = {
    ACM_EXTENDED_LEN - 1,      // last bulk endpoint
    ACM_EXTENDED_LEN - 9 - 1,  // first bulk endpoint
    9 + 5 + 4 + 9 - 1,         // notification endpoint
  };
  for (uint8_t i = 0; i < TU_ARRAY_SIZE(cut); i++) {
    TEST_ASSERT_EQUAL(0, cdch_open(0, DEV_ADDR, (const tusb_desc_interface_t *) acm_extended, cut[i]));
    TEST_ASSERT_EQUAL(TUSB_INDEX_INVALID_8, tuh_cdc_itf_get_index(DEV_ADDR, 0));
  }
}

// CDC 1.2 3.4.1: the notification element is an interrupt or bulk endpoint, an isochronous one is not opened
void test_acm_open_notification_endpoint_type(void) {
  uint8_t desc[ACM_EXTENDED_LEN];
  memcpy(desc, acm_extended, ACM_EXTENDED_LEN);
  uint8_t *notif_xfer = &desc[9 + 5 + 4 + 3];

  *notif_xfer = TUSB_XFER_ISOCHRONOUS;
  TEST_ASSERT_EQUAL(0, cdch_open(0, DEV_ADDR, (const tusb_desc_interface_t *) desc, sizeof(desc)));
  TEST_ASSERT_EQUAL(TUSB_INDEX_INVALID_8, tuh_cdc_itf_get_index(DEV_ADDR, 0));
  TEST_ASSERT_EQUAL(0, opened_count);

  *notif_xfer = TUSB_XFER_BULK;
  TEST_ASSERT_EQUAL(ACM_EXTENDED_LEN, cdch_open(0, DEV_ADDR, (const tusb_desc_interface_t *) desc, sizeof(desc)));
  TEST_ASSERT_EQUAL(3, opened_count);
}

void test_ftdi_open_walks_extended_endpoints(void) {
  device_desc.idVendor = 0x0403;
  device_desc.idProduct = 0x6001;
  device_desc.bcdDevice = 0x0600;
  const uint8_t desc[] = {
    9, TUSB_DESC_INTERFACE, 0, 0, 2, TUSB_CLASS_VENDOR_SPECIFIC, 0xff, 0xff, 0,
    9, TUSB_DESC_ENDPOINT, EP_OUT, TUSB_XFER_BULK, U16_TO_U8S_LE(EP_SIZE), 0, 0xA5, 0xA5,
    9, TUSB_DESC_ENDPOINT, EP_IN, TUSB_XFER_BULK, U16_TO_U8S_LE(EP_SIZE), 0, 0xA5, 0xA5,
  };
  TEST_ASSERT_EQUAL(0, cdch_open(0, DEV_ADDR, (const tusb_desc_interface_t *) desc, sizeof(desc) - 1));
  TEST_ASSERT_EQUAL(sizeof(desc), cdch_open(0, DEV_ADDR, (const tusb_desc_interface_t *) desc, sizeof(desc)));
}

// A span ending inside the second endpoint is rejected before either endpoint is opened
void test_ftdi_open_truncated_second_endpoint_opens_none(void) {
  device_desc.idVendor = 0x0403;
  device_desc.idProduct = 0x6001;
  device_desc.bcdDevice = 0x0600;
  const uint8_t desc[] = {
    9, TUSB_DESC_INTERFACE, 0, 0, 2, TUSB_CLASS_VENDOR_SPECIFIC, 0xff, 0xff, 0,
    7, TUSB_DESC_ENDPOINT, EP_OUT, TUSB_XFER_BULK, U16_TO_U8S_LE(EP_SIZE), 0,
    7, TUSB_DESC_ENDPOINT, EP_IN, TUSB_XFER_BULK, U16_TO_U8S_LE(EP_SIZE), 0,
  };
  TEST_ASSERT_EQUAL(0, cdch_open(0, DEV_ADDR, (const tusb_desc_interface_t *) desc, sizeof(desc) - 3));
  TEST_ASSERT_EQUAL(TUSB_INDEX_INVALID_8, tuh_cdc_itf_get_index(DEV_ADDR, 0));
  TEST_ASSERT_EQUAL(0, opened_count);
}

// usbh closes the drivers of a device whose configuration failed to parse: an interface the application never got
// tuh_cdc_mount_cb() for gets no tuh_cdc_umount_cb()
void test_close_unmounted_interface_skips_umount_cb(void) {
  device_desc.idVendor = 0x0403;
  device_desc.idProduct = 0x6001;
  device_desc.bcdDevice = 0x0600;
  const uint8_t desc[] = {
    9, TUSB_DESC_INTERFACE, 0, 0, 2, TUSB_CLASS_VENDOR_SPECIFIC, 0xff, 0xff, 0,
    7, TUSB_DESC_ENDPOINT, EP_OUT, TUSB_XFER_BULK, U16_TO_U8S_LE(EP_SIZE), 0,
    7, TUSB_DESC_ENDPOINT, EP_IN, TUSB_XFER_BULK, U16_TO_U8S_LE(EP_SIZE), 0,
  };
  TEST_ASSERT_EQUAL(sizeof(desc), cdch_open(0, DEV_ADDR, (const tusb_desc_interface_t *) desc, sizeof(desc)));
  cdch_close(DEV_ADDR);
  TEST_ASSERT_EQUAL(0, umount_count);
  TEST_ASSERT_EQUAL(TUSB_INDEX_INVALID_8, tuh_cdc_itf_get_index(DEV_ADDR, 0));

  open_ftdi(0x0600, 0);
  cdch_close(DEV_ADDR);
  TEST_ASSERT_EQUAL(1, umount_count);
}

// A slot freed by one device starts clean for the next: the ACM notification endpoint must not route a completion
// on that address to the FTDI interface now in the slot
void test_reused_slot_drops_previous_notification_endpoint(void) {
  const uint8_t acm[] = {
    9, TUSB_DESC_INTERFACE, 0, 0, 1, TUSB_CLASS_CDC, CDC_COMM_SUBCLASS_ABSTRACT_CONTROL_MODEL, 0, 0,
    7, TUSB_DESC_ENDPOINT, EP_NOTIF, TUSB_XFER_INTERRUPT, U16_TO_U8S_LE(8), 16,
    9, TUSB_DESC_INTERFACE, 1, 0, 2, TUSB_CLASS_CDC_DATA, 0, 0, 0,
    7, TUSB_DESC_ENDPOINT, EP_OUT, TUSB_XFER_BULK, U16_TO_U8S_LE(EP_SIZE), 0,
    7, TUSB_DESC_ENDPOINT, EP_IN, TUSB_XFER_BULK, U16_TO_U8S_LE(EP_SIZE), 0,
  };
  TEST_ASSERT_EQUAL(sizeof(acm), cdch_open(0, DEV_ADDR, (const tusb_desc_interface_t *) acm, sizeof(acm)));
  cdch_close(DEV_ADDR);

  open_ftdi(0x0600, 0);
  TEST_ASSERT_FALSE(cdch_xfer_cb(DEV_ADDR, EP_NOTIF, XFER_RESULT_SUCCESS, 0));
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

static xfer_result_t line_coding_result;

static void line_coding_complete(tuh_xfer_t *xfer) {
  TEST_ASSERT_EQUAL_HEX32(0x5678, xfer->user_data);
  line_coding_result = xfer->result;
  callback_count++;
}

// the data format stage fails to queue after the baudrate stage: the caller still gets a failed completion, as
// enumeration does when it sets the line coding on mount (#4064)
void test_line_coding_data_format_not_queued_completes_failed(void) {
  const uint8_t idx = open_ftdi(0x0400, 0);
  const cdc_line_coding_t coding = {.bit_rate = 9600, .stop_bits = 0, .parity = 0, .data_bits = 5};
  line_coding_result = XFER_RESULT_SUCCESS;

  TEST_ASSERT_TRUE(tuh_cdc_set_line_coding(idx, &coding, line_coding_complete, 0x5678));
  TEST_ASSERT_EQUAL(1, request_count);
  TEST_ASSERT_EQUAL_HEX8(3, pending_setup.bRequest); // FTDI SET_BAUDRATE
  complete_request();

  TEST_ASSERT_FALSE(pending);
  TEST_ASSERT_EQUAL(1, request_count);
  TEST_ASSERT_EQUAL(1, callback_count);
  TEST_ASSERT_EQUAL(XFER_RESULT_FAILED, line_coding_result);
}

void test_sio_unsupported_rate_sends_no_request(void) {
  const uint8_t idx = open_ftdi(0x0100, 0);
  TEST_ASSERT_FALSE(tuh_cdc_set_baudrate(idx, 12345, baudrate_complete, 0x1234));
  TEST_ASSERT_EQUAL(0, request_count);
  TEST_ASSERT_FALSE(pending);
  TEST_ASSERT_EQUAL(0, callback_count);
}

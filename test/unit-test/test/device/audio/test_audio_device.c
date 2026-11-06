/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 Mete Balci
 * SPDX-License-Identifier: MIT
 */
#include <string.h>

#include "unity.h"

#include "device/usbd_pvt.h"
#include "class/audio/audio_device.h"

TEST_SOURCE_FILE("audio_device.c")
TEST_SOURCE_FILE("tusb_fifo.c")

// A UAC2 function with one isochronous OUT endpoint on alternate setting 1 of the streaming interface. The
// class-specific descriptors are left out: the driver only needs the interface and endpoint descriptors to open
// the endpoint.
typedef struct TU_ATTR_PACKED {
  tusb_desc_interface_t ac;
  tusb_desc_interface_t as_alt0;
  tusb_desc_interface_t as_alt1;
  tusb_desc_endpoint_t  ep_out;
} audio_fixture_t;

#define EP_OUT      0x01
#define EP_OUT_SIZE 64

static const audio_fixture_t descriptor = {
  .ac      = {.bLength            = sizeof(tusb_desc_interface_t),
              .bDescriptorType    = TUSB_DESC_INTERFACE,
              .bInterfaceNumber   = 0,
              .bInterfaceClass    = TUSB_CLASS_AUDIO,
              .bInterfaceSubClass = AUDIO_SUBCLASS_CONTROL,
              .bInterfaceProtocol = AUDIO_INT_PROTOCOL_CODE_V2},
  .as_alt0 = {.bLength            = sizeof(tusb_desc_interface_t),
              .bDescriptorType    = TUSB_DESC_INTERFACE,
              .bInterfaceNumber   = 1,
              .bInterfaceClass    = TUSB_CLASS_AUDIO,
              .bInterfaceSubClass = AUDIO_SUBCLASS_STREAMING,
              .bInterfaceProtocol = AUDIO_INT_PROTOCOL_CODE_V2},
  .as_alt1 = {.bLength            = sizeof(tusb_desc_interface_t),
              .bDescriptorType    = TUSB_DESC_INTERFACE,
              .bInterfaceNumber   = 1,
              .bAlternateSetting  = 1,
              .bNumEndpoints      = 1,
              .bInterfaceClass    = TUSB_CLASS_AUDIO,
              .bInterfaceSubClass = AUDIO_SUBCLASS_STREAMING,
              .bInterfaceProtocol = AUDIO_INT_PROTOCOL_CODE_V2},
  .ep_out  = {.bLength          = sizeof(tusb_desc_endpoint_t),
              .bDescriptorType  = TUSB_DESC_ENDPOINT,
              .bEndpointAddress = EP_OUT,
              .bmAttributes     = {.xfer = TUSB_XFER_ISOCHRONOUS, .sync = 1}, // asynchronous
              .wMaxPacketSize   = EP_OUT_SIZE,
              .bInterval        = 1}};

static unsigned out_submitted; // transfers queued on EP_OUT
static uint8_t* out_buffer;    // buffer of the last transfer queued on EP_OUT
static unsigned rx_done_count;
static uint16_t rx_done_bytes;

bool usbd_edpt_iso_alloc(uint8_t rhport, uint8_t ep_addr, uint16_t largest_packet_size) {
  (void) rhport;
  (void) ep_addr;
  (void) largest_packet_size;
  return true;
}

bool usbd_edpt_iso_activate(uint8_t rhport, const tusb_desc_endpoint_t* desc_ep) {
  (void) rhport;
  (void) desc_ep;
  return true;
}

bool usbd_edpt_xfer(uint8_t rhport, uint8_t ep_addr, uint8_t* buffer, uint16_t total_bytes, bool is_isr) {
  (void) rhport;
  (void) is_isr;
  if (ep_addr == EP_OUT) {
    TEST_ASSERT_EQUAL(EP_OUT_SIZE, total_bytes);
    out_buffer = buffer;
    out_submitted++;
  }
  return true;
}

uint8_t* usbd_get_ctrl_buf(void) {
  static uint8_t ctrl_buf[CFG_TUD_ENDPOINT0_BUFSIZE];
  return ctrl_buf;
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

bool tud_audio_rx_done_isr(uint8_t rhport, uint16_t n_bytes_received, uint8_t func_id, uint8_t ep_out,
                           uint8_t cur_alt_setting) {
  (void) rhport;
  (void) func_id;
  (void) ep_out;
  (void) cur_alt_setting;
  rx_done_count++;
  rx_done_bytes = n_bytes_received;
  return true;
}

static void set_interface(uint8_t itf, uint8_t alt) {
  const tusb_control_request_t request = {.bmRequestType = 0x01,
                                          .bRequest      = TUSB_REQ_SET_INTERFACE,
                                          .wValue        = alt,
                                          .wIndex        = itf};
  TEST_ASSERT_TRUE(audiod_control_xfer_cb(0, CONTROL_STAGE_SETUP, &request));
}

// The DCD has written the packet into the buffer of the queued transfer and completes it from its ISR
static void receive_packet(const uint8_t* data, uint16_t len) {
  TEST_ASSERT_NOT_NULL(out_buffer);
  if (len > 0) {
    memcpy(out_buffer, data, len);
  }
  TEST_ASSERT_TRUE(audiod_xfer_isr(0, EP_OUT, XFER_RESULT_SUCCESS, len));
}

void setUp(void) {
  out_submitted = 0;
  out_buffer    = NULL;
  rx_done_count = 0;
  rx_done_bytes = 0xFFFF;
  audiod_init();
  TEST_ASSERT_EQUAL(sizeof(descriptor), audiod_open(0, &descriptor.ac, sizeof(descriptor)));
  set_interface(1, 1);
  TEST_ASSERT_EQUAL(1, out_submitted);
}

void tearDown(void) {
  audiod_reset(0);
}

// A zero-length packet is a Transfer Delimiter (USB Audio Data Formats 2.0, 2.1), which the function handles like a
// (micro)frame without a packet. The OUT endpoint must stay armed after one, or it receives nothing until the next
// SET_INTERFACE.
void test_audio_out_zero_length_packet_rearms_endpoint(void) {
  const uint8_t first[8]  = {1, 2, 3, 4, 5, 6, 7, 8};
  const uint8_t second[8] = {9, 10, 11, 12, 13, 14, 15, 16};

  receive_packet(first, sizeof(first));
  TEST_ASSERT_EQUAL(2, out_submitted);

  receive_packet(NULL, 0);
  TEST_ASSERT_EQUAL(3, out_submitted);
  TEST_ASSERT_EQUAL(sizeof(first), tud_audio_available());

  receive_packet(second, sizeof(second));
  TEST_ASSERT_EQUAL(4, out_submitted);

  uint8_t read[16];
  TEST_ASSERT_EQUAL(sizeof(read), tud_audio_read(read, sizeof(read)));
  TEST_ASSERT_EQUAL_UINT8_ARRAY(first, read, sizeof(first));
  TEST_ASSERT_EQUAL_UINT8_ARRAY(second, read + sizeof(first), sizeof(second));
}

void test_audio_out_zero_length_packet_invokes_rx_done(void) {
  receive_packet(NULL, 0);
  TEST_ASSERT_EQUAL(1, rx_done_count);
  TEST_ASSERT_EQUAL(0, rx_done_bytes);
}

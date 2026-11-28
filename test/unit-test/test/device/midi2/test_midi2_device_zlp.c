/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 TinyUSB contributors
 * SPDX-License-Identifier: MIT
 */

// The IN endpoint's ZLP after a transfer of whole packets, on a packet size that is not a power of 2:
// a 48-byte Interrupt IN endpoint (Bulk sizes are all powers of 2).

#include <string.h>

#include "unity.h"

#include "tusb.h"
#include "usbd.h"
TEST_SOURCE_FILE("usbd_control.c")
TEST_SOURCE_FILE("midi2_device.c")
TEST_SOURCE_FILE("tusb_fifo.c")

#include "mock_dcd.h"
#include "midi2_device_test.h"

enum {
  IN_MPS  = 48,
  OUT_MPS = 64, // the rx stream counts whole packets by masking, so its size stays a power of 2
};

// midi2_test_configuration with every endpoint Interrupt and the IN endpoint IN_MPS bytes
static uint8_t zlp_configuration[sizeof(midi2_test_configuration)];

static uint8_t  in_xfer_count;
static uint16_t in_xfer_len; // bytes of the last IN transfer

uint32_t tusb_time_millis_api(void) { return 0; }
uint8_t const* tud_descriptor_device_cb(void) { return NULL; }
uint8_t const* tud_descriptor_configuration_cb(uint8_t index) {
  (void) index;
  return zlp_configuration;
}
uint16_t const* tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
  (void) index;
  (void) langid;
  return NULL;
}

static void log_in_xfer(uint8_t ep_addr, uint16_t total_bytes) {
  if (ep_addr == EDPT_MIDI2_IN) {
    in_xfer_count++;
    in_xfer_len = total_bytes;
  }
}

static bool stub_edpt_xfer(uint8_t port, uint8_t ep_addr, uint8_t* buffer, uint16_t total_bytes, bool is_isr,
                           int cmock_num_calls) {
  (void) port; (void) buffer; (void) is_isr; (void) cmock_num_calls;
  log_in_xfer(ep_addr, total_bytes);
  return true;
}

static bool stub_edpt_xfer_fifo(uint8_t port, uint8_t ep_addr, tu_fifo_t* ff, uint16_t total_bytes, bool is_isr,
                                int cmock_num_calls) {
  (void) port; (void) is_isr; (void) cmock_num_calls;
  if (ep_addr == EDPT_MIDI2_IN) {
    tu_fifo_advance_read_pointer(ff, total_bytes); // the DCD drained the FIFO into the packet
  }
  log_in_xfer(ep_addr, total_bytes);
  return true;
}

void setUp(void) {
  memcpy(zlp_configuration, midi2_test_configuration, sizeof(zlp_configuration));
  for (uint16_t i = 0; i < sizeof(zlp_configuration); i += zlp_configuration[i]) {
    if (zlp_configuration[i + 1] == TUSB_DESC_ENDPOINT) {
      tusb_desc_endpoint_t* ep = (tusb_desc_endpoint_t*) &zlp_configuration[i];
      ep->bmAttributes.xfer = TUSB_XFER_INTERRUPT;
      ep->wMaxPacketSize = tu_edpt_dir(ep->bEndpointAddress) == TUSB_DIR_IN ? IN_MPS : OUT_MPS;
      ep->bInterval = 1;
    }
  }

  midi2_test_init();
  midi2_test_configure();
  midi2_test_set_alt(1);

  in_xfer_count = 0;
  in_xfer_len = 0xFFFF;
  dcd_edpt_xfer_Stub(stub_edpt_xfer);
  dcd_edpt_xfer_fifo_Stub(stub_edpt_xfer_fifo);
}

void tearDown(void) {}

// Queue `words` one-word UMPs as one IN transfer, then complete it with the TX FIFO left empty
static void send_and_complete(uint8_t words) {
  uint32_t ump[IN_MPS / 4];
  for (uint8_t i = 0; i < words; i++) {
    ump[i] = 0x20903C7Fu; // MIDI 1.0 channel voice message, one word
  }
  TEST_ASSERT_EQUAL(words, tud_midi2_ump_write(ump, words));
  TEST_ASSERT_EQUAL(1, in_xfer_count);
  TEST_ASSERT_EQUAL(words * 4, in_xfer_len);

  in_xfer_count = 0;
  dcd_event_xfer_complete(rhport, EDPT_MIDI2_IN, words * 4u, XFER_RESULT_SUCCESS, false);
  tud_task();
}

// A transfer that ends on a full packet needs a ZLP to end it: 48 is one, while 48 & (48 - 1) is not 0
void test_full_packet_on_non_pow2_mps_sends_zlp(void) {
  send_and_complete(IN_MPS / 4);

  TEST_ASSERT_EQUAL(1, in_xfer_count);
  TEST_ASSERT_EQUAL(0, in_xfer_len);
}

// A short packet already ends the transfer: 16 is not a multiple of 48, while 16 & (48 - 1) is 0
void test_short_packet_on_non_pow2_mps_sends_no_zlp(void) {
  send_and_complete(4);

  TEST_ASSERT_EQUAL(0, in_xfer_count);
}

/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 TinyUSB contributors
 * SPDX-License-Identifier: MIT
 */

// UMP Stream protocol negotiation replies of the midi2 device driver, with the endpoints driven through the
// dedicated hardware FIFO path the unit tests build with.

#include <string.h>

#include "unity.h"

#include "tusb.h"
#include "usbd.h"
TEST_SOURCE_FILE("usbd_control.c")
TEST_SOURCE_FILE("midi2_device.c")
TEST_SOURCE_FILE("tusb_fifo.c")

#include "mock_dcd.h"
#include "midi2_device_test.h"

MIDI2_TEST_CALLBACKS

TU_VERIFY_STATIC(CFG_TUD_EDPT_DEDICATED_HWFIFO, "the stubs below move UMP through the endpoint FIFOs");

enum {
  STREAM_CONFIG_REQUEST = 0x005,
  STREAM_CONFIG_NOTIFY  = 0x006,
};

static tu_fifo_t* rx_ff;  // where the host's OUT data lands
static tu_fifo_t* tx_ff;  // where the queued IN transfer reads from
static uint16_t   tx_len; // bytes of that IN transfer, 0 when none is queued

static bool stub_edpt_xfer_fifo(uint8_t port, uint8_t ep_addr, tu_fifo_t* ff, uint16_t total_bytes, bool is_isr,
                                int cmock_num_calls) {
  (void) port;
  (void) is_isr;
  (void) cmock_num_calls;
  if (ep_addr == EDPT_MIDI2_OUT) {
    rx_ff = ff;
  } else if (ep_addr == EDPT_MIDI2_IN) {
    TEST_ASSERT_EQUAL(0, tx_len);
    tx_ff  = ff;
    tx_len = total_bytes;
  }
  return true;
}

static void send_request(uint8_t type, uint8_t request, uint16_t value, uint16_t index) {
  const tusb_control_request_t req = {.bmRequestType = type, .bRequest = request, .wValue = value, .wIndex = index};
  dcd_event_setup_received(rhport, (const uint8_t*) &req, false);
  tud_task();
}

static void select_alt1(void) {
  rx_ff  = NULL;
  tx_ff  = NULL;
  tx_len = 0;
  dcd_edpt_xfer_fifo_Stub(stub_edpt_xfer_fifo);
  send_request(0x00, TUSB_REQ_SET_CONFIGURATION, 1, 0);
  send_request(0x01, TUSB_REQ_SET_INTERFACE, 1, ITF_MIDI2_STREAMING);
  TEST_ASSERT_EQUAL(1, tud_midi2_alt_setting());
  TEST_ASSERT_NOT_NULL(rx_ff);
}

static void host_send_ump(const uint32_t words[4]) {
  TEST_ASSERT_EQUAL(16, tu_fifo_write_n(rx_ff, words, 16));
  dcd_event_xfer_complete(rhport, EDPT_MIDI2_OUT, 16, XFER_RESULT_SUCCESS, false);
  tud_task();
}

// The host takes the queued IN transfer; returns whether it held a Stream Configuration Notify for `protocol`
static bool host_receive_config_notify(uint8_t protocol) {
  TEST_ASSERT_NOT_EQUAL(0, tx_len);
  uint32_t words[TUD_EPSIZE_BULK_MAX / 4];
  const uint16_t len = tx_len;
  TEST_ASSERT_EQUAL(len, tu_fifo_read_n(tx_ff, words, len));
  bool found = false;
  for (uint16_t i = 0; i < len / 4; i++) {
    found |= (words[i] >> 28) == 0xF && ((words[i] >> 16) & 0x3FF) == STREAM_CONFIG_NOTIFY &&
             ((words[i] >> 8) & 0xFF) == protocol;
  }
  tx_len = 0;
  dcd_event_xfer_complete(rhport, EDPT_MIDI2_IN, len, XFER_RESULT_SUCCESS, false);
  tud_task();
  return found;
}

void setUp(void) {
  midi2_test_init();
  dcd_edpt_xfer_IgnoreAndReturn(true);
  dcd_edpt_open_IgnoreAndReturn(true);
}

void tearDown(void) {}

// A Stream Configuration Request arriving while the TX FIFO is full still gets its Notify, once room frees up
void test_config_notify_waits_for_full_tx_fifo(void) {
  select_alt1();

  // the IN transfer stays queued and the application fills the FIFO behind it
  const uint32_t utility = 0x00000000; // MT 0 NOOP, one word
  while (tud_midi2_n_ump_write(0, &utility, 1) == 1) {}
  TEST_ASSERT_NOT_EQUAL(0, tx_len);

  const uint32_t request[4] = {(0xFu << 28) | (STREAM_CONFIG_REQUEST << 16) | (MIDI_PROTOCOL_MIDI1 << 8), 0, 0, 0};
  host_send_ump(request);
  TEST_ASSERT_TRUE(tud_midi2_negotiated());
  TEST_ASSERT_EQUAL(MIDI_PROTOCOL_MIDI1, tud_midi2_protocol());

  bool notified = false;
  for (uint8_t i = 0; i < 8 && tx_len != 0 && !notified; i++) {
    notified = host_receive_config_notify(MIDI_PROTOCOL_MIDI1);
  }
  TEST_ASSERT_TRUE(notified);
}

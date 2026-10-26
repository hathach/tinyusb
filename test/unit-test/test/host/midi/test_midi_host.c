/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 TinyUSB contributors
 * SPDX-License-Identifier: MIT
 */

#include <string.h>

#include "unity.h"
#include "midi_host_test.h"

TEST_SOURCE_FILE("midi_host.c")
TEST_SOURCE_FILE("tusb_fifo.c")

enum {
  MIDI_DEV_ADDR = 1,
  MIDI_ITF_NUM  = 0,
  MIDI_EP_IN    = 0x81,
  MIDI_EP_OUT   = 0x01,
  MIDI_EP_SIZE  = 64,
};

#define TEST_MIDI_ITF_DESC                                                                          \
  9, TUSB_DESC_INTERFACE, MIDI_ITF_NUM, 0, 2, TUSB_CLASS_AUDIO, AUDIO_SUBCLASS_MIDI_STREAMING, 0, 0

#define TEST_MIDI_HEADER_DESC                                                                  \
  7, TUSB_DESC_CS_INTERFACE, MIDI_CS_INTERFACE_HEADER, U16_TO_U8S_LE(0x0100), U16_TO_U8S_LE(7)

// Audio-class bulk endpoint followed by its CS endpoint with one embedded jack
#define TEST_MIDI_EP_DESC(_addr, _jack_id)                                                                      \
  9, TUSB_DESC_ENDPOINT, _addr, TUSB_XFER_BULK, U16_TO_U8S_LE(MIDI_EP_SIZE), 0, 0, 0, 5, TUSB_DESC_CS_ENDPOINT, \
    MIDI_CS_ENDPOINT_GENERAL, 1, _jack_id

static const uint8_t midi_itf_desc[] = {TEST_MIDI_ITF_DESC, TEST_MIDI_HEADER_DESC, TEST_MIDI_EP_DESC(MIDI_EP_OUT, 1),
                                        TEST_MIDI_EP_DESC(MIDI_EP_IN, 3)};

static uint8_t *rx_ep_buf;

//--------------------------------------------------------------------+
// Host stack functions called by midi_host.c
//--------------------------------------------------------------------+
bool tuh_edpt_open(uint8_t daddr, const tusb_desc_endpoint_t *desc_ep) {
  (void)daddr;
  (void)desc_ep;
  return true;
}

void usbh_driver_set_config_complete(uint8_t dev_addr, uint8_t itf_num) {
  (void)dev_addr;
  (void)itf_num;
}

// tu_edpt_stream_read_xfer_complete() references it for device streams, a host stream never calls it
void usbd_edpt_rx_consume(uint8_t rhport, uint8_t ep_addr) {
  (void)rhport;
  (void)ep_addr;
}

bool tu_edpt_stream_init(tu_edpt_stream_t *s, bool is_host, bool is_tx, bool overwritable, void *ff_buf,
                         uint16_t ff_bufsize, uint8_t *ep_buf) {
  s->is_host = is_host;
  s->ep_buf  = ep_buf;
  if (!is_tx) {
    rx_ep_buf = ep_buf;
  }
  return tu_fifo_config(&s->ff, ff_buf, ff_bufsize, overwritable);
}

uint32_t tu_edpt_stream_write(tu_edpt_stream_t *s, const void *buffer, uint32_t bufsize) {
  (void)s;
  (void)buffer;
  (void)bufsize;
  return 0;
}

uint32_t tu_edpt_stream_write_xfer(tu_edpt_stream_t *s) {
  (void)s;
  return 0;
}

bool tu_edpt_stream_write_zlp_if_needed(tu_edpt_stream_t *s, uint32_t last_xferred_bytes) {
  (void)s;
  (void)last_xferred_bytes;
  return false;
}

uint32_t tu_edpt_stream_write_available(tu_edpt_stream_t *s) {
  (void)s;
  return 0;
}

uint32_t tu_edpt_stream_read(tu_edpt_stream_t *s, void *buffer, uint32_t bufsize) {
  return tu_fifo_read_n(&s->ff, buffer, (uint16_t)bufsize);
}

uint32_t tu_edpt_stream_read_xfer(tu_edpt_stream_t *s) {
  (void)s;
  return 0;
}

//--------------------------------------------------------------------+
// Helper
//--------------------------------------------------------------------+
static void receive_packets(const uint8_t *data, uint16_t len) {
  memcpy(rx_ep_buf, data, len);
  TEST_ASSERT_TRUE(midih_xfer_cb(MIDI_DEV_ADDR, MIDI_EP_IN, XFER_RESULT_SUCCESS, len));
}

//--------------------------------------------------------------------+
// Tests
//--------------------------------------------------------------------+
void setUp(void) {
  rx_ep_buf = NULL;
  TEST_ASSERT_TRUE(midih_init());
  TEST_ASSERT_NOT_NULL(rx_ep_buf);
  TEST_ASSERT_EQUAL(sizeof(midi_itf_desc),
                    midih_open(0, MIDI_DEV_ADDR, (const tusb_desc_interface_t *)midi_itf_desc, sizeof(midi_itf_desc)));
  TEST_ASSERT_TRUE(midih_set_config(MIDI_DEV_ADDR, MIDI_ITF_NUM));
}

void tearDown(void) {
  midih_close(MIDI_DEV_ADDR);
}

// A message larger than the room left in the buffer must stay in the FIFO, not be written past bufsize
void test_stream_read_stays_within_bufsize(void) {
  const uint8_t packets[] = {
    0x0C, 0xC0, 0x05, 0x00, // program change, 2 bytes
    0x09, 0x90, 0x3C, 0x7F, // note on, 3 bytes
  };
  receive_packets(packets, sizeof(packets));

  uint8_t buf[8];
  memset(buf, 0xAA, sizeof(buf));
  uint8_t cable_num = 0xFF;

  TEST_ASSERT_EQUAL(2, tuh_midi_stream_read(0, &cable_num, buf, 4));
  TEST_ASSERT_EQUAL(0, cable_num);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(((const uint8_t[]){0xC0, 0x05}), buf, 2);
  TEST_ASSERT_EACH_EQUAL_HEX8(0xAA, buf + 2, sizeof(buf) - 2);

  TEST_ASSERT_EQUAL(3, tuh_midi_stream_read(0, &cable_num, buf, 4));
  TEST_ASSERT_EQUAL_HEX8_ARRAY(((const uint8_t[]){0x90, 0x3C, 0x7F}), buf, 3);
  TEST_ASSERT_EACH_EQUAL_HEX8(0xAA, buf + 4, sizeof(buf) - 4);

  TEST_ASSERT_EQUAL(0, tuh_midi_read_available(0));
}

// A buffer too small for a full event must not consume one from the FIFO
void test_stream_read_rejects_buffer_smaller_than_event(void) {
  const uint8_t packet[] = {0x09, 0x90, 0x3C, 0x7F};
  receive_packets(packet, sizeof(packet));

  uint8_t buf[2];
  uint8_t cable_num = 0;
  TEST_ASSERT_EQUAL(0, tuh_midi_stream_read(0, &cable_num, buf, sizeof(buf)));
  TEST_ASSERT_EQUAL(sizeof(packet), tuh_midi_read_available(0));
}

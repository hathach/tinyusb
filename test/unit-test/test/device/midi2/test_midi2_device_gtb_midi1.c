/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2026 Joel de Guzman
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

// An application Group Terminal Block that declares MIDI 1.0.

#include "unity.h"

#include "tusb.h"
#include "usbd.h"
TEST_SOURCE_FILE("usbd_control.c")
TEST_SOURCE_FILE("midi2_device.c")
TEST_SOURCE_FILE("tusb_fifo.c")

#include "mock_dcd.h"
#include "midi2_device_test.h"

MIDI2_TEST_CALLBACKS

static uint8_t const gtb_midi1[] = {
  TUD_MIDI2_GTB_HEADER(1),
  TUD_MIDI2_GTB_BLOCK_PROTOCOL(1, MIDI2_GTB_BIDIRECTIONAL, 0, 1, 0, MIDI2_GTB_PROTOCOL_MIDI1_64)
};

uint8_t const* tud_midi2_gtb_desc_cb(uint8_t itf, uint16_t* len) {
  (void) itf;
  *len = sizeof(gtb_midi1);
  return gtb_midi1;
}

void setUp(void) {
  midi2_test_init();
}

void tearDown(void) {}

// Before any Stream Configuration, the protocol in use is the one the block
// declares, so tud_midi2_protocol() agrees with what the host was told.
void test_alt1_starts_with_the_blocks_midi1_protocol(void) {
  midi2_test_configure();
  midi2_test_set_alt(1);

  TEST_ASSERT_EQUAL(1, tud_midi2_alt_setting());
  TEST_ASSERT_FALSE(tud_midi2_negotiated());
  TEST_ASSERT_EQUAL(MIDI_PROTOCOL_MIDI1, tud_midi2_protocol());
}

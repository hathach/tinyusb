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

// The default Group Terminal Block: no tud_midi2_gtb_desc_cb override.

#include "unity.h"

#include "tusb.h"
#include "usbd.h"
TEST_SOURCE_FILE("usbd_control.c")
TEST_SOURCE_FILE("midi2_device.c")
TEST_SOURCE_FILE("tusb_fifo.c")

#include "mock_dcd.h"
#include "midi2_device_test.h"

MIDI2_TEST_CALLBACKS

void setUp(void) {
  midi2_test_init();
}

void tearDown(void) {}

// A block left at 0x00 is opened as MIDI 1.0, or not at all, by macOS.
void test_default_gtb_declares_midi2(void) {
  uint16_t len = 0;
  uint8_t const* gtb = tud_midi2_gtb_desc_cb(0, &len);

  TEST_ASSERT_EQUAL(TUD_MIDI2_GTB_DESC_LEN(1), len);
  TEST_ASSERT_EQUAL(MIDI2_GRP_TRM_BLOCK_ENTRY, gtb[MIDI2_GTB_HEADER_LEN + 2]);
  TEST_ASSERT_EQUAL_HEX8(CFG_TUD_MIDI2_GTB_PROTOCOL, gtb[MIDI2_GTB_HEADER_LEN + 8]);
  TEST_ASSERT_EQUAL_HEX8(MIDI2_GTB_PROTOCOL_MIDI2, gtb[MIDI2_GTB_HEADER_LEN + 8]);
}

void test_alt1_starts_with_midi2_protocol(void) {
  midi2_test_configure();
  midi2_test_set_alt(1);

  TEST_ASSERT_EQUAL(1, tud_midi2_alt_setting());
  TEST_ASSERT_FALSE(tud_midi2_negotiated());
  TEST_ASSERT_EQUAL(MIDI_PROTOCOL_MIDI2, tud_midi2_protocol());
}

/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2026 Saulo Verissimo
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

#include "unity.h"
#include "tusb_option.h"
#include "class/midi/midi.h"
#include "class/midi/midi2_host.h"

void setUp(void) {}
void tearDown(void) {}

//--------------------------------------------------------------------+
// UMP Word Count (shared helper, defined in midi.h)
//--------------------------------------------------------------------+

void test_midi2_host_ump_word_count_1word(void) {
  uint8_t types[] = {0x0, 0x1, 0x2, 0x6, 0x7};
  for (int i = 0; i < 5; i++) {
    TEST_ASSERT_EQUAL(1, midi2_ump_word_count(types[i]));
  }
}

void test_midi2_host_ump_word_count_2word(void) {
  uint8_t types[] = {0x3, 0x4, 0x8, 0x9, 0xA};
  for (int i = 0; i < 5; i++) {
    TEST_ASSERT_EQUAL(2, midi2_ump_word_count(types[i]));
  }
}

void test_midi2_host_ump_word_count_4word(void) {
  uint8_t types[] = {0x5, 0xD, 0xE, 0xF};
  for (int i = 0; i < 4; i++) {
    TEST_ASSERT_EQUAL(4, midi2_ump_word_count(types[i]));
  }
}

//--------------------------------------------------------------------+
// Callback struct field validation
//--------------------------------------------------------------------+

void test_midi2_descriptor_cb_struct_fields(void) {
  tuh_midi2_descriptor_cb_t desc = {
    .protocol_version = 1,
    .bcdMSC_hi = 0x02,
    .bcdMSC_lo = 0x00,
    .rx_cable_count = 1,
    .tx_cable_count = 1
  };
  TEST_ASSERT_EQUAL(1, desc.protocol_version);
  TEST_ASSERT_EQUAL(0x02, desc.bcdMSC_hi);
  TEST_ASSERT_EQUAL(0x00, desc.bcdMSC_lo);
  TEST_ASSERT_EQUAL(1, desc.rx_cable_count);
  TEST_ASSERT_EQUAL(1, desc.tx_cable_count);
}

void test_midi2_mount_cb_struct_fields(void) {
  tuh_midi2_mount_cb_t mount = {
    .daddr = 1,
    .bInterfaceNumber = 0,
    .protocol_version = 1,
    .alt_setting_active = 1,
    .rx_cable_count = 2,
    .tx_cable_count = 2
  };
  TEST_ASSERT_EQUAL(1, mount.daddr);
  TEST_ASSERT_EQUAL(0, mount.bInterfaceNumber);
  TEST_ASSERT_EQUAL(1, mount.protocol_version);
  TEST_ASSERT_EQUAL(1, mount.alt_setting_active);
  TEST_ASSERT_EQUAL(2, mount.rx_cable_count);
  TEST_ASSERT_EQUAL(2, mount.tx_cable_count);
}

//--------------------------------------------------------------------+
// CS Endpoint subtypes
//--------------------------------------------------------------------+

void test_midi2_host_cs_endpoint_subtypes(void) {
  TEST_ASSERT_EQUAL(0x01, MIDI_CS_ENDPOINT_GENERAL);
  TEST_ASSERT_EQUAL(0x02, MIDI_CS_ENDPOINT_GENERAL_2_0);
}

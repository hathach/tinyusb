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
#include "tusb_types.h"
#include "class/audio/audio.h"
#include "class/midi/midi.h"
#include "device/usbd.h"

void setUp(void) {}
void tearDown(void) {}

//--------------------------------------------------------------------+
// UMP Word Count: all 16 message types
//--------------------------------------------------------------------+

void test_ump_word_count_1word_types(void) {
  uint8_t types[] = {0x0, 0x1, 0x2, 0x6, 0x7};
  for (int i = 0; i < 5; i++) {
    TEST_ASSERT_EQUAL(1, midi2_ump_word_count(types[i]));
  }
}

void test_ump_word_count_2word_types(void) {
  uint8_t types[] = {0x3, 0x4, 0x8, 0x9, 0xA};
  for (int i = 0; i < 5; i++) {
    TEST_ASSERT_EQUAL(2, midi2_ump_word_count(types[i]));
  }
}

void test_ump_word_count_3word_types(void) {
  TEST_ASSERT_EQUAL(3, midi2_ump_word_count(0xB));
  TEST_ASSERT_EQUAL(3, midi2_ump_word_count(0xC));
}

void test_ump_word_count_4word_types(void) {
  uint8_t types[] = {0x5, 0xD, 0xE, 0xF};
  for (int i = 0; i < 4; i++) {
    TEST_ASSERT_EQUAL(4, midi2_ump_word_count(types[i]));
  }
}

void test_ump_word_count_covers_all_16(void) {
  for (uint8_t mt = 0; mt <= 0xF; mt++) {
    uint8_t wc = midi2_ump_word_count(mt);
    TEST_ASSERT_TRUE(wc >= 1 && wc <= 4);
  }
}

//--------------------------------------------------------------------+
// CS Endpoint subtypes (defined in midi.h)
//--------------------------------------------------------------------+

void test_cs_endpoint_subtypes(void) {
  TEST_ASSERT_EQUAL(0x01, MIDI_CS_ENDPOINT_GENERAL);
  TEST_ASSERT_EQUAL(0x02, MIDI_CS_ENDPOINT_GENERAL_2_0);
}

//--------------------------------------------------------------------+
// Descriptor macro length calculations
//--------------------------------------------------------------------+

void test_midi1_desc_len(void) {
  TEST_ASSERT_EQUAL(TUD_MIDI_DESC_HEAD_LEN + TUD_MIDI_DESC_JACK_LEN + TUD_MIDI_DESC_EP_LEN(1) * 2,
                    TUD_MIDI_DESC_LEN);
}

void test_midi2_alt1_head_len(void) {
  TEST_ASSERT_EQUAL(16, TUD_MIDI2_DESC_ALT1_HEAD_LEN);
}

void test_midi2_alt1_ep_len(void) {
  // EP(7) + CS base(4) + numgtbs
  TEST_ASSERT_EQUAL(12, TUD_MIDI2_DESC_ALT1_EP_LEN(1));
  TEST_ASSERT_EQUAL(13, TUD_MIDI2_DESC_ALT1_EP_LEN(2));
  TEST_ASSERT_EQUAL(18, TUD_MIDI2_DESC_ALT1_EP_LEN(7));
}

void test_midi2_desc_len(void) {
  int expected = TUD_MIDI_DESC_LEN + TUD_MIDI2_DESC_ALT1_HEAD_LEN + TUD_MIDI2_DESC_ALT1_EP_LEN(1) * 2;
  TEST_ASSERT_EQUAL(expected, TUD_MIDI2_DESC_LEN);
}

void test_midi2_desc_len_greater_than_midi1(void) {
  TEST_ASSERT_TRUE(TUD_MIDI2_DESC_LEN > TUD_MIDI_DESC_LEN);
}

//--------------------------------------------------------------------+
// Descriptor macro byte validation
//--------------------------------------------------------------------+

void test_midi2_descriptor_bytes(void) {
  uint8_t desc[] = { TUD_MIDI2_DESCRIPTOR(0, 0, 0x01, 0x81, 64) };

  TEST_ASSERT_EQUAL(TUD_MIDI2_DESC_LEN, sizeof(desc));

  // First byte: Audio Control Interface descriptor length = 9
  TEST_ASSERT_EQUAL(9, desc[0]);
  TEST_ASSERT_EQUAL(TUSB_DESC_INTERFACE, desc[1]);
  TEST_ASSERT_EQUAL(0, desc[2]);

  // Find Alt Setting 1 by scanning
  int alt1_offset = -1;
  int pos = 0;
  while (pos < (int)sizeof(desc)) {
    if (desc[pos + 1] == TUSB_DESC_INTERFACE && desc[pos + 3] == 1) {
      alt1_offset = pos;
      break;
    }
    pos += desc[pos];
  }

  TEST_ASSERT_TRUE_MESSAGE(alt1_offset >= 0, "Alt Setting 1 interface not found");

  TEST_ASSERT_EQUAL(9, desc[alt1_offset]);
  TEST_ASSERT_EQUAL(TUSB_DESC_INTERFACE, desc[alt1_offset + 1]);
  TEST_ASSERT_EQUAL(1, desc[alt1_offset + 2]);   // bInterfaceNumber
  TEST_ASSERT_EQUAL(1, desc[alt1_offset + 3]);   // bAlternateSetting
  TEST_ASSERT_EQUAL(2, desc[alt1_offset + 4]);   // bNumEndpoints
  TEST_ASSERT_EQUAL(TUSB_CLASS_AUDIO, desc[alt1_offset + 5]);

  // MS Header after Alt Setting 1 interface: bcdMSC = 0x0200
  int ms2_offset = alt1_offset + 9;
  TEST_ASSERT_EQUAL(7, desc[ms2_offset]);
  TEST_ASSERT_EQUAL(TUSB_DESC_CS_INTERFACE, desc[ms2_offset + 1]);
  TEST_ASSERT_EQUAL(MIDI_CS_INTERFACE_HEADER, desc[ms2_offset + 2]);
  TEST_ASSERT_EQUAL(0x00, desc[ms2_offset + 3]);
  TEST_ASSERT_EQUAL(0x02, desc[ms2_offset + 4]);
}

void test_midi2_descriptor_alt1_cs_endpoint_subtype(void) {
  uint8_t desc[] = { TUD_MIDI2_DESCRIPTOR(0, 0, 0x01, 0x81, 64) };

  int cs_ep_count = 0;
  int pos = 0;
  while (pos < (int)sizeof(desc)) {
    if (desc[pos + 1] == TUSB_DESC_CS_ENDPOINT &&
        desc[pos + 2] == MIDI_CS_ENDPOINT_GENERAL_2_0) {
      cs_ep_count++;
      TEST_ASSERT_EQUAL(1, desc[pos + 3]);
    }
    pos += desc[pos];
  }
  TEST_ASSERT_EQUAL(2, cs_ep_count);
}

void test_midi2_descriptor_has_both_alt_settings(void) {
  uint8_t desc[] = { TUD_MIDI2_DESCRIPTOR(0, 0, 0x01, 0x81, 64) };

  int alt0_count = 0;
  int alt1_count = 0;
  int pos = 0;
  while (pos < (int)sizeof(desc)) {
    if (desc[pos + 1] == TUSB_DESC_INTERFACE) {
      if (desc[pos + 3] == 0) alt0_count++;
      if (desc[pos + 3] == 1) alt1_count++;
    }
    pos += desc[pos];
  }
  TEST_ASSERT_TRUE(alt0_count >= 2);
  TEST_ASSERT_EQUAL(1, alt1_count);
}

void test_midi2_descriptor_endpoint_addresses(void) {
  uint8_t desc[] = { TUD_MIDI2_DESCRIPTOR(0, 0, 0x02, 0x82, 64) };

  int ep_out_count = 0;
  int ep_in_count = 0;
  int pos = 0;
  while (pos < (int)sizeof(desc)) {
    if (desc[pos + 1] == TUSB_DESC_ENDPOINT) {
      uint8_t ep_addr = desc[pos + 2];
      if (ep_addr == 0x02) ep_out_count++;
      if (ep_addr == 0x82) ep_in_count++;
      TEST_ASSERT_EQUAL(TUSB_XFER_BULK, desc[pos + 3]);
      TEST_ASSERT_EQUAL(64, desc[pos + 4]);
      TEST_ASSERT_EQUAL(0, desc[pos + 5]);
    }
    pos += desc[pos];
  }
  TEST_ASSERT_EQUAL(2, ep_out_count);
  TEST_ASSERT_EQUAL(2, ep_in_count);
}

void test_midi2_descriptor_nonzero_itfnum(void) {
  uint8_t desc[] = { TUD_MIDI2_DESCRIPTOR(2, 0, 0x03, 0x83, 64) };

  TEST_ASSERT_EQUAL(2, desc[2]);

  int pos = desc[0];
  while (pos < (int)sizeof(desc)) {
    if (desc[pos + 1] == TUSB_DESC_INTERFACE) {
      TEST_ASSERT_EQUAL(3, desc[pos + 2]);
      break;
    }
    pos += desc[pos];
  }
}

//--------------------------------------------------------------------+
// Descriptor traversal integrity
//--------------------------------------------------------------------+

void test_midi2_descriptor_no_zero_length(void) {
  uint8_t desc[] = { TUD_MIDI2_DESCRIPTOR(0, 0, 0x01, 0x81, 64) };

  int pos = 0;
  int desc_count = 0;
  while (pos < (int)sizeof(desc)) {
    TEST_ASSERT_TRUE_MESSAGE(desc[pos] > 0, "Zero-length descriptor found");
    TEST_ASSERT_TRUE_MESSAGE(desc[pos] <= (int)sizeof(desc) - pos,
                             "Descriptor length exceeds remaining bytes");
    pos += desc[pos];
    desc_count++;
  }
  TEST_ASSERT_EQUAL((int)sizeof(desc), pos);
  TEST_ASSERT_TRUE(desc_count > 5);
}

void test_midi2_descriptor_valid_types(void) {
  uint8_t desc[] = { TUD_MIDI2_DESCRIPTOR(0, 0, 0x01, 0x81, 64) };

  int pos = 0;
  while (pos < (int)sizeof(desc)) {
    uint8_t dtype = desc[pos + 1];
    bool valid = (dtype == TUSB_DESC_INTERFACE ||
                  dtype == TUSB_DESC_ENDPOINT ||
                  dtype == TUSB_DESC_CS_INTERFACE ||
                  dtype == TUSB_DESC_CS_ENDPOINT);
    TEST_ASSERT_TRUE_MESSAGE(valid, "Invalid descriptor type found");
    pos += desc[pos];
  }
}

//--------------------------------------------------------------------+
// Edge cases
//--------------------------------------------------------------------+

void test_ump_word_count_with_values_beyond_0xf(void) {
  TEST_ASSERT_EQUAL(4, midi2_ump_word_count(0x10));
  TEST_ASSERT_EQUAL(4, midi2_ump_word_count(0xFF));
}

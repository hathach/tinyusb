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

// Shared by the midi2 device driver tests: a configuration with one MIDI 2.0
// function (interfaces 0 and 1), and the standard requests that configure the
// device and select an alt setting. Each test file defines the descriptor
// callbacks (MIDI2_TEST_CALLBACKS), since the test runner includes this too.

#ifndef MIDI2_DEVICE_TEST_H_
#define MIDI2_DEVICE_TEST_H_

#include "tusb.h"
#include "mock_dcd.h"

enum {
  EDPT_MIDI2_OUT = 0x01,
  EDPT_MIDI2_IN  = 0x81,
  ITF_MIDI2_STREAMING = 1,
};

static uint8_t const rhport = 0;

static uint8_t const midi2_test_configuration[] = {
  TUD_CONFIG_DESCRIPTOR(1, 2, 0, TUD_CONFIG_DESC_LEN + TUD_MIDI2_DESC_LEN, 0x00, 100),
  TUD_MIDI2_DESCRIPTOR(0, 0, EDPT_MIDI2_OUT, EDPT_MIDI2_IN, TUD_OPT_HIGH_SPEED ? 512 : 64)
};

static inline void midi2_test_init(void) {
  dcd_int_disable_Ignore();
  dcd_int_enable_Ignore();
  dcd_edpt0_setup_begin_Ignore();

  if (!tud_inited()) {
    tusb_rhport_init_t dev_init = {
      .role = TUSB_ROLE_DEVICE,
      .speed = TUSB_SPEED_AUTO
    };
    dcd_init_ExpectAndReturn(0, &dev_init, true);
    tusb_init(0, &dev_init);
  }

  dcd_event_bus_reset(rhport, TUSB_SPEED_HIGH, false);
  tud_task();
}

static inline void midi2_test_request(uint8_t type, uint8_t request, uint16_t value, uint16_t index) {
  tusb_control_request_t const req = {
    .bmRequestType = type,
    .bRequest      = request,
    .wValue        = value,
    .wIndex        = index,
    .wLength       = 0
  };

  dcd_edpt_open_IgnoreAndReturn(true);
  dcd_edpt_xfer_IgnoreAndReturn(true);
  dcd_edpt_xfer_fifo_IgnoreAndReturn(true);
  dcd_event_setup_received(rhport, (uint8_t const*) &req, false);
  tud_task();
}

static inline void midi2_test_configure(void) {
  midi2_test_request(0x00, TUSB_REQ_SET_CONFIGURATION, 1, 0);
}

static inline void midi2_test_set_alt(uint8_t alt) {
  midi2_test_request(0x01, TUSB_REQ_SET_INTERFACE, alt, ITF_MIDI2_STREAMING);
}

#define MIDI2_TEST_CALLBACKS                                              \
  uint32_t tusb_time_millis_api(void) { return 0; }                     \
  uint8_t const* tud_descriptor_device_cb(void) { return NULL; }        \
  uint8_t const* tud_descriptor_configuration_cb(uint8_t index) {       \
    (void) index;                                                       \
    return midi2_test_configuration;                                    \
  }                                                                     \
  uint16_t const* tud_descriptor_string_cb(uint8_t index, uint16_t langid) { \
    (void) index;                                                       \
    (void) langid;                                                      \
    return NULL;                                                        \
  }

#endif

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
 *
 * This file is part of the TinyUSB stack.
 */

#ifndef TUSB_MIDI2_DEVICE_H_
#define TUSB_MIDI2_DEVICE_H_

#include "class/audio/audio.h"
#include "midi.h"

//--------------------------------------------------------------------+
// Class Driver Configuration
//--------------------------------------------------------------------+

// Config defaults are in tusb_option.h:
//   CFG_TUD_MIDI2_RX_EPSIZE, CFG_TUD_MIDI2_TX_EPSIZE,
//   CFG_TUD_MIDI2_RX_BUFSIZE, CFG_TUD_MIDI2_TX_BUFSIZE,
//   CFG_TUD_MIDI2_NUM_GROUPS, CFG_TUD_MIDI2_NUM_FUNCTION_BLOCKS,
//   CFG_TUD_MIDI2_EP_NAME, CFG_TUD_MIDI2_PRODUCT_ID

#ifdef __cplusplus
extern "C" {
#endif

//--------------------------------------------------------------------+
// Application Callback API (weak, optional)
//--------------------------------------------------------------------+
void tud_midi2_rx_cb(uint8_t itf);
void tud_midi2_set_itf_cb(uint8_t itf, uint8_t alt);
bool tud_midi2_get_req_itf_cb(uint8_t rhport, const tusb_control_request_t* request);

//--------------------------------------------------------------------+
// Application API (Multiple Interfaces)
//--------------------------------------------------------------------+

bool     tud_midi2_n_mounted(uint8_t itf);
uint32_t tud_midi2_n_available(uint8_t itf);
uint8_t  tud_midi2_n_alt_setting(uint8_t itf);
bool     tud_midi2_n_negotiated(uint8_t itf);
uint8_t  tud_midi2_n_protocol(uint8_t itf);

uint32_t tud_midi2_n_ump_read(uint8_t itf, uint32_t* words, uint32_t max_words);
uint32_t tud_midi2_n_ump_write(uint8_t itf, const uint32_t* words, uint32_t count);

bool     tud_midi2_n_packet_read(uint8_t itf, uint8_t packet[4]);
bool     tud_midi2_n_packet_write(uint8_t itf, const uint8_t packet[4]);

//--------------------------------------------------------------------+
// Application API (Single Interface)
//--------------------------------------------------------------------+
TU_ATTR_ALWAYS_INLINE static inline bool tud_midi2_mounted(void) {
  return tud_midi2_n_mounted(0);
}

TU_ATTR_ALWAYS_INLINE static inline uint32_t tud_midi2_available(void) {
  return tud_midi2_n_available(0);
}

TU_ATTR_ALWAYS_INLINE static inline uint8_t tud_midi2_alt_setting(void) {
  return tud_midi2_n_alt_setting(0);
}

TU_ATTR_ALWAYS_INLINE static inline bool tud_midi2_negotiated(void) {
  return tud_midi2_n_negotiated(0);
}

TU_ATTR_ALWAYS_INLINE static inline uint8_t tud_midi2_protocol(void) {
  return tud_midi2_n_protocol(0);
}

TU_ATTR_ALWAYS_INLINE static inline uint32_t
tud_midi2_ump_read(uint32_t* words, uint32_t max_words) {
  return tud_midi2_n_ump_read(0, words, max_words);
}

TU_ATTR_ALWAYS_INLINE static inline uint32_t
tud_midi2_ump_write(const uint32_t* words, uint32_t count) {
  return tud_midi2_n_ump_write(0, words, count);
}

TU_ATTR_ALWAYS_INLINE static inline bool tud_midi2_packet_read(uint8_t packet[4]) {
  return tud_midi2_n_packet_read(0, packet);
}

TU_ATTR_ALWAYS_INLINE static inline bool tud_midi2_packet_write(const uint8_t packet[4]) {
  return tud_midi2_n_packet_write(0, packet);
}

//--------------------------------------------------------------------+
// Internal Class Driver API
//--------------------------------------------------------------------+
void     midi2d_init(void);
bool     midi2d_deinit(void);
void     midi2d_reset(uint8_t rhport);
uint16_t midi2d_open(uint8_t rhport, const tusb_desc_interface_t* itf_desc, uint16_t max_len);
bool     midi2d_control_xfer_cb(uint8_t rhport, uint8_t stage, const tusb_control_request_t* request);
bool     midi2d_xfer_cb(uint8_t rhport, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes);

#ifdef __cplusplus
}
#endif

#endif

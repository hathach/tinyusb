/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 Saulo Verissimo
 * SPDX-FileCopyrightText: Copyright (c) 2026 Ha Thach (tinyusb.org)
 * SPDX-License-Identifier: MIT
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
//   CFG_TUD_MIDI2_NUM_GROUPS,
//   CFG_TUD_MIDI2_EP_NAME, CFG_TUD_MIDI2_PRODUCT_ID

#ifdef __cplusplus
extern "C" {
#endif

//--------------------------------------------------------------------+
// Class Driver Configuration
//--------------------------------------------------------------------+

#ifndef CFG_TUD_MIDI2_TX_EPSIZE
  #define CFG_TUD_MIDI2_TX_EPSIZE   TUD_EPSIZE_BULK_MAX
#endif

#ifndef CFG_TUD_MIDI2_RX_EPSIZE
  #define CFG_TUD_MIDI2_RX_EPSIZE   TUD_EPSIZE_BULK_MAX
#endif

#ifndef CFG_TUD_MIDI2_TX_BUFSIZE
  #define CFG_TUD_MIDI2_TX_BUFSIZE  CFG_TUD_MIDI2_TX_EPSIZE
#endif

#ifndef CFG_TUD_MIDI2_RX_BUFSIZE
  #define CFG_TUD_MIDI2_RX_BUFSIZE  CFG_TUD_MIDI2_RX_EPSIZE
#endif

#ifndef CFG_TUD_MIDI2_NUM_GROUPS
  #define CFG_TUD_MIDI2_NUM_GROUPS  1
#endif

#ifndef CFG_TUD_MIDI2_EP_NAME
  #define CFG_TUD_MIDI2_EP_NAME     "TinyUSB MIDI 2.0"
#endif

#ifndef CFG_TUD_MIDI2_PRODUCT_ID
  #define CFG_TUD_MIDI2_PRODUCT_ID  "TinyUSB-MIDI2"
#endif

// String descriptor index for the Group Terminal Block (iBlockItem, Table 5-6).
// 0 = no string descriptor (default, spec-allowed).
#ifndef CFG_TUD_MIDI2_BLOCK_STRIDX
  #define CFG_TUD_MIDI2_BLOCK_STRIDX 0
#endif

//--------------------------------------------------------------------+
// Group Terminal Block descriptor builders (USB-MIDI 2.0 Table 5-5/5-6)
//--------------------------------------------------------------------+
// Group Terminal Block descriptor type and subtypes.
enum {
  MIDI2_CS_GRP_TRM_BLOCK     = 0x26,  // bDescriptorType
  MIDI2_GRP_TRM_BLOCK_HEADER = 0x01,  // bDescriptorSubtype: list header
  MIDI2_GRP_TRM_BLOCK_ENTRY  = 0x02,  // bDescriptorSubtype: block entry
};

// Block direction (bGrpTrmBlkType).
enum {
  MIDI2_GTB_BIDIRECTIONAL = 0x00,
  MIDI2_GTB_INPUT_ONLY    = 0x01,
  MIDI2_GTB_OUTPUT_ONLY   = 0x02,
};

// GTB descriptor sizes (bytes).
enum {
  MIDI2_GTB_HEADER_LEN = 5,   // list header
  MIDI2_GTB_ENTRY_LEN  = 13,  // one block entry
};

// Build a multi-block GTB descriptor for tud_midi2_gtb_desc_cb. Function Block
// Info is derived from these blocks (direction + group span).
#define TUD_MIDI2_GTB_DESC_LEN(_nblocks)  (MIDI2_GTB_HEADER_LEN + MIDI2_GTB_ENTRY_LEN * (_nblocks))
#define TUD_MIDI2_GTB_HEADER(_nblocks) \
  MIDI2_GTB_HEADER_LEN, MIDI2_CS_GRP_TRM_BLOCK, MIDI2_GRP_TRM_BLOCK_HEADER, \
  U16_TO_U8S_LE(TUD_MIDI2_GTB_DESC_LEN(_nblocks))
#define TUD_MIDI2_GTB_BLOCK(_id, _type, _first_group, _num_groups, _stridx) \
  MIDI2_GTB_ENTRY_LEN, MIDI2_CS_GRP_TRM_BLOCK, MIDI2_GRP_TRM_BLOCK_ENTRY, \
  (_id), (_type), (_first_group), (_num_groups), (_stridx), 0x00, 0, 0, 0, 0

//--------------------------------------------------------------------+
// MIDI Protocol Values (returned by tud_midi2_n_protocol)
//--------------------------------------------------------------------+

// Per USB-MIDI 2.0 spec, UMP Stream Configuration messages.
enum {
  MIDI_PROTOCOL_MIDI1 = 0x01,
  MIDI_PROTOCOL_MIDI2 = 0x02,
};

// Result of the optional UMP Stream message callback (tud_midi2_stream_msg_cb).
// PASS lets the built-in responder handle the message; the others mean the
// application answered it. NEGOTIATED_* also record the protocol so
// tud_midi2_n_protocol / tud_midi2_n_negotiated stay accurate.
typedef enum {
  MIDI2_STREAM_PASS = 0,
  MIDI2_STREAM_HANDLED,
  MIDI2_STREAM_NEGOTIATED_MIDI1,
  MIDI2_STREAM_NEGOTIATED_MIDI2,
} tud_midi2_stream_result_t;

//--------------------------------------------------------------------+
// Application Callback API (weak, optional)
//--------------------------------------------------------------------+
void tud_midi2_rx_cb(uint8_t itf);
void tud_midi2_set_itf_cb(uint8_t itf, uint8_t alt);
bool tud_midi2_get_req_itf_cb(uint8_t rhport, const tusb_control_request_t* request);

// Per-interface UMP Stream config (override for per-itf values).
const char* tud_midi2_ep_name_cb(uint8_t itf);
const char* tud_midi2_product_id_cb(uint8_t itf);

// Group Terminal Block descriptor source: the single source of truth for block
// topology. Override to expose multiple blocks with independent directions and
// group spans. Function Block Info is derived from this descriptor.
const uint8_t* tud_midi2_gtb_desc_cb(uint8_t itf, uint16_t* len);

// Optional per-block name, sent as a Function Block Name Notification during
// discovery. Return NULL or "" for no name.
const char* tud_midi2_fb_name_cb(uint8_t itf, uint8_t fb_idx);

// Optional: intercept an incoming UMP Stream message (MT 0xF). Return PASS to
// let the built-in responder handle it, or HANDLED / NEGOTIATED_* if the app
// answered it (e.g. via tud_midi2_n_ump_write). Lets an app override a single
// response without reimplementing the whole negotiation.
tud_midi2_stream_result_t tud_midi2_stream_msg_cb(uint8_t itf, const uint32_t* ump_words);

//--------------------------------------------------------------------+
// Application API (Multiple Interfaces)
//--------------------------------------------------------------------+

bool     tud_midi2_n_mounted(uint8_t itf);
uint32_t tud_midi2_n_available(uint8_t itf);
uint8_t  tud_midi2_n_alt_setting(uint8_t itf);
bool     tud_midi2_n_negotiated(uint8_t itf);
uint8_t  tud_midi2_n_protocol(uint8_t itf);

// Read up to max_words UMP words from the RX FIFO. Returns the number of
// words actually read (0 if FIFO is empty).
//
// NOTE: this function returns when max_words is reached or when the FIFO is
// empty, whichever comes first. Applications should invoke it in a loop
// until it returns 0 to guarantee the RX FIFO is fully drained per
// tud_midi2_rx_cb callback. Leaving words in the FIFO across callbacks can
// prevent subsequent bulk OUT transfers from landing.
uint32_t tud_midi2_n_ump_read(uint8_t itf, uint32_t* words, uint32_t max_words);
uint32_t tud_midi2_n_ump_write(uint8_t itf, const uint32_t* words, uint32_t count);

uint32_t tud_midi2_n_packet_read(uint8_t itf, uint8_t packets[], uint32_t max_packets);
uint32_t tud_midi2_n_packet_write(uint8_t itf, const uint8_t packets[], uint32_t count);

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

TU_ATTR_ALWAYS_INLINE static inline uint32_t
tud_midi2_packet_read(uint8_t packets[], uint32_t max_packets) {
  return tud_midi2_n_packet_read(0, packets, max_packets);
}

TU_ATTR_ALWAYS_INLINE static inline uint32_t
tud_midi2_packet_write(const uint8_t packets[], uint32_t count) {
  return tud_midi2_n_packet_write(0, packets, count);
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

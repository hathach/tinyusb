/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 Saulo Verissimo
 * SPDX-FileCopyrightText: Copyright (c) 2026 Ha Thach (tinyusb.org)
 * SPDX-License-Identifier: MIT
 *
 * This file is part of the TinyUSB stack.
 */

#include "tusb_option.h"

#if CFG_TUD_ENABLED && CFG_TUD_MIDI2

#include <string.h>

#include "device/usbd.h"
#include "device/usbd_pvt.h"
#include "midi2_device.h"

//--------------------------------------------------------------------+
// Weak stubs
//--------------------------------------------------------------------+
TU_ATTR_WEAK void tud_midi2_rx_cb(uint8_t itf) { (void) itf; }
TU_ATTR_WEAK void tud_midi2_set_itf_cb(uint8_t itf, uint8_t alt) { (void) itf; (void) alt; }
TU_ATTR_WEAK bool tud_midi2_get_req_itf_cb(uint8_t rhport, const tusb_control_request_t* request) {
  (void) rhport; (void) request; return false;
}
TU_ATTR_WEAK const char* tud_midi2_ep_name_cb(uint8_t itf) {
  (void) itf; return CFG_TUD_MIDI2_EP_NAME;
}
TU_ATTR_WEAK const char* tud_midi2_product_id_cb(uint8_t itf) {
  (void) itf; return CFG_TUD_MIDI2_PRODUCT_ID;
}
TU_ATTR_WEAK const char* tud_midi2_fb_name_cb(uint8_t itf, uint8_t fb_idx) {
  (void) itf; (void) fb_idx; return NULL;
}
TU_ATTR_WEAK tud_midi2_stream_result_t tud_midi2_stream_msg_cb(uint8_t itf, const uint32_t* ump_words) {
  (void) itf; (void) ump_words; return MIDI2_STREAM_PASS;
}

//--------------------------------------------------------------------+
// Byte order note
//--------------------------------------------------------------------+
// Per USB-MIDI 2.0 Section 3.2.2, each 32-bit UMP word is transmitted with the
// least significant byte first. This driver reads and writes UMP words as
// native uint32_t through tu_edpt_stream_read/write. All TinyUSB targets are
// little-endian, so the in-memory layout already matches the wire order and no
// swap is needed. If a big-endian target is ever supported, wrap access with
// tu_htole32 / tu_le32toh at the buffer boundary.

//--------------------------------------------------------------------+
// UMP Stream Message Constants
//--------------------------------------------------------------------+
// UMP Message Type for Stream messages (bits 31:28)
enum {
  MT_STREAM                 = 0x0F,
};

// UMP Stream Status values (10-bit, bits 25:16)
enum {
  STREAM_ENDPOINT_DISCOVERY = 0x000,
  STREAM_ENDPOINT_INFO      = 0x001,
  STREAM_EP_NAME            = 0x003,
  STREAM_PROD_INSTANCE_ID   = 0x004,
  STREAM_CONFIG_REQUEST     = 0x005,
  STREAM_CONFIG_NOTIFY      = 0x006,
  STREAM_FB_DISCOVERY       = 0x010,
  STREAM_FB_INFO            = 0x011,
  STREAM_FB_NAME            = 0x012,
};

enum {
  UMP_VER_MAJOR = 1,
  UMP_VER_MINOR = 1,
};

// Function Block Info Notification low byte: UI hint (bits 5:4) + direction
// (bits 1:0). See USB-MIDI 2.0 / UMP Function Block Info Notification.
enum {
  FB_DIR_INPUT   = 0x1,
  FB_DIR_OUTPUT  = 0x2,
  FB_DIR_BIDIR   = 0x3,
  FB_UI_RECEIVER = (1u << 4),
  FB_UI_SENDER   = (1u << 5),
};

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+
typedef struct {
  uint8_t ep_addr;
  uint16_t mps;
  tu_fifo_t ff;

#if CFG_TUD_EDPT_DEDICATED_HWFIFO == 0
  uint8_t* ep_buf;
#endif
} midi2d_tx_t;

typedef struct {
  uint8_t rhport;
  uint8_t itf_num;
  uint8_t alt_setting;
  uint8_t protocol;
  bool    negotiated;

  /*------------- From this point, data is not cleared by bus reset -------------*/
  struct {
    midi2d_tx_t      tx;
    tu_edpt_stream_t rx;

    uint8_t rx_ff_buf[CFG_TUD_MIDI2_RX_BUFSIZE];
    uint8_t tx_ff_buf[CFG_TUD_MIDI2_TX_BUFSIZE];
  } ep_stream;
} midi2d_interface_t;

// Skip local EP buffer if dedicated hw FIFO is supported
#if CFG_TUD_EDPT_DEDICATED_HWFIFO == 0
typedef struct {
  TUD_EPBUF_DEF(epin, CFG_TUD_MIDI2_TX_EPSIZE);
  TUD_EPBUF_DEF(epout, CFG_TUD_MIDI2_RX_EPSIZE);
} midi2d_epbuf_t;

CFG_TUD_MEM_SECTION static midi2d_epbuf_t _midi2d_epbuf[CFG_TUD_MIDI2];
#endif

TU_VERIFY_STATIC(CFG_TUD_MIDI2_NUM_GROUPS >= 1 && CFG_TUD_MIDI2_NUM_GROUPS <= 16,
                 "CFG_TUD_MIDI2_NUM_GROUPS must be 1..16");

#define ITF_MEM_RESET_SIZE offsetof(midi2d_interface_t, ep_stream)

static midi2d_interface_t _midi2d_itf[CFG_TUD_MIDI2];

// Default Group Terminal Block descriptor (USB-MIDI 2.0 spec, Table 5-5/5-6)
static const uint8_t _default_gtb_desc[] = {
  // GTB Header (5 bytes)
  5,                                        // bLength
  MIDI2_CS_GRP_TRM_BLOCK,                   // bDescriptorType
  MIDI2_GRP_TRM_BLOCK_HEADER,               // bDescriptorSubtype
  U16_TO_U8S_LE(18),                        // wTotalLength (5 + 13 = 18)

  // GTB Entry (13 bytes)
  13,                                       // bLength
  MIDI2_CS_GRP_TRM_BLOCK,                   // bDescriptorType
  MIDI2_GRP_TRM_BLOCK_ENTRY,                // bDescriptorSubtype
  1,                                        // bGrpTrmBlkID
  0x00,                                     // bGrpTrmBlkType: bidirectional
  0x00,                                     // nGroupTrm: first group (0)
  CFG_TUD_MIDI2_NUM_GROUPS,                 // nNumGroupTrm
  CFG_TUD_MIDI2_BLOCK_STRIDX,               // iBlockItem: string descriptor index (0 = none)
  0x00,                                     // bMIDIProtocol: unknown/not fixed
  0, 0,                                     // wMaxInputBandwidth: unknown
  0, 0                                      // wMaxOutputBandwidth: unknown
};

// Map GTB bGrpTrmBlkType to the FB Info Notification low byte (UI hint + dir).
static inline uint8_t _fb_dir_byte(uint8_t gtb_type) {
  switch (gtb_type) {
    case MIDI2_GTB_INPUT_ONLY:  return FB_UI_RECEIVER | FB_DIR_INPUT;
    case MIDI2_GTB_OUTPUT_ONLY: return FB_UI_SENDER | FB_DIR_OUTPUT;
    default:                    return FB_UI_RECEIVER | FB_UI_SENDER | FB_DIR_BIDIR;
  }
}

// Walk the GTB descriptor. Returns the number of block entries. When block
// `idx` exists, fills its type / first group / group count.
static uint8_t _gtb_blocks(const uint8_t* desc, uint16_t len, uint8_t idx,
                           uint8_t* type, uint8_t* first_group, uint8_t* num_groups) {
  uint8_t count = 0;
  uint16_t off = MIDI2_GTB_HEADER_LEN;  // skip the list header
  while (off + MIDI2_GTB_ENTRY_LEN <= len && desc[off] >= MIDI2_GTB_ENTRY_LEN) {
    if (desc[off + 2] == MIDI2_GRP_TRM_BLOCK_ENTRY) {
      if (count == idx) {
        if (type)        *type = desc[off + 4];        // bGrpTrmBlkType
        if (first_group) *first_group = desc[off + 5]; // nGroupTrm
        if (num_groups)  *num_groups = desc[off + 6];  // nNumGroupTrm
      }
      count++;
    }
    off = (uint16_t)(off + desc[off]);
  }
  return count;
}

static bool _gtb_desc_valid(const uint8_t* desc, uint16_t len) {
  return desc != NULL && len >= TUD_MIDI2_GTB_DESC_LEN(1);
}

// GTB descriptor source: the single source of truth for block topology.
// Override to expose multiple Group Terminal Blocks with independent
// directions and group spans.
TU_ATTR_WEAK const uint8_t* tud_midi2_gtb_desc_cb(uint8_t itf, uint16_t* len) {
  (void) itf;
  *len = (uint16_t) sizeof(_default_gtb_desc);
  return _default_gtb_desc;
}

//--------------------------------------------------------------------+
// Common utility functions
//--------------------------------------------------------------------+

static inline uint8_t _itf_idx(const midi2d_interface_t* p_midi) {
  return (uint8_t)(p_midi - _midi2d_itf);
}

static uint8_t _gtb_block_count(midi2d_interface_t* p_midi) {
  uint16_t len = 0;
  const uint8_t* gtb = tud_midi2_gtb_desc_cb(_itf_idx(p_midi), &len);
  TU_ASSERT(_gtb_desc_valid(gtb, len), 0);
  return _gtb_blocks(gtb, len, 0xFF, NULL, NULL, NULL);
}

static inline bool _tx_opened(const midi2d_interface_t* p_midi) {
  return p_midi->ep_stream.tx.ep_addr != 0;
}

static uint8_t _tx_byte_at(const tu_fifo_buffer_info_t* info, uint16_t offset) {
  if (offset < info->linear.len) {
    return info->linear.ptr[offset];
  }

  offset = (uint16_t) (offset - info->linear.len);
  if (offset < info->wrapped.len) {
    return info->wrapped.ptr[offset];
  }

  return 0;
}

// Calculate the largest byte count that contains only whole UMP packets and
// fits in one USB transfer (<= mps).
static uint16_t _tx_nonseg_len_to_mps(midi2d_tx_t* tx) {
  tu_fifo_buffer_info_t info;
  tu_fifo_get_read_info(&tx->ff, &info);

  const uint16_t available = (uint16_t) (info.linear.len + info.wrapped.len);
  uint16_t bytes = 0;

  while (bytes < tx->mps) {
    if ((uint16_t) (available - bytes) < 4) break;

    uint8_t mt = (uint8_t)((_tx_byte_at(&info, (uint16_t) (bytes + 3)) >> 4) & 0x0F);
    uint8_t pkt_words = midi2_ump_word_count(mt);
    uint16_t pkt_bytes = (uint16_t) pkt_words * 4;

    if (pkt_bytes == 0) break;
    if ((uint16_t) (available - bytes) < pkt_bytes) break;
    if ((uint16_t) (bytes + pkt_bytes) > tx->mps) break;

    bytes = (uint16_t) (bytes + pkt_bytes);
  }

  return bytes;
}

// Start one IN transfer capped at mps, return number of bytes queued to the controller, or 0 if nothing was queued.
static uint16_t _tx_start_xfer(midi2d_interface_t* p_midi) {
  midi2d_tx_t* tx = &p_midi->ep_stream.tx;
  uint16_t ff_count = tu_fifo_count(&tx->ff);

  if (ff_count == 0) return 0;

  if (!usbd_edpt_claim(p_midi->rhport, tx->ep_addr)) return 0;

  uint16_t bytes;
  if (p_midi->alt_setting == 1) {
    bytes = _tx_nonseg_len_to_mps(tx);
  } else {
    bytes = tu_min16(tu_fifo_count(&tx->ff), tx->mps);
  }
  if (bytes == 0) {
    usbd_edpt_release(p_midi->rhport, tx->ep_addr);
    return 0;
  }

#if CFG_TUD_EDPT_DEDICATED_HWFIFO
  TU_ASSERT(usbd_edpt_xfer_fifo(p_midi->rhport, tx->ep_addr, &tx->ff, bytes, false), 0);
#else
  tu_fifo_read_n(&tx->ff, tx->ep_buf, bytes);
  TU_ASSERT(usbd_edpt_xfer(p_midi->rhport, tx->ep_addr, tx->ep_buf, bytes, false), 0);
#endif

  return bytes;
}

static uint32_t _tx_ump_write(midi2d_interface_t* p_midi, const uint32_t* words, uint32_t count) {
  uint32_t written = 0;
  while (written < count) {
    uint8_t mt = (uint8_t)((words[written] >> 28) & 0x0F);
    uint8_t pkt_words = midi2_ump_word_count(mt);
    uint16_t pkt_bytes = (uint16_t) pkt_words * 4;

    if (written + pkt_words > count) break;
    if (tu_fifo_remaining(&p_midi->ep_stream.tx.ff) < pkt_bytes) break;

    if (tu_fifo_write_n(&p_midi->ep_stream.tx.ff, &words[written], pkt_bytes) != pkt_bytes) break;
    written += pkt_words;
  }

  (void) _tx_start_xfer(p_midi);
  return written;
}

//--------------------------------------------------------------------+
// Protocol Negotiation
//--------------------------------------------------------------------+
static void _nego_send_ump(midi2d_interface_t* p_midi, const uint32_t* words, uint8_t count) {
  if (!_tx_opened(p_midi)) return;
  if (tu_fifo_remaining(&p_midi->ep_stream.tx.ff) < (uint32_t) count * 4) return;
  (void) _tx_ump_write(p_midi, words, count);
}

static void _nego_send_endpoint_info(midi2d_interface_t* p_midi) {
  uint32_t msg[4] = {0};
  msg[0] = ((uint32_t) MT_STREAM << 28)
         | ((uint32_t) STREAM_ENDPOINT_INFO << 16)
         | ((uint32_t) UMP_VER_MAJOR << 8)
         | (uint32_t) UMP_VER_MINOR;
  msg[1] = (UINT32_C(1) << 31)  // Static Function Blocks flag
         | ((uint32_t)(_gtb_block_count(p_midi) & 0x7F) << 24)
         | (UINT32_C(1) << 9)   // MIDI 2.0 Protocol capability
         | (UINT32_C(1) << 8);  // MIDI 1.0 Protocol capability
  _nego_send_ump(p_midi, msg, 4);
}

// Send a UMP Stream text notification, multi-packet (Complete/Start/Continue/
// End in the Format field). When has_index is set, word0 bits 15:8 carry an
// index byte (the Function Block number for FB Name) and 13 chars fit per
// packet; otherwise the text starts there and 14 chars fit (Endpoint Name,
// Product Instance Id).
static void _nego_send_stream_text(midi2d_interface_t* p_midi, uint16_t status,
                                   bool has_index, uint8_t index, const char* str) {
  if (!str || str[0] == '\0') return;

  uint16_t total_len = (uint16_t) strlen(str);
  uint16_t offset = 0;
  const uint8_t per_pkt = has_index ? 13 : 14;
  const uint8_t head_chars = has_index ? 1 : 2;  // chars carried in word0

  while (offset < total_len) {
    uint16_t remaining = total_len - offset;
    uint8_t n = (uint8_t)((remaining > per_pkt) ? per_pkt : remaining);
    bool is_first = (offset == 0);
    bool is_last  = (remaining <= per_pkt);

    uint8_t form;
    if (is_first && is_last) form = 0;
    else if (is_first)       form = 1;
    else if (is_last)        form = 3;
    else                     form = 2;

    uint32_t msg[4] = {0};
    msg[0] = ((uint32_t) MT_STREAM << 28)
           | ((uint32_t) form << 26)
           | ((uint32_t) status << 16);

    const char* p = str + offset;
    if (has_index) {
      msg[0] |= ((uint32_t) index << 8);                       // bits 15:8 = index
      if (n > 0) msg[0] |= (uint32_t)(uint8_t) p[0];           // bits  7:0 = char 0
    } else {
      if (n > 0) msg[0] |= ((uint32_t)(uint8_t) p[0] << 8);    // bits 15:8 = char 0
      if (n > 1) msg[0] |= (uint32_t)(uint8_t) p[1];           // bits  7:0 = char 1
    }
    for (uint8_t i = head_chars; i < n; i++) {
      uint8_t word_idx = (uint8_t)(1 + (i - head_chars) / 4);
      uint8_t shift    = (uint8_t)(24 - ((i - head_chars) % 4) * 8);
      msg[word_idx] |= ((uint32_t)(uint8_t) p[i] << shift);
    }

    _nego_send_ump(p_midi, msg, 4);
    offset += n;
  }
}

static void _nego_send_config_notify(midi2d_interface_t* p_midi, uint8_t protocol) {
  uint32_t msg[4] = {0};
  msg[0] = ((uint32_t) MT_STREAM << 28)
         | ((uint32_t) STREAM_CONFIG_NOTIFY << 16)
         | ((uint32_t) protocol << 8);
  _nego_send_ump(p_midi, msg, 4);
}

static void _nego_send_fb_info(midi2d_interface_t* p_midi, uint8_t fb_idx) {
  // Derive direction and group span for this block from the GTB descriptor.
  uint16_t gtb_len = 0;
  const uint8_t* gtb = tud_midi2_gtb_desc_cb(_itf_idx(p_midi), &gtb_len);
  TU_ASSERT(_gtb_desc_valid(gtb, gtb_len),);
  uint8_t type = 0x00, first_group = 0, num_groups = (uint8_t) CFG_TUD_MIDI2_NUM_GROUPS;
  _gtb_blocks(gtb, gtb_len, fb_idx, &type, &first_group, &num_groups);

  uint32_t msg[4] = {0};
  msg[0] = ((uint32_t) MT_STREAM << 28)
         | ((uint32_t) STREAM_FB_INFO << 16)
         | (UINT32_C(1) << 15)
         | ((uint32_t) fb_idx << 8)
         | _fb_dir_byte(type);  // UI hint + bDirection from the GTB block type
  msg[1] = ((uint32_t) first_group << 24)
         | ((uint32_t) num_groups << 16);
  _nego_send_ump(p_midi, msg, 4);
}

static void _nego_handle_stream_msg(midi2d_interface_t* p_midi, const uint32_t* words) {
  // Let the application override this message before the built-in responder.
  switch (tud_midi2_stream_msg_cb(_itf_idx(p_midi), words)) {
    case MIDI2_STREAM_HANDLED:
      return;
    case MIDI2_STREAM_NEGOTIATED_MIDI1:
      p_midi->protocol = MIDI_PROTOCOL_MIDI1;
      p_midi->negotiated = true;
      return;
    case MIDI2_STREAM_NEGOTIATED_MIDI2:
      p_midi->protocol = MIDI_PROTOCOL_MIDI2;
      p_midi->negotiated = true;
      return;
    case MIDI2_STREAM_PASS:
    default:
      break;
  }

  uint16_t status = (words[0] >> 16) & 0x3FF;

  switch (status) {
    case STREAM_ENDPOINT_DISCOVERY:
      _nego_send_endpoint_info(p_midi);
      _nego_send_stream_text(p_midi, STREAM_EP_NAME, false, 0, tud_midi2_ep_name_cb(_itf_idx(p_midi)));
      _nego_send_stream_text(p_midi, STREAM_PROD_INSTANCE_ID, false, 0, tud_midi2_product_id_cb(_itf_idx(p_midi)));
      break;

    case STREAM_CONFIG_REQUEST: {
      uint8_t req_proto = (words[0] >> 8) & 0xFF;
      if (req_proto == MIDI_PROTOCOL_MIDI1 || req_proto == MIDI_PROTOCOL_MIDI2) {
        p_midi->protocol = req_proto;
      }
      _nego_send_config_notify(p_midi, p_midi->protocol);
      p_midi->negotiated = true;
      break;
    }

    case STREAM_FB_DISCOVERY: {
      uint8_t fb_idx = (words[0] >> 8) & 0xFF;
      uint8_t filter = words[0] & 0xFF;  // bit 0: FB Info, bit 1: FB Name
      uint8_t fb_count = _gtb_block_count(p_midi);
      for (uint8_t f = 0; f < fb_count; f++) {
        if (fb_idx != 0xFF && fb_idx != f) continue;
        if (filter & 0x01) _nego_send_fb_info(p_midi, f);
        if (filter & 0x02) _nego_send_stream_text(p_midi, STREAM_FB_NAME, true, f, tud_midi2_fb_name_cb(_itf_idx(p_midi), f));
      }
      break;
    }

    default:
      break;
  }
}

static void _nego_process_rx(midi2d_interface_t* p_midi) {
  tu_edpt_stream_t* ep_rx = &p_midi->ep_stream.rx;
  uint8_t word_bytes[4];

  while (tu_fifo_peek_n(&ep_rx->ff, word_bytes, 4) == 4) {
    // UMP words travel LSB-first on the wire and in LE memory, so MT is in
    // the high nibble of byte 3, not byte 0.
    uint8_t mt = (word_bytes[3] >> 4) & 0x0F;
    uint8_t pkt_words = midi2_ump_word_count(mt);
    uint32_t pkt_bytes = (uint32_t)pkt_words * 4;

    if (mt != MT_STREAM) break;
    if (tu_edpt_stream_read_available(ep_rx) < pkt_bytes) break;

    uint32_t buf[4] = {0};
    tu_edpt_stream_read(ep_rx, buf, pkt_bytes);
    _nego_handle_stream_msg(p_midi, buf);
  }
}

//--------------------------------------------------------------------+
// READ API
//--------------------------------------------------------------------+
bool tud_midi2_n_mounted(uint8_t itf) {
  TU_VERIFY(itf < CFG_TUD_MIDI2, false);
  midi2d_interface_t* p_midi = &_midi2d_itf[itf];
  return _tx_opened(p_midi) &&
         tu_edpt_stream_is_opened(&p_midi->ep_stream.rx);
}

uint32_t tud_midi2_n_available(uint8_t itf) {
  TU_VERIFY(itf < CFG_TUD_MIDI2, 0);
  midi2d_interface_t* p_midi = &_midi2d_itf[itf];
  return tu_edpt_stream_read_available(&p_midi->ep_stream.rx) / 4;
}

uint32_t tud_midi2_n_ump_read(uint8_t itf, uint32_t* words, uint32_t max_words) {
  TU_VERIFY(itf < CFG_TUD_MIDI2 && words != NULL && max_words > 0, 0);
  midi2d_interface_t* p_midi = &_midi2d_itf[itf];

  // UMP API is only valid on Alt Setting 1 (USB-MIDI 2.0).
  // Alt 0 carries USB-MIDI 1.0 32-bit Event Packets, not UMP words.
  if (p_midi->alt_setting != 1) { return 0; }

  tu_edpt_stream_t* ep_rx = &p_midi->ep_stream.rx;

  uint32_t total_read = 0;
  while (total_read < max_words) {
    uint8_t word_bytes[4];
    if (tu_fifo_peek_n(&ep_rx->ff, word_bytes, 4) < 4) break;

    // UMP words travel LSB-first; MT is the high nibble of byte 3, not byte 0.
    uint8_t mt = (word_bytes[3] >> 4) & 0x0F;
    uint8_t pkt_words = midi2_ump_word_count(mt);

    if (total_read + pkt_words > max_words) break;
    if (tu_edpt_stream_read_available(ep_rx) < (uint32_t)pkt_words * 4) break;

    tu_edpt_stream_read(ep_rx, &words[total_read], pkt_words * 4);
    total_read += pkt_words;
  }

  return total_read;
}

uint32_t tud_midi2_n_packet_read(uint8_t itf, uint8_t packets[], uint32_t max_packets) {
  TU_VERIFY(itf < CFG_TUD_MIDI2 && packets != NULL && max_packets > 0, 0);
  midi2d_interface_t* p_midi = &_midi2d_itf[itf];
  return tu_edpt_stream_read(&p_midi->ep_stream.rx, packets, max_packets * 4u) >> 2u;
}

//--------------------------------------------------------------------+
// WRITE API
//--------------------------------------------------------------------+
uint32_t tud_midi2_n_ump_write(uint8_t itf, const uint32_t* words, uint32_t count) {
  TU_VERIFY(itf < CFG_TUD_MIDI2 && words != NULL && count > 0, 0);
  midi2d_interface_t* p_midi = &_midi2d_itf[itf];

  // UMP API is only valid on Alt Setting 1 (USB-MIDI 2.0).
  // Alt 0 carries USB-MIDI 1.0 32-bit Event Packets, not UMP words.
  if (p_midi->alt_setting != 1) { return 0; }
  TU_VERIFY(_tx_opened(p_midi), 0);

  return _tx_ump_write(p_midi, words, count);
}

uint32_t tud_midi2_n_packet_write(uint8_t itf, const uint8_t packets[], uint32_t count) {
  TU_VERIFY(itf < CFG_TUD_MIDI2 && packets != NULL && count > 0, 0);
  midi2d_interface_t* p_midi = &_midi2d_itf[itf];
  midi2d_tx_t* tx = &p_midi->ep_stream.tx;

  // Packet API is for Alt Setting 0 (USB-MIDI 1.0) event packets.
  TU_VERIFY(p_midi->alt_setting == 0, 0);
  TU_VERIFY(_tx_opened(p_midi), 0);

  uint32_t written = 0;
  while (written < count) {
    if (tu_fifo_remaining(&tx->ff) < 4) break;

    if (tu_fifo_write_n(&tx->ff, packets + written * 4u, 4) != 4) break;
    written++;
  }

  (void) _tx_start_xfer(p_midi);

  return written;
}

//--------------------------------------------------------------------+
// STATE GETTERS
//--------------------------------------------------------------------+
uint8_t tud_midi2_n_alt_setting(uint8_t itf) {
  TU_VERIFY(itf < CFG_TUD_MIDI2, 0);
  return _midi2d_itf[itf].alt_setting;
}

bool tud_midi2_n_negotiated(uint8_t itf) {
  TU_VERIFY(itf < CFG_TUD_MIDI2, false);
  return _midi2d_itf[itf].negotiated;
}

uint8_t tud_midi2_n_protocol(uint8_t itf) {
  TU_VERIFY(itf < CFG_TUD_MIDI2, 0);
  return _midi2d_itf[itf].protocol;
}

//--------------------------------------------------------------------+
// USBD Driver API
//--------------------------------------------------------------------+
void midi2d_init(void) {
  tu_memclr(_midi2d_itf, sizeof(_midi2d_itf));
  for (uint8_t i = 0; i < CFG_TUD_MIDI2; i++) {
    midi2d_interface_t* p_midi = &_midi2d_itf[i];
    p_midi->protocol = MIDI_PROTOCOL_MIDI2;

  #if CFG_TUD_EDPT_DEDICATED_HWFIFO
    uint8_t *epout_buf = NULL;
    uint8_t *epin_buf  = NULL;
  #else
    uint8_t *epout_buf = _midi2d_epbuf[i].epout;
    uint8_t *epin_buf  = _midi2d_epbuf[i].epin;
  #endif

    tu_edpt_stream_init(&p_midi->ep_stream.rx, false, false, false,
                        p_midi->ep_stream.rx_ff_buf, CFG_TUD_MIDI2_RX_BUFSIZE, epout_buf);

    midi2d_tx_t* tx = &p_midi->ep_stream.tx;
    (void) tu_fifo_config(&tx->ff, p_midi->ep_stream.tx_ff_buf, CFG_TUD_MIDI2_TX_BUFSIZE, false);
#if CFG_TUD_EDPT_DEDICATED_HWFIFO == 0
    tx->ep_buf = epin_buf;
#else
    (void) epin_buf;
#endif
  }
}

bool midi2d_deinit(void) {
  for (uint8_t i = 0; i < CFG_TUD_MIDI2; i++) {
    midi2d_interface_t* p_midi = &_midi2d_itf[i];
    tu_edpt_stream_deinit(&p_midi->ep_stream.rx);
  }
  return true;
}

void midi2d_reset(uint8_t rhport) {
  (void) rhport;
  for (uint8_t i = 0; i < CFG_TUD_MIDI2; i++) {
    midi2d_interface_t* p_midi = &_midi2d_itf[i];
    tu_memclr(p_midi, ITF_MEM_RESET_SIZE);

    tu_edpt_stream_clear(&p_midi->ep_stream.rx);
    tu_edpt_stream_close(&p_midi->ep_stream.rx);

    tu_fifo_clear(&p_midi->ep_stream.tx.ff);
    p_midi->ep_stream.tx.ep_addr = 0;
  }
}

TU_ATTR_ALWAYS_INLINE static inline uint8_t find_midi2_itf(uint8_t ep_addr) {
  for (uint8_t idx = 0; idx < CFG_TUD_MIDI2; idx++) {
    const midi2d_interface_t* p_midi = &_midi2d_itf[idx];
    if (ep_addr == p_midi->ep_stream.rx.ep_addr || ep_addr == p_midi->ep_stream.tx.ep_addr) {
      return idx;
    }
  }
  return TUSB_INDEX_INVALID_8;
}

static uint8_t find_midi2_itf_by_num(uint8_t itf_num) {
  for (uint8_t idx = 0; idx < CFG_TUD_MIDI2; idx++) {
    if (_midi2d_itf[idx].itf_num == itf_num) return idx;
  }
  return TUSB_INDEX_INVALID_8;
}

uint16_t midi2d_open(uint8_t rhport, const tusb_desc_interface_t* desc_itf, uint16_t max_len) {
  const uint8_t* p_desc   = (const uint8_t*) desc_itf;
  const uint8_t* desc_end = p_desc + max_len;

  // 1st Interface: Audio Control v1 (optional)
  if (TUSB_CLASS_AUDIO               == desc_itf->bInterfaceClass    &&
      AUDIO_SUBCLASS_CONTROL         == desc_itf->bInterfaceSubClass &&
      AUDIO_FUNC_PROTOCOL_CODE_UNDEF == desc_itf->bInterfaceProtocol) {
    p_desc = tu_desc_next(desc_itf);
    while (tu_desc_in_bounds(p_desc, desc_end) && TUSB_DESC_CS_INTERFACE == tu_desc_type(p_desc)) {
      p_desc = tu_desc_next(p_desc);
    }
  }

  // 2nd Interface: MIDI Streaming
  TU_VERIFY(TUSB_DESC_INTERFACE == tu_desc_type(p_desc), 0);
  const tusb_desc_interface_t* desc_midi = (const tusb_desc_interface_t*) p_desc;

  TU_VERIFY(TUSB_CLASS_AUDIO              == desc_midi->bInterfaceClass &&
            AUDIO_SUBCLASS_MIDI_STREAMING == desc_midi->bInterfaceSubClass &&
            AUDIO_FUNC_PROTOCOL_CODE_UNDEF == desc_midi->bInterfaceProtocol,
            0);

  uint8_t idx = find_midi2_itf(0);
  TU_ASSERT(idx < CFG_TUD_MIDI2, 0);
  midi2d_interface_t* p_midi = &_midi2d_itf[idx];

  p_midi->rhport      = rhport;
  p_midi->itf_num     = desc_midi->bInterfaceNumber;
  p_midi->alt_setting = 0;
  p_midi->protocol    = MIDI_PROTOCOL_MIDI2;
  p_midi->negotiated  = false;

  p_desc = tu_desc_next(p_desc);

  // Skip class-specific descriptors
  while (tu_desc_in_bounds(p_desc, desc_end) && TUSB_DESC_CS_INTERFACE == tu_desc_type(p_desc)) {
    p_desc = tu_desc_next(p_desc);
  }

  // Find and open endpoint descriptors
  uint8_t found_ep = 0;
  while ((found_ep < desc_midi->bNumEndpoints) && tu_desc_in_bounds(p_desc, desc_end)) {
    if (TUSB_DESC_ENDPOINT == tu_desc_type(p_desc)) {
      const tusb_desc_endpoint_t* desc_ep = (const tusb_desc_endpoint_t*) p_desc;
      TU_ASSERT(usbd_edpt_open(rhport, desc_ep), 0);
      const uint8_t ep_addr = desc_ep->bEndpointAddress;

      if (tu_edpt_dir(ep_addr) == TUSB_DIR_IN) {
        p_midi->ep_stream.tx.ep_addr = ep_addr;
        p_midi->ep_stream.tx.mps = tu_edpt_packet_size(desc_ep);
        tu_fifo_clear(&p_midi->ep_stream.tx.ff);
      } else {
        tu_edpt_stream_open(&p_midi->ep_stream.rx, rhport, desc_ep, tu_edpt_packet_size(desc_ep));
        tu_edpt_stream_clear(&p_midi->ep_stream.rx);
        TU_ASSERT(tu_edpt_stream_read_xfer(&p_midi->ep_stream.rx) > 0, 0);
      }

      found_ep++;
    }

    p_desc = tu_desc_next(p_desc);
  }

  // Skip remaining descriptors (alt setting 1, CS endpoints, GTB)
  // Stop at any interface descriptor that is not our MIDI Streaming alt setting
  while (tu_desc_in_bounds(p_desc, desc_end)) {
    uint8_t dtype = tu_desc_type(p_desc);

    if (dtype == TUSB_DESC_INTERFACE) {
      const tusb_desc_interface_t* next_itf = (const tusb_desc_interface_t*) p_desc;
      // Continue only if this is an alternate setting of our own interface
      if (next_itf->bInterfaceNumber != desc_midi->bInterfaceNumber) break;
    } else if (dtype != TUSB_DESC_CS_INTERFACE && dtype != TUSB_DESC_CS_ENDPOINT &&
               dtype != TUSB_DESC_ENDPOINT) {
      break;
    }

    p_desc = tu_desc_next(p_desc);
  }

  return (uint16_t)(p_desc - (const uint8_t*) desc_itf);
}

bool midi2d_control_xfer_cb(uint8_t rhport, uint8_t stage, const tusb_control_request_t* request) {
  TU_LOG2("MIDI2 ctrl: stage=%u bRequest=0x%02X wValue=0x%04X wIndex=0x%04X wLength=%u\r\n",
          stage, request->bRequest, request->wValue, request->wIndex, request->wLength);

  if (stage != CONTROL_STAGE_SETUP) return true;

  switch (request->bRequest) {
    case TUSB_REQ_SET_INTERFACE: {
      uint8_t itf_num = tu_u16_low(request->wIndex);
      uint8_t alt     = tu_u16_low(request->wValue);

      // Only Alt Setting 0 (MIDI 1.0) and 1 (UMP) are valid
      if (alt > 1) return false;

      uint8_t idx = find_midi2_itf_by_num(itf_num);
      if (idx >= CFG_TUD_MIDI2) return false;

      midi2d_interface_t* p_midi = &_midi2d_itf[idx];
      p_midi->alt_setting = alt;

      tu_edpt_stream_clear(&p_midi->ep_stream.rx);
      tu_fifo_clear(&p_midi->ep_stream.tx.ff);

      if (alt == 1) {
        p_midi->negotiated = false;
        p_midi->protocol   = MIDI_PROTOCOL_MIDI2;
      }

      // Re-arm RX endpoint for receiving data after alt setting change
      tu_edpt_stream_read_xfer(&p_midi->ep_stream.rx);

      tud_midi2_set_itf_cb(idx, alt);
      tud_control_status(rhport, request);
      return true;
    }

    case TUSB_REQ_GET_DESCRIPTOR: {
      // USB-MIDI 2.0 Section 6: GTB descriptor retrieval
      //   bmRequestType = 0x81 (Device-to-Host, Standard, Interface)
      //   wValue  = CS_GR_TRM_BLOCK (0x26) in high byte, alt setting in low byte
      //   wIndex  = interface number
      if (request->bmRequestType_bit.direction != TUSB_DIR_IN) return false;
      if (request->bmRequestType_bit.type      != TUSB_REQ_TYPE_STANDARD) return false;
      if (request->bmRequestType_bit.recipient != TUSB_REQ_RCPT_INTERFACE) return false;
      if (tu_u16_high(request->wValue)         != MIDI2_CS_GRP_TRM_BLOCK) return false;

      uint8_t itf_num = tu_u16_low(request->wIndex);
      uint8_t idx     = find_midi2_itf_by_num(itf_num);
      if (idx >= CFG_TUD_MIDI2) return false;

      // Only Alt Setting 1 exposes Group Terminal Block descriptors.
      if (tu_u16_low(request->wValue) != 0x01) return false;

      if (tud_midi2_get_req_itf_cb(rhport, request)) return true;

      uint16_t gtb_len = 0;
      const uint8_t* gtb = tud_midi2_gtb_desc_cb(idx, &gtb_len);
      TU_ASSERT(_gtb_desc_valid(gtb, gtb_len), false);

      uint16_t len = request->wLength;
      if (len > gtb_len) {
        len = gtb_len;
      }
      tud_control_xfer(rhport, request, (void*)(uintptr_t) gtb, len);
      return true;
    }

    default:
      return false;
  }
}

bool midi2d_xfer_cb(uint8_t rhport, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes) {
  (void) rhport;

  uint8_t idx = find_midi2_itf(ep_addr);
  TU_ASSERT(idx < CFG_TUD_MIDI2);
  midi2d_interface_t* p_midi = &_midi2d_itf[idx];

  tu_edpt_stream_t* ep_rx = &p_midi->ep_stream.rx;
  midi2d_tx_t* ep_tx = &p_midi->ep_stream.tx;

  if (ep_addr == ep_rx->ep_addr) {
    if (result == XFER_RESULT_SUCCESS) {
      tu_edpt_stream_read_xfer_complete(ep_rx, xferred_bytes);
      if (p_midi->alt_setting == 1) {
        _nego_process_rx(p_midi);
      }
      tud_midi2_rx_cb(idx);
    }
    tu_edpt_stream_read_xfer(ep_rx);
  } else if (ep_addr == ep_tx->ep_addr && result == XFER_RESULT_SUCCESS) {
    uint16_t queued = _tx_start_xfer(p_midi);
    // Send ZLP if no more data is queued but the last transfer was exactly mps
    if (queued == 0 && tu_fifo_count(&ep_tx->ff) == 0 && xferred_bytes > 0 &&
        (0 == (xferred_bytes & (ep_tx->mps - 1)))) {
      if (usbd_edpt_claim(rhport, ep_tx->ep_addr)) {
          usbd_edpt_xfer(rhport, ep_tx->ep_addr, NULL, 0, false);
      }
    }
  } else {
    return false;
  }

  return true;
}

#endif

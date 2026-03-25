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
};

// MIDI Protocol values (per USB-MIDI 2.0 spec)
enum {
  MIDI_PROTOCOL_MIDI1       = 0x01,
  MIDI_PROTOCOL_MIDI2       = 0x02,
};

enum {
  UMP_VER_MAJOR = 1,
  UMP_VER_MINOR = 1,
};

// Group Terminal Block descriptor types (USB-MIDI 2.0)
enum {
  MIDI2_CS_GRP_TRM_BLOCK      = 0x26,
  MIDI2_GRP_TRM_BLOCK_HEADER  = 0x01,
  MIDI2_GRP_TRM_BLOCK_ENTRY   = 0x02,
};

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+
typedef struct {
  uint8_t rhport;
  uint8_t itf_num;
  uint8_t alt_setting;
  uint8_t protocol;
  bool    negotiated;

  /*------------- From this point, data is not cleared by bus reset -------------*/
  struct {
    tu_edpt_stream_t tx;
    tu_edpt_stream_t rx;

    uint8_t rx_ff_buf[CFG_TUD_MIDI2_RX_BUFSIZE];
    uint8_t tx_ff_buf[CFG_TUD_MIDI2_TX_BUFSIZE];
  } ep_stream;
} midi2d_interface_t;

TU_VERIFY_STATIC(CFG_TUD_MIDI2_NUM_GROUPS >= 1 && CFG_TUD_MIDI2_NUM_GROUPS <= 16,
                 "CFG_TUD_MIDI2_NUM_GROUPS must be 1..16");
TU_VERIFY_STATIC(CFG_TUD_MIDI2_NUM_FUNCTION_BLOCKS >= 1 && CFG_TUD_MIDI2_NUM_FUNCTION_BLOCKS <= 32,
                 "CFG_TUD_MIDI2_NUM_FUNCTION_BLOCKS must be 1..32");

#define ITF_MEM_RESET_SIZE offsetof(midi2d_interface_t, ep_stream)

static midi2d_interface_t _midi2d_itf[CFG_TUD_MIDI2];

#if CFG_TUD_EDPT_DEDICATED_HWFIFO == 0
typedef struct {
  TUD_EPBUF_DEF(epin, CFG_TUD_MIDI2_TX_EPSIZE);
  TUD_EPBUF_DEF(epout, CFG_TUD_MIDI2_RX_EPSIZE);
} midi2d_epbuf_t;

CFG_TUD_MEM_SECTION static midi2d_epbuf_t _midi2d_epbuf[CFG_TUD_MIDI2];
#endif

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
  0,                                        // iBlockItem: no string
  0x00,                                     // bMIDIProtocol: unknown/not fixed
  0, 0,                                     // wMaxInputBandwidth: unknown
  0, 0                                      // wMaxOutputBandwidth: unknown
};

//--------------------------------------------------------------------+
// Protocol Negotiation
//--------------------------------------------------------------------+
static void _nego_send_ump(midi2d_interface_t* p_midi, const uint32_t* words, uint8_t count) {
  tu_edpt_stream_t* ep_tx = &p_midi->ep_stream.tx;
  if (!tu_edpt_stream_is_opened(ep_tx)) return;
  if (tu_edpt_stream_write_available(ep_tx) < count * 4) return;
  tu_edpt_stream_write(ep_tx, words, count * 4);
  tu_edpt_stream_write_xfer(ep_tx);
}

static void _nego_send_endpoint_info(midi2d_interface_t* p_midi) {
  uint32_t msg[4] = {0};
  msg[0] = ((uint32_t) MT_STREAM << 28)
         | ((uint32_t) STREAM_ENDPOINT_INFO << 16)
         | ((uint32_t) UMP_VER_MAJOR << 8)
         | (uint32_t) UMP_VER_MINOR;
  msg[1] = (1u << 31)  // Static Function Blocks flag
         | ((uint32_t)(CFG_TUD_MIDI2_NUM_FUNCTION_BLOCKS & 0x7F) << 24)
         | (1u << 9)   // MIDI 2.0 Protocol capability
         | (1u << 8);  // MIDI 1.0 Protocol capability
  _nego_send_ump(p_midi, msg, 4);
}

static void _nego_send_stream_text(midi2d_interface_t* p_midi, uint16_t status, const char* str) {
  if (!str || str[0] == '\0') return;

  uint16_t total_len = (uint16_t) strlen(str);
  uint16_t offset = 0;

  while (offset < total_len) {
    uint16_t remaining = total_len - offset;
    uint8_t n = (uint8_t)((remaining > 14) ? 14 : remaining);
    bool is_first = (offset == 0);
    bool is_last  = (remaining <= 14);

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
    if (n > 0) msg[0] |= ((uint32_t)(uint8_t) p[0] << 8);
    if (n > 1) msg[0] |= (uint32_t)(uint8_t) p[1];
    for (uint8_t i = 2; i < n; i++) {
      uint8_t word_idx = (uint8_t)(1 + (i - 2) / 4);
      uint8_t shift    = (uint8_t)(24 - ((i - 2) % 4) * 8);
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
  uint32_t msg[4] = {0};
  msg[0] = ((uint32_t) MT_STREAM << 28)
         | ((uint32_t) STREAM_FB_INFO << 16)
         | (1u << 15)
         | ((uint32_t) fb_idx << 8)
         | 0x02;  // bDirection: bidirectional
  msg[1] = ((uint32_t) 0 << 24)  // bFirstGroup
         | ((uint32_t) CFG_TUD_MIDI2_NUM_GROUPS << 16);
  _nego_send_ump(p_midi, msg, 4);
}

static void _nego_handle_stream_msg(midi2d_interface_t* p_midi, const uint32_t* words) {
  uint16_t status = (words[0] >> 16) & 0x3FF;

  switch (status) {
    case STREAM_ENDPOINT_DISCOVERY:
      _nego_send_endpoint_info(p_midi);
      _nego_send_stream_text(p_midi, STREAM_EP_NAME, CFG_TUD_MIDI2_EP_NAME);
      _nego_send_stream_text(p_midi, STREAM_PROD_INSTANCE_ID, CFG_TUD_MIDI2_PRODUCT_ID);
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
      if (fb_idx == 0xFF) {
        for (uint8_t f = 0; f < CFG_TUD_MIDI2_NUM_FUNCTION_BLOCKS; f++) {
          _nego_send_fb_info(p_midi, f);
        }
      } else if (fb_idx < CFG_TUD_MIDI2_NUM_FUNCTION_BLOCKS) {
        _nego_send_fb_info(p_midi, fb_idx);
      }
      break;
    }

    default:
      break;
  }
}

static void _nego_process_rx(midi2d_interface_t* p_midi) {
  tu_edpt_stream_t* ep_rx = &p_midi->ep_stream.rx;
  uint8_t first_byte;

  while (tu_edpt_stream_peek(ep_rx, &first_byte)) {
    uint8_t mt = (first_byte >> 4) & 0x0F;
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
  return tu_edpt_stream_is_opened(&p_midi->ep_stream.tx) &&
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
  tu_edpt_stream_t* ep_rx = &p_midi->ep_stream.rx;

  uint32_t total_read = 0;
  while (total_read < max_words) {
    uint8_t first_byte;
    if (!tu_edpt_stream_peek(ep_rx, &first_byte)) break;

    uint8_t mt = (first_byte >> 4) & 0x0F;
    uint8_t pkt_words = midi2_ump_word_count(mt);

    if (total_read + pkt_words > max_words) break;
    if (tu_edpt_stream_read_available(ep_rx) < (uint32_t)pkt_words * 4) break;

    tu_edpt_stream_read(ep_rx, &words[total_read], pkt_words * 4);
    total_read += pkt_words;
  }

  return total_read;
}

bool tud_midi2_n_packet_read(uint8_t itf, uint8_t packet[4]) {
  TU_VERIFY(itf < CFG_TUD_MIDI2, false);
  midi2d_interface_t* p_midi = &_midi2d_itf[itf];
  return 4 == tu_edpt_stream_read(&p_midi->ep_stream.rx, packet, 4);
}

//--------------------------------------------------------------------+
// WRITE API
//--------------------------------------------------------------------+
uint32_t tud_midi2_n_ump_write(uint8_t itf, const uint32_t* words, uint32_t count) {
  TU_VERIFY(itf < CFG_TUD_MIDI2 && words != NULL && count > 0, 0);
  midi2d_interface_t* p_midi = &_midi2d_itf[itf];
  tu_edpt_stream_t* ep_tx = &p_midi->ep_stream.tx;
  TU_VERIFY(tu_edpt_stream_is_opened(ep_tx), 0);

  uint32_t written = 0;
  while (written < count) {
    uint8_t mt = (uint8_t)((words[written] >> 28) & 0x0F);
    uint8_t pkt_words = midi2_ump_word_count(mt);

    if (written + pkt_words > count) break;
    if (tu_edpt_stream_write_available(ep_tx) < pkt_words * 4) break;

    tu_edpt_stream_write(ep_tx, &words[written], pkt_words * 4);
    written += pkt_words;
  }

  (void) tu_edpt_stream_write_xfer(ep_tx);
  return written;
}

bool tud_midi2_n_packet_write(uint8_t itf, const uint8_t packet[4]) {
  TU_VERIFY(itf < CFG_TUD_MIDI2, false);
  midi2d_interface_t* p_midi = &_midi2d_itf[itf];
  tu_edpt_stream_t* ep_tx = &p_midi->ep_stream.tx;
  TU_VERIFY(tu_edpt_stream_is_opened(ep_tx), false);
  TU_VERIFY(tu_edpt_stream_write_available(ep_tx) >= 4, false);
  TU_VERIFY(tu_edpt_stream_write(ep_tx, packet, 4) > 0, false);
  (void) tu_edpt_stream_write_xfer(ep_tx);
  return true;
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
    uint8_t* epout_buf = NULL;
    uint8_t* epin_buf  = NULL;
  #else
    midi2d_epbuf_t* p_epbuf = &_midi2d_epbuf[i];
    uint8_t* epout_buf = p_epbuf->epout;
    uint8_t* epin_buf  = p_epbuf->epin;
  #endif

    tu_edpt_stream_init(&p_midi->ep_stream.rx, false, false, false,
                        p_midi->ep_stream.rx_ff_buf, CFG_TUD_MIDI2_RX_BUFSIZE, epout_buf);
    tu_edpt_stream_init(&p_midi->ep_stream.tx, false, true, false,
                        p_midi->ep_stream.tx_ff_buf, CFG_TUD_MIDI2_TX_BUFSIZE, epin_buf);
  }
}

bool midi2d_deinit(void) {
  for (uint8_t i = 0; i < CFG_TUD_MIDI2; i++) {
    midi2d_interface_t* p_midi = &_midi2d_itf[i];
    tu_edpt_stream_deinit(&p_midi->ep_stream.rx);
    tu_edpt_stream_deinit(&p_midi->ep_stream.tx);
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

    tu_edpt_stream_clear(&p_midi->ep_stream.tx);
    tu_edpt_stream_close(&p_midi->ep_stream.tx);
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
        tu_edpt_stream_open(&p_midi->ep_stream.tx, rhport, desc_ep, CFG_TUD_MIDI2_TX_EPSIZE);
        tu_edpt_stream_clear(&p_midi->ep_stream.tx);
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
  while (tu_desc_in_bounds(p_desc, desc_end)) {
    uint8_t dtype = tu_desc_type(p_desc);
    if (dtype != TUSB_DESC_CS_INTERFACE && dtype != TUSB_DESC_CS_ENDPOINT &&
        dtype != TUSB_DESC_INTERFACE && dtype != TUSB_DESC_ENDPOINT) {
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

      uint8_t idx = find_midi2_itf_by_num(itf_num);
      if (idx >= CFG_TUD_MIDI2) return false;

      midi2d_interface_t* p_midi = &_midi2d_itf[idx];
      p_midi->alt_setting = alt;

      tu_edpt_stream_clear(&p_midi->ep_stream.rx);
      tu_edpt_stream_clear(&p_midi->ep_stream.tx);

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
      // wValue: descriptor type (high) | index (low)
      // 0x26 = CS_GRP_TRM_BLOCK, index 0x01
      if (request->wValue == ((uint16_t)MIDI2_CS_GRP_TRM_BLOCK << 8 | 0x01)) {
        if (tud_midi2_get_req_itf_cb(rhport, request)) return true;

        uint16_t len = request->wLength;
        if (len > sizeof(_default_gtb_desc)) {
          len = sizeof(_default_gtb_desc);
        }
        tud_control_xfer(rhport, request, (void*)(uintptr_t) _default_gtb_desc, len);
        return true;
      }
      return false;
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
  tu_edpt_stream_t* ep_tx = &p_midi->ep_stream.tx;

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
    if (0 == tu_edpt_stream_write_xfer(ep_tx)) {
      (void) tu_edpt_stream_write_zlp_if_needed(ep_tx, xferred_bytes);
    }
  } else {
    return false;
  }

  return true;
}

#endif

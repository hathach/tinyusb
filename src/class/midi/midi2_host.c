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

#if (CFG_TUH_ENABLED && CFG_TUH_MIDI2)

#include "host/usbh.h"
#include "host/usbh_pvt.h"
#include "midi2_host.h"

#define TU_LOG_DRV(...)   TU_LOG(CFG_TUH_MIDI2_LOG_LEVEL, __VA_ARGS__)

//--------------------------------------------------------------------+
// Weak stubs for application callbacks
//--------------------------------------------------------------------+

TU_ATTR_WEAK void tuh_midi2_descriptor_cb(uint8_t idx, const tuh_midi2_descriptor_cb_t *desc_cb_data) {
  (void) idx; (void) desc_cb_data;
}

TU_ATTR_WEAK void tuh_midi2_mount_cb(uint8_t idx, const tuh_midi2_mount_cb_t *mount_cb_data) {
  (void) idx; (void) mount_cb_data;
}

TU_ATTR_WEAK void tuh_midi2_rx_cb(uint8_t idx, uint32_t xferred_bytes) {
  (void) idx; (void) xferred_bytes;
}

TU_ATTR_WEAK void tuh_midi2_tx_cb(uint8_t idx, uint32_t xferred_bytes) {
  (void) idx; (void) xferred_bytes;
}

TU_ATTR_WEAK void tuh_midi2_umount_cb(uint8_t idx) {
  (void) idx;
}

//--------------------------------------------------------------------+
// Internal structure and state
//--------------------------------------------------------------------+

typedef struct {
  uint8_t   ep_addr;
  uint16_t  mps;
  tu_fifo_t ff;
  uint8_t*  ep_buf;
} midih2_tx_t;

typedef struct {
  uint8_t daddr;
  uint8_t bInterfaceNumber;

  uint8_t alt_setting_current;

  uint8_t protocol_version;
  uint8_t bcdMSC_hi, bcdMSC_lo;
  uint8_t rx_cable_count_alt0;
  uint8_t tx_cable_count_alt0;
  uint8_t rx_cable_count_alt1;
  uint8_t tx_cable_count_alt1;

  struct {
    midih2_tx_t      tx;
    tu_edpt_stream_t rx;

    uint8_t rx_ff_buf[CFG_TUH_MIDI2_RX_BUFSIZE];
    uint8_t tx_ff_buf[CFG_TUH_MIDI2_TX_BUFSIZE];
  } ep_stream;

  bool mounted;
} midih2_interface_t;

static midih2_interface_t _midi2_host[CFG_TUH_MIDI2];

typedef struct {
  TUH_EPBUF_DEF(tx, TUH_EPSIZE_BULK_MAX);
  TUH_EPBUF_DEF(rx, TUH_EPSIZE_BULK_MAX);
} midih2_epbuf_t;

CFG_TUH_MEM_SECTION static midih2_epbuf_t _midi2_epbuf[CFG_TUH_MIDI2];

//--------------------------------------------------------------------+
// Helper functions
//--------------------------------------------------------------------+

static inline uint8_t find_new_midi2_index(void) {
  for (uint8_t idx = 0; idx < CFG_TUH_MIDI2; idx++) {
    if (_midi2_host[idx].daddr == 0) {
      return idx;
    }
  }
  return TUSB_INDEX_INVALID_8;
}

static inline uint8_t get_idx_by_ep_addr(uint8_t daddr, uint8_t ep_addr) {
  for (uint8_t idx = 0; idx < CFG_TUH_MIDI2; idx++) {
    const midih2_interface_t *p_midi = &_midi2_host[idx];
    if ((p_midi->daddr == daddr) &&
        (ep_addr == p_midi->ep_stream.rx.ep_addr || ep_addr == p_midi->ep_stream.tx.ep_addr)) {
      return idx;
    }
  }
  return TUSB_INDEX_INVALID_8;
}

static inline bool _tuh_tx_opened(const midih2_interface_t* p_midi) {
  return p_midi->ep_stream.tx.ep_addr != 0;
}

static uint8_t _tuh_tx_byte_at(const tu_fifo_buffer_info_t* info, uint16_t offset) {
  if (offset < info->linear.len) {
    return info->linear.ptr[offset];
  }
  offset = (uint16_t)(offset - info->linear.len);
  if (offset < info->wrapped.len) {
    return info->wrapped.ptr[offset];
  }
  return 0;
}

// Largest byte count containing only whole UMP packets and fitting one xfer.
static uint16_t _tuh_tx_nonseg_len_to_mps(midih2_tx_t* tx) {
  tu_fifo_buffer_info_t info;
  tu_fifo_get_read_info(&tx->ff, &info);

  const uint16_t available = (uint16_t)(info.linear.len + info.wrapped.len);
  uint16_t bytes = 0;

  while (bytes < tx->mps) {
    if ((uint16_t)(available - bytes) < 4) break;

    uint8_t mt = (uint8_t)((_tuh_tx_byte_at(&info, (uint16_t)(bytes + 3)) >> 4) & 0x0F);
    uint8_t pkt_words = midi2_ump_word_count(mt);
    uint16_t pkt_bytes = (uint16_t)(pkt_words * 4);

    if (pkt_bytes == 0) break;
    if ((uint16_t)(available - bytes) < pkt_bytes) break;
    if ((uint16_t)(bytes + pkt_bytes) > tx->mps) break;

    bytes = (uint16_t)(bytes + pkt_bytes);
  }

  return bytes;
}

// Start one OUT transfer capped at mps. Returns bytes queued, or 0 if nothing.
static uint16_t _tuh_tx_start_xfer(midih2_interface_t* p_midi) {
  midih2_tx_t* tx = &p_midi->ep_stream.tx;
  uint16_t ff_count = tu_fifo_count(&tx->ff);
  if (ff_count == 0) return 0;
  if (!usbh_edpt_claim(p_midi->daddr, tx->ep_addr)) return 0;

  uint16_t bytes;
  if (p_midi->alt_setting_current == 1) {
    bytes = _tuh_tx_nonseg_len_to_mps(tx);
  } else {
    bytes = tu_min16(tu_fifo_count(&tx->ff), tx->mps);
  }
  if (bytes == 0) {
    usbh_edpt_release(p_midi->daddr, tx->ep_addr);
    return 0;
  }

  tu_fifo_read_n(&tx->ff, tx->ep_buf, bytes);
  TU_ASSERT(usbh_edpt_xfer(p_midi->daddr, tx->ep_addr, tx->ep_buf, bytes), 0);
  return bytes;
}

static uint32_t _tuh_tx_ump_write(midih2_interface_t* p_midi, const uint32_t* words, uint32_t count) {
  uint32_t written = 0;
  while (written < count) {
    uint8_t mt = (uint8_t)((words[written] >> 28) & 0x0F);
    uint8_t pkt_words = midi2_ump_word_count(mt);
    uint16_t pkt_bytes = (uint16_t)(pkt_words * 4);

    if (written + pkt_words > count) break;
    if (tu_fifo_remaining(&p_midi->ep_stream.tx.ff) < pkt_bytes) break;
    if (tu_fifo_write_n(&p_midi->ep_stream.tx.ff, &words[written], pkt_bytes) != pkt_bytes) break;
    written += pkt_words;
  }

  (void) _tuh_tx_start_xfer(p_midi);
  return written;
}

//--------------------------------------------------------------------+
// Descriptor parsing
//--------------------------------------------------------------------+

// Parse Alt Setting 0 (MIDI 1.0) descriptors. Returns pointer past last consumed descriptor.
static const uint8_t* midih2_parse_descriptors_alt0(midih2_interface_t *p_midi,
    const tusb_desc_interface_t *desc_itf, const uint8_t *desc_end) {
  TU_VERIFY(AUDIO_SUBCLASS_MIDI_STREAMING == desc_itf->bInterfaceSubClass, NULL);

  p_midi->bInterfaceNumber = desc_itf->bInterfaceNumber;

  const uint8_t *p_desc = (const uint8_t *) desc_itf;
  p_desc = tu_desc_next(p_desc);

  uint8_t rx_cable_count = 0;
  uint8_t tx_cable_count = 0;
  bool found_new_interface = false;

  while (tu_desc_in_bounds(p_desc, desc_end) && !found_new_interface) {
    switch (tu_desc_type(p_desc)) {
      case TUSB_DESC_INTERFACE:
        found_new_interface = true;
        break;

      case TUSB_DESC_ENDPOINT: {
        const tusb_desc_endpoint_t *p_ep = (const tusb_desc_endpoint_t *) p_desc;

        TU_ASSERT(tuh_edpt_open(p_midi->daddr, p_ep), NULL);
        if (tu_edpt_dir(p_ep->bEndpointAddress) == TUSB_DIR_IN) {
          tu_edpt_stream_open(&p_midi->ep_stream.rx, p_midi->daddr, p_ep, tu_edpt_packet_size(p_ep));
          tu_edpt_stream_clear(&p_midi->ep_stream.rx);
        } else {
          p_midi->ep_stream.tx.ep_addr = p_ep->bEndpointAddress;
          p_midi->ep_stream.tx.mps     = tu_edpt_packet_size(p_ep);
          tu_fifo_clear(&p_midi->ep_stream.tx.ff);
        }

        p_desc = tu_desc_next(p_desc);
        if (tu_desc_in_bounds(p_desc, desc_end) && tu_desc_type(p_desc) == TUSB_DESC_CS_ENDPOINT) {
          const midi_desc_cs_endpoint_t *p_csep = (const midi_desc_cs_endpoint_t *) p_desc;
          if (tu_edpt_dir(p_ep->bEndpointAddress) == TUSB_DIR_OUT) {
            tx_cable_count = p_csep->bNumEmbMIDIJack;
          } else {
            rx_cable_count = p_csep->bNumEmbMIDIJack;
          }
        }
        break;
      }

      default:
        break;
    }

    if (!found_new_interface) {
      p_desc = tu_desc_next(p_desc);
    }
  }

  p_midi->rx_cable_count_alt0 = rx_cable_count;
  p_midi->tx_cable_count_alt0 = tx_cable_count;
  return p_desc;
}

// Parse Alt Setting 1 (MIDI 2.0 UMP) descriptors. Returns pointer past last consumed descriptor.
static const uint8_t* midih2_parse_descriptors_alt1(midih2_interface_t *p_midi,
    const tusb_desc_interface_t *desc_itf, const uint8_t *desc_end) {
  TU_VERIFY(AUDIO_SUBCLASS_MIDI_STREAMING == desc_itf->bInterfaceSubClass, NULL);
  TU_VERIFY(desc_itf->bAlternateSetting == 1, NULL);

  const uint8_t *p_desc = (const uint8_t *) desc_itf;
  p_desc = tu_desc_next(p_desc);

  uint8_t rx_cable_count = 0;
  uint8_t tx_cable_count = 0;
  bool found_new_interface = false;

  while (tu_desc_in_bounds(p_desc, desc_end) && !found_new_interface) {
    switch (tu_desc_type(p_desc)) {
      case TUSB_DESC_INTERFACE:
        found_new_interface = true;
        break;

      case TUSB_DESC_CS_INTERFACE:
        if (tu_desc_subtype(p_desc) == MIDI_CS_INTERFACE_HEADER) {
          // bcdMSC at offset 3-4 in CS Interface Header
          const uint8_t *bcd_ptr = p_desc + 3;
          p_midi->bcdMSC_lo = bcd_ptr[0];
          p_midi->bcdMSC_hi = bcd_ptr[1];
          if (p_midi->bcdMSC_hi == 0x02) {  // bcdMSC 0x0200 = USB-MIDI 2.0
            p_midi->protocol_version = 1;
          }
        }
        break;

      case TUSB_DESC_ENDPOINT: {
        const tusb_desc_endpoint_t *p_ep = (const tusb_desc_endpoint_t *) p_desc;
        p_desc = tu_desc_next(p_desc);

        if (tu_desc_in_bounds(p_desc, desc_end) && tu_desc_type(p_desc) == TUSB_DESC_CS_ENDPOINT) {
          // MIDI 2.0 CS Endpoint General 2.0: bNumGrpTrmBlk at offset 3
          if (p_desc[0] >= 4 && p_desc[2] == MIDI_CS_ENDPOINT_GENERAL_2_0) {
            uint8_t num_grp_trm_blk = p_desc[3];
            if (tu_edpt_dir(p_ep->bEndpointAddress) == TUSB_DIR_OUT) {
              tx_cable_count = num_grp_trm_blk;
            } else {
              rx_cable_count = num_grp_trm_blk;
            }
          }
        }
        break;
      }

      default:
        break;
    }

    if (!found_new_interface) {
      p_desc = tu_desc_next(p_desc);
    }
  }

  p_midi->rx_cable_count_alt1 = rx_cable_count;
  p_midi->tx_cable_count_alt1 = tx_cable_count;
  return p_desc;
}

//--------------------------------------------------------------------+
// Auto-selection logic
//--------------------------------------------------------------------+

static void midih2_auto_select_alt_setting(midih2_interface_t *p_midi) {
  p_midi->alt_setting_current = 0;
  if (p_midi->protocol_version == 1) {
    p_midi->alt_setting_current = 1;
  }
}

//--------------------------------------------------------------------+
// Init/Deinit
//--------------------------------------------------------------------+

bool midih2_init(void) {
  tu_memclr(&_midi2_host, sizeof(_midi2_host));
  for (int inst = 0; inst < CFG_TUH_MIDI2; inst++) {
    midih2_interface_t *p_midi = &_midi2_host[inst];

    tu_edpt_stream_init(&p_midi->ep_stream.rx, true, false, false,
      p_midi->ep_stream.rx_ff_buf, CFG_TUH_MIDI2_RX_BUFSIZE, _midi2_epbuf[inst].rx);

    // TX uses raw tu_fifo + direct usbh_edpt_xfer (no FIFO wrapper) to preserve
    // UMP packet boundaries across USB transfers.
    midih2_tx_t* tx = &p_midi->ep_stream.tx;
    (void) tu_fifo_config(&tx->ff, p_midi->ep_stream.tx_ff_buf, CFG_TUH_MIDI2_TX_BUFSIZE, false);
    tx->ep_buf = _midi2_epbuf[inst].tx;
  }
  return true;
}

bool midih2_deinit(void) {
  for (size_t i = 0; i < CFG_TUH_MIDI2; i++) {
    midih2_interface_t* p_midi = &_midi2_host[i];
    tu_edpt_stream_deinit(&p_midi->ep_stream.rx);
  }
  return true;
}

//--------------------------------------------------------------------+
// Class driver callbacks
//--------------------------------------------------------------------+

uint16_t midih2_open(uint8_t rhport, uint8_t dev_addr, const tusb_desc_interface_t *desc_itf, uint16_t max_len) {
  (void) rhport;

  TU_VERIFY(TUSB_CLASS_AUDIO == desc_itf->bInterfaceClass, 0);

  // For Alt Setting 1, reuse existing slot for same device+interface
  uint8_t idx = TUSB_INDEX_INVALID_8;
  if (desc_itf->bAlternateSetting > 0) {
    for (uint8_t i = 0; i < CFG_TUH_MIDI2; i++) {
      if (_midi2_host[i].daddr == dev_addr &&
          _midi2_host[i].bInterfaceNumber == desc_itf->bInterfaceNumber) {
        idx = i;
        break;
      }
    }
  }
  if (idx == TUSB_INDEX_INVALID_8) {
    idx = find_new_midi2_index();
  }
  TU_VERIFY(idx < CFG_TUH_MIDI2, 0);

  midih2_interface_t *p_midi = &_midi2_host[idx];
  p_midi->daddr = dev_addr;

  const uint8_t *desc_start = (const uint8_t *) desc_itf;
  const uint8_t *desc_end = desc_start + max_len;

  // Skip Audio Control interface and any non-MIDI-Streaming descriptors
  // (following midi_host.c pattern from Ha Thach)
  if (AUDIO_SUBCLASS_CONTROL == desc_itf->bInterfaceSubClass) {
    const uint8_t *p_desc = tu_desc_next((const uint8_t *)desc_itf);
    // Skip CS_INTERFACE header
    TU_VERIFY(tu_desc_type(p_desc) == TUSB_DESC_CS_INTERFACE, 0);
    p_desc = tu_desc_next(p_desc);
    desc_itf = (const tusb_desc_interface_t *) p_desc;
    // Skip until we find MIDI Streaming interface
    while (tu_desc_in_bounds(p_desc, desc_end) &&
           (desc_itf->bDescriptorType != TUSB_DESC_INTERFACE ||
            (desc_itf->bInterfaceClass == TUSB_CLASS_AUDIO &&
             desc_itf->bInterfaceSubClass != AUDIO_SUBCLASS_MIDI_STREAMING))) {
      p_desc = tu_desc_next(p_desc);
      desc_itf = (const tusb_desc_interface_t *) p_desc;
    }
    TU_VERIFY(tu_desc_in_bounds(p_desc, desc_end), 0);
    TU_VERIFY(TUSB_CLASS_AUDIO == desc_itf->bInterfaceClass, 0);
  }

  TU_VERIFY(AUDIO_SUBCLASS_MIDI_STREAMING == desc_itf->bInterfaceSubClass, 0);

  TU_LOG_DRV("MIDI2 opening Interface %u Alt %u (addr = %u)\r\n",
             desc_itf->bInterfaceNumber, desc_itf->bAlternateSetting, dev_addr);

  // Dispatch to appropriate parser based on Alt Setting
  const uint8_t *p_end = NULL;
  if (desc_itf->bAlternateSetting == 0) {
    p_end = midih2_parse_descriptors_alt0(p_midi, desc_itf, desc_end);
  } else if (desc_itf->bAlternateSetting == 1) {
    p_end = midih2_parse_descriptors_alt1(p_midi, desc_itf, desc_end);
  }

  // Return number of bytes consumed (following midi_host.c pattern)
  uint16_t const parsed_len = (p_end != NULL) ? (uint16_t)(p_end - desc_start) : 0;
  return parsed_len;
}

static void midih2_set_config_complete(midih2_interface_t *p_midi, uint8_t idx) {
  uint8_t dev_addr = p_midi->daddr;

  // Invoke descriptor_cb
  tuh_midi2_descriptor_cb_t desc_cb = {
    .protocol_version = p_midi->protocol_version,
    .bcdMSC_hi = p_midi->bcdMSC_hi,
    .bcdMSC_lo = p_midi->bcdMSC_lo,
    .rx_cable_count = (p_midi->alt_setting_current == 0) ?
                      p_midi->rx_cable_count_alt0 : p_midi->rx_cable_count_alt1,
    .tx_cable_count = (p_midi->alt_setting_current == 0) ?
                      p_midi->tx_cable_count_alt0 : p_midi->tx_cable_count_alt1,
  };
  tuh_midi2_descriptor_cb(idx, &desc_cb);

  // Mark as mounted
  TU_LOG_DRV("MIDI2 mounted addr = %u, alt = %u, protocol = %u\r\n",
             dev_addr, p_midi->alt_setting_current, p_midi->protocol_version);
  p_midi->mounted = true;

  // Invoke mount_cb
  tuh_midi2_mount_cb_t mount_cb = {
    .daddr = dev_addr,
    .bInterfaceNumber = p_midi->bInterfaceNumber,
    .protocol_version = p_midi->protocol_version,
    .alt_setting_active = p_midi->alt_setting_current,
    .rx_cable_count = desc_cb.rx_cable_count,
    .tx_cable_count = desc_cb.tx_cable_count,
  };
  tuh_midi2_mount_cb(idx, &mount_cb);

  // Prepare RX transfer
  tu_edpt_stream_read_xfer(&p_midi->ep_stream.rx);

  // Signal USBH that configuration is complete
  usbh_driver_set_config_complete(dev_addr, p_midi->bInterfaceNumber);
}

static void midih2_set_interface_cb(tuh_xfer_t *xfer) {
  uint8_t const dev_addr = xfer->daddr;
  uint8_t const itf_num = (uint8_t) tu_le16toh(xfer->setup->wIndex);

  // Find our interface
  for (uint8_t idx = 0; idx < CFG_TUH_MIDI2; idx++) {
    if (_midi2_host[idx].daddr == dev_addr && _midi2_host[idx].bInterfaceNumber == itf_num) {
      if (xfer->result == XFER_RESULT_SUCCESS) {
        midih2_set_config_complete(&_midi2_host[idx], idx);
      } else {
        // SET_INTERFACE failed, fall back to alt 0
        TU_LOG_DRV("MIDI2 SET_INTERFACE failed, falling back to alt 0\r\n");
        _midi2_host[idx].alt_setting_current = 0;
        midih2_set_config_complete(&_midi2_host[idx], idx);
      }
      return;
    }
  }
}

bool midih2_set_config(uint8_t dev_addr, uint8_t itf_num) {
  uint8_t idx = 0;
  for (idx = 0; idx < CFG_TUH_MIDI2; idx++) {
    if (_midi2_host[idx].daddr == dev_addr && _midi2_host[idx].bInterfaceNumber == itf_num) {
      break;
    }
  }

  if (idx >= CFG_TUH_MIDI2) {
    // Not our interface (e.g. Audio Control) - pass through to next
    usbh_driver_set_config_complete(dev_addr, itf_num);
    return true;
  }

  midih2_interface_t *p_midi = &_midi2_host[idx];

  // Auto-select alt setting
  midih2_auto_select_alt_setting(p_midi);

  // If MIDI 2.0 detected, issue SET_INTERFACE to activate Alt Setting 1
  if (p_midi->alt_setting_current == 1) {
    TU_LOG_DRV("MIDI2 requesting SET_INTERFACE alt 1 for itf %u\r\n", itf_num);
    TU_ASSERT(tuh_interface_set(dev_addr, itf_num, 1, midih2_set_interface_cb, 0));
  } else {
    // MIDI 1.0 only, complete immediately
    midih2_set_config_complete(p_midi, idx);
  }

  return true;
}

void midih2_close(uint8_t dev_addr) {
  for (uint8_t idx = 0; idx < CFG_TUH_MIDI2; idx++) {
    midih2_interface_t *p_midi = &_midi2_host[idx];
    if (p_midi->daddr == dev_addr) {
      TU_LOG_DRV("  MIDI2 close addr = %u index = %u\r\n", dev_addr, idx);
      tu_edpt_stream_close(&p_midi->ep_stream.rx);
      tu_fifo_clear(&p_midi->ep_stream.tx.ff);
      p_midi->ep_stream.tx.ep_addr = 0;
      tuh_midi2_umount_cb(idx);
      tu_memclr(p_midi, sizeof(midih2_interface_t));
    }
  }
}

bool midih2_xfer_cb(uint8_t dev_addr, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes) {
  uint8_t idx = get_idx_by_ep_addr(dev_addr, ep_addr);
  TU_VERIFY(idx < CFG_TUH_MIDI2);

  midih2_interface_t *p_midi = &_midi2_host[idx];
  midih2_tx_t* ep_tx = &p_midi->ep_stream.tx;

  if (ep_addr == p_midi->ep_stream.rx.ep_addr) {
    if (result == XFER_RESULT_SUCCESS && xferred_bytes > 0) {
      tu_edpt_stream_read_xfer_complete(&p_midi->ep_stream.rx, xferred_bytes);
      tuh_midi2_rx_cb(idx, xferred_bytes);
    }
    tu_edpt_stream_read_xfer(&p_midi->ep_stream.rx);
  } else if (ep_addr == ep_tx->ep_addr) {
    tuh_midi2_tx_cb(idx, xferred_bytes);
    if (result == XFER_RESULT_SUCCESS) {
      uint16_t queued = _tuh_tx_start_xfer(p_midi);
      // Send ZLP if no more data is queued but the last transfer was exactly mps
      if (queued == 0 && tu_fifo_count(&ep_tx->ff) == 0 && xferred_bytes > 0 &&
          (0 == (xferred_bytes & (ep_tx->mps - 1)))) {
        if (usbh_edpt_claim(dev_addr, ep_tx->ep_addr)) {
          usbh_edpt_xfer(dev_addr, ep_tx->ep_addr, NULL, 0);
        }
      }
    }
  }

  return true;
}

//--------------------------------------------------------------------+
// Public API
//--------------------------------------------------------------------+

bool tuh_midi2_mounted(uint8_t idx) {
  TU_VERIFY(idx < CFG_TUH_MIDI2);
  return _midi2_host[idx].mounted;
}

uint8_t tuh_midi2_get_protocol_version(uint8_t idx) {
  TU_VERIFY(idx < CFG_TUH_MIDI2);
  return _midi2_host[idx].protocol_version;
}

uint8_t tuh_midi2_get_alt_setting_active(uint8_t idx) {
  TU_VERIFY(idx < CFG_TUH_MIDI2);
  return _midi2_host[idx].alt_setting_current;
}

uint8_t tuh_midi2_get_cable_count(uint8_t idx) {
  TU_VERIFY(idx < CFG_TUH_MIDI2);
  return (_midi2_host[idx].alt_setting_current == 0) ?
         _midi2_host[idx].rx_cable_count_alt0 : _midi2_host[idx].rx_cable_count_alt1;
}

uint32_t tuh_midi2_ump_read(uint8_t idx, uint32_t* words, uint32_t max_words) {
  TU_VERIFY(idx < CFG_TUH_MIDI2 && words && max_words);

  midih2_interface_t *p_midi = &_midi2_host[idx];
  tu_edpt_stream_t *ep_rx = &p_midi->ep_stream.rx;

  uint32_t n_words = 0;
  for (uint32_t i = 0; i < max_words; i++) {
    if (tu_edpt_stream_read_available(ep_rx) >= 4) {
      tu_edpt_stream_read(ep_rx, (uint8_t *) &words[i], 4);
      n_words++;
    } else {
      break;
    }
  }

  return n_words;
}

uint32_t tuh_midi2_ump_write(uint8_t idx, const uint32_t* words, uint32_t count) {
  TU_VERIFY(idx < CFG_TUH_MIDI2 && words && count);

  midih2_interface_t *p_midi = &_midi2_host[idx];
  TU_VERIFY(_tuh_tx_opened(p_midi), 0);

  return _tuh_tx_ump_write(p_midi, words, count);
}

uint32_t tuh_midi2_write_flush(uint8_t idx) {
  TU_VERIFY(idx < CFG_TUH_MIDI2);
  return _tuh_tx_start_xfer(&_midi2_host[idx]);
}

#endif // CFG_TUH_ENABLED && CFG_TUH_MIDI2

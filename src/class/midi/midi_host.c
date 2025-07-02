/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
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

#if (CFG_TUH_ENABLED && CFG_TUH_MIDI)

#include "host/usbh.h"
#include "host/usbh_pvt.h"

#include "midi_host.h"

// Level where CFG_TUSB_DEBUG must be at least for this driver is logged
#ifndef CFG_TUH_MIDI_LOG_LEVEL
  #define CFG_TUH_MIDI_LOG_LEVEL   CFG_TUH_LOG_LEVEL
#endif

#define TU_LOG_DRV(...)   TU_LOG(CFG_TUH_MIDI_LOG_LEVEL, __VA_ARGS__)

//--------------------------------------------------------------------+
// Weak stubs: invoked if no strong implementation is available
//--------------------------------------------------------------------+
TU_ATTR_WEAK void tuh_midi_descriptor_cb(uint8_t idx, const tuh_midi_descriptor_cb_t * desc_cb_data) { (void) idx; (void) desc_cb_data; }
TU_ATTR_WEAK void tuh_midi_mount_cb(uint8_t idx, const tuh_midi_mount_cb_t* mount_cb_data) { (void) idx; (void) mount_cb_data; }
TU_ATTR_WEAK void tuh_midi_umount_cb(uint8_t idx) { (void) idx; }
TU_ATTR_WEAK void tuh_midi_rx_cb(uint8_t idx, uint32_t xferred_bytes) { (void) idx; (void) xferred_bytes; }
TU_ATTR_WEAK void tuh_midi_tx_cb(uint8_t idx, uint32_t xferred_bytes) { (void) idx; (void) xferred_bytes; }

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+

typedef struct {
  uint8_t daddr;
  uint8_t bInterfaceNumber; // interface number of MIDI streaming
  uint8_t iInterface;
  uint8_t itf_count;        // number of interface including Audio Control + MIDI streaming

  uint8_t ep_in;          // IN endpoint address
  uint8_t ep_out;         // OUT endpoint address

  uint8_t rx_cable_count;  // IN endpoint CS descriptor bNumEmbMIDIJack value
  uint8_t tx_cable_count;  // OUT endpoint CS descriptor bNumEmbMIDIJack value

  #if CFG_TUH_MIDI_STREAM_API
  // For Stream read()/write() API
  // Messages are always 4 bytes long, queue them for reading and writing so the
  // callers can use the Stream interface with single-byte read/write calls.
  midi_driver_stream_t stream_write;
  midi_driver_stream_t stream_read;
  #endif

  // Endpoint stream
  struct {
    tu_edpt_stream_t tx;
    tu_edpt_stream_t rx;

    uint8_t rx_ff_buf[CFG_TUH_MIDI_RX_BUFSIZE];
    uint8_t tx_ff_buf[CFG_TUH_MIDI_TX_BUFSIZE];
  } ep_stream;

  bool mounted;
}midih_interface_t;

typedef struct {
  TUH_EPBUF_DEF(tx, TUH_EPSIZE_BULK_MPS);
  TUH_EPBUF_DEF(rx, TUH_EPSIZE_BULK_MPS);
} midih_epbuf_t;

static midih_interface_t _midi_host[CFG_TUH_MIDI];
CFG_TUH_MEM_SECTION static midih_epbuf_t _midi_epbuf[CFG_TUH_MIDI];

//--------------------------------------------------------------------+
// Helper
//--------------------------------------------------------------------+
TU_ATTR_ALWAYS_INLINE static inline uint8_t find_new_midi_index(void) {
  for (uint8_t idx = 0; idx < CFG_TUH_MIDI; idx++) {
    if (_midi_host[idx].daddr == 0) {
      return idx;
    }
  }
  return TUSB_INDEX_INVALID_8;
}

static inline uint8_t get_idx_by_ep_addr(uint8_t daddr, uint8_t ep_addr) {
  for (uint8_t idx = 0; idx < CFG_TUH_MIDI; idx++) {
    const midih_interface_t *p_midi = &_midi_host[idx];
    if ((p_midi->daddr == daddr) &&
        (ep_addr == p_midi->ep_stream.rx.ep_addr || ep_addr == p_midi->ep_stream.tx.ep_addr)) {
      return idx;
    }
  }
  return TUSB_INDEX_INVALID_8;
}

//--------------------------------------------------------------------+
// USBH API
//--------------------------------------------------------------------+
bool midih_init(void) {
  tu_memclr(&_midi_host, sizeof(_midi_host));
  for (int inst = 0; inst < CFG_TUH_MIDI; inst++) {
    midih_interface_t *p_midi_host = &_midi_host[inst];
    tu_edpt_stream_init(&p_midi_host->ep_stream.rx, true, false, false,
      p_midi_host->ep_stream.rx_ff_buf, CFG_TUH_MIDI_RX_BUFSIZE, _midi_epbuf->rx, TUH_EPSIZE_BULK_MPS);
    tu_edpt_stream_init(&p_midi_host->ep_stream.tx, true, true, false,
      p_midi_host->ep_stream.tx_ff_buf, CFG_TUH_MIDI_TX_BUFSIZE, _midi_epbuf->tx, TUH_EPSIZE_BULK_MPS);
  }
  return true;
}

bool midih_deinit(void) {
  for (size_t i = 0; i < CFG_TUH_MIDI; i++) {
    midih_interface_t* p_midi = &_midi_host[i];
    tu_edpt_stream_deinit(&p_midi->ep_stream.rx);
    tu_edpt_stream_deinit(&p_midi->ep_stream.tx);
  }
  return true;
}

void midih_close(uint8_t daddr) {
  for (uint8_t idx = 0; idx < CFG_TUH_MIDI; idx++) {
    midih_interface_t* p_midi = &_midi_host[idx];
    if (p_midi->daddr == daddr) {
      TU_LOG_DRV("  MIDI close addr = %u index = %u\r\n", daddr, idx);
      tuh_midi_umount_cb(idx);

      p_midi->ep_in = 0;
      p_midi->ep_out = 0;
      p_midi->bInterfaceNumber = 0;
      p_midi->rx_cable_count = 0;
      p_midi->tx_cable_count = 0;
      p_midi->daddr = 0;
      p_midi->mounted = false;
#if CFG_TUH_MIDI_STREAM_API
      tu_memclr(&p_midi->stream_read, sizeof(p_midi->stream_read));
      tu_memclr(&p_midi->stream_write, sizeof(p_midi->stream_write));
#endif
      tu_edpt_stream_close(&p_midi->ep_stream.rx);
      tu_edpt_stream_close(&p_midi->ep_stream.tx);
    }
  }
}

bool midih_xfer_cb(uint8_t dev_addr, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes) {
  (void) result;
  const uint8_t idx = get_idx_by_ep_addr(dev_addr, ep_addr);
  TU_VERIFY(idx < CFG_TUH_MIDI);
  midih_interface_t *p_midi = &_midi_host[idx];

  if (ep_addr == p_midi->ep_stream.rx.ep_addr) {
    // receive new data, put it into FIFO and invoke callback if available
    // Note: some devices send back all zero packets even if there is no data ready
    if (xferred_bytes && !tu_mem_is_zero(p_midi->ep_stream.rx.ep_buf, xferred_bytes)) {
      tu_edpt_stream_read_xfer_complete(&p_midi->ep_stream.rx, xferred_bytes);
      tuh_midi_rx_cb(idx, xferred_bytes);
    }

    tu_edpt_stream_read_xfer(dev_addr, &p_midi->ep_stream.rx); // prepare for next transfer
  } else if (ep_addr == p_midi->ep_stream.tx.ep_addr) {
    tuh_midi_tx_cb(idx, xferred_bytes);

    if (0 == tu_edpt_stream_write_xfer(dev_addr, &p_midi->ep_stream.tx)) {
      // If there is no data left, a ZLP should be sent if
      // xferred_bytes is multiple of EP size and not zero
      tu_edpt_stream_write_zlp_if_needed(dev_addr, &p_midi->ep_stream.tx, xferred_bytes);
    }
  }

  return true;
}

//--------------------------------------------------------------------+
// Enumeration
//--------------------------------------------------------------------+
bool midih_open(uint8_t rhport, uint8_t dev_addr, tusb_desc_interface_t const *desc_itf, uint16_t max_len) {
  (void) rhport;

  TU_VERIFY(TUSB_CLASS_AUDIO == desc_itf->bInterfaceClass);
  const uint8_t *p_end = ((const uint8_t *) desc_itf) + max_len;
  const uint8_t *p_desc = (const uint8_t *) desc_itf;

  const uint8_t idx = find_new_midi_index();
  TU_VERIFY(idx < CFG_TUH_MIDI);
  midih_interface_t *p_midi = &_midi_host[idx];
  p_midi->itf_count = 0;

  tuh_midi_descriptor_cb_t desc_cb = { 0 };
  desc_cb.jack_num = 0;

  // There can be just a MIDI or an Audio + MIDI interface
  // If there is Audio Control Interface + Audio Header descriptor, skip it
  if (AUDIO_SUBCLASS_CONTROL == desc_itf->bInterfaceSubClass) {
    TU_VERIFY(max_len > 2*sizeof(tusb_desc_interface_t) + sizeof(audio_desc_cs_ac_interface_t));

    p_desc = tu_desc_next(p_desc);
    TU_VERIFY(tu_desc_type(p_desc) == TUSB_DESC_CS_INTERFACE &&
              tu_desc_subtype(p_desc) == AUDIO_CS_AC_INTERFACE_HEADER);
    desc_cb.desc_audio_control = desc_itf;

    p_desc = tu_desc_next(p_desc);
    desc_itf = (const tusb_desc_interface_t *)p_desc;
    TU_VERIFY(TUSB_CLASS_AUDIO == desc_itf->bInterfaceClass);
    p_midi->itf_count = 1;
  }
  TU_VERIFY(AUDIO_SUBCLASS_MIDI_STREAMING == desc_itf->bInterfaceSubClass);

  TU_LOG_DRV("MIDI opening Interface %u (addr = %u)\r\n", desc_itf->bInterfaceNumber, dev_addr);
  p_midi->bInterfaceNumber = desc_itf->bInterfaceNumber;
  p_midi->iInterface = desc_itf->iInterface;
  p_midi->itf_count++;
  desc_cb.desc_midi = desc_itf;

  p_desc = tu_desc_next(p_desc); // next to CS Header

  bool found_new_interface = false;
  while ((p_desc < p_end) && (tu_desc_next(p_desc) <= p_end) && !found_new_interface) {
    switch (tu_desc_type(p_desc)) {
      case TUSB_DESC_INTERFACE:
        found_new_interface = true;
        break;

      case TUSB_DESC_CS_INTERFACE:
        switch (tu_desc_subtype(p_desc)) {
          case MIDI_CS_INTERFACE_HEADER:
            TU_LOG_DRV("  Interface Header descriptor\r\n");
            desc_cb.desc_header = p_desc;
            break;

          case MIDI_CS_INTERFACE_IN_JACK:
          case MIDI_CS_INTERFACE_OUT_JACK: {
            TU_LOG_DRV("  Jack %s %s descriptor \r\n",
                       tu_desc_subtype(p_desc) == MIDI_CS_INTERFACE_IN_JACK ? "IN" : "OUT",
                       p_desc[3] == MIDI_JACK_EXTERNAL ? "External" : "Embedded");
            if (desc_cb.jack_num < TU_ARRAY_SIZE(desc_cb.desc_jack)) {
                desc_cb.desc_jack[desc_cb.jack_num++] = p_desc;
            }
            break;
          }

          case MIDI_CS_INTERFACE_ELEMENT:
            TU_LOG_DRV("  Element descriptor\r\n");
            desc_cb.desc_element = p_desc;
            break;

          default:
            TU_LOG_DRV("  Unknown CS Interface sub-type %u\r\n", tu_desc_subtype(p_desc));
            break;
        }
        break;

      case TUSB_DESC_ENDPOINT: {
        const tusb_desc_endpoint_t *p_ep = (const tusb_desc_endpoint_t *) p_desc;
        p_desc = tu_desc_next(p_desc); // next to CS endpoint
        TU_VERIFY(p_desc < p_end && tu_desc_next(p_desc) <= p_end);
        const midi_desc_cs_endpoint_t *p_csep = (const midi_desc_cs_endpoint_t *) p_desc;

        TU_LOG_DRV("  Endpoint and CS_Endpoint descriptor %02x\r\n", p_ep->bEndpointAddress);
        if (tu_edpt_dir(p_ep->bEndpointAddress) == TUSB_DIR_OUT) {
          p_midi->ep_out = p_ep->bEndpointAddress;
          p_midi->tx_cable_count = p_csep->bNumEmbMIDIJack;
          desc_cb.desc_epout = p_ep;

          TU_ASSERT(tuh_edpt_open(dev_addr, p_ep));
          tu_edpt_stream_open(&p_midi->ep_stream.tx, p_ep);
        } else {
          p_midi->ep_in = p_ep->bEndpointAddress;
          p_midi->rx_cable_count = p_csep->bNumEmbMIDIJack;
          desc_cb.desc_epin = p_ep;

          TU_ASSERT(tuh_edpt_open(dev_addr, p_ep));
          tu_edpt_stream_open(&p_midi->ep_stream.rx, p_ep);
        }
        break;
      }

      default: break; // skip unknown descriptor
    }
    p_desc = tu_desc_next(p_desc);
  }
  desc_cb.desc_midi_total_len = (uint16_t) ((uintptr_t)p_desc - (uintptr_t) desc_itf);

  p_midi->daddr = dev_addr;
  tuh_midi_descriptor_cb(idx, &desc_cb);

  return true;
}

bool midih_set_config(uint8_t dev_addr, uint8_t itf_num) {
  uint8_t idx = tuh_midi_itf_get_index(dev_addr, itf_num);
  TU_ASSERT(idx < CFG_TUH_MIDI);
  midih_interface_t *p_midi = &_midi_host[idx];
  p_midi->mounted = true;

  const tuh_midi_mount_cb_t mount_cb_data = {
    .daddr = dev_addr,
    .bInterfaceNumber = itf_num,
    .rx_cable_count = p_midi->rx_cable_count,
    .tx_cable_count = p_midi->tx_cable_count,
  };
  tuh_midi_mount_cb(idx, &mount_cb_data);

  tu_edpt_stream_read_xfer(dev_addr, &p_midi->ep_stream.rx); // prepare for incoming data

  // No special config things to do for MIDI
  usbh_driver_set_config_complete(dev_addr, p_midi->bInterfaceNumber);
  return true;
}

//--------------------------------------------------------------------+
// API
//--------------------------------------------------------------------+
bool tuh_midi_mounted(uint8_t idx) {
  TU_VERIFY(idx < CFG_TUH_MIDI);
  midih_interface_t *p_midi = &_midi_host[idx];
  return p_midi->mounted;
}

uint8_t tuh_midi_itf_get_index(uint8_t daddr, uint8_t itf_num) {
  for (uint8_t idx = 0; idx < CFG_TUH_MIDI; idx++) {
    const midih_interface_t *p_midi = &_midi_host[idx];
    if (p_midi->daddr == daddr &&
        (p_midi->bInterfaceNumber == itf_num ||
         p_midi->bInterfaceNumber == (uint8_t) (itf_num + p_midi->itf_count - 1))) {
      return idx;
    }
  }
  return TUSB_INDEX_INVALID_8;
}

bool tuh_midi_itf_get_info(uint8_t idx, tuh_itf_info_t* info) {
  midih_interface_t* p_midi = &_midi_host[idx];
  TU_VERIFY(p_midi && info);

  info->daddr = p_midi->daddr;

  // re-construct descriptor
  tusb_desc_interface_t* desc = &info->desc;
  desc->bLength            = sizeof(tusb_desc_interface_t);
  desc->bDescriptorType    = TUSB_DESC_INTERFACE;

  desc->bInterfaceNumber   = p_midi->bInterfaceNumber;
  desc->bAlternateSetting  = 0;
  desc->bNumEndpoints      = (uint8_t)((p_midi->ep_in != 0 ? 1:0) + (p_midi->ep_out != 0 ? 1:0));
  desc->bInterfaceClass    = TUSB_CLASS_AUDIO;
  desc->bInterfaceSubClass = AUDIO_SUBCLASS_MIDI_STREAMING;
  desc->bInterfaceProtocol = 0;
  desc->iInterface         = p_midi->iInterface;

  return true;
}

uint8_t tuh_midi_get_tx_cable_count (uint8_t idx) {
  TU_VERIFY(idx < CFG_TUH_MIDI);
  midih_interface_t *p_midi = &_midi_host[idx];
  TU_VERIFY(p_midi->ep_stream.tx.ep_addr != 0, 0);
  return p_midi->tx_cable_count;
}

uint8_t tuh_midi_get_rx_cable_count (uint8_t idx) {
  TU_VERIFY(idx < CFG_TUH_MIDI);
  midih_interface_t *p_midi = &_midi_host[idx];
  TU_VERIFY(p_midi->ep_stream.rx.ep_addr != 0, 0);
  return p_midi->rx_cable_count;
}

uint32_t tuh_midi_read_available(uint8_t idx) {
  TU_VERIFY(idx < CFG_TUH_MIDI);
  midih_interface_t *p_midi = &_midi_host[idx];
  return tu_edpt_stream_read_available(&p_midi->ep_stream.rx);
}

uint32_t tuh_midi_write_flush(uint8_t idx) {
  TU_VERIFY(idx < CFG_TUH_MIDI);
  midih_interface_t *p_midi = &_midi_host[idx];
  return tu_edpt_stream_write_xfer(p_midi->daddr, &p_midi->ep_stream.tx);
}

//--------------------------------------------------------------------+
// Packet API
//--------------------------------------------------------------------+
uint32_t tuh_midi_packet_read_n(uint8_t idx, uint8_t* buffer, uint32_t bufsize) {
  TU_VERIFY(idx < CFG_TUH_MIDI && buffer && bufsize > 0, 0);
  midih_interface_t *p_midi = &_midi_host[idx];

  uint32_t count4 = tu_min32(bufsize, tu_edpt_stream_read_available(&p_midi->ep_stream.rx));
  count4 = tu_align4(count4); // round down to multiple of 4
  TU_VERIFY(count4 > 0, 0);
  return tu_edpt_stream_read(p_midi->daddr, &p_midi->ep_stream.rx, buffer, count4);
}

uint32_t tuh_midi_packet_write_n(uint8_t idx, const uint8_t* buffer, uint32_t bufsize) {
  TU_VERIFY(idx < CFG_TUH_MIDI && buffer && bufsize > 0, 0);
  midih_interface_t *p_midi = &_midi_host[idx];

  const uint32_t bufsize4 = tu_align4(bufsize);
  TU_VERIFY(bufsize4 > 0, 0);
  return tu_edpt_stream_write(p_midi->daddr, &p_midi->ep_stream.tx, buffer, bufsize4);
}

//--------------------------------------------------------------------+
// Stream API
//--------------------------------------------------------------------+
#if CFG_TUH_MIDI_STREAM_API
uint32_t tuh_midi_stream_write(uint8_t idx, uint8_t cable_num, uint8_t const *buffer, uint32_t bufsize) {
  TU_VERIFY(idx < CFG_TUH_MIDI && buffer && bufsize > 0);
  midih_interface_t *p_midi = &_midi_host[idx];
  TU_VERIFY(cable_num < p_midi->tx_cable_count);
  midi_driver_stream_t *stream = &p_midi->stream_write;

  uint32_t byte_count = 0;
  while ((byte_count < bufsize) && (tu_edpt_stream_write_available(p_midi->daddr, &p_midi->ep_stream.tx) >= 4)) {
    uint8_t const data = buffer[byte_count];
    byte_count++;
    if (data >= MIDI_STATUS_SYSREAL_TIMING_CLOCK) {
      // real-time messages need to be sent right away
      midi_driver_stream_t streamrt;
      streamrt.buffer[0] = MIDI_CIN_SYSEX_END_1BYTE;
      streamrt.buffer[1] = data;
      streamrt.index = 2;
      streamrt.total = 2;
      uint32_t const count = tu_edpt_stream_write(p_midi->daddr, &p_midi->ep_stream.tx, streamrt.buffer, 4);
      TU_ASSERT(count == 4, byte_count); // Check FIFO overflown, since we already check fifo remaining. It is probably race condition
    } else if (stream->index == 0) {
      //------------- New event packet -------------//

      uint8_t const msg = data >> 4;

      stream->index = 2;
      stream->buffer[1] = data;

      // Check to see if we're still in a SysEx transmit.
      if (stream->buffer[0] == MIDI_CIN_SYSEX_START) {
        if (data == MIDI_STATUS_SYSEX_END) {
          stream->buffer[0] = MIDI_CIN_SYSEX_END_1BYTE;
          stream->total = 2;
        } else {
          stream->total = 4;
        }
      } else if ((msg >= 0x8 && msg <= 0xB) || msg == 0xE) {
        // Channel Voice Messages
        stream->buffer[0] = (uint8_t) ((cable_num << 4) | msg);
        stream->total = 4;
      } else if (msg == 0xC || msg == 0xD) {
        // Channel Voice Messages, two-byte variants (Program Change and Channel Pressure)
        stream->buffer[0] = (uint8_t) ((cable_num << 4) | msg);
        stream->total = 3;
      } else if (msg == 0xf) {
        // System message
        if (data == MIDI_STATUS_SYSEX_START) {
          stream->buffer[0] = MIDI_CIN_SYSEX_START;
          stream->total = 4;
        } else if (data == MIDI_STATUS_SYSCOM_TIME_CODE_QUARTER_FRAME || data == MIDI_STATUS_SYSCOM_SONG_SELECT) {
          stream->buffer[0] = MIDI_CIN_SYSCOM_2BYTE;
          stream->total = 3;
        } else if (data == MIDI_STATUS_SYSCOM_SONG_POSITION_POINTER) {
          stream->buffer[0] = MIDI_CIN_SYSCOM_3BYTE;
          stream->total = 4;
        } else {
          stream->buffer[0] = MIDI_CIN_SYSEX_END_1BYTE;
          stream->total = 2;
        }
      } else {
        // Pack individual bytes if we don't support packing them into words.
        stream->buffer[0] = (uint8_t) (cable_num << 4 | 0xf);
        stream->buffer[2] = 0;
        stream->buffer[3] = 0;
        stream->index = 2;
        stream->total = 2;
      }
    } else {
      //------------- On-going (buffering) packet -------------//
      TU_ASSERT(stream->index < 4, byte_count);
      stream->buffer[stream->index] = data;
      stream->index++;
      // See if this byte ends a SysEx.
      if (stream->buffer[0] == MIDI_CIN_SYSEX_START && data == MIDI_STATUS_SYSEX_END) {
        stream->buffer[0] = MIDI_CIN_SYSEX_START + (stream->index - 1);
        stream->total = stream->index;
      }
    }

    // Send out packet
    if (stream->index >= 2 && stream->index == stream->total) {
      // zeroes unused bytes
      for (uint8_t i = stream->total; i < 4; i++) {
        stream->buffer[i] = 0;
      }
      TU_LOG3_MEM(stream->buffer, 4, 2);

      const uint32_t count = tu_edpt_stream_write(p_midi->daddr, &p_midi->ep_stream.tx, stream->buffer, 4);

      // complete current event packet, reset stream
      stream->index = 0;
      stream->total = 0;

      // FIFO overflown, since we already check fifo remaining. It is probably race condition
      TU_ASSERT(count == 4, byte_count);
    }
  }
  return byte_count;
}

uint32_t tuh_midi_stream_read(uint8_t idx, uint8_t *p_cable_num, uint8_t *p_buffer, uint16_t bufsize) {
  TU_VERIFY(idx < CFG_TUH_MIDI && p_cable_num && p_buffer && bufsize > 0);
  midih_interface_t *p_midi = &_midi_host[idx];
  uint32_t bytes_buffered = 0;
  uint8_t one_byte;
  if (!tu_edpt_stream_peek(&p_midi->ep_stream.rx, &one_byte)) {
    return 0;
  }
  *p_cable_num = (one_byte >> 4) & 0xf;
  uint32_t nread = tu_edpt_stream_read(p_midi->daddr, &p_midi->ep_stream.rx, p_midi->stream_read.buffer, 4);
  static uint16_t cable_sysex_in_progress;// bit i is set if received MIDI_STATUS_SYSEX_START but not MIDI_STATUS_SYSEX_END
  while (nread == 4 && bytes_buffered < bufsize) {
    *p_cable_num = (p_midi->stream_read.buffer[0] >> 4) & 0x0f;
    uint8_t bytes_to_add_to_stream = 0;
    if (*p_cable_num < p_midi->rx_cable_count) {
      // ignore the CIN field; too many devices out there encode this wrong
      uint8_t status = p_midi->stream_read.buffer[1];
      uint16_t cable_mask = (uint16_t) (1 << *p_cable_num);
      if (status <= MIDI_MAX_DATA_VAL || status == MIDI_STATUS_SYSEX_START) {
        if (status == MIDI_STATUS_SYSEX_START) {
          cable_sysex_in_progress |= cable_mask;
        }
        // only add the packet if a sysex message is in progress
        if (cable_sysex_in_progress & cable_mask) {
          ++bytes_to_add_to_stream;
          for (uint8_t i = 2; i < 4; i++) {
            if (p_midi->stream_read.buffer[i] <= MIDI_MAX_DATA_VAL) {
              ++bytes_to_add_to_stream;
            } else if (p_midi->stream_read.buffer[i] == MIDI_STATUS_SYSEX_END) {
              ++bytes_to_add_to_stream;
              cable_sysex_in_progress &= (uint16_t) ~cable_mask;
              i = 4;// force the loop to exit; I hate break statements in loops
            }
          }
        }
      } else if (status < MIDI_STATUS_SYSEX_START) {
        // then it is a channel message either three bytes or two
        uint8_t fake_cin = (status & 0xf0) >> 4;
        switch (fake_cin) {
          case MIDI_CIN_NOTE_OFF:
          case MIDI_CIN_NOTE_ON:
          case MIDI_CIN_POLY_KEYPRESS:
          case MIDI_CIN_CONTROL_CHANGE:
          case MIDI_CIN_PITCH_BEND_CHANGE:
            bytes_to_add_to_stream = 3;
            break;
          case MIDI_CIN_PROGRAM_CHANGE:
          case MIDI_CIN_CHANNEL_PRESSURE:
            bytes_to_add_to_stream = 2;
            break;
          default:
            break;// Should not get this
        }
        cable_sysex_in_progress &= (uint16_t) ~cable_mask;
      } else if (status < MIDI_STATUS_SYSREAL_TIMING_CLOCK) {
        switch (status) {
          case MIDI_STATUS_SYSCOM_TIME_CODE_QUARTER_FRAME:
          case MIDI_STATUS_SYSCOM_SONG_SELECT:
            bytes_to_add_to_stream = 2;
            break;
          case MIDI_STATUS_SYSCOM_SONG_POSITION_POINTER:
            bytes_to_add_to_stream = 3;
            break;
          case MIDI_STATUS_SYSCOM_TUNE_REQUEST:
          case MIDI_STATUS_SYSEX_END:
            bytes_to_add_to_stream = 1;
            break;
          default:
            break;
        }
        cable_sysex_in_progress &= (uint16_t) ~cable_mask;
      } else {
        // Real-time message: can be inserted into a sysex message,
        // so do don't clear cable_sysex_in_progress bit
        bytes_to_add_to_stream = 1;
      }
    }

    for (uint8_t i = 1; i <= bytes_to_add_to_stream; i++) {
      *p_buffer++ = p_midi->stream_read.buffer[i];
    }
    bytes_buffered += bytes_to_add_to_stream;
    nread = 0;
    if (tu_edpt_stream_peek(&p_midi->ep_stream.rx, &one_byte)) {
      uint8_t new_cable = (one_byte >> 4) & 0xf;
      if (new_cable == *p_cable_num) {
        // still on the same cable. Continue reading the stream
        nread = tu_edpt_stream_read(p_midi->daddr, &p_midi->ep_stream.rx, p_midi->stream_read.buffer, 4);
      }
    }
  }

  return bytes_buffered;
}
#endif

#endif

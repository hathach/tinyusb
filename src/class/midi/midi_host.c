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
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+

typedef struct {
  uint8_t dev_addr;
  uint8_t itf_num;

  uint8_t ep_in;          // IN endpoint address
  uint8_t ep_out;         // OUT endpoint address

  uint8_t num_cables_rx;  // IN endpoint CS descriptor bNumEmbMIDIJack value
  uint8_t num_cables_tx;  // OUT endpoint CS descriptor bNumEmbMIDIJack value

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

  bool configured;
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
TU_ATTR_ALWAYS_INLINE static inline midih_interface_t* find_midi_by_daddr(uint8_t dev_addr) {
  for (uint8_t i = 0; i < CFG_TUH_MIDI; i++) {
    if (_midi_host[i].dev_addr == dev_addr) {
      return &_midi_host[i];
    }
  }
  return NULL;
}

TU_ATTR_ALWAYS_INLINE static inline midih_interface_t* find_new_midi(void) {
  return find_midi_by_daddr(0);
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

void midih_close(uint8_t dev_addr) {
  midih_interface_t *p_midi_host = find_midi_by_daddr(dev_addr);
  if (p_midi_host == NULL) {
    return;
  }
  if (tuh_midi_umount_cb) {
    tuh_midi_umount_cb(dev_addr);
  }
  p_midi_host->ep_in = 0;
  p_midi_host->ep_out = 0;
  p_midi_host->itf_num = 0;
  p_midi_host->num_cables_rx = 0;
  p_midi_host->num_cables_tx = 0;
  p_midi_host->dev_addr = 0;
  p_midi_host->configured = false;
  tu_memclr(&p_midi_host->stream_read, sizeof(p_midi_host->stream_read));
  tu_memclr(&p_midi_host->stream_write, sizeof(p_midi_host->stream_write));

  tu_edpt_stream_close(&p_midi_host->ep_stream.rx);
  tu_edpt_stream_close(&p_midi_host->ep_stream.tx);
}

bool midih_xfer_cb(uint8_t dev_addr, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes) {
  (void) result;
  midih_interface_t *p_midi_host = find_midi_by_daddr(dev_addr);
  TU_VERIFY(p_midi_host != NULL);
  if (ep_addr == p_midi_host->ep_stream.rx.ep_addr) {
    // receive new data if available
    if (xferred_bytes) {
      // put in the RX FIFO only non-zero MIDI IN 4-byte packets
      uint32_t packets_queued = 0;
      uint8_t *buf = _midi_epbuf->rx;
      const uint32_t npackets = xferred_bytes / 4;
      for (uint32_t p = 0; p < npackets; p++) {
        // some devices send back all zero packets even if there is no data ready
        const uint32_t packet = tu_unaligned_read32(buf);
        if (packet != 0) {
          tu_edpt_stream_read_xfer_complete_with_buf(&p_midi_host->ep_stream.rx, buf, 4);
          ++packets_queued;
          TU_LOG3("MIDI RX=%08x\r\n", packet);
        }
        buf += 4;
      }

      if (tuh_midi_rx_cb) {
        tuh_midi_rx_cb(dev_addr, packets_queued); // invoke receive callback
      }
    }

    tu_edpt_stream_read_xfer(dev_addr, &p_midi_host->ep_stream.rx); // prepare for next transfer
  } else if (ep_addr == p_midi_host->ep_stream.tx.ep_addr) {
    if (tuh_midi_tx_cb) {
      tuh_midi_tx_cb(dev_addr);
    }
    if (0 == tu_edpt_stream_write_xfer(dev_addr, &p_midi_host->ep_stream.tx)) {
      // If there is no data left, a ZLP should be sent if
      // xferred_bytes is multiple of EP size and not zero
      tu_edpt_stream_write_zlp_if_needed(dev_addr, &p_midi_host->ep_stream.tx, xferred_bytes);
    }
  }

  return true;
}

//--------------------------------------------------------------------+
// Enumeration
//--------------------------------------------------------------------+
bool midih_open(uint8_t rhport, uint8_t dev_addr, tusb_desc_interface_t const *desc_itf, uint16_t max_len) {
  (void) rhport;

  midih_interface_t *p_midi = find_new_midi();
  TU_VERIFY(p_midi != NULL);

  TU_VERIFY(TUSB_CLASS_AUDIO == desc_itf->bInterfaceClass);
  // There can be just a MIDI interface or an audio and a MIDI interface. Only open the MIDI interface
  uint8_t const *p_desc = (uint8_t const *) desc_itf;
  uint16_t len_parsed = 0;
  if (AUDIO_SUBCLASS_CONTROL == desc_itf->bInterfaceSubClass) {
    // This driver does not support audio streaming. However, if this is the audio control interface
    // there might be a MIDI interface following it. Search through every descriptor until a MIDI
    // interface is found or the end of the descriptor is found
    while (len_parsed < max_len &&
          (desc_itf->bInterfaceClass != TUSB_CLASS_AUDIO || desc_itf->bInterfaceSubClass != AUDIO_SUBCLASS_MIDI_STREAMING)) {
      len_parsed += desc_itf->bLength;
      p_desc = tu_desc_next(p_desc);
      desc_itf = (tusb_desc_interface_t const *)p_desc;
    }

    TU_VERIFY(TUSB_CLASS_AUDIO == desc_itf->bInterfaceClass);
  }
  TU_VERIFY(AUDIO_SUBCLASS_MIDI_STREAMING == desc_itf->bInterfaceSubClass);
  len_parsed += desc_itf->bLength;

  TU_LOG_DRV("MIDI opening Interface %u (addr = %u)\r\n", desc_itf->bInterfaceNumber, dev_addr);
  p_midi->itf_num = desc_itf->bInterfaceNumber;

  // CS Header descriptor
  p_desc = tu_desc_next(p_desc);
  midi_desc_header_t const *p_mdh = (midi_desc_header_t const *) p_desc;
  TU_VERIFY(p_mdh->bDescriptorType == TUSB_DESC_CS_INTERFACE &&
            p_mdh->bDescriptorSubType == MIDI_CS_INTERFACE_HEADER);
  TU_LOG_DRV("  Interface Header descriptor\r\n");

  // p_desc = tu_desc_next(p_desc);
  uint8_t prev_ep_addr = 0; // the CS endpoint descriptor is associated with the previous endpoint descriptor
  tusb_desc_endpoint_t const* in_desc = NULL;
  tusb_desc_endpoint_t const* out_desc = NULL;
  while (len_parsed < max_len) {
    TU_VERIFY((p_mdh->bDescriptorType == TUSB_DESC_CS_INTERFACE) ||
              (p_mdh->bDescriptorType == TUSB_DESC_CS_ENDPOINT && p_mdh->bDescriptorSubType == MIDI_CS_ENDPOINT_GENERAL) ||
              p_mdh->bDescriptorType == TUSB_DESC_ENDPOINT);

    if (p_mdh->bDescriptorType == TUSB_DESC_CS_INTERFACE) {
      // The USB host doesn't really need this information unless it uses
      // the string descriptor for a jack or Element

      // assume it is an input jack
      midi_desc_in_jack_t const *p_mdij = (midi_desc_in_jack_t const *) p_desc;
      if (p_mdij->bDescriptorSubType == MIDI_CS_INTERFACE_HEADER) {
        TU_LOG_DRV("  Interface Header descriptor\r\n");
      } else if (p_mdij->bDescriptorSubType == MIDI_CS_INTERFACE_IN_JACK) {
        // Then it is an in jack.
        TU_LOG_DRV("  IN Jack %s descriptor \r\n", p_mdij->bJackType == MIDI_JACK_EXTERNAL ? "External" : "Embedded");
      } else if (p_mdij->bDescriptorSubType == MIDI_CS_INTERFACE_OUT_JACK) {
        // then it is an out jack
        TU_LOG_DRV("  OUT Jack %s descriptor\r\n", p_mdij->bJackType == MIDI_JACK_EXTERNAL ? "External" : "Embedded");
      } else if (p_mdij->bDescriptorSubType == MIDI_CS_INTERFACE_ELEMENT) {
        // the it is an element;
        TU_LOG_DRV("Found element\r\n");
      } else {
        TU_LOG_DRV("  Unknown CS Interface sub-type %u\r\n", p_mdij->bDescriptorSubType);
        TU_VERIFY(false);// unknown CS Interface sub-type
      }
      len_parsed += p_mdij->bLength;
    } else if (p_mdh->bDescriptorType == TUSB_DESC_CS_ENDPOINT) {
      TU_LOG_DRV("  CS_ENDPOINT descriptor\r\n");
      TU_VERIFY(prev_ep_addr != 0);
      // parse out the mapping between the device's embedded jacks and the endpoints
      // Each embedded IN jack is associated with an OUT endpoint
      midi_desc_cs_endpoint_t const *p_csep = (midi_desc_cs_endpoint_t const *) p_mdh;
      if (tu_edpt_dir(prev_ep_addr) == TUSB_DIR_OUT) {
        TU_VERIFY(p_midi->ep_out == prev_ep_addr);
        TU_VERIFY(p_midi->num_cables_tx == 0);
        p_midi->num_cables_tx = p_csep->bNumEmbMIDIJack;
      } else {
        TU_VERIFY(p_midi->ep_in == prev_ep_addr);
        TU_VERIFY(p_midi->num_cables_rx == 0);
        p_midi->num_cables_rx = p_csep->bNumEmbMIDIJack;
      }
      len_parsed += p_csep->bLength;
      prev_ep_addr = 0;
    } else if (p_mdh->bDescriptorType == TUSB_DESC_ENDPOINT) {
      // parse out the bulk endpoint info
      tusb_desc_endpoint_t const *p_ep = (tusb_desc_endpoint_t const *) p_mdh;
      TU_LOG_DRV("  Endpoint descriptor %02x\r\n", p_ep->bEndpointAddress);
      if (tu_edpt_dir(p_ep->bEndpointAddress) == TUSB_DIR_OUT) {
        TU_VERIFY(p_midi->ep_out == 0);
        TU_VERIFY(p_midi->num_cables_tx == 0);
        p_midi->ep_out = p_ep->bEndpointAddress;
        prev_ep_addr = p_midi->ep_out;
        out_desc = p_ep;
      } else {
        TU_VERIFY(p_midi->ep_in == 0);
        TU_VERIFY(p_midi->num_cables_rx == 0);
        p_midi->ep_in = p_ep->bEndpointAddress;
        prev_ep_addr = p_midi->ep_in;
        in_desc = p_ep;
      }
      len_parsed += p_mdh->bLength;
    }
    p_desc = tu_desc_next(p_desc);
    p_mdh = (midi_desc_header_t const *) p_desc;
  }
  TU_VERIFY((p_midi->ep_out != 0 && p_midi->num_cables_tx != 0) ||
            (p_midi->ep_in != 0 && p_midi->num_cables_rx != 0));

  if (in_desc) {
    TU_ASSERT(tuh_edpt_open(dev_addr, in_desc));
    tu_edpt_stream_open(&p_midi->ep_stream.rx, in_desc);
  }
  if (out_desc) {
    TU_ASSERT(tuh_edpt_open(dev_addr, out_desc));
    tu_edpt_stream_open(&p_midi->ep_stream.tx, out_desc);
  }
  p_midi->dev_addr = dev_addr;

  // if (tuh_midi_interface_descriptor_cb) {
  //   tuh_midi_interface_descriptor_cb(dev_addr, desc_itf, );
  // }

  return true;
}

bool midih_set_config(uint8_t dev_addr, uint8_t itf_num) {
  (void) itf_num;
  midih_interface_t *p_midi_host = find_midi_by_daddr(dev_addr);
  TU_VERIFY(p_midi_host != NULL);
  p_midi_host->configured = true;

  if (tuh_midi_mount_cb) {
    tuh_midi_mount_cb(dev_addr, p_midi_host->num_cables_rx, p_midi_host->num_cables_tx);
  }

  tu_edpt_stream_read_xfer(dev_addr, &p_midi_host->ep_stream.rx); // prepare for incoming data

  // No special config things to do for MIDI
  usbh_driver_set_config_complete(dev_addr, p_midi_host->itf_num);
  return true;
}

//--------------------------------------------------------------------+
// API
//--------------------------------------------------------------------+
bool tuh_midi_mounted(uint8_t dev_addr) {
  midih_interface_t *p_midi_host = find_midi_by_daddr(dev_addr);
  TU_VERIFY(p_midi_host != NULL);
  return p_midi_host->configured;
}

uint8_t tuh_midi_get_num_tx_cables (uint8_t dev_addr) {
  midih_interface_t *p_midi_host = find_midi_by_daddr(dev_addr);
  TU_VERIFY(p_midi_host != NULL, 0);
  TU_VERIFY(p_midi_host->ep_stream.tx.ep_addr != 0, 0);
  return p_midi_host->num_cables_tx;
}

uint8_t tuh_midi_get_num_rx_cables (uint8_t dev_addr) {
  midih_interface_t *p_midi_host = find_midi_by_daddr(dev_addr);
  TU_VERIFY(p_midi_host != NULL, 0);
  TU_VERIFY(p_midi_host->ep_stream.rx.ep_addr != 0, 0);
  return p_midi_host->num_cables_rx;
}

uint32_t tuh_midi_read_available(uint8_t dev_addr) {
  midih_interface_t *p_midi_host = find_midi_by_daddr(dev_addr);
  TU_VERIFY(p_midi_host != NULL);
  return tu_edpt_stream_read_available(&p_midi_host->ep_stream.rx);
}

uint32_t tuh_midi_write_flush(uint8_t dev_addr) {
  midih_interface_t *p_midi = find_midi_by_daddr(dev_addr);
  TU_VERIFY(p_midi != NULL);
  return tu_edpt_stream_write_xfer(p_midi->dev_addr, &p_midi->ep_stream.tx);
}

//--------------------------------------------------------------------+
// Packet API
//--------------------------------------------------------------------+

uint32_t tuh_midi_packet_read_n(uint8_t dev_addr, uint8_t* buffer, uint32_t bufsize) {
  midih_interface_t *p_midi_host = find_midi_by_daddr(dev_addr);
  TU_VERIFY(p_midi_host != NULL);

  uint32_t count4 = tu_min32(bufsize, tu_edpt_stream_read_available(&p_midi_host->ep_stream.rx));
  count4 = tu_align4(count4); // round down to multiple of 4
  TU_VERIFY(count4 > 0, 0);
  return tu_edpt_stream_read(dev_addr, &p_midi_host->ep_stream.rx, buffer, count4);
}

uint32_t tuh_midi_packet_write_n(uint8_t dev_addr, const uint8_t* buffer, uint32_t bufsize) {
  midih_interface_t *p_midi_host = find_midi_by_daddr(dev_addr);
  TU_VERIFY(p_midi_host != NULL, 0);
  uint32_t bufsize4 = tu_align4(bufsize);
  return tu_edpt_stream_write(dev_addr, &p_midi_host->ep_stream.tx, buffer, bufsize4);
}

//--------------------------------------------------------------------+
// Stream API
//--------------------------------------------------------------------+
#if CFG_TUH_MIDI_STREAM_API
uint32_t tuh_midi_stream_write(uint8_t dev_addr, uint8_t cable_num, uint8_t const *buffer, uint32_t bufsize) {
  midih_interface_t *p_midi_host = find_midi_by_daddr(dev_addr);
  TU_VERIFY(p_midi_host != NULL);
  TU_VERIFY(cable_num < p_midi_host->num_cables_tx);
  midi_driver_stream_t *stream = &p_midi_host->stream_write;

  uint32_t i = 0;
  while ((i < bufsize) && (tu_edpt_stream_write_available(dev_addr, &p_midi_host->ep_stream.tx) >= 4)) {
    uint8_t const data = buffer[i];
    i++;
    if (data >= MIDI_STATUS_SYSREAL_TIMING_CLOCK) {
      // real-time messages need to be sent right away
      midi_driver_stream_t streamrt;
      streamrt.buffer[0] = MIDI_CIN_SYSEX_END_1BYTE;
      streamrt.buffer[1] = data;
      streamrt.index = 2;
      streamrt.total = 2;
      uint32_t const count = tu_edpt_stream_write(dev_addr, &p_midi_host->ep_stream.tx, streamrt.buffer, 4);
      TU_ASSERT(count == 4, i); // Check FIFO overflown, since we already check fifo remaining. It is probably race condition
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
      TU_ASSERT(stream->index < 4, i);
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
      for (uint8_t idx = stream->total; idx < 4; idx++) { stream->buffer[idx] = 0; }
      TU_LOG3_MEM(stream->buffer, 4, 2);

      uint32_t const count = tu_edpt_stream_write(dev_addr, &p_midi_host->ep_stream.tx, stream->buffer, 4);

      // complete current event packet, reset stream
      stream->index = 0;
      stream->total = 0;

      // FIFO overflown, since we already check fifo remaining. It is probably race condition
      TU_ASSERT(count == 4, i);
    }
  }
  return i;
}

uint32_t tuh_midi_stream_read(uint8_t dev_addr, uint8_t *p_cable_num, uint8_t *p_buffer, uint16_t bufsize) {
  midih_interface_t *p_midi_host = find_midi_by_daddr(dev_addr);
  TU_VERIFY(p_midi_host != NULL);
  uint32_t bytes_buffered = 0;
  TU_ASSERT(p_cable_num);
  TU_ASSERT(p_buffer);
  TU_ASSERT(bufsize);
  uint8_t one_byte;
  if (!tu_edpt_stream_peek(&p_midi_host->ep_stream.rx, &one_byte)) {
    return 0;
  }
  *p_cable_num = (one_byte >> 4) & 0xf;
  uint32_t nread = tu_edpt_stream_read(dev_addr, &p_midi_host->ep_stream.rx, p_midi_host->stream_read.buffer, 4);
  static uint16_t cable_sysex_in_progress; // bit i is set if received MIDI_STATUS_SYSEX_START but not MIDI_STATUS_SYSEX_END
  while (nread == 4 && bytes_buffered < bufsize) {
    *p_cable_num = (p_midi_host->stream_read.buffer[0] >> 4) & 0x0f;
    uint8_t bytes_to_add_to_stream = 0;
    if (*p_cable_num < p_midi_host->num_cables_rx) {
      // ignore the CIN field; too many devices out there encode this wrong
      uint8_t status = p_midi_host->stream_read.buffer[1];
      uint16_t cable_mask = (uint16_t) (1 << *p_cable_num);
      if (status <= MIDI_MAX_DATA_VAL || status == MIDI_STATUS_SYSEX_START) {
        if (status == MIDI_STATUS_SYSEX_START) {
          cable_sysex_in_progress |= cable_mask;
        }
        // only add the packet if a sysex message is in progress
        if (cable_sysex_in_progress & cable_mask) {
          ++bytes_to_add_to_stream;
          for (uint8_t idx = 2; idx < 4; idx++) {
            if (p_midi_host->stream_read.buffer[idx] <= MIDI_MAX_DATA_VAL) {
              ++bytes_to_add_to_stream;
            } else if (p_midi_host->stream_read.buffer[idx] == MIDI_STATUS_SYSEX_END) {
              ++bytes_to_add_to_stream;
              cable_sysex_in_progress &= (uint16_t) ~cable_mask;
              idx = 4;// force the loop to exit; I hate break statements in loops
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
            cable_sysex_in_progress &= (uint16_t) ~cable_mask;
        }
      } else {
        // Real-time message: can be inserted into a sysex message,
        // so do don't clear cable_sysex_in_progress bit
        bytes_to_add_to_stream = 1;
      }
    }

    for (uint8_t idx = 1; idx <= bytes_to_add_to_stream; idx++) {
      *p_buffer++ = p_midi_host->stream_read.buffer[idx];
    }
    bytes_buffered += bytes_to_add_to_stream;
    nread = 0;
    if (tu_edpt_stream_peek(&p_midi_host->ep_stream.rx, &one_byte)) {
      uint8_t new_cable = (one_byte >> 4) & 0xf;
      if (new_cable == *p_cable_num) {
        // still on the same cable. Continue reading the stream
        nread = tu_edpt_stream_read(dev_addr, &p_midi_host->ep_stream.rx, p_midi_host->stream_read.buffer, 4);
      }
    }
  }

  return bytes_buffered;
}

#endif

#endif

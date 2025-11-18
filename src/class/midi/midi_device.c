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

#if (CFG_TUD_ENABLED && CFG_TUD_MIDI)

//--------------------------------------------------------------------+
// INCLUDE
//--------------------------------------------------------------------+
#include "device/usbd.h"
#include "device/usbd_pvt.h"

#include "midi_device.h"

//--------------------------------------------------------------------+
// Weak stubs: invoked if no strong implementation is available
//--------------------------------------------------------------------+
TU_ATTR_WEAK void tud_midi_rx_cb(uint8_t itf) {
  (void)itf;
}

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+
typedef struct {
  uint8_t rhport;
  uint8_t itf_num;

  // For Stream read()/write() API
  // Messages are always 4 bytes long, queue them for reading and writing so the
  // callers can use the Stream interface with single-byte read/write calls.
  midi_driver_stream_t stream_write;
  midi_driver_stream_t stream_read;

  /*------------- From this point, data is not cleared by bus reset -------------*/
  // Endpoint stream
  struct {
    tu_edpt_stream_t tx;
    tu_edpt_stream_t rx;

    uint8_t rx_ff_buf[CFG_TUD_MIDI_RX_BUFSIZE];
    uint8_t tx_ff_buf[CFG_TUD_MIDI_TX_BUFSIZE];
  } ep_stream;
} midid_interface_t;

#define ITF_MEM_RESET_SIZE offsetof(midid_interface_t, ep_stream)

static midid_interface_t _midid_itf[CFG_TUD_MIDI];

// Endpoint Transfer buffer
typedef struct {
  TUD_EPBUF_DEF(epin, CFG_TUD_MIDI_EP_BUFSIZE);
  TUD_EPBUF_DEF(epout, CFG_TUD_MIDI_EP_BUFSIZE);
} midid_epbuf_t;

CFG_TUD_MEM_SECTION static midid_epbuf_t _midid_epbuf[CFG_TUD_MIDI];

//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+
bool tud_midi_n_mounted (uint8_t itf) {
  midid_interface_t *p_midi = &_midid_itf[itf];

  const bool tx_opened = tu_edpt_stream_is_opened(&p_midi->ep_stream.tx);
  const bool rx_opened = tu_edpt_stream_is_opened(&p_midi->ep_stream.rx);
  return tx_opened && rx_opened;
}

//--------------------------------------------------------------------+
// READ API
//--------------------------------------------------------------------+
uint32_t tud_midi_n_available(uint8_t itf, uint8_t cable_num) {
  (void) cable_num;
  const midid_interface_t    *p_midi = &_midid_itf[itf];
  const midi_driver_stream_t *stream = &p_midi->stream_read;
  const tu_edpt_stream_t     *ep_str = &p_midi->ep_stream.rx;

  // when using with packet API stream total & index are both zero
  return tu_edpt_stream_read_available(ep_str) + (uint8_t)(stream->total - stream->index);
}

uint32_t tud_midi_n_stream_read(uint8_t itf, uint8_t cable_num, void *buffer, uint32_t bufsize) {
  (void) cable_num;
  TU_VERIFY(buffer != NULL && bufsize > 0, 0);

  uint8_t              *buf8   = (uint8_t *)buffer;
  midid_interface_t    *p_midi = &_midid_itf[itf];
  midi_driver_stream_t *stream = &p_midi->stream_read;

  uint32_t total_read = 0;
  while (bufsize > 0) {
    // Get new packet from fifo, then set packet expected bytes
    if (stream->total == 0) {
      if (!tud_midi_n_packet_read(itf, stream->buffer)) {
        return total_read; // return if there is no more data from fifo
      }

      const uint8_t code_index = stream->buffer[0] & 0x0f;

      // MIDI 1.0 Table 4-1: Code Index Number Classifications
      switch (code_index) {
        case MIDI_CIN_MISC:
        case MIDI_CIN_CABLE_EVENT:
          // These are reserved and unused, possibly issue somewhere, skip this packet
          return 0;

        case MIDI_CIN_SYSEX_END_1BYTE:
        case MIDI_CIN_1BYTE_DATA:
          stream->total = 1;
          break;

        case MIDI_CIN_SYSCOM_2BYTE     :
        case MIDI_CIN_SYSEX_END_2BYTE  :
        case MIDI_CIN_PROGRAM_CHANGE   :
        case MIDI_CIN_CHANNEL_PRESSURE :
          stream->total = 2;
          break;

        default:
          stream->total = 3;
          break;
      }
    }

    // Copy data up to bufsize
    const uint8_t count = (uint8_t)tu_min32((uint32_t)(stream->total - stream->index), bufsize);

    // Skip the header (1st byte) in the buffer
    TU_VERIFY(0 == tu_memcpy_s(buf8, bufsize, stream->buffer + 1 + stream->index, count));

    total_read += count;
    stream->index += count;
    buf8 += count;
    bufsize -= count;

    // complete current event packet, reset stream
    if (stream->total == stream->index) {
      stream->index = 0;
      stream->total = 0;
    }
  }

  return total_read;
}

bool tud_midi_n_packet_read(uint8_t itf, uint8_t packet[4]) {
  midid_interface_t *p_midi = &_midid_itf[itf];
  tu_edpt_stream_t  *ep_str = &p_midi->ep_stream.rx;
  TU_VERIFY(tu_edpt_stream_is_opened(ep_str));
  return 4 == tu_edpt_stream_read(p_midi->rhport, ep_str, packet, 4);
}

uint32_t tud_midi_n_packet_read_n(uint8_t itf, uint8_t packets[], uint32_t max_packets) {
  midid_interface_t *p_midi = &_midid_itf[itf];
  tu_edpt_stream_t  *ep_str = &p_midi->ep_stream.rx;
  TU_VERIFY(tu_edpt_stream_is_opened(ep_str), 0);

  const uint32_t num_read = tu_edpt_stream_read(p_midi->rhport, ep_str, packets, 4u * max_packets);
  return num_read >> 2u;
}

//--------------------------------------------------------------------+
// WRITE API
//--------------------------------------------------------------------+
uint32_t tud_midi_n_stream_write(uint8_t itf, uint8_t cable_num, const uint8_t *buffer, uint32_t bufsize) {
  midid_interface_t *p_midi = &_midid_itf[itf];
  midi_driver_stream_t *stream = &p_midi->stream_write;
  tu_edpt_stream_t  *ep_str = &p_midi->ep_stream.tx;
  TU_VERIFY(tu_edpt_stream_is_opened(ep_str), 0);

  uint32_t i = 0;
  while (i < bufsize) {
    if (tu_edpt_stream_write_available(p_midi->rhport, ep_str) < 4) {
      break;
    }

    const uint8_t data = buffer[i];
    i++;

    if (stream->index == 0) {
      //------------- New event packet -------------//
      const uint8_t msg = data >> 4;

      stream->index     = 2;
      stream->buffer[1] = data;

      // Check to see if we're still in a SysEx transmit.
      if (((stream->buffer[0]) & 0xF) == MIDI_CIN_SYSEX_START) {
        if (data == MIDI_STATUS_SYSEX_END) {
          stream->buffer[0] = (uint8_t)((cable_num << 4) | MIDI_CIN_SYSEX_END_1BYTE);
          stream->total     = 2;
        } else {
          stream->total = 4;
        }
      } else if ((msg >= 0x8 && msg <= 0xB) || msg == 0xE) {
        // Channel Voice Messages
        stream->buffer[0] = (uint8_t)((cable_num << 4) | msg);
        stream->total     = 4;
      } else if (msg == 0xC || msg == 0xD) {
        // Channel Voice Messages, two-byte variants (Program Change and Channel Pressure)
        stream->buffer[0] = (uint8_t)((cable_num << 4) | msg);
        stream->total     = 3;
      } else if (msg == 0xf) {
        // System message
        if (data == MIDI_STATUS_SYSEX_START) {
          stream->buffer[0] = MIDI_CIN_SYSEX_START;
          stream->total     = 4;
        } else if (data == MIDI_STATUS_SYSCOM_TIME_CODE_QUARTER_FRAME || data == MIDI_STATUS_SYSCOM_SONG_SELECT) {
          stream->buffer[0] = MIDI_CIN_SYSCOM_2BYTE;
          stream->total     = 3;
        } else if (data == MIDI_STATUS_SYSCOM_SONG_POSITION_POINTER) {
          stream->buffer[0] = MIDI_CIN_SYSCOM_3BYTE;
          stream->total     = 4;
        } else {
          stream->buffer[0] = MIDI_CIN_SYSEX_END_1BYTE;
          stream->total     = 2;
        }
        stream->buffer[0] |= (uint8_t)(cable_num << 4);
      } else {
        // Pack individual bytes if we don't support packing them into words.
        stream->buffer[0] = (uint8_t)(cable_num << 4 | 0xf);
        stream->buffer[2] = 0;
        stream->buffer[3] = 0;
        stream->total     = 2; // index already set to 2
      }
    } else {
      //------------- On-going (buffering) packet -------------//
      TU_ASSERT(stream->index < 4, i);
      stream->buffer[stream->index] = data;
      stream->index++;

      // See if this byte ends a SysEx.
      if ((stream->buffer[0] & 0xF) == MIDI_CIN_SYSEX_START && data == MIDI_STATUS_SYSEX_END) {
        stream->buffer[0] = (uint8_t)((cable_num << 4) | (MIDI_CIN_SYSEX_START + (stream->index - 1)));
        stream->total     = stream->index;
      }
    }

    // Send out packet
    if (stream->index == stream->total) {
      // zeroes unused bytes
      for (uint8_t idx = stream->total; idx < 4; idx++) {
        stream->buffer[idx] = 0;
      }

      const uint32_t count = tu_edpt_stream_write(p_midi->rhport, ep_str, stream->buffer, 4);

      // complete current event packet, reset stream
      stream->index = stream->total = 0;

      // FIFO overflown, since we already check fifo remaining. It is probably race condition
      TU_ASSERT(count == 4, i);
    }
  }

  (void)tu_edpt_stream_write_xfer(p_midi->rhport, ep_str);

  return i;
}

bool tud_midi_n_packet_write (uint8_t itf, const uint8_t packet[4]) {
  midid_interface_t *p_midi = &_midid_itf[itf];
  tu_edpt_stream_t  *ep_str = &p_midi->ep_stream.tx;
  TU_VERIFY(tu_edpt_stream_is_opened(ep_str));

  TU_VERIFY(tu_edpt_stream_write_available(p_midi->rhport, ep_str) >= 4);
  TU_VERIFY(tu_edpt_stream_write(p_midi->rhport, ep_str, packet, 4) > 0);
  (void)tu_edpt_stream_write_xfer(p_midi->rhport, ep_str);

  return true;
}

uint32_t tud_midi_n_packet_write_n(uint8_t itf, const uint8_t packets[], uint32_t n_packets) {
  midid_interface_t *p_midi = &_midid_itf[itf];
  tu_edpt_stream_t  *ep_str = &p_midi->ep_stream.tx;
  TU_VERIFY(tu_edpt_stream_is_opened(ep_str), 0);

  uint32_t n_bytes = tu_edpt_stream_write_available(p_midi->rhport, ep_str);
  n_bytes          = tu_min32(tu_align4(n_bytes), n_packets << 2u);

  const uint32_t n_write = tu_edpt_stream_write(p_midi->rhport, ep_str, packets, n_bytes);
  (void)tu_edpt_stream_write_xfer(p_midi->rhport, ep_str);

  return n_write >> 2u;
}

//--------------------------------------------------------------------+
// USBD Driver API
//--------------------------------------------------------------------+
void midid_init(void) {
  tu_memclr(_midid_itf, sizeof(_midid_itf));

  for (uint8_t i = 0; i < CFG_TUD_MIDI; i++) {
    midid_interface_t *p_midi  = &_midid_itf[i];
    midid_epbuf_t     *p_epbuf = &_midid_epbuf[i];

    tu_edpt_stream_init(
        &p_midi->ep_stream.rx, false, false, false, p_midi->ep_stream.rx_ff_buf, CFG_TUD_MIDI_RX_BUFSIZE,
        p_epbuf->epout, CFG_TUD_MIDI_EP_BUFSIZE);

    tu_edpt_stream_init(
        &p_midi->ep_stream.tx, false, true, false, p_midi->ep_stream.tx_ff_buf, CFG_TUD_MIDI_TX_BUFSIZE, p_epbuf->epin,
        CFG_TUD_MIDI_EP_BUFSIZE);
  }
}

bool midid_deinit(void) {
  for (uint8_t i = 0; i < CFG_TUD_MIDI; i++) {
    midid_interface_t *p_midi = &_midid_itf[i];
    tu_edpt_stream_deinit(&p_midi->ep_stream.rx);
    tu_edpt_stream_deinit(&p_midi->ep_stream.tx);
  }
  return true;
}

void midid_reset(uint8_t rhport) {
  (void)rhport;
  for (uint8_t i = 0; i < CFG_TUD_MIDI; i++) {
    midid_interface_t *p_midi = &_midid_itf[i];
    tu_memclr(p_midi, ITF_MEM_RESET_SIZE);

    tu_edpt_stream_clear(&p_midi->ep_stream.rx);
    tu_edpt_stream_close(&p_midi->ep_stream.rx);

    tu_edpt_stream_clear(&p_midi->ep_stream.tx);
    tu_edpt_stream_close(&p_midi->ep_stream.tx);
  }
}

TU_ATTR_ALWAYS_INLINE static inline uint8_t find_midi_itf(uint8_t ep_addr) {
  for (uint8_t idx = 0; idx < CFG_TUD_MIDI; idx++) {
    const midid_interface_t *p_midi = &_midid_itf[idx];
    if (ep_addr == p_midi->ep_stream.rx.ep_addr || ep_addr == p_midi->ep_stream.tx.ep_addr) {
      return idx;
    }
  }
  return TUSB_INDEX_INVALID_8;
}

uint16_t midid_open(uint8_t rhport, const tusb_desc_interface_t *desc_itf, uint16_t max_len) {
  const uint8_t *p_desc   = (const uint8_t *)desc_itf;
  const uint8_t *desc_end = p_desc + max_len;

  // 1st Interface is Audio Control v1 (optional)
  if (TUSB_CLASS_AUDIO               == desc_itf->bInterfaceClass    &&
      AUDIO_SUBCLASS_CONTROL         == desc_itf->bInterfaceSubClass &&
      AUDIO_FUNC_PROTOCOL_CODE_UNDEF == desc_itf->bInterfaceProtocol) {
    p_desc = tu_desc_next(desc_itf);
    // Skip Class Specific descriptors
    while (tu_desc_in_bounds(p_desc, desc_end) && TUSB_DESC_CS_INTERFACE == tu_desc_type(p_desc)) {
      p_desc = tu_desc_next(p_desc);
    }
  }

  // 2nd Interface is MIDI Streaming
  TU_VERIFY(TUSB_DESC_INTERFACE == tu_desc_type(p_desc), 0);
  const tusb_desc_interface_t* desc_midi = (const tusb_desc_interface_t*) p_desc;

  TU_VERIFY(TUSB_CLASS_AUDIO == desc_midi->bInterfaceClass &&
              AUDIO_SUBCLASS_MIDI_STREAMING == desc_midi->bInterfaceSubClass &&
              AUDIO_FUNC_PROTOCOL_CODE_UNDEF == desc_midi->bInterfaceProtocol,
            0);

  uint8_t idx = find_midi_itf(0); // find unused interface
  TU_ASSERT(idx < CFG_TUD_MIDI, 0);
  midid_interface_t *p_midi = &_midid_itf[idx];

  p_midi->rhport  = rhport;
  p_midi->itf_num = desc_midi->bInterfaceNumber;
  (void) p_midi->itf_num;

  p_desc = tu_desc_next(p_desc);

  // Find and open endpoint descriptors
  uint8_t found_ep = 0;
  while ((found_ep < desc_midi->bNumEndpoints) && tu_desc_in_bounds(p_desc, desc_end)) {
    if (TUSB_DESC_ENDPOINT == tu_desc_type(p_desc)) {
      const tusb_desc_endpoint_t *desc_ep = (const tusb_desc_endpoint_t *)p_desc;
      TU_ASSERT(usbd_edpt_open(rhport, desc_ep), 0);
      const uint8_t ep_addr = ((const tusb_desc_endpoint_t *)p_desc)->bEndpointAddress;

      if (tu_edpt_dir(ep_addr) == TUSB_DIR_IN) {
        tu_edpt_stream_t *stream_tx = &p_midi->ep_stream.tx;
        tu_edpt_stream_open(stream_tx, desc_ep);
        tu_edpt_stream_clear(stream_tx);
      } else {
        tu_edpt_stream_t *stream_rx = &p_midi->ep_stream.rx;
        tu_edpt_stream_open(stream_rx, desc_ep);
        tu_edpt_stream_clear(stream_rx);
        TU_ASSERT(tu_edpt_stream_read_xfer(rhport, stream_rx) > 0, 0); // prepare to receive data
      }

      p_desc = tu_desc_next(p_desc);                                   // skip CS Endpoint descriptor
      found_ep++;
    }

    p_desc = tu_desc_next(p_desc);
  }

  return (uint16_t)(p_desc - (const uint8_t *)desc_itf);
}

// Invoked when a control transfer occurred on an interface of this class
// Driver response accordingly to the request and the transfer stage (setup/data/ack)
// return false to stall control endpoint (e.g unsupported request)
bool midid_control_xfer_cb(uint8_t rhport, uint8_t stage, const tusb_control_request_t* request) {
  (void) rhport; (void) stage; (void) request;
  return false; // driver doesn't support any request yet
}

bool midid_xfer_cb(uint8_t rhport, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes) {
  (void)result;

  uint8_t idx = find_midi_itf(ep_addr);
  TU_ASSERT(idx < CFG_TUD_MIDI);
  midid_interface_t *p_midi = &_midid_itf[idx];

  tu_edpt_stream_t *ep_st_rx = &p_midi->ep_stream.rx;
  tu_edpt_stream_t *ep_st_tx = &p_midi->ep_stream.tx;

  if (ep_addr == ep_st_rx->ep_addr) {
    // Received new data: put into stream's fifo
    if (result == XFER_RESULT_SUCCESS) {
      tu_edpt_stream_read_xfer_complete(ep_st_rx, xferred_bytes);
      tud_midi_rx_cb(idx);                      // invoke callback
    }
    tu_edpt_stream_read_xfer(rhport, ep_st_rx); // prepare for next data
  } else if (ep_addr == ep_st_tx->ep_addr && result == XFER_RESULT_SUCCESS) {
    // sent complete: try to send more if possible
    if (0 == tu_edpt_stream_write_xfer(rhport, ep_st_tx)) {
      // If there is no data left, a ZLP should be sent if needed
      (void)tu_edpt_stream_write_zlp_if_needed(rhport, ep_st_tx, xferred_bytes);
    }
  } else {
    return false;
  }

  return true;
}

#endif

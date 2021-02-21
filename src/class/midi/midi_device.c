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

#if (TUSB_OPT_DEVICE_ENABLED && CFG_TUD_MIDI)

//--------------------------------------------------------------------+
// INCLUDE
//--------------------------------------------------------------------+
#include "midi_device.h"
#include "class/audio/audio.h"
#include "device/usbd_pvt.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+
typedef struct
{
  uint8_t itf_num;
  uint8_t ep_in;
  uint8_t ep_out;

  /*------------- From this point, data is not cleared by bus reset -------------*/
  // FIFO
  tu_fifo_t rx_ff;
  tu_fifo_t tx_ff;
  uint8_t rx_ff_buf[CFG_TUD_MIDI_RX_BUFSIZE];
  uint8_t tx_ff_buf[CFG_TUD_MIDI_TX_BUFSIZE];

  #if CFG_FIFO_MUTEX
  osal_mutex_def_t rx_ff_mutex;
  osal_mutex_def_t tx_ff_mutex;
  #endif

  // Messages are always 4 bytes long, queue them for reading and writing so the
  // callers can use the Stream interface with single-byte read/write calls.
  uint8_t write_buffer[4];
  uint8_t write_buffer_length;
  uint8_t write_target_length;

  uint8_t read_buffer[4];
  uint8_t read_buffer_length;
  uint8_t read_target_length;

  // Endpoint Transfer buffer
  CFG_TUSB_MEM_ALIGN uint8_t epout_buf[CFG_TUD_MIDI_EP_BUFSIZE];
  CFG_TUSB_MEM_ALIGN uint8_t epin_buf[CFG_TUD_MIDI_EP_BUFSIZE];

} midid_interface_t;

#define ITF_MEM_RESET_SIZE   offsetof(midid_interface_t, rx_ff)

//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+
CFG_TUSB_MEM_SECTION midid_interface_t _midid_itf[CFG_TUD_MIDI];

bool tud_midi_n_mounted (uint8_t itf)
{
  midid_interface_t* midi = &_midid_itf[itf];
  return midi->ep_in && midi->ep_out;
}

static void _prep_out_transaction (midid_interface_t* p_midi)
{
  uint8_t const rhport = TUD_OPT_RHPORT;
  uint16_t available = tu_fifo_remaining(&p_midi->rx_ff);

  // Prepare for incoming data but only allow what we can store in the ring buffer.
  // TODO Actually we can still carry out the transfer, keeping count of received bytes
  // and slowly move it to the FIFO when read().
  // This pre-check reduces endpoint claiming
  TU_VERIFY(available >= sizeof(p_midi->epout_buf), );

  // claim endpoint
  TU_VERIFY(usbd_edpt_claim(rhport, p_midi->ep_out), );

  // fifo can be changed before endpoint is claimed
  available = tu_fifo_remaining(&p_midi->rx_ff);

  if ( available >= sizeof(p_midi->epout_buf) )  {
    usbd_edpt_xfer(rhport, p_midi->ep_out, p_midi->epout_buf, sizeof(p_midi->epout_buf));
  }else
  {
    // Release endpoint since we don't make any transfer
    usbd_edpt_release(rhport, p_midi->ep_out);
  }
}

//--------------------------------------------------------------------+
// READ API
//--------------------------------------------------------------------+
uint32_t tud_midi_n_available(uint8_t itf, uint8_t cable_num)
{
  (void) cable_num;
  return tu_fifo_count(&_midid_itf[itf].rx_ff);
}

uint32_t tud_midi_n_read(uint8_t itf, uint8_t cable_num, void* buffer, uint32_t bufsize)
{
  (void) cable_num;
  midid_interface_t* midi = &_midid_itf[itf];

  // Fill empty buffer
  if (midi->read_buffer_length == 0) {
    if (!tud_midi_n_receive(itf, midi->read_buffer)) return 0;

    uint8_t code_index = midi->read_buffer[0] & 0x0f;
    // We always copy over the first byte.
    uint8_t count = 1;
    // Ignore subsequent bytes based on the code.
    if (code_index != 0x5 && code_index != 0xf) {
      count = 2;
      if (code_index != 0x2 && code_index != 0x6 && code_index != 0xc && code_index != 0xd) {
        count = 3;
      }
    }

    midi->read_buffer_length = count;
  }

  uint32_t n = midi->read_buffer_length - midi->read_target_length;
  if (bufsize < n) n = bufsize;

  // Skip the header in the buffer
  memcpy(buffer, midi->read_buffer + 1 + midi->read_target_length, n);
  midi->read_target_length += n;

  if (midi->read_target_length == midi->read_buffer_length) {
    midi->read_buffer_length = 0;
    midi->read_target_length = 0;
  }

  return n;
}

void tud_midi_n_read_flush (uint8_t itf, uint8_t cable_num)
{
  (void) cable_num;
  midid_interface_t* p_midi = &_midid_itf[itf];
  tu_fifo_clear(&p_midi->rx_ff);
  _prep_out_transaction(p_midi);
}

bool tud_midi_n_receive (uint8_t itf, uint8_t packet[4])
{
  midid_interface_t* p_midi = &_midid_itf[itf];
  uint32_t num_read = tu_fifo_read_n(&p_midi->rx_ff, packet, 4);
  _prep_out_transaction(p_midi);
  return (num_read == 4);
}

void midi_rx_done_cb(midid_interface_t* midi, uint8_t const* buffer, uint32_t bufsize) {
  tu_fifo_write_n(&midi->rx_ff, buffer, bufsize);
}

//--------------------------------------------------------------------+
// WRITE API
//--------------------------------------------------------------------+

static uint32_t write_flush(midid_interface_t* midi)
{
  // No data to send
  if ( !tu_fifo_count(&midi->tx_ff) ) return 0;

  uint8_t const rhport = TUD_OPT_RHPORT;

  // skip if previous transfer not complete
  TU_VERIFY( usbd_edpt_claim(rhport, midi->ep_in), 0 );

  uint16_t count = tu_fifo_read_n(&midi->tx_ff, midi->epin_buf, CFG_TUD_MIDI_EP_BUFSIZE);
  if (count > 0)
  {
    TU_ASSERT( usbd_edpt_xfer(rhport, midi->ep_in, midi->epin_buf, count), 0 );
    return count;
  }else
  {
    // Release endpoint since we don't make any transfer
    usbd_edpt_release(rhport, midi->ep_in);
    return 0;
  }
}

uint32_t tud_midi_n_write(uint8_t itf, uint8_t cable_num, uint8_t const* buffer, uint32_t bufsize)
{
  midid_interface_t* midi = &_midid_itf[itf];
  if (midi->itf_num == 0) {
    return 0;
  }

  uint32_t i = 0;
  while (i < bufsize) {
    uint8_t data = buffer[i];
    if (midi->write_buffer_length == 0) {
        uint8_t msg = data >> 4;
        midi->write_buffer[1] = data;
        midi->write_buffer_length = 2;
        // Check to see if we're still in a SysEx transmit.
        if (midi->write_buffer[0] == 0x4) {
            if (data == 0xf7) {
                midi->write_buffer[0] = 0x5;
                midi->write_target_length = 2;
            } else {
                midi->write_target_length = 4;
            }
        } else if ((msg >= 0x8 && msg <= 0xB) || msg == 0xE) {
            midi->write_buffer[0] = cable_num << 4 | msg;
            midi->write_target_length = 4;
        } else if (msg == 0xf) {
            if (data == 0xf0) {
                midi->write_buffer[0] = 0x4;
                midi->write_target_length = 4;
            } else if (data == 0xf1 || data == 0xf3) {
                midi->write_buffer[0] = 0x2;
                midi->write_target_length = 3;
            } else if (data == 0xf2) {
                midi->write_buffer[0] = 0x3;
                midi->write_target_length = 4;
            } else {
                midi->write_buffer[0] = 0x5;
                midi->write_target_length = 2;
            }
        } else {
            // Pack individual bytes if we don't support packing them into words.
            midi->write_buffer[0] = cable_num << 4 | 0xf;
            midi->write_buffer[2] = 0;
            midi->write_buffer[3] = 0;
            midi->write_buffer_length = 2;
            midi->write_target_length = 2;
        }
    } else {
        midi->write_buffer[midi->write_buffer_length] = data;
        midi->write_buffer_length += 1;
        // See if this byte ends a SysEx.
        if (midi->write_buffer[0] == 0x4 && data == 0xf7) {
            midi->write_buffer[0] = 0x4 + (midi->write_buffer_length - 1);
            midi->write_target_length = midi->write_buffer_length;
        }
    }

    if (midi->write_buffer_length == midi->write_target_length) {
        uint16_t written = tu_fifo_write_n(&midi->tx_ff, midi->write_buffer, 4);
        if (written < 4) {
            TU_ASSERT( written == 0 );
            break;
        }
        midi->write_buffer_length = 0;
    }
    i++;
  }

  write_flush(midi);

  return i;
}

bool tud_midi_n_send (uint8_t itf, uint8_t const packet[4])
{
  midid_interface_t* midi = &_midid_itf[itf];
  if (midi->itf_num == 0) {
    return 0;
  }

  if (tu_fifo_remaining(&midi->tx_ff) < 4)
    return false;

  tu_fifo_write_n(&midi->tx_ff, packet, 4);
  write_flush(midi);

  return true;
}

//--------------------------------------------------------------------+
// USBD Driver API
//--------------------------------------------------------------------+
void midid_init(void)
{
  tu_memclr(_midid_itf, sizeof(_midid_itf));

  for(uint8_t i=0; i<CFG_TUD_MIDI; i++)
  {
    midid_interface_t* midi = &_midid_itf[i];

    // config fifo
    tu_fifo_config(&midi->rx_ff, midi->rx_ff_buf, CFG_TUD_MIDI_RX_BUFSIZE, 1, false); // true, true
    tu_fifo_config(&midi->tx_ff, midi->tx_ff_buf, CFG_TUD_MIDI_TX_BUFSIZE, 1, false); // OBVS.

    #if CFG_FIFO_MUTEX
    tu_fifo_config_mutex(&midi->rx_ff, osal_mutex_create(&midi->rx_ff_mutex));
    tu_fifo_config_mutex(&midi->tx_ff, osal_mutex_create(&midi->tx_ff_mutex));
    #endif
  }
}

void midid_reset(uint8_t rhport)
{
  (void) rhport;

  for(uint8_t i=0; i<CFG_TUD_MIDI; i++)
  {
    midid_interface_t* midi = &_midid_itf[i];
    tu_memclr(midi, ITF_MEM_RESET_SIZE);
    tu_fifo_clear(&midi->rx_ff);
    tu_fifo_clear(&midi->tx_ff);
  }
}

uint16_t midid_open(uint8_t rhport, tusb_desc_interface_t const * desc_itf, uint16_t max_len)
{
  // 1st Interface is Audio Control v1
  TU_VERIFY(TUSB_CLASS_AUDIO                      == desc_itf->bInterfaceClass    &&
            AUDIO_SUBCLASS_CONTROL                == desc_itf->bInterfaceSubClass &&
            AUDIO_FUNC_PROTOCOL_CODE_UNDEF        == desc_itf->bInterfaceProtocol, 0);

  uint16_t drv_len = tu_desc_len(desc_itf);
  uint8_t const * p_desc = tu_desc_next(desc_itf);

  // Skip Class Specific descriptors
  while ( TUSB_DESC_CS_INTERFACE == tu_desc_type(p_desc) && drv_len <= max_len )
  {
    drv_len += tu_desc_len(p_desc);
    p_desc   = tu_desc_next(p_desc);
  }

  // 2nd Interface is MIDI Streaming
  TU_VERIFY(TUSB_DESC_INTERFACE == tu_desc_type(p_desc), 0);
  tusb_desc_interface_t const * desc_midi = (tusb_desc_interface_t const *) p_desc;

  TU_VERIFY(TUSB_CLASS_AUDIO                      == desc_midi->bInterfaceClass    &&
            AUDIO_SUBCLASS_MIDI_STREAMING         == desc_midi->bInterfaceSubClass &&
            AUDIO_FUNC_PROTOCOL_CODE_UNDEF        == desc_midi->bInterfaceProtocol, 0);

  // Find available interface
  midid_interface_t * p_midi = NULL;
  for(uint8_t i=0; i<CFG_TUD_MIDI; i++)
  {
    if ( _midid_itf[i].ep_in == 0 && _midid_itf[i].ep_out == 0 )
    {
      p_midi = &_midid_itf[i];
      break;
    }
  }

  p_midi->itf_num = desc_midi->bInterfaceNumber;

  // next descriptor
  drv_len += tu_desc_len(p_desc);
  p_desc   = tu_desc_next(p_desc);

  // Find and open endpoint descriptors
  uint8_t found_endpoints = 0;
  while ( (found_endpoints < desc_midi->bNumEndpoints) && (drv_len <= max_len)  )
  {
    if ( TUSB_DESC_ENDPOINT == tu_desc_type(p_desc) )
    {
      TU_ASSERT(usbd_edpt_open(rhport, (tusb_desc_endpoint_t const *) p_desc), 0);
      uint8_t ep_addr = ((tusb_desc_endpoint_t const *) p_desc)->bEndpointAddress;

      if (tu_edpt_dir(ep_addr) == TUSB_DIR_IN)
      {
        p_midi->ep_in = ep_addr;
      } else {
        p_midi->ep_out = ep_addr;
      }

      drv_len += tu_desc_len(p_desc);
      p_desc   = tu_desc_next(p_desc);

      found_endpoints += 1;
    }

    drv_len += tu_desc_len(p_desc);
    p_desc   = tu_desc_next(p_desc);
  }

  // Prepare for incoming data
  _prep_out_transaction(p_midi);

  return drv_len;
}

// Invoked when a control transfer occurred on an interface of this class
// Driver response accordingly to the request and the transfer stage (setup/data/ack)
// return false to stall control endpoint (e.g unsupported request)
bool midid_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const * request)
{
  (void) rhport;
  (void) stage;
  (void) request;

  // driver doesn't support any request yet
  return false;
}

bool midid_xfer_cb(uint8_t rhport, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes)
{
  (void) result;
  (void) rhport;

  uint8_t itf;
  midid_interface_t* p_midi;

  // Identify which interface to use
  for (itf = 0; itf < CFG_TUD_MIDI; itf++)
  {
    p_midi = &_midid_itf[itf];
    if ( ( ep_addr == p_midi->ep_out ) || ( ep_addr == p_midi->ep_in ) ) break;
  }
  TU_ASSERT(itf < CFG_TUD_MIDI);

  // receive new data
  if ( ep_addr == p_midi->ep_out )
  {
    tu_fifo_write_n(&p_midi->rx_ff, p_midi->epout_buf, xferred_bytes);

    // invoke receive callback if available
    if (tud_midi_rx_cb) tud_midi_rx_cb(itf);

    // prepare for next
    // TODO for now ep_out is not used by public API therefore there is no race condition,
    // and does not need to claim like ep_in
    _prep_out_transaction(p_midi);
  }
  else if ( ep_addr == p_midi->ep_in )
  {
    if (0 == write_flush(p_midi))
    {
      // If there is no data left, a ZLP should be sent if
      // xferred_bytes is multiple of EP size and not zero
      if ( !tu_fifo_count(&p_midi->tx_ff) && xferred_bytes && (0 == (xferred_bytes % CFG_TUD_MIDI_EP_BUFSIZE)) )
      {
        if ( usbd_edpt_claim(rhport, p_midi->ep_in) )
        {
          usbd_edpt_xfer(rhport, p_midi->ep_in, NULL, 0);
        }
      }
    }
  }

  return true;
}

#endif

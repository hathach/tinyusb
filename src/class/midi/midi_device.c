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

  // We need to pack messages into words before queueing their transmission so buffer across write
  // calls.
  uint8_t message_buffer[4];
  uint8_t message_buffer_length;
  uint8_t message_target_length;

  // Endpoint Transfer buffer
  CFG_TUSB_MEM_ALIGN uint8_t epout_buf[CFG_TUD_MIDI_EPSIZE];
  CFG_TUSB_MEM_ALIGN uint8_t epin_buf[CFG_TUD_MIDI_EPSIZE];

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

//--------------------------------------------------------------------+
// READ API
//--------------------------------------------------------------------+
uint32_t tud_midi_n_available(uint8_t itf, uint8_t jack_id)
{
  (void) jack_id;
  return tu_fifo_count(&_midid_itf[itf].rx_ff);
}

uint32_t tud_midi_n_read(uint8_t itf, uint8_t jack_id, void* buffer, uint32_t bufsize)
{
  (void) jack_id;
  return tu_fifo_read_n(&_midid_itf[itf].rx_ff, buffer, bufsize);
}

void tud_midi_n_read_flush (uint8_t itf, uint8_t jack_id)
{
  (void) jack_id;
  tu_fifo_clear(&_midid_itf[itf].rx_ff);
}

void midi_rx_done_cb(midid_interface_t* midi, uint8_t const* buffer, uint32_t bufsize) {
  if (bufsize % 4 != 0) {
    return;
  }

  for(uint32_t i=0; i<bufsize; i += 4) {
    uint8_t header = buffer[i];
    // uint8_t cable_number = (header & 0xf0) >> 4;
    uint8_t code_index = header & 0x0f;
    // We always copy over the first byte.
    uint8_t count = 1;
    // Ignore subsequent bytes based on the code.
    if (code_index != 0x5 && code_index != 0xf) {
      count = 2;
      if (code_index != 0x2 && code_index != 0x6 && code_index != 0xc && code_index != 0xd) {
        count = 3;
      }
    }
    tu_fifo_write_n(&midi->rx_ff, &buffer[i + 1], count);
  }
}


//--------------------------------------------------------------------+
// WRITE API
//--------------------------------------------------------------------+

static bool maybe_transmit(midid_interface_t* midi, uint8_t itf_index)
{
  (void) itf_index;

  // skip if previous transfer not complete
  TU_VERIFY( !usbd_edpt_busy(TUD_OPT_RHPORT, midi->ep_in) );

  uint16_t count = tu_fifo_read_n(&midi->tx_ff, midi->epin_buf, CFG_TUD_MIDI_EPSIZE);
  if (count > 0)
  {
    TU_ASSERT( usbd_edpt_xfer(TUD_OPT_RHPORT, midi->ep_in, midi->epin_buf, count) );
  }
  return true;
}

uint32_t tud_midi_n_write(uint8_t itf, uint8_t jack_id, uint8_t const* buffer, uint32_t bufsize)
{
  midid_interface_t* midi = &_midid_itf[itf];
  if (midi->itf_num == 0) {
    return 0;
  }

  uint32_t i = 0;
  while (i < bufsize) {
    uint8_t data = buffer[i];
    if (midi->message_buffer_length == 0) {
        uint8_t msg = data >> 4;
        midi->message_buffer[1] = data;
        midi->message_buffer_length = 2;
        // Check to see if we're still in a SysEx transmit.
        if (midi->message_buffer[0] == 0x4) {
            if (data == 0xf7) {
                midi->message_buffer[0] = 0x5;
            } else {
                midi->message_buffer_length = 4;
            }
        } else if ((msg >= 0x8 && msg <= 0xB) || msg == 0xE) {
            midi->message_buffer[0] = jack_id << 4 | msg;
            midi->message_target_length = 4;
        } else if (msg == 0xC || msg == 0xD) {
            midi->message_buffer[0] = jack_id << 4 | msg;
            midi->message_target_length = 3;
        } else if (msg == 0xf) {
            if (data == 0xf0) {
                midi->message_buffer[0] = 0x4;
                midi->message_target_length = 4;
            } else if (data == 0xf1 || data == 0xf3) {
                midi->message_buffer[0] = 0x2;
                midi->message_target_length = 3;
            } else if (data == 0xf2) {
                midi->message_buffer[0] = 0x3;
                midi->message_target_length = 4;
            } else {
                midi->message_buffer[0] = 0x5;
                midi->message_target_length = 2;
            }
        } else {
            // Pack individual bytes if we don't support packing them into words.
            midi->message_buffer[0] = jack_id << 4 | 0xf;
            midi->message_buffer[2] = 0;
            midi->message_buffer[3] = 0;
            midi->message_buffer_length = 2;
            midi->message_target_length = 2;
        }
    } else {
        midi->message_buffer[midi->message_buffer_length] = data;
        midi->message_buffer_length += 1;
        // See if this byte ends a SysEx.
        if (midi->message_buffer[0] == 0x4 && data == 0xf7) {
            midi->message_buffer[0] = 0x4 + (midi->message_buffer_length - 1);
            midi->message_target_length = midi->message_buffer_length;
        }
    }

    if (midi->message_buffer_length == midi->message_target_length) {
        uint16_t written = tu_fifo_write_n(&midi->tx_ff, midi->message_buffer, 4);
        if (written < 4) {
            TU_ASSERT( written == 0 );
            break;
        }
        midi->message_buffer_length = 0;
    }
    i++;
  }
  maybe_transmit(midi, itf);

  return i;
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
    tu_fifo_config(&midi->rx_ff, midi->rx_ff_buf, CFG_TUD_MIDI_RX_BUFSIZE, 1, true);
    tu_fifo_config(&midi->tx_ff, midi->tx_ff_buf, CFG_TUD_MIDI_TX_BUFSIZE, 1, true);

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

bool midid_open(uint8_t rhport, tusb_desc_interface_t const * p_interface_desc, uint16_t *p_length)
{
  // For now handle the audio control interface as well.
  if ( AUDIO_SUBCLASS_CONTROL == p_interface_desc->bInterfaceSubClass) {
    uint8_t const * p_desc = tu_desc_next ( (uint8_t const *) p_interface_desc );
    (*p_length) = sizeof(tusb_desc_interface_t);

    // Skip over the class specific descriptor.
    (*p_length) += tu_desc_len(p_desc);
    p_desc = tu_desc_next(p_desc);
    return true;
  }

  TU_VERIFY(AUDIO_SUBCLASS_MIDI_STREAMING == p_interface_desc->bInterfaceSubClass &&
            AUDIO_PROTOCOL_V1 == p_interface_desc->bInterfaceProtocol );

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

  p_midi->itf_num  = p_interface_desc->bInterfaceNumber;

  uint8_t const * p_desc = tu_desc_next( (uint8_t const *) p_interface_desc );
  (*p_length) = sizeof(tusb_desc_interface_t);

  uint8_t found_endpoints = 0;
  while (found_endpoints < p_interface_desc->bNumEndpoints)
  {
    if ( TUSB_DESC_ENDPOINT == p_desc[DESC_OFFSET_TYPE])
    {
        TU_ASSERT( dcd_edpt_open(rhport, (tusb_desc_endpoint_t const *) p_desc), false);
        uint8_t ep_addr = ((tusb_desc_endpoint_t const *) p_desc)->bEndpointAddress;
        if (tu_edpt_dir(ep_addr) == TUSB_DIR_IN) {
            p_midi->ep_in = ep_addr;
        } else {
            p_midi->ep_out = ep_addr;
        }

        (*p_length) += p_desc[DESC_OFFSET_LEN];
        p_desc = tu_desc_next(p_desc);
        found_endpoints += 1;
    }
    (*p_length) += p_desc[DESC_OFFSET_LEN];
    p_desc = tu_desc_next(p_desc);
  }

  // Prepare for incoming data
  TU_ASSERT( usbd_edpt_xfer(rhport, p_midi->ep_out, p_midi->epout_buf, CFG_TUD_MIDI_EPSIZE), false);

  return true;
}

bool midid_control_complete(uint8_t rhport, tusb_control_request_t const * p_request)
{
  (void) rhport;
  (void) p_request;
  return true;
}

bool midid_control_request(uint8_t rhport, tusb_control_request_t const * p_request)
{
  (void) rhport;
  (void) p_request;

  // driver doesn't support any request yet
  return false;
}

bool midid_xfer_cb(uint8_t rhport, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes)
{
  (void) result;

  uint8_t itf = 0;
  midid_interface_t* p_midi = _midid_itf;

  for ( ; ; itf++, p_midi++)
  {
    if (itf >= TU_ARRAY_SIZE(_midid_itf)) return false;

    if ( ep_addr == p_midi->ep_out ) break;
  }

  // receive new data
  if ( ep_addr == p_midi->ep_out )
  {
    midi_rx_done_cb(p_midi, p_midi->epout_buf, xferred_bytes);

    // prepare for next
    TU_ASSERT( usbd_edpt_xfer(rhport, p_midi->ep_out, p_midi->epout_buf, CFG_TUD_MIDI_EPSIZE), false );
  } else if ( ep_addr == p_midi->ep_in ) {
    maybe_transmit(p_midi, itf);
  }

  // nothing to do with in and notif endpoint

  return true;
}

#endif

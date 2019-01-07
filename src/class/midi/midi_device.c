/**************************************************************************/
/*!
    @file     cdc_device.c
    @author   hathach (tinyusb.org)

    @section LICENSE

    Software License Agreement (BSD License)

    Copyright (c) 2013, hathach (tinyusb.org)
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:
    1. Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
    3. Neither the name of the copyright holders nor the
    names of its contributors may be used to endorse or promote products
    derived from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ''AS IS'' AND ANY
    EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

    This file is part of the tinyusb stack.
*/
/**************************************************************************/

#include "tusb_option.h"

#if (TUSB_OPT_DEVICE_ENABLED && CFG_TUD_MIDI)

#define _TINY_USB_SOURCE_FILE_
//--------------------------------------------------------------------+
// INCLUDE
//--------------------------------------------------------------------+
#include "midi_device.h"
#include "class/audio/audio.h"
#include "common/tusb_txbuf.h"
#include "device/usbd_pvt.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+
typedef struct
{
  uint8_t itf_num;
  uint8_t ep_in;
  uint8_t ep_out;

  // FIFO
  tu_fifo_t rx_ff;
  uint8_t rx_ff_buf[CFG_TUD_MIDI_RX_BUFSIZE];
  osal_mutex_def_t rx_ff_mutex;

  #if CFG_TUD_MIDI_TX_BUFSIZE % CFG_TUD_MIDI_EPSIZE != 0
  #error "TX buffer size must be multiple of endpoint size."
  #endif

  // This is a ring buffer that aligns to word boundaries so that it can be transferred directly to
  // the USB peripheral. There are three states to the data: free, transmitting and pending.
  CFG_TUSB_MEM_ALIGN uint8_t raw_tx_buffer[CFG_TUD_MIDI_TX_BUFSIZE];
  tu_txbuf_t txbuf;

  // We need to pack messages into words before queueing their transmission so buffer across write
  // calls.
  uint8_t message_buffer[4];
  uint8_t message_buffer_length;
  uint8_t message_target_length;

  // Endpoint Transfer buffer
  CFG_TUSB_MEM_ALIGN uint8_t epout_buf[CFG_TUD_MIDI_EPSIZE];

} midid_interface_t;

#define ITF_MEM_RESET_SIZE   offsetof(midid_interface_t, rx_ff)

//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+
CFG_TUSB_ATTR_USBRAM midid_interface_t _midid_itf[CFG_TUD_MIDI];

bool tud_midi_n_connected(uint8_t itf) {
  midid_interface_t* midi = &_midid_itf[itf];
  return midi->itf_num != 0;
}

//--------------------------------------------------------------------+
// READ API
//--------------------------------------------------------------------+
uint32_t tud_midi_n_available(uint8_t itf, uint8_t jack_id)
{
  return tu_fifo_count(&_midid_itf[itf].rx_ff);
}

char tud_midi_n_read_char(uint8_t itf, uint8_t jack_id)
{
  char ch;
  return tu_fifo_read(&_midid_itf[itf].rx_ff, &ch) ? ch : (-1);
}

uint32_t tud_midi_n_read(uint8_t itf, uint8_t jack_id, void* buffer, uint32_t bufsize)
{
  return tu_fifo_read_n(&_midid_itf[itf].rx_ff, buffer, bufsize);
}

void tud_midi_n_read_flush (uint8_t itf, uint8_t jack_id)
{
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
        tu_txbuf_write_n(&midi->txbuf, midi->message_buffer, 4);
        midi->message_buffer_length = 0;
    }
    i++;
  }

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
    #if CFG_FIFO_MUTEX
    tu_fifo_config_mutex(&midi->rx_ff, osal_mutex_create(&midi->rx_ff_mutex));
    #endif

    tu_txbuf_config(&midi->txbuf, midi->raw_tx_buffer, CFG_TUD_MIDI_TX_BUFSIZE, dcd_edpt_xfer);
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
    tu_txbuf_clear(&midi->txbuf);
  }
}

bool midid_open(uint8_t rhport, tusb_desc_interface_t const * p_interface_desc, uint16_t *p_length)
{
  // For now handle the audio control interface as well.
  if ( AUDIO_SUBCLASS_AUDIO_CONTROL == p_interface_desc->bInterfaceSubClass) {
    uint8_t const * p_desc = tu_desc_next ( (uint8_t const *) p_interface_desc );
    (*p_length) = sizeof(tusb_desc_interface_t);

    // Skip over the class specific descriptor.
    (*p_length) += p_desc[DESC_OFFSET_LEN];
    p_desc = tu_desc_next(p_desc);
    return true;
  }

  if ( AUDIO_SUBCLASS_MIDI_STREAMING != p_interface_desc->bInterfaceSubClass ||
       p_interface_desc->bInterfaceProtocol != AUDIO_PROTOCOL_V1 ) {
    return false;
  }

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
  while (found_endpoints < p_interface_desc->bNumEndpoints) {
    if ( TUSB_DESC_ENDPOINT == p_desc[DESC_OFFSET_TYPE])
    {
        TU_ASSERT( dcd_edpt_open(rhport, (tusb_desc_endpoint_t const *) p_desc), false);
        uint8_t ep_addr = ((tusb_desc_endpoint_t const *) p_desc)->bEndpointAddress;
        if (tu_edpt_dir(ep_addr) == TUSB_DIR_IN) {
            p_midi->ep_in = ep_addr;
            tu_txbuf_set_ep_addr(&p_midi->txbuf, ep_addr);
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
  TU_ASSERT( dcd_edpt_xfer(rhport, p_midi->ep_out, p_midi->epout_buf, CFG_TUD_MIDI_EPSIZE), false);

  return true;
}

bool midid_control_request_complete(uint8_t rhport, tusb_control_request_t const * p_request)
{
  return false;
}

bool midid_control_request(uint8_t rhport, tusb_control_request_t const * p_request)
{
  //------------- Class Specific Request -------------//
  if (p_request->bmRequestType_bit.type != TUSB_REQ_TYPE_CLASS) return false;

  return false;
}

bool midid_xfer_cb(uint8_t rhport, uint8_t edpt_addr, xfer_result_t result, uint32_t xferred_bytes)
{
  // TODO Support multiple interfaces
  uint8_t const itf = 0;
  midid_interface_t* p_midi = &_midid_itf[itf];

  // receive new data
  if ( edpt_addr == p_midi->ep_out )
  {
    midi_rx_done_cb(p_midi, p_midi->epout_buf, xferred_bytes);

    // prepare for next
    TU_ASSERT( dcd_edpt_xfer(rhport, p_midi->ep_out, p_midi->epout_buf, CFG_TUD_MIDI_EPSIZE), false );
  } else if ( edpt_addr == p_midi->ep_in ) {
    tu_txbuf_transmit_done_cb(&p_midi->txbuf, xferred_bytes);
  }

  // nothing to do with in and notif endpoint

  return TUSB_ERROR_NONE;
}

#endif

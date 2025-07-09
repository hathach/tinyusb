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

#ifndef TUSB_MIDI_HOST_H_
#define TUSB_MIDI_HOST_H_

#include "class/audio/audio.h"
#include "midi.h"

#ifdef __cplusplus
 extern "C" {
#endif

//--------------------------------------------------------------------+
// Class Driver Configuration
//--------------------------------------------------------------------+
#ifndef CFG_TUH_MIDI_RX_BUFSIZE
#define CFG_TUH_MIDI_RX_BUFSIZE TUH_EPSIZE_BULK_MPS
#endif

#ifndef CFG_TUH_MIDI_TX_BUFSIZE
#define CFG_TUH_MIDI_TX_BUFSIZE TUH_EPSIZE_BULK_MPS
#endif

#ifndef CFG_TUH_MIDI_EP_BUFSIZE
#define CFG_TUH_MIDI_EP_BUFSIZE TUH_EPSIZE_BULK_MPS
#endif

// Enable the MIDI stream read/write API. Some library can work with raw USB MIDI packet
// Disable this can save driver footprint.
#ifndef CFG_TUH_MIDI_STREAM_API
#define CFG_TUH_MIDI_STREAM_API 1
#endif

//--------------------------------------------------------------------+
// Application Types
//--------------------------------------------------------------------+
typedef struct {
  const tusb_desc_interface_t* desc_audio_control;
  const tusb_desc_interface_t* desc_midi; // start of whole midi interface descriptor
  uint16_t desc_midi_total_len;

  const uint8_t* desc_header;
  const uint8_t* desc_element;
  const tusb_desc_endpoint_t* desc_epin;  // endpoint IN descriptor, CS_ENDPOINT is right after
  const tusb_desc_endpoint_t* desc_epout; // endpoint OUT descriptor, CS_ENDPOINT is right after

  uint8_t jack_num;
  const uint8_t* desc_jack[32]; // list of jack descriptors (embedded + external)
} tuh_midi_descriptor_cb_t;

typedef struct {
  uint8_t daddr;
  uint8_t bInterfaceNumber; // interface number of MIDI streaming
  uint8_t rx_cable_count;
  uint8_t tx_cable_count;
} tuh_midi_mount_cb_t;

//--------------------------------------------------------------------+
// Application API
//--------------------------------------------------------------------+

// Check if MIDI interface is mounted
bool tuh_midi_mounted(uint8_t idx);

// Get Interface index from device address + interface number
// return TUSB_INDEX_INVALID_8 (0xFF) if not found
uint8_t tuh_midi_itf_get_index(uint8_t daddr, uint8_t itf_num);

// Get Interface information
// return true if index is correct and interface is currently mounted
bool tuh_midi_itf_get_info(uint8_t idx, tuh_itf_info_t* info);

// return the number of virtual midi cables on the device's IN endpoint
uint8_t tuh_midi_get_rx_cable_count(uint8_t idx);

// return the number of virtual midi cables on the device's OUT endpoint
uint8_t tuh_midi_get_tx_cable_count(uint8_t idx);

// return the raw number of bytes available.
// Note: this is related but not the same as number of stream bytes available.
uint32_t tuh_midi_read_available(uint8_t idx);

// Send any queued packets to the device if the host hardware is able to do it
// Returns the number of bytes flushed to the host hardware or 0 if
// the host hardware is busy or there is nothing in queue to send.
uint32_t tuh_midi_write_flush(uint8_t idx);

//--------------------------------------------------------------------+
// Packet API
//--------------------------------------------------------------------+

// Read all available MIDI packets from the connected device
// Return number of bytes read (always multiple of 4)
uint32_t tuh_midi_packet_read_n(uint8_t idx, uint8_t* buffer, uint32_t bufsize);

// Read a raw MIDI packet from the connected device
// Return true if a packet was returned
TU_ATTR_ALWAYS_INLINE static inline
bool tuh_midi_packet_read (uint8_t idx, uint8_t packet[4]) {
 return 4 == tuh_midi_packet_read_n(idx, packet, 4);
}

// Write all 4-byte packets, data is locally buffered and only transferred when buffered bytes
// reach the endpoint packet size or tuh_midi_write_flush() is called
uint32_t tuh_midi_packet_write_n(uint8_t idx, const uint8_t* buffer, uint32_t bufsize);

// Write a 4-bytes packet to the device.
// Returns true if the packet was successfully queued.
TU_ATTR_ALWAYS_INLINE static inline
bool tuh_midi_packet_write (uint8_t idx, uint8_t const packet[4]) {
 return 4 == tuh_midi_packet_write_n(idx, packet, 4);
}

//--------------------------------------------------------------------+
// Stream API
//--------------------------------------------------------------------+
#if CFG_TUH_MIDI_STREAM_API

// Queue a message to the device using stream API. data is locally buffered and only transferred when buffered bytes
// reach the endpoint packet size or tuh_midi_write_flush() is called
// Returns number of bytes was successfully queued.
uint32_t tuh_midi_stream_write(uint8_t idx, uint8_t cable_num, uint8_t const *p_buffer, uint32_t bufsize);

// Get the MIDI stream from the device. Set the value pointed
// to by p_cable_num to the MIDI cable number intended to receive it.
// The MIDI stream will be stored in the buffer pointed to by p_buffer.
// Return the number of bytes added to the buffer.
// Note that this function ignores the CIN field of the MIDI packet
// because a number of commercial devices out there do not encode
// it properly.
uint32_t tuh_midi_stream_read(uint8_t idx, uint8_t *p_cable_num, uint8_t *p_buffer, uint16_t bufsize);

#endif

//--------------------------------------------------------------------+
// Callbacks (Weak is optional)
//--------------------------------------------------------------------+

// Invoked when MIDI interface is detected in enumeration. Application can copy/parse descriptor if needed.
// Note: may be fired before tuh_midi_mount_cb(), therefore midi interface is not mounted/ready.
void tuh_midi_descriptor_cb(uint8_t idx, const tuh_midi_descriptor_cb_t * desc_cb_data);

// Invoked when device with MIDI interface is mounted.
void tuh_midi_mount_cb(uint8_t idx, const tuh_midi_mount_cb_t* mount_cb_data);

// Invoked when device with MIDI interface is un-mounted
void tuh_midi_umount_cb(uint8_t idx);

// Invoked when received new data
void tuh_midi_rx_cb(uint8_t idx, uint32_t xferred_bytes);

// Invoked when a TX is complete and therefore space becomes available in TX buffer
void tuh_midi_tx_cb(uint8_t idx, uint32_t xferred_bytes);

//--------------------------------------------------------------------+
// Internal Class Driver API
//--------------------------------------------------------------------+
bool midih_init       (void);
bool midih_deinit     (void);
bool midih_open       (uint8_t rhport, uint8_t dev_addr, tusb_desc_interface_t const *desc_itf, uint16_t max_len);
bool midih_set_config (uint8_t dev_addr, uint8_t itf_num);
bool midih_xfer_cb    (uint8_t dev_addr, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes);
void midih_close      (uint8_t daddr);

#ifdef __cplusplus
}
#endif

#endif

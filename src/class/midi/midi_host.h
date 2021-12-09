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

#ifndef _TUSB_MIDI_HOST_H_
#define _TUSB_MIDI_HOST_H_

#include "class/audio/audio.h"
#include "midi.h"

#ifdef __cplusplus
 extern "C" {
#endif

//--------------------------------------------------------------------+
// Class Driver Configuration
//--------------------------------------------------------------------+

// TODO Highspeed bulk transfer can be up to 512 bytes
#ifndef CFG_TUH_HID_EPIN_BUFSIZE
#define CFG_TUH_HID_EPIN_BUFSIZE 64
#endif

#ifndef CFG_TUH_HID_EPOUT_BUFSIZE
#define CFG_TUH_HID_EPOUT_BUFSIZE 64
#endif


//--------------------------------------------------------------------+
// Application API (Single Interface)
//--------------------------------------------------------------------+
bool     tuh_midi_configured      (void);
uint32_t tuh_midi_available    (void);

// return the number of virtual midi cables on the device's OUT endpoint
uint8_t tuh_midih_get_num_tx_cables (void);

// return the number of virtual midi cables on the device's IN endpoint
uint8_t tuh_midih_get_num_rx_cables (void);

// request available data from the device. tuh_midi_message_received_cb() will
// be called if the device has any data to send. Otherwise, the device will
// respond NAK. This function blocks until the transfer completes or the
// devices sends NAK.
// This function will return false if the hardware is busy.
bool tuh_midi_read_poll( void );

// Send a message to the device. This function blocks until the send completes.
// If the device sends NAK during the transmission, this function will block
// until the NAK clears and the whole message gets sent.
// NOTE: check the return value and make sure the whole buffer was sent.
// The transmit queue might be full or the transmit hardware may not be able
// to start a new transmission.
uint32_t tuh_midi_stream_write (uint8_t cable_num, uint8_t const* buffer, uint32_t bufsize);

//--------------------------------------------------------------------+
// Internal Class Driver API
//--------------------------------------------------------------------+
void midih_init       (void);
bool midih_open       (uint8_t rhport, uint8_t dev_addr, tusb_desc_interface_t const *desc_itf, uint16_t max_len);
bool midih_set_config (uint8_t dev_addr, uint8_t itf_num);
bool midih_xfer_cb    (uint8_t dev_addr, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes);
void midih_close      (uint8_t dev_addr);

//--------------------------------------------------------------------+
// Callbacks (Weak is optional)
//--------------------------------------------------------------------+

// Invoked when device with MIDI interface is mounted.
// If the MIDI host application requires MIDI IN, it should requst an
// IN transfer here. The device will likely NAK this transfer. How the driver
// handles the NAK is hardware dependent.
TU_ATTR_WEAK void tuh_midi_mount_cb(uint8_t dev_addr, uint8_t in_ep, uint8_t out_ep, uint8_t num_cables_rx, uint16_t num_cables_tx);

// Invoked when device with MIDI interface is un-mounted
// For now, the instance parameter is always 0 and can be ignored
TU_ATTR_WEAK void tuh_midi_umount_cb(uint8_t dev_addr, uint8_t instance);

TU_ATTR_WEAK void tuh_midi_rx_cb(uint8_t dev_addr, uint32_t num_packets);
#ifdef __cplusplus
}
#endif

#endif /* _TUSB_MIDI_HOST_H_ */

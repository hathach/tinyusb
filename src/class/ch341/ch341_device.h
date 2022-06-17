/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 * Portions Copyright (c) 2022 Travis Robinson (libusbdotnet@gmail.com)
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

#ifndef _TUSB_CH341_DEVICE_H_
#define _TUSB_CH341_DEVICE_H_

#include "common/tusb_common.h"
#include "ch341.h"

////////////////////////////////////////////////////////////////////////////////
// TR 06-09-22 NOTE: ///////////////////////////////////////////////////////////
// I've left the "_n_" functions in to maintain some API compatibility with the
// CDC implementation but there is no way to have more than one CH341 interface
// due to the nature of how host and device communicate.
////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------+
// Class Driver Configuration
//--------------------------------------------------------------------+
#ifndef CFG_TUD_CH341_EP_RX_MAX_PACKET
  #define CFG_TUD_CH341_EP_RX_MAX_PACKET    (TUD_OPT_HIGH_SPEED ? 512 : 64)
#endif

#ifndef CFG_TUD_CH341_EP_TX_MAX_PACKET
  #define CFG_TUD_CH341_EP_TX_MAX_PACKET    CFG_TUD_CH341_EP_RX_MAX_PACKET
#endif

#ifndef CFG_TUD_CH341_EP_TXNOTIFY_MAX_PACKET
  #define CFG_TUD_CH341_EP_TXNOTIFY_MAX_PACKET    (8)
#endif

#ifndef CFG_TUD_CH341_FIFO_SIZE
  #define CFG_TUD_CH341_FIFO_SIZE           CFG_TUD_CH341_EP_RX_MAX_PACKET
#endif

#ifdef __cplusplus
 extern "C" {
#endif

/** \addtogroup CH341_Serial Serial
 *  @{
 *  \defgroup   CH341_Serial_Device Device
 *  @{ */

//--------------------------------------------------------------------+
// Application API (Single Port)
// The CH341 is a vendor class device that sends it's requests directly
// to the device, hence there can be only 1 CH341 interface per device.
//--------------------------------------------------------------------+

// Check if terminal is connected to this port
bool     tud_ch341_connected       (void);

// Get current line state. Bit 0:  DTR (Data Terminal Ready), Bit 1: RTS (Request to Send)
ch341_line_state_t  tud_ch341_get_line_state  (void);

// Get current line encoding: bit rate, stop bits parity etc ..
void     tud_ch341_get_line_coding (ch341_line_coding_t* coding);

// Set special character that will trigger tud_ch341_rx_wanted_cb() callback on receiving
void     tud_ch341_set_wanted_char (char wanted);

// Get the number of bytes available for reading
uint32_t tud_ch341_available       (void);

// Read received bytes
uint32_t tud_ch341_read            (void* buffer, uint32_t bufsize);

// Read a byte, return -1 if there is none
int32_t  tud_ch341_read_char       (void);

// Clear the received FIFO
void     tud_ch341_read_flush      (void);

// Get a byte from FIFO at the specified position without removing it
bool     tud_ch341_peek            (uint8_t* ui8);

// Write bytes to TX FIFO, data may remain in the FIFO for a while
uint32_t tud_ch341_write           (void const* buffer, uint32_t bufsize);

// Write a byte
uint32_t tud_ch341_write_char      (char ch);

// Write a null-terminated string
uint32_t tud_ch341_write_str       (char const* str);

// Force sending data if possible, return number of forced bytes
uint32_t tud_ch341_write_flush     (void);

// Return the number of bytes (characters) available for writing to TX FIFO buffer in a single n_write operation.
uint32_t tud_ch341_write_available (void);

// Clear the transmit FIFO
bool tud_ch341_write_clear (void);

// Send line events to host. (IE: CTS, DSR, RI, DCD)
uint32_t tud_ch341_set_modem_state(ch341_modem_state_t modem_states);

// Returns the current state of the modem. (IE: CTS, DSR, RI, DCD)
ch341_modem_state_t tud_ch341_get_modem_state(void);

// Force sending notify data if possible, return number of forced bytes
uint32_t tud_ch341_notify_flush(void);

//--------------------------------------------------------------------+
// Application Callback API (weak is optional)
//--------------------------------------------------------------------+

// Invoked when received new data
TU_ATTR_WEAK void tud_ch341_rx_cb(void);

// Invoked when received `wanted_char`
TU_ATTR_WEAK void tud_ch341_rx_wanted_cb(char wanted_char);

// Invoked when space becomes available in TX buffer
TU_ATTR_WEAK void tud_ch341_tx_complete_cb(void);

// Invoked when line state DTR & RTS are changed via SET_CONTROL_LINE_STATE
TU_ATTR_WEAK void tud_ch341_line_state_cb(ch341_line_state_t line_state);

// Invoked when line coding is change via SET_LINE_CODING
TU_ATTR_WEAK void tud_ch341_line_coding_cb(ch341_line_coding_t const* p_line_coding);

// Invoked when received send break
TU_ATTR_WEAK void tud_ch341_send_break_cb(bool is_break_active);

/** @} */
/** @} */

//--------------------------------------------------------------------+
// INTERNAL USBD-CLASS DRIVER API
//--------------------------------------------------------------------+
void     ch341d_init            (void);
void     ch341d_reset           (uint8_t rhport);
uint16_t ch341d_open            (uint8_t rhport, tusb_desc_interface_t const * itf_desc, uint16_t max_len);
bool     ch341d_control_xfer_cb (uint8_t rhport, uint8_t stage, tusb_control_request_t const * request);
bool     ch341d_xfer_cb         (uint8_t rhport, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes);

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_CH341_DEVICE_H_ */

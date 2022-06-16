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
// Application API (Multiple Ports)
// CFG_TUD_CH341 > 1
//--------------------------------------------------------------------+

// Check if terminal is connected to this port
bool     tud_ch341_n_connected       (uint8_t itf);

// Get current line state. Bit 0:  DTR (Data Terminal Ready), Bit 1: RTS (Request to Send)
ch341_line_state_t  tud_ch341_n_get_line_state  (uint8_t itf);

// Get current line encoding: bit rate, stop bits parity etc ..
void     tud_ch341_n_get_line_coding (uint8_t itf, ch341_line_coding_t* coding);

// Set special character that will trigger tud_ch341_rx_wanted_cb() callback on receiving
void     tud_ch341_n_set_wanted_char (uint8_t itf, char wanted);

// Get the number of bytes available for reading
uint32_t tud_ch341_n_available       (uint8_t itf);

// Read received bytes
uint32_t tud_ch341_n_read            (uint8_t itf, void* buffer, uint32_t bufsize);

// Read a byte, return -1 if there is none
static inline
int32_t  tud_ch341_n_read_char       (uint8_t itf);

// Clear the received FIFO
void     tud_ch341_n_read_flush      (uint8_t itf);

// Get a byte from FIFO at the specified position without removing it
bool     tud_ch341_n_peek            (uint8_t itf, uint8_t* ui8);

// Write bytes to TX FIFO, data may remain in the FIFO for a while
uint32_t tud_ch341_n_write           (uint8_t itf, void const* buffer, uint32_t bufsize);

// Write a byte
static inline
uint32_t tud_ch341_n_write_char      (uint8_t itf, char ch);

// Write a null-terminated string
static inline
uint32_t tud_ch341_n_write_str       (uint8_t itf, char const* str);

// Force sending data if possible, return number of forced bytes
uint32_t tud_ch341_n_write_flush     (uint8_t itf);

// Return the number of bytes (characters) available for writing to TX FIFO buffer in a single n_write operation.
uint32_t tud_ch341_n_write_available (uint8_t itf);

// Clear the transmit FIFO
bool tud_ch341_n_write_clear (uint8_t itf);

// Send line events to host. (IE: CTS, DSR, RI, DCD)
uint32_t tud_ch341_n_set_modem_state(uint8_t itf, ch341_modem_state_t modem_states);

ch341_modem_state_t tud_ch341_n_get_modem_state(uint8_t itf);

uint32_t tud_ch341_n_set_break(uint8_t itf, bool on);

uint32_t tud_ch341_n_notify_flush(uint8_t itf);

//--------------------------------------------------------------------+
// Application API (Single Port)
//--------------------------------------------------------------------+
static inline bool     tud_ch341_connected       (void);
static inline ch341_line_state_t  tud_ch341_get_line_state  (void);
static inline void     tud_ch341_get_line_coding (ch341_line_coding_t* coding);
static inline void     tud_ch341_set_wanted_char (char wanted);

static inline uint32_t tud_ch341_available       (void);
static inline int32_t  tud_ch341_read_char       (void);
static inline uint32_t tud_ch341_read            (void* buffer, uint32_t bufsize);
static inline void     tud_ch341_read_flush      (void);
static inline bool     tud_ch341_peek            (uint8_t* ui8);

static inline uint32_t tud_ch341_write_char      (char ch);
static inline uint32_t tud_ch341_write           (void const* buffer, uint32_t bufsize);
static inline uint32_t tud_ch341_write_str       (char const* str);
static inline uint32_t tud_ch341_write_flush     (void);
static inline uint32_t tud_ch341_write_available (void);
static inline bool     tud_ch341_write_clear     (void);
static inline uint32_t tud_ch341_set_modem_state(ch341_modem_state_t modem_states);
static inline ch341_modem_state_t tud_ch341_get_modem_state(void);
static inline uint32_t tud_ch341_notify_flush(void);

//--------------------------------------------------------------------+
// Application Callback API (weak is optional)
//--------------------------------------------------------------------+

// Invoked when received new data
TU_ATTR_WEAK void tud_ch341_rx_cb(uint8_t itf);

// Invoked when received `wanted_char`
TU_ATTR_WEAK void tud_ch341_rx_wanted_cb(uint8_t itf, char wanted_char);

// Invoked when space becomes available in TX buffer
TU_ATTR_WEAK void tud_ch341_tx_complete_cb(uint8_t itf);

// Invoked when line state DTR & RTS are changed via SET_CONTROL_LINE_STATE
TU_ATTR_WEAK void tud_ch341_line_state_cb(uint8_t itf, ch341_line_state_t line_state);

// Invoked when line coding is change via SET_LINE_CODING
TU_ATTR_WEAK void tud_ch341_line_coding_cb(uint8_t itf, ch341_line_coding_t const* p_line_coding);

// Invoked when received send break
TU_ATTR_WEAK void tud_ch341_send_break_cb(uint8_t itf, bool is_break_active);

//--------------------------------------------------------------------+
// Inline Functions
//--------------------------------------------------------------------+
static inline int32_t tud_ch341_n_read_char (uint8_t itf)
{
  uint8_t ch;
  return tud_ch341_n_read(itf, &ch, 1) ? (int32_t) ch : -1;
}

static inline uint32_t tud_ch341_n_write_char(uint8_t itf, char ch)
{
  return tud_ch341_n_write(itf, &ch, 1);
}

static inline uint32_t tud_ch341_n_write_str (uint8_t itf, char const* str)
{
  return tud_ch341_n_write(itf, str, strlen(str));
}

static inline bool tud_ch341_connected (void)
{
  return tud_ch341_n_connected(0);
}

static inline ch341_line_state_t tud_ch341_get_line_state (void)
{
  return tud_ch341_n_get_line_state(0);
}

static inline void tud_ch341_get_line_coding (ch341_line_coding_t* coding)
{
  tud_ch341_n_get_line_coding(0, coding);
}

static inline void tud_ch341_set_wanted_char (char wanted)
{
  tud_ch341_n_set_wanted_char(0, wanted);
}

static inline uint32_t tud_ch341_available (void)
{
  return tud_ch341_n_available(0);
}

static inline int32_t tud_ch341_read_char (void)
{
  return tud_ch341_n_read_char(0);
}

static inline uint32_t tud_ch341_read (void* buffer, uint32_t bufsize)
{
  return tud_ch341_n_read(0, buffer, bufsize);
}

static inline void tud_ch341_read_flush (void)
{
  tud_ch341_n_read_flush(0);
}

static inline bool tud_ch341_peek (uint8_t* ui8)
{
  return tud_ch341_n_peek(0, ui8);
}

static inline uint32_t tud_ch341_write_char (char ch)
{
  return tud_ch341_n_write_char(0, ch);
}

static inline uint32_t tud_ch341_write (void const* buffer, uint32_t bufsize)
{
  return tud_ch341_n_write(0, buffer, bufsize);
}

static inline uint32_t tud_ch341_write_str (char const* str)
{
  return tud_ch341_n_write_str(0, str);
}

static inline uint32_t tud_ch341_write_flush (void)
{
  return tud_ch341_n_write_flush(0);
}

static inline uint32_t tud_ch341_write_available(void)
{
  return tud_ch341_n_write_available(0);
}

static inline bool tud_ch341_write_clear(void)
{
  return tud_ch341_n_write_clear(0);
}

static inline uint32_t tud_ch341_set_modem_state(ch341_modem_state_t modem_states)
{
  return tud_ch341_n_set_modem_state(0, modem_states);
}

static inline ch341_modem_state_t tud_ch341_get_modem_state(void)
{
  return tud_ch341_n_get_modem_state(0);
}

static inline uint32_t tud_ch341_notify_flush(void)
{
  return tud_ch341_n_notify_flush(0);
}

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

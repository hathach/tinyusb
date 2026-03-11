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

#ifndef TUSB_VENDOR_DEVICE_H_
#define TUSB_VENDOR_DEVICE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "common/tusb_common.h"

//--------------------------------------------------------------------+
// Configuration
//--------------------------------------------------------------------+
#ifndef CFG_TUD_VENDOR_EPSIZE
  #define CFG_TUD_VENDOR_EPSIZE 64
#endif

// RX FIFO can be disabled by setting this value to 0
#ifndef CFG_TUD_VENDOR_RX_BUFSIZE
  #define CFG_TUD_VENDOR_RX_BUFSIZE 64
#endif

// TX FIFO can be disabled by setting this value to 0
#ifndef CFG_TUD_VENDOR_TX_BUFSIZE
  #define CFG_TUD_VENDOR_TX_BUFSIZE 64
#endif

// Vendor is buffered (FIFO mode) if both TX and RX buffers are configured
// If either is 0, vendor operates in non-buffered (direct transfer) mode
#ifndef CFG_TUD_VENDOR_TXRX_BUFFERED
  #define CFG_TUD_VENDOR_TXRX_BUFFERED ((CFG_TUD_VENDOR_RX_BUFSIZE > 0) && (CFG_TUD_VENDOR_TX_BUFSIZE > 0))
#endif

// Application will manually schedule RX transfer. This can be useful when using with non-fifo (buffered) mode
// i.e. CFG_TUD_VENDOR_TXRX_BUFFERED = 0
#ifndef CFG_TUD_VENDOR_RX_MANUAL_XFER
  #define CFG_TUD_VENDOR_RX_MANUAL_XFER 0
#endif

//--------------------------------------------------------------------+
// Application API (Multiple Interfaces) i.e CFG_TUD_VENDOR > 1
//--------------------------------------------------------------------+

// Return whether the vendor interface is mounted
bool tud_vendor_n_mounted(uint8_t idx);

//------------- RX -------------//
#if CFG_TUD_VENDOR_TXRX_BUFFERED
// Return number of available bytes for reading
uint32_t tud_vendor_n_available(uint8_t idx);

// Peek a byte from RX buffer
bool tud_vendor_n_peek(uint8_t idx, uint8_t *ui8);

// Read from RX FIFO
uint32_t tud_vendor_n_read(uint8_t idx, void *buffer, uint32_t bufsize);

// Flush (clear) RX FIFO
void tud_vendor_n_read_flush(uint8_t idx);
#endif

#if CFG_TUD_VENDOR_RX_MANUAL_XFER
// Start a new RX transfer to fill the RX FIFO, return false if previous transfer is still ongoing
bool tud_vendor_n_read_xfer(uint8_t idx);
#endif

//------------- TX -------------//
// Write to TX FIFO. This can be buffered and not sent immediately unless buffered bytes >= USB endpoint size
uint32_t tud_vendor_n_write(uint8_t idx, const void *buffer, uint32_t bufsize);

// Return number of bytes available for writing in TX FIFO (or endpoint if non-buffered)
uint32_t tud_vendor_n_write_available(uint8_t idx);

#if CFG_TUD_VENDOR_TXRX_BUFFERED
// Force sending buffered data, return number of bytes sent
uint32_t tud_vendor_n_write_flush(uint8_t idx);

// Clear the transmit FIFO
bool tud_vendor_n_write_clear(uint8_t idx);
#endif

// Write a null-terminated string to TX FIFO
TU_ATTR_ALWAYS_INLINE static inline uint32_t tud_vendor_n_write_str(uint8_t idx, const char *str) {
  return tud_vendor_n_write(idx, str, strlen(str));
}

// backward compatible
#define tud_vendor_n_flush(idx) tud_vendor_n_write_flush(idx)

//--------------------------------------------------------------------+
// Application API (Single Port) i.e CFG_TUD_VENDOR = 1
//--------------------------------------------------------------------+
TU_ATTR_ALWAYS_INLINE static inline bool tud_vendor_mounted(void) {
  return tud_vendor_n_mounted(0);
}

#if CFG_TUD_VENDOR_TXRX_BUFFERED
TU_ATTR_ALWAYS_INLINE static inline uint32_t tud_vendor_available(void) {
  return tud_vendor_n_available(0);
}

TU_ATTR_ALWAYS_INLINE static inline bool tud_vendor_peek(uint8_t *ui8) {
  return tud_vendor_n_peek(0, ui8);
}

TU_ATTR_ALWAYS_INLINE static inline uint32_t tud_vendor_read(void *buffer, uint32_t bufsize) {
  return tud_vendor_n_read(0, buffer, bufsize);
}

TU_ATTR_ALWAYS_INLINE static inline void tud_vendor_read_flush(void) {
  tud_vendor_n_read_flush(0);
}

TU_ATTR_ALWAYS_INLINE static inline uint32_t tud_vendor_write_flush(void) {
  return tud_vendor_n_write_flush(0);
}

TU_ATTR_ALWAYS_INLINE static inline bool tud_vendor_write_clear(void) {
  return tud_vendor_n_write_clear(0);
}
#endif

#if CFG_TUD_VENDOR_RX_MANUAL_XFER
TU_ATTR_ALWAYS_INLINE static inline bool tud_vendor_read_xfer(void) {
  return tud_vendor_n_read_xfer(0);
}
#endif

TU_ATTR_ALWAYS_INLINE static inline uint32_t tud_vendor_write(const void *buffer, uint32_t bufsize) {
  return tud_vendor_n_write(0, buffer, bufsize);
}

TU_ATTR_ALWAYS_INLINE static inline uint32_t tud_vendor_write_str(const char *str) {
  return tud_vendor_n_write_str(0, str);
}

TU_ATTR_ALWAYS_INLINE static inline uint32_t tud_vendor_write_available(void) {
  return tud_vendor_n_write_available(0);
}

// backward compatible
#define tud_vendor_flush() tud_vendor_write_flush()

//--------------------------------------------------------------------+
// Application Callback API (weak is optional)
//--------------------------------------------------------------------+

// Invoked when received new data.
// - CFG_TUD_VENDOR_TXRX_BUFFERED = 1: buffer and bufsize must not be used (both NULL,0) since data is in RX FIFO
// - CFG_TUD_VENDOR_TXRX_BUFFERED = 0: Buffer and bufsize are valid
void tud_vendor_rx_cb(uint8_t idx, const uint8_t *buffer, uint32_t bufsize);

// Invoked when tx transfer is finished
void tud_vendor_tx_cb(uint8_t idx, uint32_t sent_bytes);

//--------------------------------------------------------------------+
// Internal Class Driver API
//--------------------------------------------------------------------+
void     vendord_init(void);
bool     vendord_deinit(void);
void     vendord_reset(uint8_t rhport);
uint16_t vendord_open(uint8_t rhport, const tusb_desc_interface_t *idx_desc, uint16_t max_len);
bool     vendord_xfer_cb(uint8_t rhport, uint8_t ep_addr, xfer_result_t event, uint32_t xferred_bytes);

#ifdef __cplusplus
}
#endif

#endif /* TUSB_VENDOR_DEVICE_H_ */

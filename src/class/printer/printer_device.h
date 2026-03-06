/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2026 Ha Thach (tinyusb.org)
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

#ifndef TUSB_PRINTER_DEVICE_H_
#define TUSB_PRINTER_DEVICE_H_

#include "printer.h"

#ifdef __cplusplus
extern "C" {
#endif

//--------------------------------------------------------------------+
// Application API (Multiple Ports) i.e. CFG_TUD_PRINTER > 1
//--------------------------------------------------------------------+

// Get the number of bytes available for reading
uint32_t tud_printer_n_read_available(uint8_t itf);

// Read received bytes
uint32_t tud_printer_n_read(uint8_t itf, void *buffer, uint32_t bufsize);

// Get the number of bytes available for writing
uint32_t tud_printer_n_write_available(uint8_t itf);

// Clear the received FIFO
void tud_printer_n_read_flush(uint8_t itf);

// Get a byte from FIFO without removing it
bool tud_printer_n_peek(uint8_t itf, uint8_t *ui8);

// Write data to host
uint32_t tud_printer_n_write(uint8_t itf, const void *buffer, uint32_t bufsize);

// Force sending data in the TX FIFO
uint32_t tud_printer_n_write_flush(uint8_t itf);

// Clear the transmit FIFO
bool tud_printer_n_write_clear(uint8_t itf);

//--------------------------------------------------------------------+
// Application API (Single Port)
//--------------------------------------------------------------------+

TU_ATTR_ALWAYS_INLINE static inline uint32_t tud_printer_read_available(void) {
  return tud_printer_n_read_available(0);
}

TU_ATTR_ALWAYS_INLINE static inline uint32_t tud_printer_write_available(void) {
  return tud_printer_n_write_available(0);
}

TU_ATTR_ALWAYS_INLINE static inline uint32_t tud_printer_read(void *buffer, uint32_t bufsize) {
  return tud_printer_n_read(0, buffer, bufsize);
}

TU_ATTR_ALWAYS_INLINE static inline void tud_printer_read_flush(void) {
  tud_printer_n_read_flush(0);
}

TU_ATTR_ALWAYS_INLINE static inline bool tud_printer_peek(uint8_t *ui8) {
  return tud_printer_n_peek(0, ui8);
}

TU_ATTR_ALWAYS_INLINE static inline uint32_t tud_printer_write(const void *buffer, uint32_t bufsize) {
  return tud_printer_n_write(0, buffer, bufsize);
}

TU_ATTR_ALWAYS_INLINE static inline uint32_t tud_printer_write_flush(void) {
  return tud_printer_n_write_flush(0);
}

TU_ATTR_ALWAYS_INLINE static inline bool tud_printer_write_clear(void) {
  return tud_printer_n_write_clear(0);
}

//--------------------------------------------------------------------+
// Application Callback API (weak is optional)
//--------------------------------------------------------------------+

// Invoked when received new data
void tud_printer_rx_cb(uint8_t itf);

// Invoked when last write transfer is completed
void tud_printer_tx_complete_cb(uint8_t itf);

// Invoked when host requests device ID string (IEEE 1284).
// Application returns pointer to device ID buffer (must remain valid until transfer completes).
// First 2 bytes of returned buffer must contain big-endian length (including the 2 length bytes).
const uint8_t *tud_printer_get_device_id_cb(uint8_t itf);

// Invoked when host requests port status.
uint8_t tud_printer_get_port_status_cb(uint8_t itf);

// Invoked when host requests soft reset.
void tud_printer_soft_reset_cb(uint8_t itf);

// Invoked when a control request is completed (GET_DEVICE_ID, GET_PORT_STATUS, etc.)
void tud_printer_request_complete_cb(uint8_t itf, tusb_control_request_t const *request);


//--------------------------------------------------------------------+
// Internal Class Driver API
//--------------------------------------------------------------------+
void     printerd_init(void);
bool     printerd_deinit(void);
void     printerd_reset(uint8_t rhport);
uint16_t printerd_open(uint8_t rhport, const tusb_desc_interface_t *itf_desc, uint16_t max_len);
bool     printerd_control_xfer_cb(uint8_t rhport, uint8_t stage, const tusb_control_request_t *request);
bool     printerd_xfer_cb(uint8_t rhport, uint8_t ep_addr, xfer_result_t event, uint32_t xferred_bytes);


#ifdef __cplusplus
}
#endif

#endif

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

typedef struct TU_ATTR_PACKED {
  uint8_t rx_persistent : 1; // keep rx fifo on bus reset or disconnect
  uint8_t tx_persistent : 1; // keep tx fifo on bus reset or disconnect
} tud_printer_configure_fifo_t;

//--------------------------------------------------------------------+
// Application API (Multiple Ports) i.e. CFG_TUD_PRINTER > 1
//--------------------------------------------------------------------+

// Get the number of bytes available for reading
uint32_t tud_printer_n_available(uint8_t itf);

// Read received bytes
uint32_t tud_printer_n_read(uint8_t itf, void *buffer, uint32_t bufsize);

// Clear the received FIFO
void tud_printer_n_read_flush(uint8_t itf);

// Get a byte from FIFO without removing it
bool tud_printer_n_peek(uint8_t itf, uint8_t *ui8);

//--------------------------------------------------------------------+
// Application Callback API (weak is optional)
//--------------------------------------------------------------------+

// Invoked when received new data
TU_ATTR_WEAK void tud_printer_rx_cb(uint8_t itf, size_t n);

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

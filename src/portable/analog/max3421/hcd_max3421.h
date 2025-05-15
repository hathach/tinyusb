/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2025 Ha Thach (tinyusb.org)
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
#ifndef TUSB_HCD_MAX3421_H
#define TUSB_HCD_MAX3421_H

#ifdef __cplusplus
 extern "C" {
#endif

//--------------------------------------------------------------------+
// SPI transfer API with MAX3421E are implemented by application
// - spi_cs_api(), spi_xfer_api(), int_api()
//--------------------------------------------------------------------+

// API to control MAX3421 SPI CS
extern void tuh_max3421_spi_cs_api(uint8_t rhport, bool active);

// API to transfer data with MAX3421 SPI
// Either tx_buf or rx_buf can be NULL, which means transfer is write or read only
extern bool tuh_max3421_spi_xfer_api(uint8_t rhport, uint8_t const* tx_buf, uint8_t* rx_buf, size_t xfer_bytes);

// API to enable/disable MAX3421 INTR pin interrupt
extern void tuh_max3421_int_api(uint8_t rhport, bool enabled);

//--------------------------------------------------------------------+
// API for read/write MAX3421 registers
// are implemented by this driver, can be used by application
//--------------------------------------------------------------------+

// API to read MAX3421's register. Implemented by TinyUSB
uint8_t tuh_max3421_reg_read(uint8_t rhport, uint8_t reg, bool in_isr);

// API to write MAX3421's register. Implemented by TinyUSB
bool tuh_max3421_reg_write(uint8_t rhport, uint8_t reg, uint8_t data, bool in_isr);

#ifdef __cplusplus
 }
#endif

#endif

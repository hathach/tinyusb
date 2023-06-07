/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2023 Ha Thach (thach@tinyusb.org) for Adafruit Industries
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

#ifndef _TUSB_TCD_H_
#define _TUSB_TCD_H_

#include "common/tusb_common.h"
#include "osal/osal.h"
#include "common/tusb_fifo.h"

#ifdef __cplusplus
extern "C" {
#endif

//--------------------------------------------------------------------+
//
//--------------------------------------------------------------------+
typedef struct {
  uint8_t rhport;
  uint8_t event_id;


} tcd_event_t;;

//--------------------------------------------------------------------+
//
//--------------------------------------------------------------------+

// Initialize controller
bool tcd_init(uint8_t rhport, tusb_typec_port_type_t port_type);

// Enable interrupt
void tcd_int_enable (uint8_t rhport);

// Disable interrupt
void tcd_int_disable(uint8_t rhport);

// Interrupt Handler
void tcd_int_handler(uint8_t rhport);

//--------------------------------------------------------------------+
//
//--------------------------------------------------------------------+

bool tcd_rx_start(uint8_t rhport, uint8_t* buffer, uint16_t total_bytes);
bool tcd_tx_start(uint8_t rhport, uint8_t const* buffer, uint16_t total_bytes);

#endif

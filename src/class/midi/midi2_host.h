/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2026 Saulo Verissimo
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

#ifndef TUSB_MIDI2_HOST_H_
#define TUSB_MIDI2_HOST_H_

#include "class/audio/audio.h"
#include "midi.h"

#ifdef __cplusplus
extern "C" {
#endif

//--------------------------------------------------------------------+
// Callback Type Definitions
//--------------------------------------------------------------------+

typedef struct {
  uint8_t protocol_version;                       // 0 = MIDI 1.0 only, 1 = MIDI 2.0
  uint8_t bcdMSC_hi, bcdMSC_lo;                   // MIDI version from descriptor
  uint8_t rx_cable_count;                         // For both alt settings (same for Alt 0 and Alt 1)
  uint8_t tx_cable_count;
} tuh_midi2_descriptor_cb_t;

typedef struct {
  uint8_t daddr;
  uint8_t bInterfaceNumber;
  uint8_t protocol_version;                       // 0 = MIDI 1.0, 1 = MIDI 2.0
  uint8_t alt_setting_active;                     // 0 or 1
  uint8_t rx_cable_count;
  uint8_t tx_cable_count;
} tuh_midi2_mount_cb_t;

//--------------------------------------------------------------------+
// Application Callback API (weak, optional)
//--------------------------------------------------------------------+

void tuh_midi2_descriptor_cb(uint8_t idx, const tuh_midi2_descriptor_cb_t *desc_cb_data);
void tuh_midi2_mount_cb(uint8_t idx, const tuh_midi2_mount_cb_t *mount_cb_data);
void tuh_midi2_rx_cb(uint8_t idx, uint32_t xferred_bytes);
void tuh_midi2_tx_cb(uint8_t idx, uint32_t xferred_bytes);
void tuh_midi2_umount_cb(uint8_t idx);

//--------------------------------------------------------------------+
// Application API - Query
//--------------------------------------------------------------------+

bool     tuh_midi2_mounted(uint8_t idx);
uint8_t  tuh_midi2_get_protocol_version(uint8_t idx);
uint8_t  tuh_midi2_get_alt_setting_active(uint8_t idx);
uint8_t  tuh_midi2_get_cable_count(uint8_t idx);

//--------------------------------------------------------------------+
// Application API - I/O
//--------------------------------------------------------------------+

uint32_t tuh_midi2_ump_read(uint8_t idx, uint32_t* words, uint32_t max_words);
uint32_t tuh_midi2_ump_write(uint8_t idx, const uint32_t* words, uint32_t count);
uint32_t tuh_midi2_write_flush(uint8_t idx);

//--------------------------------------------------------------------+
// Internal Class Driver API
//--------------------------------------------------------------------+

bool     midih2_init(void);
bool     midih2_deinit(void);
bool     midih2_set_config(uint8_t dev_addr, uint8_t itf_num);
void     midih2_close(uint8_t dev_addr);
uint16_t midih2_open(uint8_t rhport, uint8_t dev_addr, const tusb_desc_interface_t *desc_itf, uint16_t max_len);
bool     midih2_xfer_cb(uint8_t dev_addr, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes);

#ifdef __cplusplus
}
#endif

#endif

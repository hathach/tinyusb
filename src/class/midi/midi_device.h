/**************************************************************************/
/*!
    @file     midi_device.h
    @author   hathach (tinyusb.org)

    @section LICENSE

    Software License Agreement (BSD License)

    Copyright (c) 2013, hathach (tinyusb.org)
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:
    1. Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
    3. Neither the name of the copyright holders nor the
    names of its contributors may be used to endorse or promote products
    derived from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ''AS IS'' AND ANY
    EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

    This file is part of the tinyusb stack.
*/
/**************************************************************************/

#ifndef _TUSB_MIDI_DEVICE_H_
#define _TUSB_MIDI_DEVICE_H_

#include "common/tusb_common.h"
#include "device/usbd.h"
#include "class/audio/audio.h"

//--------------------------------------------------------------------+
// Class Driver Configuration
//--------------------------------------------------------------------+
#ifndef CFG_TUD_MIDI_EPSIZE
#define CFG_TUD_MIDI_EPSIZE 64
#endif


#ifdef __cplusplus
 extern "C" {
#endif

/** \addtogroup MIDI_Serial Serial
 *  @{
 *  \defgroup   MIDI_Serial_Device Device
 *  @{ */

//--------------------------------------------------------------------+
// APPLICATION API (Multiple Interfaces)
// CFG_TUD_MIDI > 1
//--------------------------------------------------------------------+
bool     tud_midi_n_connected       (uint8_t itf);

uint32_t tud_midi_n_available       (uint8_t itf, uint8_t jack_id);
char     tud_midi_n_read_char       (uint8_t itf, uint8_t jack_id);
uint32_t tud_midi_n_read            (uint8_t itf, uint8_t jack_id, void* buffer, uint32_t bufsize);
void     tud_midi_n_read_flush      (uint8_t itf, uint8_t jack_id);
char     tud_midi_n_peek            (uint8_t itf, uint8_t jack_id, int pos);

uint32_t tud_midi_n_write_char      (uint8_t itf, char ch);
uint32_t tud_midi_n_write           (uint8_t itf, uint8_t jack_id, uint8_t const* buffer, uint32_t bufsize);
bool     tud_midi_n_write_flush     (uint8_t itf);

//--------------------------------------------------------------------+
// APPLICATION API (Interface0)
//--------------------------------------------------------------------+
static inline bool     tud_midi_connected       (void)                                 { return tud_midi_n_connected(0);              }

static inline uint32_t tud_midi_available       (void)                                 { return tud_midi_n_available(0, 0);              }
static inline char     tud_midi_read_char       (void)                                 { return tud_midi_n_read_char(0, 0);              }
static inline uint32_t tud_midi_read            (void* buffer, uint32_t bufsize)       { return tud_midi_n_read(0, 0, buffer, bufsize);  }
static inline void     tud_midi_read_flush      (void)                                 { tud_midi_n_read_flush(0, 0);                    }
static inline char     tud_midi_peek            (int pos)                              { return tud_midi_n_peek(0, 0, pos);              }

static inline uint32_t tud_midi_write_char      (char ch)                              { return tud_midi_n_write_char(0, ch);         }
static inline uint32_t tud_midi_write           (uint8_t jack_id, void const* buffer, uint32_t bufsize) { return tud_midi_n_write(0, jack_id, buffer, bufsize); }
static inline bool     tud_midi_write_flush     (void)                                 { return tud_midi_n_write_flush(0);            }

//--------------------------------------------------------------------+
// APPLICATION CALLBACK API (WEAK is optional)
//--------------------------------------------------------------------+
ATTR_WEAK void tud_midi_rx_cb(uint8_t itf);

//--------------------------------------------------------------------+
// USBD-CLASS DRIVER API
//--------------------------------------------------------------------+
#ifdef _TINY_USB_SOURCE_FILE_

void         midid_init               (void);
bool midid_open               (uint8_t rhport, tusb_desc_interface_t const * p_interface_desc, uint16_t *p_length);
bool midid_control_request (uint8_t rhport, tusb_control_request_t const * p_request);
bool midid_control_request_complete (uint8_t rhport, tusb_control_request_t const * p_request);
bool midid_xfer_cb            (uint8_t rhport, uint8_t edpt_addr, xfer_result_t result, uint32_t xferred_bytes);
void         midid_reset              (uint8_t rhport);

#endif

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_MIDI_DEVICE_H_ */

/** @} */
/** @} */

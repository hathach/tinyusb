/**************************************************************************/
/*!
    @file     cdc_device.h
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

#ifndef _TUSB_CDC_DEVICE_H_
#define _TUSB_CDC_DEVICE_H_

#include "common/tusb_common.h"
#include "device/usbd.h"
#include "cdc.h"

//--------------------------------------------------------------------+
// Class Driver Configuration
//--------------------------------------------------------------------+
#ifndef CFG_TUD_CDC_EPSIZE
#define CFG_TUD_CDC_EPSIZE 64
#endif


#ifdef __cplusplus
 extern "C" {
#endif

/** \addtogroup CDC_Serial Serial
 *  @{
 *  \defgroup   CDC_Serial_Device Device
 *  @{ */

//--------------------------------------------------------------------+
// APPLICATION API (Multiple Interfaces)
// CFG_TUD_CDC > 1
//--------------------------------------------------------------------+
bool     tud_cdc_n_connected       (uint8_t itf);
uint8_t  tud_cdc_n_get_line_state  (uint8_t itf);
void     tud_cdc_n_get_line_coding (uint8_t itf, cdc_line_coding_t* coding);
void     tud_cdc_n_set_wanted_char (uint8_t itf, char wanted);

uint32_t tud_cdc_n_available       (uint8_t itf);
char     tud_cdc_n_read_char       (uint8_t itf);
uint32_t tud_cdc_n_read            (uint8_t itf, void* buffer, uint32_t bufsize);
void     tud_cdc_n_read_flush      (uint8_t itf);
char     tud_cdc_n_peek            (uint8_t itf, int pos);

uint32_t tud_cdc_n_write_char      (uint8_t itf, char ch);
uint32_t tud_cdc_n_write           (uint8_t itf, void const* buffer, uint32_t bufsize);
bool     tud_cdc_n_write_flush     (uint8_t itf);

//--------------------------------------------------------------------+
// APPLICATION API (Interface0)
//--------------------------------------------------------------------+
static inline bool     tud_cdc_connected       (void)                                 { return tud_cdc_n_connected(0);              }
static inline uint8_t  tud_cdc_get_line_state  (void)                                 { return tud_cdc_n_get_line_state(0);         }
static inline void     tud_cdc_get_line_coding (cdc_line_coding_t* coding)            { return tud_cdc_n_get_line_coding(0, coding);}
static inline void     tud_cdc_set_wanted_char (char wanted)                          { tud_cdc_n_set_wanted_char(0, wanted);       }

static inline uint32_t tud_cdc_available       (void)                                 { return tud_cdc_n_available(0);              }
static inline char     tud_cdc_read_char       (void)                                 { return tud_cdc_n_read_char(0);              }
static inline uint32_t tud_cdc_read            (void* buffer, uint32_t bufsize)       { return tud_cdc_n_read(0, buffer, bufsize);  }
static inline void     tud_cdc_read_flush      (void)                                 { tud_cdc_n_read_flush(0);                    }
static inline char     tud_cdc_peek            (int pos)                              { return tud_cdc_n_peek(0, pos);              }

static inline uint32_t tud_cdc_write_char      (char ch)                              { return tud_cdc_n_write_char(0, ch);         }
static inline uint32_t tud_cdc_write           (void const* buffer, uint32_t bufsize) { return tud_cdc_n_write(0, buffer, bufsize); }
static inline bool     tud_cdc_write_flush     (void)                                 { return tud_cdc_n_write_flush(0);            }

//--------------------------------------------------------------------+
// APPLICATION CALLBACK API (WEAK is optional)
//--------------------------------------------------------------------+
ATTR_WEAK void tud_cdc_rx_cb(uint8_t itf);
ATTR_WEAK void tud_cdc_rx_wanted_cb(uint8_t itf, char wanted_char);
ATTR_WEAK void tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts);
ATTR_WEAK void tud_cdc_line_coding_cb(uint8_t itf, cdc_line_coding_t const* p_line_coding);

//--------------------------------------------------------------------+
// USBD-CLASS DRIVER API
//--------------------------------------------------------------------+
#ifdef _TINY_USB_SOURCE_FILE_

void         cdcd_init               (void);
tusb_error_t cdcd_open               (uint8_t rhport, tusb_desc_interface_t const * p_interface_desc, uint16_t *p_length);
tusb_error_t cdcd_control_request_st (uint8_t rhport, tusb_control_request_t const * p_request);
tusb_error_t cdcd_xfer_cb            (uint8_t rhport, uint8_t edpt_addr, tusb_event_t event, uint32_t xferred_bytes);
void         cdcd_reset              (uint8_t rhport);

#if CFG_TUD_CDC_FLUSH_ON_SOF
void cdcd_sof(uint8_t rhport);
#else
#define cdcd_sof    NULL
#endif

#endif

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_CDC_DEVICE_H_ */

/** @} */
/** @} */

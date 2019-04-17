/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2018, hathach (tinyusb.org)
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
uint32_t tud_cdc_n_write_str       (uint8_t itf, char const* str);
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
static inline uint32_t tud_cdc_write_str       (char const* str)                      { return tud_cdc_n_write_str(0, str);         }
static inline bool     tud_cdc_write_flush     (void)                                 { return tud_cdc_n_write_flush(0);            }

//--------------------------------------------------------------------+
// APPLICATION CALLBACK API (WEAK is optional)
//--------------------------------------------------------------------+
ATTR_WEAK void tud_cdc_rx_cb(uint8_t itf);
ATTR_WEAK void tud_cdc_rx_wanted_cb(uint8_t itf, char wanted_char);
ATTR_WEAK void tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts);
ATTR_WEAK void tud_cdc_line_coding_cb(uint8_t itf, cdc_line_coding_t const* p_line_coding);

//--------------------------------------------------------------------+
// INTERNAL USBD-CLASS DRIVER API
//--------------------------------------------------------------------+
void cdcd_init               (void);
bool cdcd_open               (uint8_t rhport, tusb_desc_interface_t const * p_interface_desc, uint16_t *p_length);
bool cdcd_control_request (uint8_t rhport, tusb_control_request_t const * p_request);
bool cdcd_control_request_complete (uint8_t rhport, tusb_control_request_t const * p_request);
bool cdcd_xfer_cb            (uint8_t rhport, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes);
void cdcd_reset              (uint8_t rhport);


//--------------------------------------------------------------------+
// Interface Descriptor Template
//--------------------------------------------------------------------+

// Length of template descriptor: 66 bytes
#define TUD_CDC_DESC_LEN  (8+9+5+5+4+5+7+9+7+7)

// CDC Descriptor Template
// interface number, string index, EP notification address and size, EP data address (out,in) and size.
#define TUD_CDC_DESCRIPTOR(_itfnum, _stridx, _ep_notif, _ep_notif_size, _epout, _epin, _epsize) \
  /* Interface Associate */\
  0x08, TUSB_DESC_INTERFACE_ASSOCIATION, _itfnum, 0x02, TUSB_CLASS_CDC, CDC_COMM_SUBCLASS_ABSTRACT_CONTROL_MODEL, CDC_COMM_PROTOCOL_ATCOMMAND, 0x00,\
  /* CDC Control Interface */\
  0x09, TUSB_DESC_INTERFACE, _itfnum, 0x00, 0x01, TUSB_CLASS_CDC, CDC_COMM_SUBCLASS_ABSTRACT_CONTROL_MODEL, CDC_COMM_PROTOCOL_ATCOMMAND, _stridx,\
  /* CDC Header */\
  0x05, TUSB_DESC_CLASS_SPECIFIC, CDC_FUNC_DESC_HEADER, U16_TO_U8S_LE(0x0120),\
  /* CDC Call */\
  0x05, TUSB_DESC_CLASS_SPECIFIC, CDC_FUNC_DESC_CALL_MANAGEMENT, 0x00, (_itfnum) + 1,\
  /* CDC ACM: support line request */\
  0x04, TUSB_DESC_CLASS_SPECIFIC, CDC_FUNC_DESC_ABSTRACT_CONTROL_MANAGEMENT, 0x02,\
  /* CDC Union */\
  0x05, TUSB_DESC_CLASS_SPECIFIC, CDC_FUNC_DESC_UNION, _itfnum, (_itfnum) + 1,\
  /* Endpoint Notification */\
  0x07, TUSB_DESC_ENDPOINT, _ep_notif, TUSB_XFER_INTERRUPT, U16_TO_U8S_LE(_ep_notif_size), 0x10,\
  /* CDC Data Interface */\
  0x09, TUSB_DESC_INTERFACE, (_itfnum)+1, 0x00, 0x02, TUSB_CLASS_CDC_DATA, 0x00, 0x00, 0x00,\
  /* Endpoint Out */\
  0x07, TUSB_DESC_ENDPOINT, _epout, TUSB_XFER_BULK, U16_TO_U8S_LE(_epsize), 0x00,\
  /* Endpoint In */\
  0x07, TUSB_DESC_ENDPOINT, _epin, TUSB_XFER_BULK, U16_TO_U8S_LE(_epsize), 0x00

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_CDC_DEVICE_H_ */

/** @} */
/** @} */

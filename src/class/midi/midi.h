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

/** \ingroup group_class
 *  \defgroup ClassDriver_CDC Communication Device Class (CDC)
 *            Currently only Abstract Control Model subclass is supported
 *  @{ */

#ifndef _TUSB_MIDI_H__
#define _TUSB_MIDI_H__

#include "common/tusb_common.h"

#ifdef __cplusplus
 extern "C" {
#endif

//--------------------------------------------------------------------+
// FUNCTIONAL DESCRIPTOR (COMMUNICATION INTERFACE)
//--------------------------------------------------------------------+
/// Header Functional Descriptor (Communication Interface)
typedef struct ATTR_PACKED
{
  uint8_t bLength            ; ///< Size of this descriptor in bytes.
  uint8_t bDescriptorType    ; ///< Descriptor Type, must be Class-Specific
  uint8_t bDescriptorSubType ; ///< Descriptor SubType one of above MIDI_FUCN_DESC_
  uint16_t bcdMSC            ; ///< MidiStreaming SubClass release number in Binary-Coded Decimal
  uint16_t wTotalLength      ;
}midi_desc_func_header_t;

/// Union Functional Descriptor (Communication Interface)
typedef struct ATTR_PACKED
{
  uint8_t bLength                  ; ///< Size of this descriptor in bytes.
  uint8_t bDescriptorType          ; ///< Descriptor Type, must be Class-Specific
  uint8_t bDescriptorSubType       ; ///< Descriptor SubType one of above CDC_FUCN_DESC_
  uint8_t bControlInterface        ; ///< Interface number of Communication Interface
  uint8_t bSubordinateInterface    ; ///< Array of Interface number of Data Interface
}cdc_desc_func_union_t;

/** @} */

#ifdef __cplusplus
 }
#endif

#endif

/** @} */

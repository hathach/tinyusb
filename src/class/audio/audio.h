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
 *  \defgroup ClassDriver_Audio Audio
 *            Currently only MIDI subclass is supported
 *  @{ */

#ifndef _TUSB_CDC_H__
#define _TUSB_CDC_H__

#include "common/tusb_common.h"

#ifdef __cplusplus
 extern "C" {
#endif

/// Audio Interface Subclass Codes
typedef enum
{
  AUDIO_SUBCLASS_AUDIO_CONTROL = 0x01  , ///< Audio Control
  AUDIO_SUBCLASS_AUDIO_STREAMING       , ///< Audio Streaming
  AUDIO_SUBCLASS_MIDI_STREAMING       ,  ///< MIDI Streaming
} audio_subclass_type_t;

/// Audio Protocol Codes
typedef enum
{
  AUDIO_PROTOCOL_V1                   = 0x00, ///< Version 1.0
  AUDIO_PROTOCOL_V2                   = 0x20, ///< Version 2.0
  AUDIO_PROTOCOL_V3                   = 0x30, ///< Version 3.0
} audio_protocol_type_t;


/** @} */

#ifdef __cplusplus
 }
#endif

#endif

/** @} */

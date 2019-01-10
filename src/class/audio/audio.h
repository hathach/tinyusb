/**************************************************************************/
/*!
    @file     cdc.h
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
    INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    INCLUDING NEGLIGENCE OR OTHERWISE ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

    This file is part of the tinyusb stack.
*/
/**************************************************************************/

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

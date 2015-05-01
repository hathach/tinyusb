/**************************************************************************/
/*!
    @file     std_request.h
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

/** \ingroup group_usb_definitions
 *  @{ */

#ifndef _TUSB_STD_REQUEST_H_
#define _TUSB_STD_REQUEST_H_

#ifdef __cplusplus
 extern "C" {
#endif

#include "tusb_option.h"
#include <stdbool.h>
#include <stdint.h>
#include "compiler/compiler.h"

typedef struct ATTR_PACKED{
  union {
    struct ATTR_PACKED {
      uint8_t recipient :  5; ///< Recipient type tusb_std_request_recipient_t.
      uint8_t type      :  2; ///< Request type tusb_control_request_type_t.
      uint8_t direction :  1; ///< Direction type. tusb_direction_t
    } bmRequestType_bit;
    uint8_t bmRequestType;
  };

  uint8_t  bRequest;
  uint16_t wValue;
  uint16_t wIndex;
  uint16_t wLength;
} tusb_control_request_t;

STATIC_ASSERT( sizeof(tusb_control_request_t) == 8, "mostly compiler option issue");

// TODO move to somewhere suitable
static inline uint8_t bm_request_type(uint8_t direction, uint8_t type, uint8_t recipient) ATTR_CONST ATTR_ALWAYS_INLINE;
static inline uint8_t bm_request_type(uint8_t direction, uint8_t type, uint8_t recipient)
{
  return ((uint8_t) (direction << 7)) | ((uint8_t) (type << 5)) | (recipient);
}

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_STD_REQUEST_H_ */

/** @} */

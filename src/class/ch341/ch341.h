/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 * Portions Copyright (c) 2022 Travis Robinson (libusbdotnet@gmail.com)
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
 *  \defgroup ClassDriver_CH341 Communication Device Class (CH341)
 *            Currently only Abstract Control Model subclass is supported
 *  @{ */

#ifndef _TUSB_CH341_H__
#define _TUSB_CH341_H__

#include "common/tusb_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \defgroup ClassDriver_CH341_Common Common Definitions
 *  @{ */

/// CH341 Pipe ID, used to indicate which pipe the API is addressing to (Notification, Out, In)
typedef enum
{
  CH341_PIPE_DATA_IN,      ///< Data in pipe
  CH341_PIPE_DATA_OUT,     ///< Data out pipe
  CH341_PIPE_NOTIFICATION, ///< Notification pipe
  CH341_PIPE_ERROR,        ///< Invalid Pipe ID
} ch341_pipeid_t;

typedef struct
{
  uint32_t bit_rate;
  uint8_t stop_bits; ///< 0: 1 stop bit - 2: 2 stop bits
  uint8_t parity;    ///< 0: None - 1: Odd - 2: Even - 3: Mark - 4: Space
  uint8_t data_bits; ///< can be 5, 6, 7, 8
  bool tx_en;
  bool rx_en;
} ch341_line_coding_t;

typedef enum
{
  CH341_LINE_STATE_DTR_ACTIVE = (1 << 0),
  CH341_LINE_STATE_RTS_ACTIVE = (1 << 1),
} ch341_line_state_t;

typedef enum
{
  CH341_MODEM_STATE_CTS_ACTIVE = 0x01,
  CH341_MODEM_STATE_DSR_ACTIVE = 0x02,
  CH341_MODEM_STATE_RNG_ACTIVE = 0x04,
  CH341_MODEM_STATE_DCD_ACTIVE = 0x08,

  CH341_MODEM_STATE_ALL = 0x0F,
} ch341_modem_state_t;

// The CH34x doesn't use any structures so we don't wave anything to pack
// Start of all packed definitions for compiler without per-type packed
/*
    TU_ATTR_PACKED_BEGIN
    TU_ATTR_BIT_FIELD_ORDER_BEGIN

    TU_ATTR_PACKED_END // End of all packed definitions
    TU_ATTR_BIT_FIELD_ORDER_END
 */
#ifdef __cplusplus
}
#endif

#endif

/** @} */

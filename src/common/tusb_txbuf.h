/**************************************************************************/
/*!
    @file     tusb_txbuf.h
    @author   Scott Shawcroft

    @section LICENSE

    Software License Agreement (BSD License)

    Copyright (c) 2018, Scott Shawcroft
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

/** \ingroup Group_Common
 * \defgroup group_txbuf txbuf
 *  @{ */

#ifndef _TUSB_TXBUF_H_
#define _TUSB_TXBUF_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
 extern "C" {
#endif

typedef bool (*edpt_xfer) (uint8_t rhport, uint8_t ep_addr, uint8_t* buffer, uint16_t total_bytes);


/** \struct tu_txbuf_t
 * \brief Circular transmit buffer that manages USB transfer memory. It is not threadsafe and is
 *        only meant for use in the main task.
 */
typedef struct
{
    uint8_t* buffer    ; ///< buffer pointer
    uint16_t buf_len;
    uint16_t first_free;
    uint16_t pending_count;
    uint16_t transmitting_count;
    uint16_t padding;
    uint8_t ep_addr;
    edpt_xfer xfer;
} tu_txbuf_t;

bool tu_txbuf_clear(tu_txbuf_t *f);
bool tu_txbuf_config(tu_txbuf_t *f, uint8_t* buffer, uint16_t depth, edpt_xfer xfer);

uint16_t tu_txbuf_write_n (tu_txbuf_t* txbuf, uint8_t const * buffer, uint32_t length);
void tu_txbuf_set_ep_addr(tu_txbuf_t* txbuf, uint8_t ep_addr);
uint32_t tu_txbuf_transmit_done_cb(tu_txbuf_t* buf, uint32_t bufsize);

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_TXBUF_H_ */

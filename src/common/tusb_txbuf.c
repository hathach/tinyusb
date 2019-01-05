/**************************************************************************/
/*!
    @file     tusb_txbuf.c
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
#include <stdio.h>
#include <string.h>

#include "tusb_txbuf.h"

bool tu_txbuf_config(tu_txbuf_t *txbuf, uint8_t* buffer, uint16_t buffer_length, edpt_xfer xfer)
{
  txbuf->buffer = (uint8_t*) buffer;
  txbuf->buf_len = buffer_length;
  txbuf->first_free = 0;
  txbuf->pending_count = 0;
  txbuf->transmitting_count = 0;
  txbuf->xfer = xfer;
  txbuf->padding = 0;

  return true;
}

uint16_t maybe_transmit(tu_txbuf_t* buf) {
    if (buf->transmitting_count > 0 || buf->pending_count == 0) {
        return 0;
    }
    buf->transmitting_count = buf->pending_count;
    uint16_t transmit_start_index;
    uint8_t over_aligned = 0;
    // The pending zone wraps back to the end so we must do two transfers.
    if (buf->pending_count > buf->first_free) {
        buf->transmitting_count -= buf->first_free;
        transmit_start_index = buf->buf_len - buf->transmitting_count;
    } else {
        transmit_start_index = buf->first_free - buf->transmitting_count;

        // We are transmitting up to first free so ensure it's word aligned for the next transmit.
        over_aligned = buf->first_free % sizeof(uint32_t);
        buf->padding = sizeof(uint32_t) - over_aligned;
        if (over_aligned != 0) {
            buf->first_free = (buf->first_free + buf->padding) % buf->buf_len;
        }
    }
    buf->pending_count -= buf->transmitting_count;

    uint8_t* tx_start = buf->buffer + transmit_start_index;
    if (!buf->xfer(0, buf->ep_addr, tx_start, buf->transmitting_count)) {
        return 0;
    }
    return buf->transmitting_count;
}

uint32_t tu_txbuf_transmit_done_cb(tu_txbuf_t* buf, uint32_t bufsize) {
    buf->transmitting_count -= bufsize;

    return maybe_transmit(buf);
}

/******************************************************************************/
/*!
    @brief This function will write n elements into the array index specified by
    the write pointer and increment the write index. If the write index
    exceeds the max buffer size, then it will roll over to zero.

    @param[in]  f
                Pointer to the FIFO buffer to manipulate
    @param[in]  p_data
                The pointer to data to add to the FIFO
    @param[in]  count
                Number of element
    @return Number of written elements
*/
/******************************************************************************/
uint16_t tu_txbuf_write_n(tu_txbuf_t* txbuf, uint8_t const* buffer, uint32_t bufsize) {
    uint32_t len;
    int32_t last_free = txbuf->first_free - txbuf->pending_count - txbuf->transmitting_count - txbuf->padding;
    if (last_free < 0) {
        last_free += txbuf->buf_len;
        len = last_free - txbuf->first_free;
    } else {
        len = txbuf->buf_len - txbuf->first_free;
    }
    if (bufsize < len) {
        len = bufsize;
    }
    memcpy(txbuf->buffer + txbuf->first_free, buffer, len);
    txbuf->first_free = (txbuf->first_free + len) % txbuf->buf_len;
    txbuf->pending_count += len;
    // Try to transmit now while we wrap the rest.
    maybe_transmit(txbuf);
    uint32_t remaining_bytes = bufsize - len;
    if (remaining_bytes > 0 && last_free != txbuf->first_free) {
        uint32_t second_len = remaining_bytes;
        if (second_len > (uint32_t) last_free + 1) {
            second_len = last_free + 1;
        }
        memcpy(txbuf->buffer, buffer + len, second_len);
        txbuf->first_free = (txbuf->first_free + second_len) % txbuf->buf_len;
        txbuf->pending_count += second_len;
        len += second_len;
    }

    return len;
}

void tu_txbuf_set_ep_addr(tu_txbuf_t* txbuf, uint8_t ep_addr) {
    txbuf->ep_addr = ep_addr;
}

/******************************************************************************/
/*!
    @brief Clear the txbuf including any currently transmitting data.

    @param[in]  t
                Pointer to the txbuf to manipulate
*/
/******************************************************************************/
bool tu_txbuf_clear(tu_txbuf_t *txbuf)
{

  txbuf->first_free = 0;
  txbuf->pending_count = 0;
  txbuf->transmitting_count = 0;

  return true;
}

/**************************************************************************/
/*!
    @file     test_fifo.c
    @author   Scott Shawcroft

    @section LICENSE

    Software License Agreement (BSD License)

    Copyright (c) 2019, Scott Shawcroft
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

#include "unity.h"
#include "common/tusb_txbuf.h"

uint32_t xfer_count = 0;
uint32_t freshly_transmitted = 0;

bool mock_xfer(uint8_t rhport, uint8_t ep_addr, uint8_t* buffer, uint16_t total_bytes) {
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, ((size_t) buffer) % 4, "Transferred buffer not word aligned.");
    xfer_count++;
    freshly_transmitted += total_bytes;
    return true;
}

tu_txbuf_t txbuf;
__attribute__((aligned(4))) uint8_t buffer[16];

uint8_t data[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};

void setUp(void) {
    tu_txbuf_config(&txbuf, buffer, 16, mock_xfer);
    tu_txbuf_set_ep_addr(&txbuf, 1);
}

void tearDown(void) {
    xfer_count = 0;
    freshly_transmitted = 0;
}

void xfer_callback(void) {
    uint32_t previously_transmitted = freshly_transmitted;
    freshly_transmitted = 0;

    tu_txbuf_transmit_done_cb(&txbuf, previously_transmitted);
}

// Test that we transfer immediately at the start and that the pointer is rounded to a word.
void test_normal(void) {
    uint16_t written = tu_txbuf_write_n(&txbuf, data, 4);
    TEST_ASSERT_EQUAL_INT(4, written);
    TEST_ASSERT_EQUAL_INT(1, xfer_count);
    xfer_callback();
    TEST_ASSERT_EQUAL_INT(1, xfer_count);
}

// Test that we only accept the data we have room for.
void test_nearly_full(void) {
    uint16_t written = tu_txbuf_write_n(&txbuf, data, 11);
    TEST_ASSERT_EQUAL_INT(11, written);
    TEST_ASSERT_EQUAL_INT(1, xfer_count);

    written = tu_txbuf_write_n(&txbuf, data, 11);
    // We only have space for 4 more bytes because 11 + padding are being written.
    TEST_ASSERT_EQUAL_INT(4, written);

    // Callback triggers a second write of remaining data.
    xfer_callback();
    TEST_ASSERT_EQUAL_INT(2, xfer_count);
    TEST_ASSERT_EQUAL_INT(4, freshly_transmitted);
}

// Test that we only accept the data even when we have to wrap around.
void test_wrap_around(void) {
    uint16_t written = tu_txbuf_write_n(&txbuf, data, 11);
    TEST_ASSERT_EQUAL_INT(11, written);
    TEST_ASSERT_EQUAL_INT(1, xfer_count);
    xfer_callback();
    TEST_ASSERT_EQUAL_INT(1, xfer_count);

    written = tu_txbuf_write_n(&txbuf, data, 11);
    // We can queue all 11 bytes but they are split.
    TEST_ASSERT_EQUAL_INT(11, written);
    // Four immediately and seven more after the callback.
    TEST_ASSERT_EQUAL_INT(2, xfer_count);
    TEST_ASSERT_EQUAL_INT(4, freshly_transmitted);
    xfer_callback();
    TEST_ASSERT_EQUAL_INT(7, freshly_transmitted);
    TEST_ASSERT_EQUAL_INT(3, xfer_count);
}

// Test that we only accept the data even when we have to wrap around.
void test_wrap_around_too_much(void) {
    uint16_t written = tu_txbuf_write_n(&txbuf, data, 11);
    TEST_ASSERT_EQUAL_INT(11, written);
    TEST_ASSERT_EQUAL_INT(1, xfer_count);
    xfer_callback();
    TEST_ASSERT_EQUAL_INT(1, xfer_count);

    written = tu_txbuf_write_n(&txbuf, data, 17);
    // We can queue 16 of 17 bytes but they are split.
    TEST_ASSERT_EQUAL_INT(16, written);
    // Four immediately and 12 more after the callback.
    TEST_ASSERT_EQUAL_INT(2, xfer_count);
    TEST_ASSERT_EQUAL_INT(4, freshly_transmitted);
    xfer_callback();
    TEST_ASSERT_EQUAL_INT(12, freshly_transmitted);
    TEST_ASSERT_EQUAL_INT(3, xfer_count);
}

int main(void)
{
UNITY_BEGIN();
RUN_TEST(test_normal);
RUN_TEST(test_nearly_full);
RUN_TEST(test_wrap_around);
RUN_TEST(test_wrap_around_too_much);
return UNITY_END();
}

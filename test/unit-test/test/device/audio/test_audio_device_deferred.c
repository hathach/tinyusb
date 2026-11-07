/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 HiFiPhile
 * SPDX-License-Identifier: MIT
 */

// The audio device tests on a port with TUP_USBD_XFER_ISR = 0 (WCH CH32): transfers complete through
// audiod_xfer_cb() in task context, and the driver re-arms its endpoints with is_isr = false.

#define AUDIO_DEVICE_TEST_FIXTURE
#include "audio_device_test.h"

TEST_SOURCE_FILE("audio_device.c")
TEST_SOURCE_FILE("tusb_fifo.c")

void setUp(void) {
  audio_test_setup();
}

void tearDown(void) {
  audio_test_teardown();
}

TU_VERIFY_STATIC(TUP_USBD_XFER_ISR == 0, "test_audio_device_deferred needs TUP_USBD_XFER_ISR=0");

void test_audio_out_deferred_zero_length_packet_rearms_endpoint(void) {
  check_zero_length_packet_rearms_endpoint();
}

void test_audio_out_deferred_zero_length_packet_invokes_rx_done(void) {
  check_zero_length_packet_invokes_rx_done();
}

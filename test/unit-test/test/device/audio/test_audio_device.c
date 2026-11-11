/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 Mete Balci
 * SPDX-License-Identifier: MIT
 */
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

void test_audio_out_zero_length_packet_rearms_endpoint(void) {
  check_zero_length_packet_rearms_endpoint();
}

void test_audio_out_zero_length_packet_invokes_rx_done(void) {
  check_zero_length_packet_invokes_rx_done();
}

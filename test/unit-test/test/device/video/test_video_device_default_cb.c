/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 Adafruit Industries
 * SPDX-License-Identifier: MIT
 */
// No tud_video_frame_xfer_abort_cb() override: the weak default must forward each
// abort to tud_video_frame_xfer_complete_cb() exactly once.
#include "unity.h"

TEST_SOURCE_FILE("video_device.c")

#define VIDEO_DEVICE_TEST_FIXTURE
#include "video_device_test.h"

void setUp(void) {
  video_test_setup();
}
void tearDown(void) {
  video_test_teardown();
}

void test_video_default_abort_on_halt_completes_once(void) {
  TEST_ASSERT_TRUE(tud_video_n_frame_xfer(0, 0, frame, sizeof(frame)));
  endpoint_halt(true);
  TEST_ASSERT_EQUAL(1, completed);
  // A completion queued before the halt does not release the frame again.
  claimed = false;
  TEST_ASSERT_TRUE(videod_xfer_cb(0, 0x81, XFER_RESULT_SUCCESS, payload_length));
  TEST_ASSERT_EQUAL(1, completed);
}

void test_video_default_abort_on_recommit_completes_once(void) {
  TEST_ASSERT_TRUE(tud_video_n_frame_xfer(0, 0, frame, sizeof(frame)));
  commit();
  TEST_ASSERT_EQUAL(0, completed);
  claimed = false;
  TEST_ASSERT_TRUE(videod_xfer_cb(0, 0x81, XFER_RESULT_SUCCESS, payload_length));
  TEST_ASSERT_EQUAL(1, completed);
  TEST_ASSERT_EQUAL(1, submitted);
  TEST_ASSERT_TRUE(tud_video_n_frame_xfer(0, 0, frame, sizeof(frame)));
  TEST_ASSERT_EQUAL(1, completed);
}

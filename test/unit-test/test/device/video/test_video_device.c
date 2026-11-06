/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 Adafruit Industries
 * SPDX-License-Identifier: MIT
 */
#include "unity.h"

TEST_SOURCE_FILE("video_device.c")

#define VIDEO_DEVICE_TEST_FIXTURE
#include "video_device_test.h"

void tud_video_frame_xfer_abort_cb(uint_fast8_t ctl, uint_fast8_t stream) {
  (void)ctl;
  (void)stream;
  aborted++;
  resubmit_from_callback();
}

void setUp(void) {
  video_test_setup();
}
void tearDown(void) {
  video_test_teardown();
}

void test_video_idle_recommit_does_not_notify_or_touch_endpoint(void) {
  commit();
  TEST_ASSERT_EQUAL(0, completed);
  TEST_ASSERT_EQUAL(0, aborted);
  TEST_ASSERT_EQUAL(0, submitted);
  TEST_ASSERT_TRUE(tud_video_n_frame_xfer(0, 0, frame, sizeof(frame)));
}

void test_video_recommit_drains_payload_before_releasing_frame(void) {
  TEST_ASSERT_TRUE(tud_video_n_frame_xfer(0, 0, frame, sizeof(frame)));
  uint8_t queued[64];
  memcpy(queued, payload, payload_length);
  const uint8_t old_fid = payload[1] & 1;
  commit();
  TEST_ASSERT_TRUE(claimed);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(queued, payload, payload_length);
  TEST_ASSERT_EQUAL(0, completed);
  TEST_ASSERT_EQUAL(0, aborted);
  TEST_ASSERT_FALSE(tud_video_n_frame_xfer(0, 0, frame, sizeof(frame)));

  // The core clears its claim before dispatching the queued completion. Even
  // in that window, the class must not accept a newer application frame.
  claimed = false;
  TEST_ASSERT_FALSE(tud_video_n_frame_xfer(0, 0, frame, sizeof(frame)));
  TEST_ASSERT_TRUE(videod_xfer_cb(0, 0x81, XFER_RESULT_SUCCESS, payload_length));
  TEST_ASSERT_EQUAL(0, completed);
  TEST_ASSERT_EQUAL(1, aborted);
  TEST_ASSERT_EQUAL(1, submitted); // no more payloads from the aborted frame
  TEST_ASSERT_TRUE(tud_video_n_frame_xfer(0, 0, frame, sizeof(frame)));
  TEST_ASSERT_NOT_EQUAL(old_fid, payload[1] & 1);
}

void test_video_repeated_commits_release_old_frame_once(void) {
  TEST_ASSERT_TRUE(tud_video_n_frame_xfer(0, 0, frame, sizeof(frame)));
  commit();
  commit();
  claimed = false;
  TEST_ASSERT_TRUE(videod_xfer_cb(0, 0x81, XFER_RESULT_SUCCESS, payload_length));
  TEST_ASSERT_EQUAL(0, completed);
  TEST_ASSERT_EQUAL(1, aborted);
  TEST_ASSERT_EQUAL(1, submitted);
}

void test_video_recommit_with_final_payload_completes_normally(void) {
  TEST_ASSERT_TRUE(tud_video_n_frame_xfer(0, 0, frame, sizeof(frame)));
  for (unsigned i = 0; i < sizeof(frame) && !(payload[1] & 2); ++i) {
    claimed = false;
    TEST_ASSERT_TRUE(videod_xfer_cb(0, 0x81, XFER_RESULT_SUCCESS, payload_length));
  }
  TEST_ASSERT_TRUE(payload[1] & 2); // EOF payload is queued, not completed yet
  commit();
  TEST_ASSERT_EQUAL(0, completed);
  TEST_ASSERT_FALSE(tud_video_n_frame_xfer(0, 0, frame, sizeof(frame)));
  claimed = false;
  TEST_ASSERT_TRUE(videod_xfer_cb(0, 0x81, XFER_RESULT_SUCCESS, payload_length));
  TEST_ASSERT_EQUAL(1, completed);
  TEST_ASSERT_EQUAL(0, aborted);
  TEST_ASSERT_TRUE(tud_video_n_frame_xfer(0, 0, frame, sizeof(frame)));
}

void test_video_normal_frame_still_completes_once(void) {
  deliver_frame();
  TEST_ASSERT_EQUAL(0, aborted);
  TEST_ASSERT_GREATER_THAN(1, submitted);
  TEST_ASSERT_TRUE(tud_video_n_frame_xfer(0, 0, frame, sizeof(frame)));
}

void test_video_rejected_commit_keeps_sending_existing_frame(void) {
  TEST_ASSERT_TRUE(tud_video_n_frame_xfer(0, 0, frame, sizeof(frame)));
  accept_commit = false;
  commit();
  claimed = false;
  TEST_ASSERT_TRUE(videod_xfer_cb(0, 0x81, XFER_RESULT_SUCCESS, payload_length));
  TEST_ASSERT_EQUAL(0, completed);
  TEST_ASSERT_EQUAL(0, aborted);
  TEST_ASSERT_EQUAL(2, submitted);
}

void test_video_refused_initial_payload_leaves_frame_with_caller(void) {
  refuse_xfer = true;
  TEST_ASSERT_FALSE(tud_video_n_frame_xfer(0, 0, frame, sizeof(frame)));
  commit();
  TEST_ASSERT_EQUAL(0, completed);
  TEST_ASSERT_EQUAL(0, aborted);
  refuse_xfer = false;
  TEST_ASSERT_TRUE(tud_video_n_frame_xfer(0, 0, frame, sizeof(frame)));
  TEST_ASSERT_EQUAL(1, submitted);
}

void test_video_refused_continuation_releases_accepted_frame(void) {
  TEST_ASSERT_TRUE(tud_video_n_frame_xfer(0, 0, frame, sizeof(frame)));
  claimed     = false;
  refuse_xfer = true;
  TEST_ASSERT_FALSE(videod_xfer_cb(0, 0x81, XFER_RESULT_SUCCESS, payload_length));
  TEST_ASSERT_EQUAL(1, aborted);
  TEST_ASSERT_EQUAL(0, completed);
  commit();
  refuse_xfer = false;
  TEST_ASSERT_TRUE(tud_video_n_frame_xfer(0, 0, frame, sizeof(frame)));
}

void test_video_failed_continuation_claim_releases_accepted_frame(void) {
  TEST_ASSERT_TRUE(tud_video_n_frame_xfer(0, 0, frame, sizeof(frame)));
  claimed      = false;
  refuse_claim = true;
  TEST_ASSERT_FALSE(videod_xfer_cb(0, 0x81, XFER_RESULT_SUCCESS, payload_length));
  TEST_ASSERT_EQUAL(1, aborted);
  TEST_ASSERT_EQUAL(0, completed);
  commit();
  refuse_claim = false;
  TEST_ASSERT_TRUE(tud_video_n_frame_xfer(0, 0, frame, sizeof(frame)));
}

void test_video_failed_payload_aborts_instead_of_continuing(void) {
  TEST_ASSERT_TRUE(tud_video_n_frame_xfer(0, 0, frame, sizeof(frame)));
  claimed = false;
  TEST_ASSERT_TRUE(videod_xfer_cb(0, 0x81, XFER_RESULT_FAILED, 0));
  TEST_ASSERT_EQUAL(1, aborted);
  TEST_ASSERT_EQUAL(0, completed);
  TEST_ASSERT_EQUAL(1, submitted);
  TEST_ASSERT_TRUE(tud_video_n_frame_xfer(0, 0, frame, sizeof(frame)));
}

void test_video_halt_releases_frame_without_completion(void) {
  TEST_ASSERT_TRUE(tud_video_n_frame_xfer(0, 0, frame, sizeof(frame)));
  endpoint_halt(true);
  TEST_ASSERT_EQUAL(1, aborted);
  TEST_ASSERT_EQUAL(0, completed);
  TEST_ASSERT_FALSE(tud_video_n_frame_xfer(0, 0, frame, sizeof(frame)));
  commit();
  endpoint_halt(false);
  TEST_ASSERT_TRUE(tud_video_n_frame_xfer(0, 0, frame, sizeof(frame)));
  TEST_ASSERT_EQUAL(1, aborted);
}

void test_video_halt_ignores_already_queued_completion(void) {
  TEST_ASSERT_TRUE(tud_video_n_frame_xfer(0, 0, frame, sizeof(frame)));
  endpoint_halt(true);
  claimed = false;
  TEST_ASSERT_TRUE(videod_xfer_cb(0, 0x81, XFER_RESULT_SUCCESS, payload_length));
  TEST_ASSERT_EQUAL(1, aborted);
  TEST_ASSERT_EQUAL(0, completed);
  TEST_ASSERT_EQUAL(1, submitted);
  TEST_ASSERT_FALSE(tud_video_n_frame_xfer(0, 0, frame, sizeof(frame)));
  endpoint_halt(false);
  TEST_ASSERT_TRUE(tud_video_n_frame_xfer(0, 0, frame, sizeof(frame)));
}

void test_video_clear_on_nonhalted_endpoint_keeps_pending_payload(void) {
  TEST_ASSERT_TRUE(tud_video_n_frame_xfer(0, 0, frame, sizeof(frame)));
  uint8_t queued[64];
  memcpy(queued, payload, payload_length);
  endpoint_halt(false);
  TEST_ASSERT_TRUE(claimed);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(queued, payload, payload_length);
  TEST_ASSERT_EQUAL(0, aborted);
  TEST_ASSERT_FALSE(tud_video_n_frame_xfer(0, 0, frame, sizeof(frame)));
  claimed = false;
  TEST_ASSERT_TRUE(videod_xfer_cb(0, 0x81, XFER_RESULT_SUCCESS, payload_length));
  TEST_ASSERT_EQUAL(2, submitted);
}

void test_video_rejected_commit_keeps_payload_size(void) {
  TEST_ASSERT_TRUE(tud_video_n_frame_xfer(0, 0, frame, sizeof(frame)));
  const uint16_t committed_length = payload_length;
  accept_commit                   = false;
  commit_interval(10000000);
  claimed = false;
  TEST_ASSERT_TRUE(videod_xfer_cb(0, 0x81, XFER_RESULT_SUCCESS, payload_length));
  TEST_ASSERT_EQUAL(committed_length, payload_length);

  // The same COMMIT, accepted, sizes the payloads of the next frame.
  accept_commit = true;
  commit_interval(10000000);
  claimed = false;
  TEST_ASSERT_TRUE(videod_xfer_cb(0, 0x81, XFER_RESULT_SUCCESS, payload_length));
  TEST_ASSERT_EQUAL(1, aborted);
  TEST_ASSERT_TRUE(tud_video_n_frame_xfer(0, 0, frame, sizeof(frame)));
  TEST_ASSERT_NOT_EQUAL(committed_length, payload_length);
}

// usbd ignores the class result for both SET and CLEAR_FEATURE(ENDPOINT_HALT); only the SET_FEATURE
// return shows endpoint ownership.
void test_video_set_halt_reports_endpoint_ownership(void) {
  TEST_ASSERT_TRUE(tud_video_n_frame_xfer(0, 0, frame, sizeof(frame)));
  // A halt of an endpoint no stream owns is not handled and aborts nothing.
  TEST_ASSERT_FALSE(endpoint_request(true, 0x82));
  (void)endpoint_request(false, 0x82);
  TEST_ASSERT_EQUAL(0, aborted);
  TEST_ASSERT_EQUAL(0, completed);
  claimed = false;
  TEST_ASSERT_TRUE(videod_xfer_cb(0, 0x81, XFER_RESULT_SUCCESS, payload_length));
  TEST_ASSERT_EQUAL(2, submitted);

  TEST_ASSERT_TRUE(endpoint_request(true, 0x81));
  TEST_ASSERT_EQUAL(1, aborted);
  (void)endpoint_request(false, 0x81);
  TEST_ASSERT_EQUAL(1, aborted);
  TEST_ASSERT_EQUAL(0, completed);
}

void test_video_refused_initial_payload_keeps_frame_id_sequence(void) {
  deliver_frame();
  const uint8_t last_fid = payload[1] & 1;
  refuse_xfer            = true;
  TEST_ASSERT_FALSE(tud_video_n_frame_xfer(0, 0, frame, sizeof(frame)));
  refuse_xfer = false;
  // Retried without a COMMIT, which would reset the header.
  TEST_ASSERT_TRUE(tud_video_n_frame_xfer(0, 0, frame, sizeof(frame)));
  TEST_ASSERT_NOT_EQUAL(last_fid, payload[1] & 1);
  TEST_ASSERT_EQUAL(0, aborted);
}

void test_video_recommit_releases_frame_whose_completion_was_dropped(void) {
  TEST_ASSERT_TRUE(tud_video_n_frame_xfer(0, 0, frame, sizeof(frame)));
  const uint8_t fid = payload[1] & 1;
  // A full event queue drops the completion; usbd then clears BUSY and CLAIMED.
  claimed = false;
  TEST_ASSERT_FALSE(tud_video_n_frame_xfer(0, 0, frame, sizeof(frame)));
  commit();
  TEST_ASSERT_FALSE(claimed);
  TEST_ASSERT_EQUAL(1, aborted);
  TEST_ASSERT_EQUAL(0, completed);
  TEST_ASSERT_TRUE(tud_video_n_frame_xfer(0, 0, frame, sizeof(frame)));
  TEST_ASSERT_EQUAL(2, submitted);
  TEST_ASSERT_NOT_EQUAL(fid, payload[1] & 1);
}

// COMMIT is parsed only on setting 0 (see open_iso_stream()), where ISO_ALLOC keeps the endpoint
// but tud_video_n_streaming() is false. tud_video_n_frame_xfer() still accepts a frame there, which
// a compliant application does not queue; this pins only that COMMIT drains such a frame.
void test_video_iso_recommit_on_setting_0_drains_payload_before_releasing_frame(void) {
  open_iso_stream();
  TEST_ASSERT_FALSE(tud_video_n_streaming(0, 0));
  TEST_ASSERT_TRUE(tud_video_n_frame_xfer(0, 0, frame, sizeof(frame)));
  uint8_t queued[64];
  memcpy(queued, payload, payload_length);
  commit();
  TEST_ASSERT_EQUAL_UINT8_ARRAY(queued, payload, payload_length);
  TEST_ASSERT_EQUAL(0, aborted);
  TEST_ASSERT_FALSE(tud_video_n_frame_xfer(0, 0, frame, sizeof(frame)));

  claimed = false;
  TEST_ASSERT_TRUE(videod_xfer_cb(0, 0x81, XFER_RESULT_SUCCESS, payload_length));
  TEST_ASSERT_EQUAL(1, aborted);
  TEST_ASSERT_EQUAL(0, completed);
  TEST_ASSERT_EQUAL(1, submitted);
  TEST_ASSERT_TRUE(tud_video_n_frame_xfer(0, 0, frame, sizeof(frame)));
}

void test_video_set_interface_drops_queued_frame_once(void) {
  TEST_ASSERT_TRUE(tud_video_n_frame_xfer(0, 0, frame, sizeof(frame)));
  set_interface(0);
  unsigned const released = completed + aborted;
  TEST_ASSERT_EQUAL_MESSAGE(0, released, "current behaviour: SET_INTERFACE drops the frame with no callback (#4428)");

  // A completion queued before SET_INTERFACE neither continues nor releases the dropped frame.
  claimed = false;
  TEST_ASSERT_TRUE(videod_xfer_cb(0, 0x81, XFER_RESULT_SUCCESS, payload_length));
  TEST_ASSERT_EQUAL(released, completed + aborted);
  TEST_ASSERT_EQUAL(1, submitted);
  TEST_ASSERT_TRUE(tud_video_n_frame_xfer(0, 0, frame, sizeof(frame)));
}

// Built with a real mutex (TUP_MCU_MULTIPLE_CORE): a release callback run under the class lock
// spins on it until the fixture's lock watchdog fails the test.
void test_video_release_callbacks_may_submit_next_frame(void) {
  static uint8_t next[2] = {0x11, 0x22};
  resubmit_frame         = next;
  resubmit_size          = sizeof(next);
  deliver_frame();
  // The shorter frame starts from its own first byte, not the finished frame's offset.
  TEST_ASSERT_TRUE(resubmitted);
  uint8_t const hdr_len = payload[0];
  TEST_ASSERT_EQUAL(hdr_len + sizeof(next), payload_length);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(next, &payload[hdr_len], sizeof(next));
  TEST_ASSERT_TRUE(payload[1] & 2);

  resubmit_frame = frame;
  resubmit_size  = sizeof(frame);
  resubmitted    = false;
  claimed        = false;
  TEST_ASSERT_TRUE(videod_xfer_cb(0, 0x81, XFER_RESULT_FAILED, 0));
  TEST_ASSERT_EQUAL(1, aborted);
  TEST_ASSERT_TRUE(resubmitted);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(frame, &payload[hdr_len], payload_length - hdr_len);
  TEST_ASSERT_FALSE(payload[1] & 2);
}

void test_video_halt_abort_callback_may_submit(void) {
  TEST_ASSERT_TRUE(tud_video_n_frame_xfer(0, 0, frame, sizeof(frame)));
  resubmit_frame = frame;
  resubmit_size  = sizeof(frame);
  endpoint_halt(true);
  TEST_ASSERT_EQUAL(1, aborted);
  TEST_ASSERT_NULL(resubmit_frame);
  TEST_ASSERT_FALSE(resubmitted); // refused while the endpoint is halted
}

void test_video_recommit_abort_callback_may_submit(void) {
  TEST_ASSERT_TRUE(tud_video_n_frame_xfer(0, 0, frame, sizeof(frame)));
  claimed        = false; // the completion was dropped
  resubmit_frame = frame;
  resubmit_size  = sizeof(frame);
  commit();
  TEST_ASSERT_EQUAL(1, aborted);
  TEST_ASSERT_TRUE(resubmitted);
  TEST_ASSERT_EQUAL(2, submitted);
}

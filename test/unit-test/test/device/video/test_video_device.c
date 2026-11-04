/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 Adafruit Industries
 * SPDX-License-Identifier: MIT
 */
#include "unity.h"
#include "video_device_test.h"

TEST_SOURCE_FILE("video_device.c")

// One 16x16 YUY2 bulk stream, with no alternate interface. The small frame
// still needs multiple payloads, so a COMMIT can interrupt a real frame.
typedef struct TU_ATTR_PACKED {
  tusb_desc_interface_t                           vc;
  tusb_desc_video_control_header_1itf_t           vc_header;
  tusb_desc_video_control_camera_terminal_t       camera;
  tusb_desc_video_control_output_terminal_t       output;
  tusb_desc_interface_t                           vs;
  tusb_desc_video_streaming_input_header_1byte_t  vs_header;
  tusb_desc_video_format_uncompressed_t           format;
  tusb_desc_video_frame_uncompressed_continuous_t frame;
  tusb_desc_endpoint_t                            ep;
} video_fixture_t;

static const video_fixture_t descriptor =
  {.vc        = {.bLength            = sizeof(tusb_desc_interface_t),
                 .bDescriptorType    = TUSB_DESC_INTERFACE,
                 .bInterfaceClass    = TUSB_CLASS_VIDEO,
                 .bInterfaceSubClass = VIDEO_SUBCLASS_CONTROL,
                 .bInterfaceProtocol = VIDEO_ITF_PROTOCOL_15},
   .vc_header = {.bLength            = sizeof(tusb_desc_video_control_header_1itf_t),
                 .bDescriptorType    = TUSB_DESC_CS_INTERFACE,
                 .bDescriptorSubType = VIDEO_CS_ITF_VC_HEADER,
                 .bcdUVC             = VIDEO_BCD_1_50,
                 .wTotalLength       = offsetof(video_fixture_t, vs) - sizeof(tusb_desc_interface_t),
                 .bInCollection      = 1,
                 .baInterfaceNr      = {1}},
   .camera    = {.bLength            = sizeof(tusb_desc_video_control_camera_terminal_t),
                 .bDescriptorType    = TUSB_DESC_CS_INTERFACE,
                 .bDescriptorSubType = VIDEO_CS_ITF_VC_INPUT_TERMINAL,
                 .bTerminalID        = 1,
                 .wTerminalType      = VIDEO_ITT_CAMERA,
                 .bControlSize       = 3},
   .output    = {.bLength            = sizeof(tusb_desc_video_control_output_terminal_t),
                 .bDescriptorType    = TUSB_DESC_CS_INTERFACE,
                 .bDescriptorSubType = VIDEO_CS_ITF_VC_OUTPUT_TERMINAL,
                 .bTerminalID        = 2,
                 .wTerminalType      = VIDEO_TT_STREAMING,
                 .bSourceID          = 1},
   .vs        = {.bLength            = sizeof(tusb_desc_interface_t),
                 .bDescriptorType    = TUSB_DESC_INTERFACE,
                 .bInterfaceNumber   = 1,
                 .bNumEndpoints      = 1,
                 .bInterfaceClass    = TUSB_CLASS_VIDEO,
                 .bInterfaceSubClass = VIDEO_SUBCLASS_STREAMING,
                 .bInterfaceProtocol = VIDEO_ITF_PROTOCOL_15},
   .vs_header = {.bLength            = sizeof(tusb_desc_video_streaming_input_header_1byte_t),
                 .bDescriptorType    = TUSB_DESC_CS_INTERFACE,
                 .bDescriptorSubType = VIDEO_CS_ITF_VS_INPUT_HEADER,
                 .bNumFormats        = 1,
                 .wTotalLength       = offsetof(video_fixture_t, ep) - offsetof(video_fixture_t, vs_header),
                 .bEndpointAddress   = 0x81,
                 .bTerminalLink      = 2,
                 .bControlSize       = 1},
   .format    = {.bLength              = sizeof(tusb_desc_video_format_uncompressed_t),
                 .bDescriptorType      = TUSB_DESC_CS_INTERFACE,
                 .bDescriptorSubType   = VIDEO_CS_ITF_VS_FORMAT_UNCOMPRESSED,
                 .bFormatIndex         = 1,
                 .bNumFrameDescriptors = 1,
                 .guidFormat           = {TUD_VIDEO_GUID_YUY2},
                 .bBitsPerPixel        = 16,
                 .bDefaultFrameIndex   = 1},
   .frame     = {.bLength                   = sizeof(tusb_desc_video_frame_uncompressed_continuous_t),
                 .bDescriptorType           = TUSB_DESC_CS_INTERFACE,
                 .bDescriptorSubType        = VIDEO_CS_ITF_VS_FRAME_UNCOMPRESSED,
                 .bFrameIndex               = 1,
                 .wWidth                    = 16,
                 .wHeight                   = 16,
                 .dwMinBitRate              = 4096,
                 .dwMaxBitRate              = 40960,
                 .dwMaxVideoFrameBufferSize = 512,
                 .dwDefaultFrameInterval    = 1000000,
                 .dwFrameInterval           = {1000000, 10000000, 1000000}},
   .ep        = {.bLength          = sizeof(tusb_desc_endpoint_t),
                 .bDescriptorType  = TUSB_DESC_ENDPOINT,
                 .bEndpointAddress = 0x81,
                 .bmAttributes     = {.xfer = TUSB_XFER_BULK},
                 .wMaxPacketSize   = 64}};

static bool     claimed, accept_commit, in_commit, stalled, refuse_claim, refuse_xfer;
static unsigned completed, cancelled, submitted;
static uint8_t *payload;
static uint16_t payload_length;
static void    *control_buffer;
static uint8_t  frame[512];

bool tud_control_xfer(uint8_t rhport, const tusb_control_request_t *request, void *buffer, uint16_t length) {
  (void)rhport;
  (void)request;
  (void)length;
  control_buffer = buffer;
  return true;
}
bool tud_control_status(uint8_t rhport, const tusb_control_request_t *request) {
  (void)rhport;
  (void)request;
  return true;
}
bool usbd_edpt_open(uint8_t rhport, const tusb_desc_endpoint_t *ep) {
  (void)rhport;
  (void)ep;
  TEST_ASSERT_FALSE_MESSAGE(in_commit, "COMMIT must not reopen the bulk endpoint");
  return true;
}
bool usbd_edpt_claim(uint8_t rhport, uint8_t ep) {
  (void)rhport;
  (void)ep;
  if (claimed || refuse_claim) {
    return false;
  }
  claimed = true;
  return true;
}
bool usbd_edpt_release(uint8_t rhport, uint8_t ep) {
  (void)rhport;
  (void)ep;
  claimed = false;
  return true;
}
bool usbd_edpt_stalled(uint8_t rhport, uint8_t ep) {
  (void)rhport;
  (void)ep;
  return stalled;
}
bool usbd_edpt_xfer(uint8_t rhport, uint8_t ep, uint8_t *buffer, uint16_t length, bool is_isr) {
  (void)rhport;
  (void)ep;
  (void)is_isr;
  TEST_ASSERT_TRUE(claimed);
  if (refuse_xfer) {
    /* The core releases BUSY/CLAIMED on DCD refusal; no completion follows. */
    claimed = false;
    return false;
  }
  payload        = buffer;
  payload_length = length;
  submitted++;
  return true;
}
void usbd_edpt_close(uint8_t rhport, uint8_t ep) {
  (void)rhport;
  (void)ep;
  TEST_ASSERT_FALSE_MESSAGE(in_commit, "COMMIT must not close the bulk endpoint");
}
bool usbd_edpt_iso_alloc(uint8_t rhport, uint8_t ep, uint16_t size) {
  (void)rhport;
  (void)ep;
  (void)size;
  return true;
}
bool usbd_edpt_iso_activate(uint8_t rhport, const tusb_desc_endpoint_t *ep) {
  (void)rhport;
  (void)ep;
  return true;
}
void usbd_edpt_abort(uint8_t rhport, uint8_t ep) {
  (void)rhport;
  (void)ep;
  TEST_FAIL_MESSAGE("COMMIT must not abort or reset a bulk endpoint");
}
void tud_video_frame_xfer_complete_cb(uint_fast8_t ctl, uint_fast8_t stream) {
  (void)ctl;
  (void)stream;
  completed++;
}
void tud_video_frame_xfer_cancelled_cb(uint_fast8_t ctl, uint_fast8_t stream) {
  (void)ctl;
  (void)stream;
  cancelled++;
}
int tud_video_commit_cb(uint_fast8_t ctl, uint_fast8_t stream, const video_probe_and_commit_control_t *parameters) {
  (void)ctl;
  (void)stream;
  (void)parameters;
  return accept_commit ? VIDEO_ERROR_NONE : VIDEO_ERROR_WRONG_STATE;
}

static void commit(void) {
  in_commit                      = true;
  tusb_control_request_t request = {.bmRequestType = 0x21,
                                    .bRequest      = VIDEO_REQUEST_SET_CUR,
                                    .wValue        = VIDEO_VS_CTL_COMMIT << 8,
                                    .wIndex        = 1,
                                    .wLength       = sizeof(video_probe_and_commit_control_t)};
  TEST_ASSERT_TRUE(videod_control_xfer_cb(0, CONTROL_STAGE_SETUP, &request));
  video_probe_and_commit_control_t parameters = {.bFormatIndex             = 1,
                                                 .bFrameIndex              = 1,
                                                 .dwFrameInterval          = 1000000,
                                                 .dwMaxPayloadTransferSize = 64};
  memcpy(control_buffer, &parameters, sizeof(parameters));
  /* A rejected application COMMIT must protocol-stall, not ACK. */
  TEST_ASSERT_EQUAL(accept_commit, videod_control_xfer_cb(0, CONTROL_STAGE_DATA, &request));
  in_commit = false;
}

static void endpoint_halt(bool halt) {
  tusb_control_request_t request = {.bmRequestType = 0x02,
                                    .bRequest      = halt ? TUSB_REQ_SET_FEATURE : TUSB_REQ_CLEAR_FEATURE,
                                    .wValue        = TUSB_REQ_FEATURE_EDPT_HALT,
                                    .wIndex        = 0x81};
  /* Model the core handling the halt before notifying the class. A stalled
   * transfer is aborted without a completion and CLEAR releases its claim. */
  if (halt) {
    stalled = true;
  } else if (stalled) {
    stalled = false;
    claimed = false;
  }
  (void)videod_control_xfer_cb(0, CONTROL_STAGE_SETUP, &request);
}

void setUp(void) {
  claimed       = false;
  accept_commit = true;
  in_commit = stalled = refuse_claim = refuse_xfer = false;
  completed = cancelled = submitted = 0;
  payload                           = NULL;
  memset(frame, 0xa5, sizeof(frame));
  videod_init();
  TEST_ASSERT_EQUAL(sizeof(descriptor), videod_open(0, &descriptor.vc, sizeof(descriptor)));
  commit();
}
void tearDown(void) {
  videod_reset(0);
}

void test_video_idle_recommit_does_not_notify_or_touch_endpoint(void) {
  commit();
  TEST_ASSERT_EQUAL(0, completed);
  TEST_ASSERT_EQUAL(0, cancelled);
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
  TEST_ASSERT_EQUAL(0, cancelled);
  TEST_ASSERT_FALSE(tud_video_n_frame_xfer(0, 0, frame, sizeof(frame)));

  // The core clears its claim before dispatching the queued completion. Even
  // in that window, the class must not accept a newer application frame.
  claimed = false;
  TEST_ASSERT_FALSE(tud_video_n_frame_xfer(0, 0, frame, sizeof(frame)));
  TEST_ASSERT_TRUE(videod_xfer_cb(0, 0x81, XFER_RESULT_SUCCESS, payload_length));
  TEST_ASSERT_EQUAL(0, completed);
  TEST_ASSERT_EQUAL(1, cancelled);
  TEST_ASSERT_EQUAL(1, submitted); // no more payloads from the cancelled frame
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
  TEST_ASSERT_EQUAL(1, cancelled);
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
  TEST_ASSERT_EQUAL(0, cancelled);
  TEST_ASSERT_TRUE(tud_video_n_frame_xfer(0, 0, frame, sizeof(frame)));
}

void test_video_normal_frame_still_completes_once(void) {
  TEST_ASSERT_TRUE(tud_video_n_frame_xfer(0, 0, frame, sizeof(frame)));
  for (unsigned i = 0; i <= sizeof(frame) && !completed; ++i) {
    claimed = false;
    TEST_ASSERT_TRUE(videod_xfer_cb(0, 0x81, XFER_RESULT_SUCCESS, payload_length));
  }
  TEST_ASSERT_EQUAL(1, completed);
  TEST_ASSERT_EQUAL(0, cancelled);
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
  TEST_ASSERT_EQUAL(0, cancelled);
  TEST_ASSERT_EQUAL(2, submitted);
}

void test_video_refused_initial_payload_leaves_frame_with_caller(void) {
  refuse_xfer = true;
  TEST_ASSERT_FALSE(tud_video_n_frame_xfer(0, 0, frame, sizeof(frame)));
  commit();
  TEST_ASSERT_EQUAL(0, completed);
  TEST_ASSERT_EQUAL(0, cancelled);
  refuse_xfer = false;
  TEST_ASSERT_TRUE(tud_video_n_frame_xfer(0, 0, frame, sizeof(frame)));
  TEST_ASSERT_EQUAL(1, submitted);
}

void test_video_refused_continuation_releases_accepted_frame(void) {
  TEST_ASSERT_TRUE(tud_video_n_frame_xfer(0, 0, frame, sizeof(frame)));
  claimed     = false;
  refuse_xfer = true;
  TEST_ASSERT_FALSE(videod_xfer_cb(0, 0x81, XFER_RESULT_SUCCESS, payload_length));
  TEST_ASSERT_EQUAL(1, cancelled);
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
  TEST_ASSERT_EQUAL(1, cancelled);
  TEST_ASSERT_EQUAL(0, completed);
  commit();
  refuse_claim = false;
  TEST_ASSERT_TRUE(tud_video_n_frame_xfer(0, 0, frame, sizeof(frame)));
}

void test_video_failed_payload_cancels_instead_of_continuing(void) {
  TEST_ASSERT_TRUE(tud_video_n_frame_xfer(0, 0, frame, sizeof(frame)));
  claimed = false;
  TEST_ASSERT_TRUE(videod_xfer_cb(0, 0x81, XFER_RESULT_FAILED, 0));
  TEST_ASSERT_EQUAL(1, cancelled);
  TEST_ASSERT_EQUAL(0, completed);
  TEST_ASSERT_EQUAL(1, submitted);
  TEST_ASSERT_TRUE(tud_video_n_frame_xfer(0, 0, frame, sizeof(frame)));
}

void test_video_halt_releases_frame_without_completion(void) {
  TEST_ASSERT_TRUE(tud_video_n_frame_xfer(0, 0, frame, sizeof(frame)));
  endpoint_halt(true);
  TEST_ASSERT_EQUAL(1, cancelled);
  TEST_ASSERT_EQUAL(0, completed);
  TEST_ASSERT_FALSE(tud_video_n_frame_xfer(0, 0, frame, sizeof(frame)));
  commit();
  endpoint_halt(false);
  TEST_ASSERT_TRUE(tud_video_n_frame_xfer(0, 0, frame, sizeof(frame)));
  TEST_ASSERT_EQUAL(1, cancelled);
}

void test_video_halt_ignores_already_queued_completion(void) {
  TEST_ASSERT_TRUE(tud_video_n_frame_xfer(0, 0, frame, sizeof(frame)));
  endpoint_halt(true);
  claimed = false;
  TEST_ASSERT_TRUE(videod_xfer_cb(0, 0x81, XFER_RESULT_SUCCESS, payload_length));
  TEST_ASSERT_EQUAL(1, cancelled);
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
  TEST_ASSERT_EQUAL(0, cancelled);
  TEST_ASSERT_FALSE(tud_video_n_frame_xfer(0, 0, frame, sizeof(frame)));
  claimed = false;
  TEST_ASSERT_TRUE(videod_xfer_cb(0, 0x81, XFER_RESULT_SUCCESS, payload_length));
  TEST_ASSERT_EQUAL(2, submitted);
}

/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 Adafruit Industries
 * SPDX-License-Identifier: MIT
 */
#ifndef TUSB_VIDEO_DEVICE_TEST_H_
#define TUSB_VIDEO_DEVICE_TEST_H_

#ifdef VIDEO_DEVICE_TEST_FIXTURE
#include <signal.h>
#include <unistd.h>
#endif

#include "device/usbd.h"
#include "device/usbd_pvt.h"
#include "class/video/video_device.h"

// The usbd stubs and fixture shared by the video device tests. Each test file defines
// VIDEO_DEVICE_TEST_FIXTURE before including this header; the generated runner includes it
// without and gets only the declarations above.
#ifdef VIDEO_DEVICE_TEST_FIXTURE

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

// The same stream without endpoints in alternate setting 0 and an isochronous one in setting 1.
typedef struct TU_ATTR_PACKED {
  uint8_t               head[offsetof(video_fixture_t, ep)];
  tusb_desc_interface_t vs_alt1;
  tusb_desc_endpoint_t  ep;
} video_iso_fixture_t;

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

static video_iso_fixture_t iso_descriptor;

// `claimed` stands for the core's BUSY|CLAIMED bits of the stream endpoint.
static bool     claimed, accept_commit, in_commit, stalled, refuse_claim, refuse_xfer;
static unsigned completed, aborted, submitted;
static uint8_t *payload;
static uint16_t payload_length;
static void    *control_buffer;
static uint8_t  frame[512];
// A frame the next release callback submits, as an application refilling from that callback does.
static uint8_t *resubmit_frame;
static size_t   resubmit_size;
static bool     resubmitted;

static inline void resubmit_from_callback(void) {
  if (resubmit_frame) {
    resubmitted    = tud_video_n_frame_xfer(0, 0, resubmit_frame, resubmit_size);
    resubmit_frame = NULL;
  }
}

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
  TEST_ASSERT_FALSE_MESSAGE(in_commit, "COMMIT must not reactivate the isochronous endpoint");
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
  resubmit_from_callback();
}
int tud_video_commit_cb(uint_fast8_t ctl, uint_fast8_t stream, const video_probe_and_commit_control_t *parameters) {
  (void)ctl;
  (void)stream;
  (void)parameters;
  return accept_commit ? VIDEO_ERROR_NONE : VIDEO_ERROR_WRONG_STATE;
}

// The driver derives the payload size from the interval: 8 bytes at 100 ms, 3 bytes at 1 s.
static inline void commit_interval(uint32_t frame_interval) {
  in_commit                      = true;
  tusb_control_request_t request = {.bmRequestType = 0x21,
                                    .bRequest      = VIDEO_REQUEST_SET_CUR,
                                    .wValue        = VIDEO_VS_CTL_COMMIT << 8,
                                    .wIndex        = 1,
                                    .wLength       = sizeof(video_probe_and_commit_control_t)};
  TEST_ASSERT_TRUE(videod_control_xfer_cb(0, CONTROL_STAGE_SETUP, &request));
  video_probe_and_commit_control_t parameters = {.bFormatIndex             = 1,
                                                 .bFrameIndex              = 1,
                                                 .dwFrameInterval          = frame_interval,
                                                 .dwMaxPayloadTransferSize = 64};
  memcpy(control_buffer, &parameters, sizeof(parameters));
  /* A rejected application COMMIT must protocol-stall, not ACK. */
  TEST_ASSERT_EQUAL(accept_commit, videod_control_xfer_cb(0, CONTROL_STAGE_DATA, &request));
  in_commit = false;
}

static inline void commit(void) {
  commit_interval(1000000);
}

// Returns what the class reports for a standard SET/CLEAR_FEATURE(ENDPOINT_HALT).
static inline bool endpoint_request(bool halt, uint8_t ep_addr) {
  tusb_control_request_t request = {.bmRequestType = 0x02,
                                    .bRequest      = halt ? TUSB_REQ_SET_FEATURE : TUSB_REQ_CLEAR_FEATURE,
                                    .wValue        = TUSB_REQ_FEATURE_EDPT_HALT,
                                    .wIndex        = ep_addr};
  /* Model the core handling the halt before notifying the class. A stall sets BUSY and
   * aborts the transfer without a completion; clearing a stalled endpoint releases its claim. */
  if (ep_addr == 0x81) {
    if (halt) {
      stalled = true;
      claimed = true;
    } else if (stalled) {
      stalled = false;
      claimed = false;
    }
  }
  return videod_control_xfer_cb(0, CONTROL_STAGE_SETUP, &request);
}

static inline void endpoint_halt(bool halt) {
  (void)endpoint_request(halt, 0x81);
}

static inline void set_interface(uint8_t alt) {
  tusb_control_request_t request = {.bmRequestType = 0x01,
                                    .bRequest      = TUSB_REQ_SET_INTERFACE,
                                    .wValue        = alt,
                                    .wIndex        = 1};
  TEST_ASSERT_TRUE(videod_control_xfer_cb(0, CONTROL_STAGE_SETUP, &request));
}

// Completes queued payloads until the frame completes.
static inline void deliver_frame(void) {
  TEST_ASSERT_TRUE(tud_video_n_frame_xfer(0, 0, frame, sizeof(frame)));
  unsigned const done = completed;
  for (unsigned i = 0; i <= sizeof(frame) && completed == done; ++i) {
    claimed = false;
    TEST_ASSERT_TRUE(videod_xfer_cb(0, 0x81, XFER_RESULT_SUCCESS, payload_length));
  }
  TEST_ASSERT_EQUAL(done + 1, completed);
}

// Replaces the bulk stream by the isochronous one, streams once on alternate setting 1, then
// stops and commits again on setting 0. The driver parses COMMIT against the VS header of the
// current setting, which only setting 0 has; ISO_ALLOC keeps the stream's endpoint after the stop.
static inline void open_iso_stream(void) {
  videod_reset(0);
  memcpy(iso_descriptor.head, &descriptor, sizeof(iso_descriptor.head));
  ((tusb_desc_interface_t *)(iso_descriptor.head + offsetof(video_fixture_t, vs)))->bNumEndpoints = 0;
  iso_descriptor.vs_alt1                   = descriptor.vs;
  iso_descriptor.vs_alt1.bAlternateSetting = 1;
  iso_descriptor.ep                        = descriptor.ep;
  iso_descriptor.ep.bmAttributes.xfer      = TUSB_XFER_ISOCHRONOUS;
  iso_descriptor.ep.bmAttributes.sync      = 1; // asynchronous, UVC 1.5 Table 3-20
  iso_descriptor.ep.bInterval              = 1;
  TEST_ASSERT_EQUAL(sizeof(iso_descriptor), videod_open(0, (tusb_desc_interface_t const *)&iso_descriptor,
                                                        sizeof(iso_descriptor)));
  set_interface(0);
  commit();
  set_interface(1);
  set_interface(0);
  commit();
}

// osal_none's mutex spins forever when it is taken twice. Fail the running test instead of hanging
// the binary when a callback runs under the class lock or a path returns with it held.
static void lock_watchdog(int sig) {
  (void)sig;
  TEST_FAIL_MESSAGE("video class lock still held: a callback ran under it or a path leaked it");
}

static inline void video_test_setup(void) {
  // SA_NODEFER: Unity leaves the handler by longjmp, which would keep SIGALRM blocked.
  struct sigaction watchdog = {.sa_handler = lock_watchdog, .sa_flags = SA_NODEFER};
  TEST_ASSERT_EQUAL(0, sigemptyset(&watchdog.sa_mask));
  TEST_ASSERT_EQUAL(0, sigaction(SIGALRM, &watchdog, NULL));
  alarm(5);
  claimed       = false;
  accept_commit = true;
  in_commit = stalled = refuse_claim = refuse_xfer = false;
  completed = aborted = submitted = 0;
  payload                           = NULL;
  resubmit_frame                    = NULL;
  resubmitted                       = false;
  // No two-byte run repeats, so a payload taken from a stale offset does not match.
  for (size_t i = 0; i < sizeof(frame); ++i) {
    frame[i] = (uint8_t)(i ^ (i >> 8));
  }
  videod_init();
  TEST_ASSERT_EQUAL(sizeof(descriptor), videod_open(0, &descriptor.vc, sizeof(descriptor)));
  commit();
}

static inline void video_test_teardown(void) {
  // Takes and returns the class lock, so a path that returned with the lock still held fails here.
  // Skipped after a failure, which may have aborted with the lock held.
  if (!Unity.CurrentTestFailed) {
    TEST_ASSERT_FALSE(tud_video_n_frame_xfer(0, 0, frame, 0));
  }
  alarm(0);
  videod_reset(0);
}

#endif // VIDEO_DEVICE_TEST_FIXTURE

#endif

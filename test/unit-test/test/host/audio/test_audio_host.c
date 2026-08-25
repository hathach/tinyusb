/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 TinyUSB contributors
 * SPDX-License-Identifier: MIT
 */

#include "unity.h"
#include "audio_host_test.h"

TEST_SOURCE_FILE("audio_host.c")
TEST_SOURCE_FILE("tusb_fifo.c")

enum {
  AUDIO_DEV_ADDR  = 1,
  AUDIO_AC_ITF    = 0,
  AUDIO_AS_ITF    = 1,
  AUDIO_AS_IN_ITF = 2,

  PLAYBACK_INPUT_TERM  = 1,
  PLAYBACK_FU          = 2,
  PLAYBACK_OUTPUT_TERM = 3,

  CAPTURE_INPUT_TERM  = 11,
  CAPTURE_FU          = 12,
  CAPTURE_OUTPUT_TERM = 13,
};

static bool       interface_set_result;
static tuh_xfer_t interface_xfer;
static uint8_t    interface_alt;
static uint8_t    interface_set_count;

static bool                   control_xfer_result;
static tuh_xfer_t             control_xfer;
static tusb_control_request_t control_request;
static uint8_t                control_buffer[3];
static uint8_t                control_xfer_count;

static bool                 edpt_open_result;
static tusb_desc_endpoint_t opened_ep[4];
static uint8_t              edpt_open_count;

static bool    edpt_close_result;
static uint8_t closed_ep[4];
static uint8_t edpt_close_count;

static bool     edpt_busy;
static bool     edpt_xfer_result;
static uint16_t edpt_xfer_bytes[16];
static uint8_t  edpt_xfer_data[16][8];
static uint8_t *edpt_xfer_buffer[16];
static uint8_t  edpt_xfer_count;

static uint8_t  err_cb_count;
static uint8_t  err_cb_idx;
static uint8_t  err_cb_stream_idx;
static uint16_t err_cb_xferred_bytes;

void tuh_audio_err_cb(uint8_t idx, uint8_t stream_idx, uint16_t xferred_bytes) {
  err_cb_count++;
  err_cb_idx           = idx;
  err_cb_stream_idx    = stream_idx;
  err_cb_xferred_bytes = xferred_bytes;
}

tusb_speed_t tuh_speed_get(uint8_t daddr) {
  (void)daddr;
  return TUSB_SPEED_FULL;
}

bool tuh_control_xfer(tuh_xfer_t *xfer) {
  control_xfer_count++;
  control_request    = *xfer->setup;
  control_xfer       = *xfer;
  control_xfer.setup = &control_request;
  if (xfer->buffer != NULL) {
    memcpy(control_buffer, xfer->buffer, TU_MIN(sizeof(control_buffer), control_request.wLength));
  }
  xfer->result = XFER_RESULT_SUCCESS;
  return control_xfer_result;
}

bool tuh_edpt_open(uint8_t daddr, const tusb_desc_endpoint_t *desc_ep) {
  (void)daddr;
  if (edpt_open_count < TU_ARRAY_SIZE(opened_ep)) {
    opened_ep[edpt_open_count++] = *desc_ep;
  }
  return edpt_open_result;
}

bool tuh_edpt_close(uint8_t daddr, uint8_t ep_addr) {
  (void)daddr;
  if (edpt_close_count < TU_ARRAY_SIZE(closed_ep)) {
    closed_ep[edpt_close_count++] = ep_addr;
  }
  if (edpt_close_result) {
    edpt_busy = false;
  }
  return edpt_close_result;
}

bool tuh_interface_set(uint8_t daddr, uint8_t itf_num, uint8_t itf_alt, tuh_xfer_cb_t complete_cb,
                       uintptr_t user_data) {
  (void)daddr;
  (void)itf_num;
  interface_alt              = itf_alt;
  interface_xfer.daddr       = daddr;
  interface_xfer.ep_addr     = 0;
  interface_xfer.complete_cb = complete_cb;
  interface_xfer.user_data   = user_data;
  interface_set_count++;
  return interface_set_result;
}

bool usbh_edpt_xfer_with_callback(uint8_t dev_addr, uint8_t ep_addr, uint8_t *buffer, uint16_t total_bytes,
                                  tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  (void)dev_addr;
  (void)ep_addr;
  (void)complete_cb;
  (void)user_data;
  if (edpt_xfer_count < TU_ARRAY_SIZE(edpt_xfer_bytes)) {
    edpt_xfer_bytes[edpt_xfer_count]  = total_bytes;
    edpt_xfer_buffer[edpt_xfer_count] = buffer;
    memcpy(edpt_xfer_data[edpt_xfer_count], buffer, TU_MIN(sizeof(edpt_xfer_data[0]), total_bytes));
    edpt_xfer_count++;
  }
  return edpt_xfer_result;
}

bool usbh_edpt_claim(uint8_t dev_addr, uint8_t ep_addr) {
  (void)dev_addr;
  (void)ep_addr;
  if (edpt_busy) {
    return false;
  }
  edpt_busy = true;
  return true;
}

bool usbh_edpt_release(uint8_t dev_addr, uint8_t ep_addr) {
  (void)dev_addr;
  (void)ep_addr;
  edpt_busy = false;
  return true;
}

bool usbh_edpt_busy(uint8_t dev_addr, uint8_t ep_addr) {
  (void)dev_addr;
  (void)ep_addr;
  return edpt_busy;
}

void usbh_driver_set_config_complete(uint8_t dev_addr, uint8_t itf_num) {
  (void)dev_addr;
  (void)itf_num;
}

bool tu_edpt_stream_init(tu_edpt_stream_t *s, bool is_host, bool is_tx, bool overwritable, void *ff_buf,
                         uint16_t ff_bufsize, uint8_t *ep_buf) {
  (void)is_tx;
  s->is_host = is_host;
  s->ep_buf  = ep_buf;
  return tu_fifo_config(&s->ff, ff_buf, ff_bufsize, overwritable);
}

uint32_t tu_edpt_stream_write(tu_edpt_stream_t *s, const void *buffer, uint32_t bufsize) {
  (void)s;
  (void)buffer;
  (void)bufsize;
  return 0;
}

uint32_t tu_edpt_stream_write_available(tu_edpt_stream_t *s) {
  (void)s;
  return 0;
}

uint32_t tu_edpt_stream_read(tu_edpt_stream_t *s, void *buffer, uint32_t bufsize) {
  (void)s;
  (void)buffer;
  (void)bufsize;
  return 0;
}

uint32_t tu_edpt_stream_read_xfer(tu_edpt_stream_t *s) {
  (void)s;
  return 0;
}

#define TEST_UAC1_AC_HEADER                                                                                            \
  9, TUSB_DESC_INTERFACE, AUDIO_AC_ITF, 0, 0, TUSB_CLASS_AUDIO, AUDIO_SUBCLASS_CONTROL, AUDIO_INT_PROTOCOL_CODE_V1, 0, \
    9, TUSB_DESC_CS_INTERFACE, AUDIO10_CS_AC_INTERFACE_HEADER, 0x00, 0x01, 0x09, 0x00, 1, AUDIO_AS_ITF

#define TEST_UAC1_AC_HEADER_2                                                                                          \
  9, TUSB_DESC_INTERFACE, AUDIO_AC_ITF, 0, 0, TUSB_CLASS_AUDIO, AUDIO_SUBCLASS_CONTROL, AUDIO_INT_PROTOCOL_CODE_V1, 0, \
    10, TUSB_DESC_CS_INTERFACE, AUDIO10_CS_AC_INTERFACE_HEADER, 0x00, 0x01, 0x34, 0x00, 2, AUDIO_AS_ITF,               \
    AUDIO_AS_IN_ITF

#define TEST_UAC1_INPUT_TERM(_id, _type, _channels)                                                            \
  12, TUSB_DESC_CS_INTERFACE, AUDIO10_CS_AC_INTERFACE_INPUT_TERMINAL, _id, U16_TO_U8S_LE(_type), 0, _channels, \
    U16_TO_U8S_LE(AUDIO10_CHANNEL_CONFIG_NON_PREDEFINED), 0, 0

#define TEST_UAC1_FEATURE_UNIT(_id, _source_id)                                        \
  9, TUSB_DESC_CS_INTERFACE, AUDIO10_CS_AC_INTERFACE_FEATURE_UNIT, _id, _source_id, 2, \
    U16_TO_U8S_LE(AUDIO10_FU_CONTROL_BM_VOLUME), 0

#define TEST_UAC1_OUTPUT_TERM(_id, _type, _source_id)                                                             \
  9, TUSB_DESC_CS_INTERFACE, AUDIO10_CS_AC_INTERFACE_OUTPUT_TERMINAL, _id, U16_TO_U8S_LE(_type), 0, _source_id, 0

#define TEST_UAC1_AS_INTERFACE_NUM(_itf, _alt, _ep_count)                                    \
  9, TUSB_DESC_INTERFACE, _itf, _alt, _ep_count, TUSB_CLASS_AUDIO, AUDIO_SUBCLASS_STREAMING, \
    AUDIO_INT_PROTOCOL_CODE_V1, 0

#define TEST_UAC1_AS_INTERFACE(_alt, _ep_count) TEST_UAC1_AS_INTERFACE_NUM(AUDIO_AS_ITF, _alt, _ep_count)

#define TEST_UAC1_AS_ALT0                       TEST_UAC1_AS_INTERFACE(0, 0)

#define TEST_UAC1_AS_GENERAL(_term_id)                                        \
  7, TUSB_DESC_CS_INTERFACE, AUDIO10_CS_AS_INTERFACE_AS_GENERAL, _term_id, 1, \
    U16_TO_U8S_LE(AUDIO10_DATA_FORMAT_TYPE_I_PCM)

#define TEST_UAC1_FORMAT(_channels, _bytes, _bits, ...)                                            \
  (8 + 3 * TU_ARGS_NUM(__VA_ARGS__)), TUSB_DESC_CS_INTERFACE, AUDIO10_CS_AS_INTERFACE_FORMAT_TYPE, \
    AUDIO10_FORMAT_TYPE_I, _channels, _bytes, _bits, TU_ARGS_NUM(__VA_ARGS__),                     \
    TU_ARGS_APPLY_EXPAND(U24_TO_U8S_LE, __VA_ARGS__)

#define TEST_UAC1_DATA_EP_SYNC(_ep, _attr, _size, _interval, _sync_ep)                                        \
  9, TUSB_DESC_ENDPOINT, _ep, (TUSB_XFER_ISOCHRONOUS | (_attr)), U16_TO_U8S_LE(_size), _interval, 0, _sync_ep

#define TEST_UAC1_DATA_EP(_ep, _attr, _size, _interval) TEST_UAC1_DATA_EP_SYNC(_ep, _attr, _size, _interval, 0)

#define TEST_UAC1_CS_DATA_EP_ATTR(_attr)                                                                               \
  7, TUSB_DESC_CS_ENDPOINT, AUDIO10_CS_EP_SUBTYPE_GENERAL, _attr, AUDIO10_CS_AS_ISO_DATA_EP_LOCK_DELAY_UNIT_UNDEFINED, \
    0, 0

#define TEST_UAC1_CS_DATA_EP TEST_UAC1_CS_DATA_EP_ATTR(AUDIO10_CS_AS_ISO_DATA_EP_ATT_SAMPLING_FRQ)

static const uint8_t playback_with_explicit_feedback[] = {
  TEST_UAC1_AC_HEADER,
  TEST_UAC1_INPUT_TERM(PLAYBACK_INPUT_TERM, AUDIO_TERM_TYPE_USB_STREAMING, 2),
  TEST_UAC1_FEATURE_UNIT(PLAYBACK_FU, PLAYBACK_INPUT_TERM),
  TEST_UAC1_OUTPUT_TERM(PLAYBACK_OUTPUT_TERM, AUDIO_TERM_TYPE_OUT_HEADPHONES, PLAYBACK_FU),
  TEST_UAC1_AS_ALT0,
  TEST_UAC1_AS_INTERFACE(1, 2),
  TEST_UAC1_AS_GENERAL(PLAYBACK_INPUT_TERM),
  TEST_UAC1_FORMAT(2, 2, 16, 48000),
  TEST_UAC1_DATA_EP(0x01, TUSB_ISO_EP_ATT_ADAPTIVE, 192, 1),
  TEST_UAC1_CS_DATA_EP,
  TEST_UAC1_DATA_EP(0x81, TUSB_ISO_EP_ATT_EXPLICIT_FB, 3, 1),
};

static const uint8_t midi_only_collection[] = {
  TEST_UAC1_AC_HEADER,
  9,
  TUSB_DESC_INTERFACE,
  AUDIO_AS_ITF,
  0,
  0,
  TUSB_CLASS_AUDIO,
  AUDIO_SUBCLASS_MIDI_STREAMING,
  AUDIO_INT_PROTOCOL_CODE_V1,
  0,
};

static const uint8_t uac2_control_interface[] = {
  9, TUSB_DESC_INTERFACE, AUDIO_AC_ITF, 0, 0, TUSB_CLASS_AUDIO, AUDIO_SUBCLASS_CONTROL, AUDIO_INT_PROTOCOL_CODE_V2, 0,
};

static const uint8_t playback_with_implicit_feedback[] = {
  TEST_UAC1_AC_HEADER_2,
  TEST_UAC1_INPUT_TERM(PLAYBACK_INPUT_TERM, AUDIO_TERM_TYPE_USB_STREAMING, 2),
  TEST_UAC1_OUTPUT_TERM(PLAYBACK_OUTPUT_TERM, AUDIO_TERM_TYPE_OUT_HEADPHONES, PLAYBACK_INPUT_TERM),
  TEST_UAC1_INPUT_TERM(CAPTURE_INPUT_TERM, AUDIO_TERM_TYPE_IN_GENERIC_MIC, 1),
  TEST_UAC1_OUTPUT_TERM(CAPTURE_OUTPUT_TERM, AUDIO_TERM_TYPE_USB_STREAMING, CAPTURE_INPUT_TERM),
  TEST_UAC1_AS_ALT0,
  TEST_UAC1_AS_INTERFACE(1, 1),
  TEST_UAC1_AS_GENERAL(PLAYBACK_INPUT_TERM),
  TEST_UAC1_FORMAT(2, 2, 16, 48000),
  TEST_UAC1_DATA_EP_SYNC(0x01, TUSB_ISO_EP_ATT_ADAPTIVE, 192, 1, 0x81),
  TEST_UAC1_CS_DATA_EP,
  TEST_UAC1_AS_INTERFACE_NUM(AUDIO_AS_IN_ITF, 0, 0),
  TEST_UAC1_AS_INTERFACE_NUM(AUDIO_AS_IN_ITF, 1, 1),
  TEST_UAC1_AS_GENERAL(CAPTURE_OUTPUT_TERM),
  TEST_UAC1_FORMAT(1, 2, 16, 48000),
  TEST_UAC1_DATA_EP(0x81, TUSB_ISO_EP_ATT_IMPLICIT_FB, 96, 1),
  TEST_UAC1_CS_DATA_EP,
};

static const uint8_t playback_with_extra_data_endpoint[] = {
  TEST_UAC1_AC_HEADER,
  TEST_UAC1_INPUT_TERM(PLAYBACK_INPUT_TERM, AUDIO_TERM_TYPE_USB_STREAMING, 2),
  TEST_UAC1_OUTPUT_TERM(PLAYBACK_OUTPUT_TERM, AUDIO_TERM_TYPE_OUT_HEADPHONES, PLAYBACK_INPUT_TERM),
  TEST_UAC1_AS_ALT0,
  TEST_UAC1_AS_INTERFACE(1, 2),
  TEST_UAC1_AS_GENERAL(PLAYBACK_INPUT_TERM),
  TEST_UAC1_FORMAT(2, 2, 16, 48000),
  TEST_UAC1_DATA_EP(0x01, TUSB_ISO_EP_ATT_ADAPTIVE, 192, 1),
  TEST_UAC1_CS_DATA_EP,
  TEST_UAC1_DATA_EP(0x81, TUSB_ISO_EP_ATT_IMPLICIT_FB, 192, 1),
};

static const uint8_t playback_fu_before_terminal[] = {
  TEST_UAC1_AC_HEADER,
  TEST_UAC1_FEATURE_UNIT(PLAYBACK_FU, PLAYBACK_INPUT_TERM),
  TEST_UAC1_INPUT_TERM(PLAYBACK_INPUT_TERM, AUDIO_TERM_TYPE_USB_STREAMING, 2),
  TEST_UAC1_OUTPUT_TERM(PLAYBACK_OUTPUT_TERM, AUDIO_TERM_TYPE_OUT_HEADPHONES, PLAYBACK_FU),
  TEST_UAC1_AS_ALT0,
  TEST_UAC1_AS_INTERFACE(1, 1),
  TEST_UAC1_AS_GENERAL(PLAYBACK_INPUT_TERM),
  TEST_UAC1_FORMAT(2, 2, 16, 48000),
  TEST_UAC1_DATA_EP(0x01, TUSB_ISO_EP_ATT_ADAPTIVE, 192, 1),
  TEST_UAC1_CS_DATA_EP,
};

static const uint8_t capture_fu_before_usb_output[] = {
  TEST_UAC1_AC_HEADER,
  TEST_UAC1_INPUT_TERM(CAPTURE_INPUT_TERM, AUDIO_TERM_TYPE_IN_GENERIC_MIC, 1),
  TEST_UAC1_FEATURE_UNIT(CAPTURE_FU, CAPTURE_INPUT_TERM),
  TEST_UAC1_OUTPUT_TERM(CAPTURE_OUTPUT_TERM, AUDIO_TERM_TYPE_USB_STREAMING, CAPTURE_FU),
  TEST_UAC1_AS_ALT0,
  TEST_UAC1_AS_INTERFACE(1, 1),
  TEST_UAC1_AS_GENERAL(CAPTURE_OUTPUT_TERM),
  TEST_UAC1_FORMAT(1, 3, 24, 32000),
  TEST_UAC1_CS_DATA_EP,
  TEST_UAC1_DATA_EP(0x81, TUSB_ISO_EP_ATT_ASYNCHRONOUS, 96, 1),
};

static const uint8_t duplex_fus_before_usb_terminals[] = {
  TEST_UAC1_AC_HEADER_2,
  TEST_UAC1_FEATURE_UNIT(PLAYBACK_FU, PLAYBACK_INPUT_TERM),
  TEST_UAC1_FEATURE_UNIT(CAPTURE_FU, CAPTURE_INPUT_TERM),
  TEST_UAC1_INPUT_TERM(PLAYBACK_INPUT_TERM, AUDIO_TERM_TYPE_USB_STREAMING, 2),
  TEST_UAC1_OUTPUT_TERM(CAPTURE_OUTPUT_TERM, AUDIO_TERM_TYPE_USB_STREAMING, CAPTURE_FU),
  TEST_UAC1_OUTPUT_TERM(PLAYBACK_OUTPUT_TERM, AUDIO_TERM_TYPE_OUT_HEADPHONES, PLAYBACK_FU),
  TEST_UAC1_INPUT_TERM(CAPTURE_INPUT_TERM, AUDIO_TERM_TYPE_IN_GENERIC_MIC, 1),
  TEST_UAC1_AS_ALT0,
  TEST_UAC1_AS_INTERFACE(1, 1),
  TEST_UAC1_AS_GENERAL(PLAYBACK_INPUT_TERM),
  TEST_UAC1_FORMAT(2, 2, 16, 48000),
  TEST_UAC1_DATA_EP(0x01, TUSB_ISO_EP_ATT_ADAPTIVE, 192, 1),
  TEST_UAC1_CS_DATA_EP,
  TEST_UAC1_AS_INTERFACE_NUM(AUDIO_AS_IN_ITF, 0, 0),
  TEST_UAC1_AS_INTERFACE_NUM(AUDIO_AS_IN_ITF, 1, 1),
  TEST_UAC1_AS_GENERAL(CAPTURE_OUTPUT_TERM),
  TEST_UAC1_FORMAT(1, 2, 16, 48000),
  TEST_UAC1_DATA_EP(0x81, TUSB_ISO_EP_ATT_ASYNCHRONOUS, 96, 1),
  TEST_UAC1_CS_DATA_EP,
};

static const uint8_t playback_with_two_frequencies[] = {
  TEST_UAC1_AC_HEADER,
  TEST_UAC1_INPUT_TERM(PLAYBACK_INPUT_TERM, AUDIO_TERM_TYPE_USB_STREAMING, 2),
  TEST_UAC1_AS_ALT0,
  TEST_UAC1_AS_INTERFACE(1, 1),
  TEST_UAC1_AS_GENERAL(PLAYBACK_INPUT_TERM),
  TEST_UAC1_FORMAT(2, 2, 16, 44100, 48000),
  TEST_UAC1_DATA_EP(0x01, TUSB_ISO_EP_ATT_ADAPTIVE, 384, 2),
  TEST_UAC1_CS_DATA_EP,
};

static const uint8_t playback_with_frequency_range[] = {
  TEST_UAC1_AC_HEADER,
  TEST_UAC1_INPUT_TERM(PLAYBACK_INPUT_TERM, AUDIO_TERM_TYPE_USB_STREAMING, 2),
  TEST_UAC1_AS_ALT0,
  TEST_UAC1_AS_INTERFACE(1, 1),
  TEST_UAC1_AS_GENERAL(PLAYBACK_INPUT_TERM),
  14,
  TUSB_DESC_CS_INTERFACE,
  AUDIO10_CS_AS_INTERFACE_FORMAT_TYPE,
  AUDIO10_FORMAT_TYPE_I,
  2,
  2,
  16,
  0,
  U24_TO_U8S_LE(44100),
  U24_TO_U8S_LE(48000),
  TEST_UAC1_DATA_EP(0x01, TUSB_ISO_EP_ATT_ADAPTIVE, 192, 1),
  TEST_UAC1_CS_DATA_EP,
};

static const uint8_t capture_with_large_channel_count[] = {
  TEST_UAC1_AC_HEADER,
  TEST_UAC1_OUTPUT_TERM(CAPTURE_OUTPUT_TERM, AUDIO_TERM_TYPE_USB_STREAMING, CAPTURE_INPUT_TERM),
  TEST_UAC1_AS_ALT0,
  TEST_UAC1_AS_INTERFACE(1, 1),
  TEST_UAC1_AS_GENERAL(CAPTURE_OUTPUT_TERM),
  TEST_UAC1_FORMAT(255, 4, 32, 100),
  TEST_UAC1_DATA_EP(0x81, TUSB_ISO_EP_ATT_ASYNCHRONOUS, 500, 1),
  TEST_UAC1_CS_DATA_EP,
};

static const uint8_t playback_with_cs_ep_before_data_ep[] = {
  TEST_UAC1_AC_HEADER,
  TEST_UAC1_INPUT_TERM(PLAYBACK_INPUT_TERM, AUDIO_TERM_TYPE_USB_STREAMING, 2),
  TEST_UAC1_AS_ALT0,
  TEST_UAC1_AS_INTERFACE(1, 1),
  TEST_UAC1_AS_GENERAL(PLAYBACK_INPUT_TERM),
  TEST_UAC1_FORMAT(2, 2, 16, 48000),
  TEST_UAC1_CS_DATA_EP,
  TEST_UAC1_DATA_EP(0x01, TUSB_ISO_EP_ATT_ADAPTIVE, 192, 1),
};

static const uint8_t playback_with_two_alternates[] = {
  TEST_UAC1_AC_HEADER,
  TEST_UAC1_INPUT_TERM(PLAYBACK_INPUT_TERM, AUDIO_TERM_TYPE_USB_STREAMING, 2),
  TEST_UAC1_AS_ALT0,
  TEST_UAC1_AS_INTERFACE(1, 1),
  TEST_UAC1_AS_GENERAL(PLAYBACK_INPUT_TERM),
  TEST_UAC1_FORMAT(2, 2, 16, 48000),
  TEST_UAC1_DATA_EP(0x01, TUSB_ISO_EP_ATT_ADAPTIVE, 192, 1),
  TEST_UAC1_CS_DATA_EP,
  TEST_UAC1_AS_INTERFACE(2, 1),
  TEST_UAC1_AS_GENERAL(PLAYBACK_INPUT_TERM),
  TEST_UAC1_FORMAT(2, 2, 16, 96000),
  TEST_UAC1_DATA_EP(0x02, TUSB_ISO_EP_ATT_ADAPTIVE, 384, 1),
  TEST_UAC1_CS_DATA_EP,
};

static const uint8_t playback_44100_max_packets_only[] = {
  TEST_UAC1_AC_HEADER,
  TEST_UAC1_INPUT_TERM(PLAYBACK_INPUT_TERM, AUDIO_TERM_TYPE_USB_STREAMING, 2),
  TEST_UAC1_AS_ALT0,
  TEST_UAC1_AS_INTERFACE(1, 1),
  TEST_UAC1_AS_GENERAL(PLAYBACK_INPUT_TERM),
  TEST_UAC1_FORMAT(2, 2, 16, 44100),
  TEST_UAC1_DATA_EP(0x01, TUSB_ISO_EP_ATT_ADAPTIVE, 180, 1),
  TEST_UAC1_CS_DATA_EP_ATTR(AUDIO10_CS_AS_ISO_DATA_EP_ATT_MAX_PACKETS_ONLY),
};

static uint8_t   fu_cb_count;
static uintptr_t fu_cb_user_data;

static void feature_unit_complete(tuh_xfer_t *xfer) {
  fu_cb_count++;
  fu_cb_user_data = xfer->user_data;
}

static void complete_interface_set(tusb_xfer_result_t result) {
  tuh_xfer_t xfer            = interface_xfer;
  interface_xfer.complete_cb = NULL;
  xfer.result                = result;
  TEST_ASSERT_NOT_NULL(xfer.complete_cb);
  xfer.complete_cb(&xfer);
}

static void complete_control_xfer(tusb_xfer_result_t result) {
  tuh_xfer_t xfer          = control_xfer;
  control_xfer.complete_cb = NULL;
  xfer.result              = result;
  TEST_ASSERT_NOT_NULL(xfer.complete_cb);
  xfer.complete_cb(&xfer);
}

void setUp(void) {
  interface_set_result = true;
  memset(&interface_xfer, 0, sizeof(interface_xfer));
  interface_alt       = 0;
  interface_set_count = 0;

  control_xfer_result = true;
  memset(&control_xfer, 0, sizeof(control_xfer));
  memset(&control_request, 0, sizeof(control_request));
  memset(control_buffer, 0, sizeof(control_buffer));
  control_xfer_count = 0;

  edpt_open_result = true;
  memset(opened_ep, 0, sizeof(opened_ep));
  edpt_open_count = 0;

  edpt_close_result = true;
  memset(closed_ep, 0, sizeof(closed_ep));
  edpt_close_count = 0;

  edpt_busy        = false;
  edpt_xfer_result = true;
  memset(edpt_xfer_bytes, 0, sizeof(edpt_xfer_bytes));
  memset(edpt_xfer_data, 0, sizeof(edpt_xfer_data));
  memset(edpt_xfer_buffer, 0, sizeof(edpt_xfer_buffer));
  edpt_xfer_count = 0;

  err_cb_count         = 0;
  err_cb_idx           = TUSB_INDEX_INVALID_8;
  err_cb_stream_idx    = TUSB_INDEX_INVALID_8;
  err_cb_xferred_bytes = 0;

  fu_cb_count     = 0;
  fu_cb_user_data = 0;

  TEST_ASSERT_TRUE(audioh_init());
}

void tearDown(void) {
  TEST_ASSERT_TRUE(audioh_deinit());
}

static void open_descriptors(const uint8_t *desc, uint16_t desc_len) {
  TEST_ASSERT_EQUAL_UINT16(desc_len, audioh_open(0, AUDIO_DEV_ADDR, (const tusb_desc_interface_t *)desc, desc_len));
}

static void mount_descriptors(const uint8_t *desc, uint16_t desc_len) {
  open_descriptors(desc, desc_len);
  TEST_ASSERT_TRUE(audioh_set_config(AUDIO_DEV_ADDR, AUDIO_AC_ITF));
  TEST_ASSERT_TRUE(tuh_audio_mounted(0));
}

void test_audio_host_ignores_explicit_feedback_endpoint(void) {
  // A MIDI-only AC collection must not consume an Audio Host instance.
  TEST_ASSERT_EQUAL_UINT16(0, audioh_open(0, AUDIO_DEV_ADDR, (const tusb_desc_interface_t *)midi_only_collection,
                                          sizeof(midi_only_collection)));
  TEST_ASSERT_EQUAL_UINT8(0, tuh_audio_get_dev_addr(0));

  open_descriptors(playback_with_explicit_feedback, sizeof(playback_with_explicit_feedback));

  TEST_ASSERT_EQUAL_UINT8(1, tuh_audio_stream_count(0));
  TEST_ASSERT_EQUAL(TUH_AUDIO_STREAM_PLAYBACK, tuh_audio_stream_direction(0, 0));
  TEST_ASSERT_EQUAL_UINT8(1, tuh_audio_config_count(0, 0));
}

void test_audio_host_rejects_uac2_interface_without_consuming_instance(void) {
  TEST_ASSERT_EQUAL_UINT16(0, audioh_open(0, AUDIO_DEV_ADDR, (const tusb_desc_interface_t *)uac2_control_interface,
                                          sizeof(uac2_control_interface)));
  TEST_ASSERT_EQUAL_UINT8(0, tuh_audio_get_dev_addr(0));

  open_descriptors(playback_with_explicit_feedback, sizeof(playback_with_explicit_feedback));
  TEST_ASSERT_EQUAL_UINT8(1, tuh_audio_stream_count(0));
}

void test_audio_host_treats_implicit_feedback_as_audio_in_endpoint(void) {
  open_descriptors(playback_with_implicit_feedback, sizeof(playback_with_implicit_feedback));

  TEST_ASSERT_EQUAL_UINT8(2, tuh_audio_stream_count(0));
  TEST_ASSERT_EQUAL(TUH_AUDIO_STREAM_PLAYBACK, tuh_audio_stream_direction(0, 0));
  TEST_ASSERT_EQUAL(TUH_AUDIO_STREAM_CAPTURE, tuh_audio_stream_direction(0, 1));
}

void test_audio_host_ignores_second_data_endpoint_in_same_as_interface(void) {
  open_descriptors(playback_with_extra_data_endpoint, sizeof(playback_with_extra_data_endpoint));

  TEST_ASSERT_EQUAL_UINT8(1, tuh_audio_stream_count(0));
  TEST_ASSERT_EQUAL(TUH_AUDIO_STREAM_PLAYBACK, tuh_audio_stream_direction(0, 0));
}

void test_audio_host_maps_playback_fu_declared_before_usb_input_terminal(void) {
  open_descriptors(playback_fu_before_terminal, sizeof(playback_fu_before_terminal));

  TEST_ASSERT_EQUAL_UINT8(PLAYBACK_FU, tuh_audio_get_feature_unit_id(0, 0));
}

void test_audio_host_maps_capture_fu_declared_before_usb_output_terminal(void) {
  uint8_t        captured[CFG_TUH_AUDIO_STREAM_BUFSIZE];
  const uint16_t fifo_depth = CFG_TUH_AUDIO_STREAM_BUFSIZE - (CFG_TUH_AUDIO_STREAM_BUFSIZE % 3);
  mount_descriptors(capture_fu_before_usb_output, sizeof(capture_fu_before_usb_output));

  TEST_ASSERT_EQUAL(TUH_AUDIO_STREAM_CAPTURE, tuh_audio_stream_direction(0, 0));
  TEST_ASSERT_EQUAL_UINT8(CAPTURE_FU, tuh_audio_get_feature_unit_id(0, 0));

  TEST_ASSERT_TRUE(tuh_audio_configure(0, 0, 0));
  TEST_ASSERT_TRUE(tuh_audio_start(0, 0));
  complete_interface_set(XFER_RESULT_SUCCESS);
  complete_control_xfer(XFER_RESULT_SUCCESS);
  TEST_ASSERT_EQUAL_UINT8(1, edpt_xfer_count);

  // Keep polling without application reads. Once full, the capture FIFO must
  // overwrite its oldest frames while retaining the newest complete packets.
  for (uint8_t packet = 0; packet < 11; packet++) {
    TEST_ASSERT_NOT_NULL(edpt_xfer_buffer[packet]);
    memset(edpt_xfer_buffer[packet], packet + 1, 96);
    edpt_busy = false;
    TEST_ASSERT_TRUE(audioh_xfer_cb(AUDIO_DEV_ADDR, 0x81, XFER_RESULT_SUCCESS, 96));
    TEST_ASSERT_EQUAL_UINT8(packet + 2, edpt_xfer_count);
  }

  TEST_ASSERT_EQUAL_UINT32(fifo_depth / 3, tuh_audio_read_available(0, 0));
  TEST_ASSERT_EQUAL_UINT32(fifo_depth / 3, tuh_audio_read(0, 0, captured, fifo_depth / 3));
  TEST_ASSERT_EACH_EQUAL_UINT8(1, captured, 63);
  TEST_ASSERT_EACH_EQUAL_UINT8(2, captured + 63, 96);
  TEST_ASSERT_EACH_EQUAL_UINT8(11, captured + fifo_depth - 96, 96);
}

void test_audio_host_maps_duplex_fus_declared_before_usb_terminals(void) {
  open_descriptors(duplex_fus_before_usb_terminals, sizeof(duplex_fus_before_usb_terminals));

  TEST_ASSERT_EQUAL_UINT8(2, tuh_audio_stream_count(0));
  TEST_ASSERT_EQUAL_UINT8(PLAYBACK_FU, tuh_audio_get_feature_unit_id(0, 0));
  TEST_ASSERT_EQUAL_UINT8(CAPTURE_FU, tuh_audio_get_feature_unit_id(0, 1));
}

void test_audio_host_sets_sampling_frequency_after_each_stream_activation(void) {
  mount_descriptors(capture_fu_before_usb_output, sizeof(capture_fu_before_usb_output));

  TEST_ASSERT_TRUE(tuh_audio_configure(0, 0, 0));
  TEST_ASSERT_EQUAL_UINT8(0, control_xfer_count);
  TEST_ASSERT_EQUAL_UINT8(0, interface_set_count);

  TEST_ASSERT_TRUE(tuh_audio_start(0, 0));
  TEST_ASSERT_EQUAL_UINT8(1, interface_set_count);
  complete_interface_set(XFER_RESULT_SUCCESS);
  TEST_ASSERT_EQUAL_UINT8(1, control_xfer_count);
  TEST_ASSERT_EQUAL_UINT8(0, edpt_xfer_count);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(((uint8_t[]){U24_TO_U8S_LE(32000)}), control_buffer, 3);
  complete_control_xfer(XFER_RESULT_SUCCESS);
  TEST_ASSERT_EQUAL_UINT8(1, edpt_xfer_count);

  TEST_ASSERT_TRUE(tuh_audio_stop(0, 0));
  TEST_ASSERT_EQUAL_UINT8(0, interface_alt);
  TEST_ASSERT_EQUAL_UINT8(2, interface_set_count);
  complete_interface_set(XFER_RESULT_SUCCESS);
  edpt_busy = false;
  TEST_ASSERT_TRUE(audioh_xfer_cb(AUDIO_DEV_ADDR, 0x81, XFER_RESULT_SUCCESS, 96));

  TEST_ASSERT_TRUE(tuh_audio_start(0, 0));
  TEST_ASSERT_EQUAL_UINT8(1, interface_alt);
  TEST_ASSERT_EQUAL_UINT8(3, interface_set_count);
  complete_interface_set(XFER_RESULT_SUCCESS);
  TEST_ASSERT_EQUAL_UINT8(2, control_xfer_count);
  TEST_ASSERT_EQUAL_UINT8(1, edpt_xfer_count);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(((uint8_t[]){U24_TO_U8S_LE(32000)}), control_buffer, 3);
  complete_control_xfer(XFER_RESULT_SUCCESS);
  TEST_ASSERT_EQUAL_UINT8(2, edpt_xfer_count);
}

void test_audio_host_reports_asynchronous_start_failures(void) {
  mount_descriptors(capture_fu_before_usb_output, sizeof(capture_fu_before_usb_output));
  TEST_ASSERT_TRUE(tuh_audio_configure(0, 0, 0));

  TEST_ASSERT_TRUE(tuh_audio_start(0, 0));
  complete_interface_set(XFER_RESULT_FAILED);
  TEST_ASSERT_EQUAL_UINT8(1, err_cb_count);
  TEST_ASSERT_EQUAL_UINT8(0, err_cb_idx);
  TEST_ASSERT_EQUAL_UINT8(0, err_cb_stream_idx);
  TEST_ASSERT_EQUAL_UINT16(0, err_cb_xferred_bytes);

  TEST_ASSERT_TRUE(tuh_audio_start(0, 0));
  complete_interface_set(XFER_RESULT_SUCCESS);
  complete_control_xfer(XFER_RESULT_STALLED);
  TEST_ASSERT_EQUAL_UINT8(2, err_cb_count);

  control_xfer_result = false;
  TEST_ASSERT_TRUE(tuh_audio_start(0, 0));
  complete_interface_set(XFER_RESULT_SUCCESS);
  TEST_ASSERT_EQUAL_UINT8(3, err_cb_count);
}

void test_audio_host_keeps_running_when_stop_cannot_be_submitted(void) {
  mount_descriptors(capture_fu_before_usb_output, sizeof(capture_fu_before_usb_output));
  TEST_ASSERT_TRUE(tuh_audio_configure(0, 0, 0));
  TEST_ASSERT_TRUE(tuh_audio_start(0, 0));
  complete_interface_set(XFER_RESULT_SUCCESS);
  complete_control_xfer(XFER_RESULT_SUCCESS);
  TEST_ASSERT_EQUAL_UINT8(1, edpt_xfer_count);

  interface_set_result = false;
  TEST_ASSERT_FALSE(tuh_audio_stop(0, 0));

  edpt_busy = false;
  TEST_ASSERT_TRUE(audioh_xfer_cb(AUDIO_DEV_ADDR, 0x81, XFER_RESULT_SUCCESS, 96));
  TEST_ASSERT_EQUAL_UINT8(2, edpt_xfer_count);
}

void test_audio_host_parses_discrete_frequencies_with_interval_greater_than_one(void) {
  tuh_audio_stream_config_t config;
  open_descriptors(playback_with_two_frequencies, sizeof(playback_with_two_frequencies));

  TEST_ASSERT_EQUAL_UINT8(2, tuh_audio_config_count(0, 0));
  TEST_ASSERT_TRUE(tuh_audio_config_get(0, 0, 0, &config));
  TEST_ASSERT_EQUAL_UINT32(44100, config.sample_rate);
  TEST_ASSERT_TRUE(tuh_audio_config_get(0, 0, 1, &config));
  TEST_ASSERT_EQUAL_UINT32(48000, config.sample_rate);
}

void test_audio_host_rejects_sampling_frequency_range(void) {
  open_descriptors(playback_with_frequency_range, sizeof(playback_with_frequency_range));

  TEST_ASSERT_EQUAL_UINT8(0, tuh_audio_stream_count(0));
}

void test_audio_host_rejects_overflowed_frame_size_for_large_channel_count(void) {
  open_descriptors(capture_with_large_channel_count, sizeof(capture_with_large_channel_count));

  TEST_ASSERT_EQUAL_UINT8(0, tuh_audio_stream_count(0));
}

void test_audio_host_uses_cs_endpoint_declared_before_data_endpoint_on_start(void) {
  mount_descriptors(playback_with_cs_ep_before_data_ep, sizeof(playback_with_cs_ep_before_data_ep));

  TEST_ASSERT_TRUE(tuh_audio_configure(0, 0, 0));
  TEST_ASSERT_EQUAL_UINT8(0, control_xfer_count);

  TEST_ASSERT_TRUE(tuh_audio_start(0, 0));
  TEST_ASSERT_EQUAL_UINT8(1, interface_set_count);
  complete_interface_set(XFER_RESULT_SUCCESS);
  TEST_ASSERT_EQUAL_UINT8(1, control_xfer_count);
  TEST_ASSERT_EQUAL_UINT16(3, tu_le16toh(control_request.wLength));
  TEST_ASSERT_EQUAL_UINT16(0x01, tu_le16toh(control_request.wIndex));
  complete_control_xfer(XFER_RESULT_SUCCESS);
}

void test_audio_host_closes_old_endpoint_and_cleans_up_failed_reconfiguration(void) {
  mount_descriptors(playback_with_two_alternates, sizeof(playback_with_two_alternates));

  TEST_ASSERT_TRUE(tuh_audio_configure(0, 0, 0));
  TEST_ASSERT_EQUAL_UINT8(1, edpt_open_count);
  TEST_ASSERT_EQUAL_HEX8(0x01, opened_ep[0].bEndpointAddress);
  TEST_ASSERT_EQUAL_UINT16(192, tu_edpt_packet_size(&opened_ep[0]));

  TEST_ASSERT_TRUE(tuh_audio_configure(0, 0, 1));
  TEST_ASSERT_EQUAL_UINT8(1, edpt_close_count);
  TEST_ASSERT_EQUAL_HEX8(0x01, closed_ep[0]);
  TEST_ASSERT_EQUAL_UINT8(2, edpt_open_count);
  TEST_ASSERT_EQUAL_HEX8(0x02, opened_ep[1].bEndpointAddress);
  TEST_ASSERT_EQUAL_UINT16(384, tu_edpt_packet_size(&opened_ep[1]));

  edpt_open_result = false;
  TEST_ASSERT_FALSE(tuh_audio_configure(0, 0, 0));
  TEST_ASSERT_EQUAL_UINT8(2, edpt_close_count);
  TEST_ASSERT_EQUAL_HEX8(0x02, closed_ep[1]);

  TEST_ASSERT_EQUAL_UINT8(3, edpt_open_count);
  TEST_ASSERT_EQUAL_UINT8(TUSB_INDEX_INVALID_8, tuh_audio_active_config(0, 0));
}

void test_audio_host_uses_correct_feature_unit_widths_and_serializes_requests(void) {
  static const uint8_t one_byte_controls[] = {
    AUDIO10_FU_CTRL_MUTE, AUDIO10_FU_CTRL_BASS,       AUDIO10_FU_CTRL_MID,      AUDIO10_FU_CTRL_TREBLE,
    AUDIO10_FU_CTRL_AGC,  AUDIO10_FU_CTRL_BASS_BOOST, AUDIO10_FU_CTRL_LOUDNESS,
  };
  static const uint8_t two_byte_controls[] = {AUDIO10_FU_CTRL_VOLUME, AUDIO10_FU_CTRL_DELAY};

  mount_descriptors(playback_fu_before_terminal, sizeof(playback_fu_before_terminal));

  for (uint8_t i = 0; i < TU_ARRAY_SIZE(one_byte_controls); i++) {
    TEST_ASSERT_TRUE(tuh_audio_feature_unit_set(0, 0, one_byte_controls[i], 0, 0x1234, NULL, 0));
    TEST_ASSERT_EQUAL_UINT16(1, tu_le16toh(control_request.wLength));
    TEST_ASSERT_EQUAL_HEX8(0x34, control_buffer[0]);
  }
  for (uint8_t i = 0; i < TU_ARRAY_SIZE(two_byte_controls); i++) {
    TEST_ASSERT_TRUE(tuh_audio_feature_unit_set(0, 0, two_byte_controls[i], 0, 0x1234, NULL, 0));
    TEST_ASSERT_EQUAL_UINT16(2, tu_le16toh(control_request.wLength));
    TEST_ASSERT_EQUAL_HEX8(0x34, control_buffer[0]);
    TEST_ASSERT_EQUAL_HEX8(0x12, control_buffer[1]);
  }
  TEST_ASSERT_FALSE(tuh_audio_feature_unit_set(0, 0, AUDIO10_FU_CTRL_GRAPHIC_EQUALIZER, 0, 0, NULL, 0));

  const uint8_t previous_xfer_count = control_xfer_count;
  TEST_ASSERT_TRUE(tuh_audio_feature_unit_set(0, 0, AUDIO10_FU_CTRL_MUTE, 0, 1, feature_unit_complete, 0x1234));
  TEST_ASSERT_FALSE(tuh_audio_feature_unit_set(0, 0, AUDIO10_FU_CTRL_VOLUME, 0, 2, feature_unit_complete, 0x5678));
  TEST_ASSERT_EQUAL_UINT8(previous_xfer_count + 1, control_xfer_count);

  complete_control_xfer(XFER_RESULT_SUCCESS);
  TEST_ASSERT_EQUAL_UINT8(1, fu_cb_count);
  TEST_ASSERT_EQUAL_HEX32(0x1234, fu_cb_user_data);
  TEST_ASSERT_TRUE(tuh_audio_feature_unit_set(0, 0, AUDIO10_FU_CTRL_VOLUME, 0, 2, feature_unit_complete, 0x5678));
  complete_control_xfer(XFER_RESULT_SUCCESS);
  TEST_ASSERT_EQUAL_UINT8(2, fu_cb_count);
  TEST_ASSERT_EQUAL_HEX32(0x5678, fu_cb_user_data);
}

void test_audio_host_schedules_44100_hz_fractional_packets_with_max_packets_only(void) {
  uint8_t samples[441 * 4] = {0};
  mount_descriptors(playback_44100_max_packets_only, sizeof(playback_44100_max_packets_only));

  TEST_ASSERT_TRUE(tuh_audio_configure(0, 0, 0));
  TEST_ASSERT_EQUAL_UINT8(0, control_xfer_count);

  TEST_ASSERT_TRUE(tuh_audio_start(0, 0));
  TEST_ASSERT_EQUAL_UINT8(1, interface_alt);
  TEST_ASSERT_EQUAL_UINT8(1, interface_set_count);
  TEST_ASSERT_EQUAL_UINT32(256, tuh_audio_write(0, 0, samples, 256));
  TEST_ASSERT_EQUAL_UINT8(0, edpt_xfer_count);
  complete_interface_set(XFER_RESULT_SUCCESS);
  TEST_ASSERT_EQUAL_UINT8(1, edpt_xfer_count);

  while (edpt_xfer_count < 5) {
    edpt_busy = false;
    TEST_ASSERT_TRUE(audioh_xfer_cb(AUDIO_DEV_ADDR, 0x01, XFER_RESULT_SUCCESS, edpt_xfer_bytes[edpt_xfer_count - 1]));
  }
  TEST_ASSERT_EQUAL_UINT32(185, tuh_audio_write(0, 0, samples, 185));
  while (edpt_xfer_count < 10) {
    edpt_busy = false;
    TEST_ASSERT_TRUE(audioh_xfer_cb(AUDIO_DEV_ADDR, 0x01, XFER_RESULT_SUCCESS, edpt_xfer_bytes[edpt_xfer_count - 1]));
  }

  for (uint8_t i = 0; i < 9; i++) {
    TEST_ASSERT_EQUAL_UINT16(176, edpt_xfer_bytes[i]);
  }
  TEST_ASSERT_EQUAL_UINT16(180, edpt_xfer_bytes[9]);
}

void test_audio_host_sends_silence_when_playback_fifo_has_too_few_frames(void) {
  uint8_t sample[4] = {1, 2, 3, 4};
  uint8_t more_samples[43 * 4];
  memset(more_samples, 0x55, sizeof(more_samples));
  mount_descriptors(playback_44100_max_packets_only, sizeof(playback_44100_max_packets_only));

  TEST_ASSERT_TRUE(tuh_audio_configure(0, 0, 0));
  TEST_ASSERT_TRUE(tuh_audio_start(0, 0));
  complete_interface_set(XFER_RESULT_SUCCESS);

  TEST_ASSERT_EQUAL_UINT8(1, edpt_xfer_count);
  TEST_ASSERT_EQUAL_UINT16(176, edpt_xfer_bytes[0]);
  TEST_ASSERT_EACH_EQUAL_HEX8(0, edpt_xfer_data[0], sizeof(edpt_xfer_data[0]));

  TEST_ASSERT_EQUAL_UINT32(1, tuh_audio_write(0, 0, sample, 1));
  edpt_busy = false;
  TEST_ASSERT_TRUE(audioh_xfer_cb(AUDIO_DEV_ADDR, 0x01, XFER_RESULT_SUCCESS, edpt_xfer_bytes[0]));

  TEST_ASSERT_EQUAL_UINT8(2, edpt_xfer_count);
  TEST_ASSERT_EQUAL_UINT16(176, edpt_xfer_bytes[1]);
  TEST_ASSERT_EACH_EQUAL_HEX8(0, edpt_xfer_data[1], sizeof(edpt_xfer_data[1]));

  TEST_ASSERT_EQUAL_UINT32(43, tuh_audio_write(0, 0, more_samples, 43));
  edpt_busy = false;
  TEST_ASSERT_TRUE(audioh_xfer_cb(AUDIO_DEV_ADDR, 0x01, XFER_RESULT_SUCCESS, edpt_xfer_bytes[1]));

  TEST_ASSERT_EQUAL_UINT8(3, edpt_xfer_count);
  TEST_ASSERT_EQUAL_UINT16(176, edpt_xfer_bytes[2]);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(sample, edpt_xfer_data[2], sizeof(sample));
}

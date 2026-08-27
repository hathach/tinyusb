/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 TinyUSB contributors
 * SPDX-License-Identifier: MIT
 */

#include "unity.h"
#include "audio_host_test.h"
#include "class/midi/midi.h"

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

  PLAYBACK_CLOCK    = 10,
  UNRELATED_CLOCK_0 = 20,
  UNRELATED_CLOCK_1 = 21,
  UNRELATED_TERM_0  = 22,
  UNRELATED_TERM_1  = 23,
  UNRELATED_FU_0    = 24,
  UNRELATED_FU_1    = 25,
};

static bool       interface_set_result;
static tuh_xfer_t interface_xfer;
static uint8_t    interface_alt;
static uint8_t    interface_set_count;

static bool                   control_xfer_result;
static tuh_xfer_t             control_xfer;
static tusb_control_request_t control_request;
static uint8_t                control_buffer[8];
static uint8_t                control_xfer_count;
static uint32_t               control_sync_actual_len;

static bool                 edpt_open_result;
static tusb_desc_endpoint_t opened_ep[4];
static uint8_t              edpt_open_count;

static bool    edpt_close_result;
static uint8_t closed_ep[4];
static uint8_t edpt_close_count;

static uint32_t edpt_busy_mask;
static bool     edpt_xfer_result;
#define AUDIO_TEST_MAX_XFERS 128
static uint8_t      edpt_xfer_ep[AUDIO_TEST_MAX_XFERS];
static uint16_t     edpt_xfer_bytes[AUDIO_TEST_MAX_XFERS];
static uint8_t      edpt_xfer_data[AUDIO_TEST_MAX_XFERS][8];
static uint8_t     *edpt_xfer_buffer[AUDIO_TEST_MAX_XFERS];
static uint8_t      edpt_xfer_count;
static tusb_speed_t test_speed;

static uint8_t                  event_cb_count;
static uint8_t                  event_cb_idx;
static uint8_t                  event_cb_stream_idx;
static tuh_audio_event_t        event_cb_event;
static tusb_xfer_result_t       event_cb_result;

static uint8_t  descriptor_cb_count;
static uint8_t  descriptor_cb_idx;
static uint8_t  descriptor_cb_protocol;
static uint8_t  descriptor_cb_ac_itf;
static uint16_t descriptor_cb_cs_len;
static bool     descriptor_cb_has_playback_fu;

static uint32_t edpt_mask(uint8_t ep_addr) {
  const uint8_t bit = tu_edpt_number(ep_addr) + (tu_edpt_dir(ep_addr) == TUSB_DIR_IN ? 16 : 0);
  return 1u << bit;
}

static void complete_edpt(uint8_t ep_addr) {
  edpt_busy_mask &= ~edpt_mask(ep_addr);
}

void tuh_audio_event_cb(uint8_t idx, uint8_t stream_idx, tuh_audio_event_t event, tusb_xfer_result_t result) {
  event_cb_count++;
  event_cb_idx        = idx;
  event_cb_stream_idx = stream_idx;
  event_cb_event      = event;
  event_cb_result     = result;
}

void tuh_audio_descriptor_cb(uint8_t idx, const tuh_audio_descriptor_cb_t *desc_cb) {
  descriptor_cb_count++;
  descriptor_cb_idx      = idx;
  descriptor_cb_protocol = desc_cb->desc_audio_control->bInterfaceProtocol;
  descriptor_cb_ac_itf   = desc_cb->desc_audio_control->bInterfaceNumber;
  descriptor_cb_cs_len   = desc_cb->desc_cs_audio_control_len;

  const uint8_t *p_desc = desc_cb->desc_cs_audio_control;
  const uint8_t *end    = p_desc + desc_cb->desc_cs_audio_control_len;
  while (p_desc < end && tu_desc_len(p_desc) >= 4 && tu_desc_len(p_desc) <= (uint16_t)(end - p_desc)) {
    const uint8_t fu_subtype = (descriptor_cb_protocol == AUDIO_INT_PROTOCOL_CODE_V2)
                                 ? AUDIO20_CS_AC_INTERFACE_FEATURE_UNIT
                                 : AUDIO10_CS_AC_INTERFACE_FEATURE_UNIT;
    if (tu_desc_type(p_desc) == TUSB_DESC_CS_INTERFACE && tu_desc_subtype(p_desc) == fu_subtype &&
        p_desc[3] == PLAYBACK_FU) {
      descriptor_cb_has_playback_fu = true;
    }
    p_desc = tu_desc_next(p_desc);
  }
}

tusb_speed_t tuh_speed_get(uint8_t daddr) {
  (void)daddr;
  return test_speed;
}

bool tuh_control_xfer(tuh_xfer_t *xfer) {
  control_xfer_count++;
  control_request    = *xfer->setup;
  control_xfer       = *xfer;
  control_xfer.setup = &control_request;
  if (xfer->buffer != NULL) {
    memcpy(control_buffer, xfer->buffer, TU_MIN(sizeof(control_buffer), control_request.wLength));
  }
  xfer->result     = XFER_RESULT_SUCCESS;
  xfer->actual_len = control_sync_actual_len;
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
    complete_edpt(ep_addr);
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
  (void)complete_cb;
  (void)user_data;
  if (edpt_xfer_count < TU_ARRAY_SIZE(edpt_xfer_bytes)) {
    edpt_xfer_ep[edpt_xfer_count]     = ep_addr;
    edpt_xfer_bytes[edpt_xfer_count]  = total_bytes;
    edpt_xfer_buffer[edpt_xfer_count] = buffer;
    memcpy(edpt_xfer_data[edpt_xfer_count], buffer, TU_MIN(sizeof(edpt_xfer_data[0]), total_bytes));
    edpt_xfer_count++;
  }
  if (!edpt_xfer_result) {
    complete_edpt(ep_addr); // match usbh_edpt_xfer() cleanup after HCD rejection
  }
  return edpt_xfer_result;
}

bool usbh_edpt_claim(uint8_t dev_addr, uint8_t ep_addr) {
  (void)dev_addr;
  const uint32_t mask = edpt_mask(ep_addr);
  if (edpt_busy_mask & mask) {
    return false;
  }
  edpt_busy_mask |= mask;
  return true;
}

bool usbh_edpt_release(uint8_t dev_addr, uint8_t ep_addr) {
  (void)dev_addr;
  complete_edpt(ep_addr);
  return true;
}

bool usbh_edpt_busy(uint8_t dev_addr, uint8_t ep_addr) {
  (void)dev_addr;
  return (edpt_busy_mask & edpt_mask(ep_addr)) != 0;
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

#define TEST_UAC1_FEATURE_UNIT_CTRL(_id, _source_id, _controls)                                                    \
  9, TUSB_DESC_CS_INTERFACE, AUDIO10_CS_AC_INTERFACE_FEATURE_UNIT, _id, _source_id, 2, U16_TO_U8S_LE(_controls), 0

#define TEST_UAC1_FEATURE_UNIT(_id, _source_id)                                                           \
  TEST_UAC1_FEATURE_UNIT_CTRL(_id, _source_id, AUDIO10_FU_CONTROL_BM_MUTE | AUDIO10_FU_CONTROL_BM_VOLUME)

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

#define TEST_UAC2_AC_HEADER(_total_len)                                                                                \
  9, TUSB_DESC_INTERFACE, AUDIO_AC_ITF, 0, 0, TUSB_CLASS_AUDIO, AUDIO_SUBCLASS_CONTROL, AUDIO_INT_PROTOCOL_CODE_V2, 0, \
    9, TUSB_DESC_CS_INTERFACE, AUDIO20_CS_AC_INTERFACE_HEADER, U16_TO_U8S_LE(0x0200), 0, U16_TO_U8S_LE(_total_len), 0

#define TEST_UAC2_CLOCK_SOURCE(_id, _access)                                                                           \
  8, TUSB_DESC_CS_INTERFACE, AUDIO20_CS_AC_INTERFACE_CLOCK_SOURCE, _id, AUDIO20_CLOCK_SOURCE_ATT_INT_PRO_CLK, _access, \
    0, 0

#define TEST_UAC2_INPUT_TERM(_id, _type, _clock_id, _channels)                                                 \
  17, TUSB_DESC_CS_INTERFACE, AUDIO20_CS_AC_INTERFACE_INPUT_TERMINAL, _id, U16_TO_U8S_LE(_type), 0, _clock_id, \
    _channels, U32_TO_U8S_LE(AUDIO20_CHANNEL_CONFIG_NON_PREDEFINED), 0, U16_TO_U8S_LE(0), 0

#define TEST_UAC2_OUTPUT_TERM(_id, _type, _source_id, _clock_id)                                                 \
  12, TUSB_DESC_CS_INTERFACE, AUDIO20_CS_AC_INTERFACE_OUTPUT_TERMINAL, _id, U16_TO_U8S_LE(_type), 0, _source_id, \
    _clock_id, U16_TO_U8S_LE(0), 0


#define TEST_UAC2_FEATURE_UNIT_STEREO(_id, _source_id, _master_controls)                                              \
  18, TUSB_DESC_CS_INTERFACE, AUDIO20_CS_AC_INTERFACE_FEATURE_UNIT, _id, _source_id, U32_TO_U8S_LE(_master_controls), \
    U32_TO_U8S_LE(0), U32_TO_U8S_LE(0), 0


#define TEST_UAC2_AS_INTERFACE_NUM(_itf, _alt, _ep_count)                                    \
  9, TUSB_DESC_INTERFACE, _itf, _alt, _ep_count, TUSB_CLASS_AUDIO, AUDIO_SUBCLASS_STREAMING, \
    AUDIO_INT_PROTOCOL_CODE_V2, 0

#define TEST_UAC2_AS_INTERFACE(_alt, _ep_count) TEST_UAC2_AS_INTERFACE_NUM(AUDIO_AS_ITF, _alt, _ep_count)

#define TEST_UAC2_AS_ALT0                       TEST_UAC2_AS_INTERFACE(0, 0)

#define TEST_UAC2_AS_GENERAL(_term_id, _channels)                                                                     \
  16, TUSB_DESC_CS_INTERFACE, AUDIO20_CS_AS_INTERFACE_AS_GENERAL, _term_id, 0, AUDIO20_FORMAT_TYPE_I,                 \
    U32_TO_U8S_LE(AUDIO20_DATA_FORMAT_TYPE_I_PCM), _channels, U32_TO_U8S_LE(AUDIO20_CHANNEL_CONFIG_NON_PREDEFINED), 0

#define TEST_UAC2_FORMAT(_bytes, _bits)                                                                \
  6, TUSB_DESC_CS_INTERFACE, AUDIO20_CS_AS_INTERFACE_FORMAT_TYPE, AUDIO20_FORMAT_TYPE_I, _bytes, _bits

#define TEST_UAC2_DATA_EP(_ep, _attr, _size, _interval)                                          \
  7, TUSB_DESC_ENDPOINT, _ep, (TUSB_XFER_ISOCHRONOUS | (_attr)), U16_TO_U8S_LE(_size), _interval

#define TEST_UAC2_CS_DATA_EP                                                                                    \
  8, TUSB_DESC_CS_ENDPOINT, AUDIO20_CS_EP_SUBTYPE_GENERAL, AUDIO20_CS_AS_ISO_DATA_EP_ATT_NON_MAX_PACKETS_OK, 0, \
    AUDIO20_CS_AS_ISO_DATA_EP_LOCK_DELAY_UNIT_UNDEFINED, U16_TO_U8S_LE(0)

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

static const uint8_t midi1_only_collection[] = {
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

static const uint8_t midi2_only_collection[] = {
  TEST_UAC1_AC_HEADER,
  9,
  TUSB_DESC_INTERFACE,
  AUDIO_AS_ITF,
  1,
  0,
  TUSB_CLASS_AUDIO,
  AUDIO_SUBCLASS_MIDI_STREAMING,
  AUDIO_FUNC_PROTOCOL_CODE_UNDEF,
  0,
  7,
  TUSB_DESC_CS_INTERFACE,
  MIDI_CS_INTERFACE_HEADER,
  U16_TO_U8S_LE(MIDI_VERSION_2_0),
  U16_TO_U8S_LE(7),
};

static const uint8_t uac2_control_interface[] = {
  9, TUSB_DESC_INTERFACE, AUDIO_AC_ITF, 0, 0, TUSB_CLASS_AUDIO, AUDIO_SUBCLASS_CONTROL, AUDIO_INT_PROTOCOL_CODE_V2, 0,
};

static const uint8_t uac2_playback[] = {
  TEST_UAC2_AC_HEADER(64),
  // Deliberately reorder AC entities to verify association is ID-based.
  TEST_UAC2_FEATURE_UNIT_STEREO(PLAYBACK_FU, PLAYBACK_INPUT_TERM,
                                (AUDIO20_CTRL_RW << AUDIO20_FEATURE_UNIT_CTRL_MUTE_POS) |
                                  (AUDIO20_CTRL_RW << AUDIO20_FEATURE_UNIT_CTRL_VOLUME_POS)),
  TEST_UAC2_OUTPUT_TERM(PLAYBACK_OUTPUT_TERM, AUDIO_TERM_TYPE_OUT_HEADPHONES, PLAYBACK_FU, PLAYBACK_CLOCK),
  TEST_UAC2_INPUT_TERM(PLAYBACK_INPUT_TERM, AUDIO_TERM_TYPE_USB_STREAMING, PLAYBACK_CLOCK, 2),
  TEST_UAC2_CLOCK_SOURCE(PLAYBACK_CLOCK, AUDIO20_CTRL_RW << AUDIO20_CLOCK_SOURCE_CTRL_CLK_FRQ_POS),
  TEST_UAC2_AS_ALT0,
  TEST_UAC2_AS_INTERFACE(1, 1),
  TEST_UAC2_AS_GENERAL(PLAYBACK_INPUT_TERM, 2),
  TEST_UAC2_FORMAT(2, 16),
  TEST_UAC1_DATA_EP(0x01, TUSB_ISO_EP_ATT_ASYNCHRONOUS, 192, 1),
  TEST_UAC2_CS_DATA_EP,
};

static const uint8_t uac2_playback_after_unrelated_clocks[] = {
  TEST_UAC2_AC_HEADER(62),
  TEST_UAC2_CLOCK_SOURCE(UNRELATED_CLOCK_0, AUDIO20_CTRL_RW << AUDIO20_CLOCK_SOURCE_CTRL_CLK_FRQ_POS),
  TEST_UAC2_CLOCK_SOURCE(UNRELATED_CLOCK_1, AUDIO20_CTRL_RW << AUDIO20_CLOCK_SOURCE_CTRL_CLK_FRQ_POS),
  TEST_UAC2_INPUT_TERM(PLAYBACK_INPUT_TERM, AUDIO_TERM_TYPE_USB_STREAMING, PLAYBACK_CLOCK, 2),
  TEST_UAC2_OUTPUT_TERM(PLAYBACK_OUTPUT_TERM, AUDIO_TERM_TYPE_OUT_HEADPHONES, PLAYBACK_INPUT_TERM, PLAYBACK_CLOCK),
  TEST_UAC2_CLOCK_SOURCE(PLAYBACK_CLOCK, AUDIO20_CTRL_RW << AUDIO20_CLOCK_SOURCE_CTRL_CLK_FRQ_POS),
  TEST_UAC2_AS_ALT0,
  TEST_UAC2_AS_INTERFACE(1, 1),
  TEST_UAC2_AS_GENERAL(PLAYBACK_INPUT_TERM, 2),
  TEST_UAC2_FORMAT(2, 16),
  TEST_UAC1_DATA_EP(0x01, TUSB_ISO_EP_ATT_ASYNCHRONOUS, 192, 1),
  TEST_UAC2_CS_DATA_EP,
};

static const uint8_t uac2_playback_read_only_feature_unit[] = {
  TEST_UAC2_AC_HEADER(64),
  TEST_UAC2_CLOCK_SOURCE(PLAYBACK_CLOCK, AUDIO20_CTRL_RW << AUDIO20_CLOCK_SOURCE_CTRL_CLK_FRQ_POS),
  TEST_UAC2_INPUT_TERM(PLAYBACK_INPUT_TERM, AUDIO_TERM_TYPE_USB_STREAMING, PLAYBACK_CLOCK, 2),
  TEST_UAC2_FEATURE_UNIT_STEREO(PLAYBACK_FU, PLAYBACK_INPUT_TERM,
                                (AUDIO20_CTRL_R << AUDIO20_FEATURE_UNIT_CTRL_MUTE_POS) |
                                  (AUDIO20_CTRL_R << AUDIO20_FEATURE_UNIT_CTRL_VOLUME_POS)),
  TEST_UAC2_OUTPUT_TERM(PLAYBACK_OUTPUT_TERM, AUDIO_TERM_TYPE_OUT_HEADPHONES, PLAYBACK_FU, PLAYBACK_CLOCK),
  TEST_UAC2_AS_ALT0,
  TEST_UAC2_AS_INTERFACE(1, 1),
  TEST_UAC2_AS_GENERAL(PLAYBACK_INPUT_TERM, 2),
  TEST_UAC2_FORMAT(2, 16),
  TEST_UAC1_DATA_EP(0x01, TUSB_ISO_EP_ATT_ASYNCHRONOUS, 192, 1),
  TEST_UAC2_CS_DATA_EP,
};

static const uint8_t uac2_playback_read_only_clock[] = {
  TEST_UAC2_AC_HEADER(46),
  TEST_UAC2_CLOCK_SOURCE(PLAYBACK_CLOCK, AUDIO20_CTRL_R << AUDIO20_CLOCK_SOURCE_CTRL_CLK_FRQ_POS),
  TEST_UAC2_INPUT_TERM(PLAYBACK_INPUT_TERM, AUDIO_TERM_TYPE_USB_STREAMING, PLAYBACK_CLOCK, 2),
  TEST_UAC2_OUTPUT_TERM(PLAYBACK_OUTPUT_TERM, AUDIO_TERM_TYPE_OUT_HEADPHONES, PLAYBACK_INPUT_TERM, PLAYBACK_CLOCK),
  TEST_UAC2_AS_ALT0,
  TEST_UAC2_AS_INTERFACE(1, 1),
  TEST_UAC2_AS_GENERAL(PLAYBACK_INPUT_TERM, 2),
  TEST_UAC2_FORMAT(2, 16),
  TEST_UAC1_DATA_EP(0x01, TUSB_ISO_EP_ATT_ASYNCHRONOUS, 192, 1),
  TEST_UAC2_CS_DATA_EP,
};

static const uint8_t uac2_duplex_shared_clock[] = {
  TEST_UAC2_AC_HEADER(75),
  TEST_UAC2_OUTPUT_TERM(CAPTURE_OUTPUT_TERM, AUDIO_TERM_TYPE_USB_STREAMING, CAPTURE_INPUT_TERM, PLAYBACK_CLOCK),
  TEST_UAC2_INPUT_TERM(PLAYBACK_INPUT_TERM, AUDIO_TERM_TYPE_USB_STREAMING, PLAYBACK_CLOCK, 2),
  TEST_UAC2_CLOCK_SOURCE(PLAYBACK_CLOCK, AUDIO20_CTRL_RW << AUDIO20_CLOCK_SOURCE_CTRL_CLK_FRQ_POS),
  TEST_UAC2_INPUT_TERM(CAPTURE_INPUT_TERM, AUDIO_TERM_TYPE_IN_GENERIC_MIC, PLAYBACK_CLOCK, 1),
  TEST_UAC2_OUTPUT_TERM(PLAYBACK_OUTPUT_TERM, AUDIO_TERM_TYPE_OUT_HEADPHONES, PLAYBACK_INPUT_TERM, PLAYBACK_CLOCK),
  TEST_UAC2_AS_ALT0,
  TEST_UAC2_AS_INTERFACE(1, 1),
  TEST_UAC2_AS_GENERAL(PLAYBACK_INPUT_TERM, 2),
  TEST_UAC2_FORMAT(2, 16),
  TEST_UAC2_CS_DATA_EP,
  TEST_UAC1_DATA_EP(0x01, TUSB_ISO_EP_ATT_ASYNCHRONOUS, 192, 1),
  TEST_UAC2_AS_INTERFACE_NUM(AUDIO_AS_IN_ITF, 0, 0),
  TEST_UAC2_AS_INTERFACE_NUM(AUDIO_AS_IN_ITF, 1, 1),
  TEST_UAC2_AS_GENERAL(CAPTURE_OUTPUT_TERM, 1),
  TEST_UAC2_FORMAT(2, 16),
  TEST_UAC1_DATA_EP(0x81, TUSB_ISO_EP_ATT_ASYNCHRONOUS, 96, 1),
  TEST_UAC2_CS_DATA_EP,
};

static const uint8_t uac2_capture_no_sync_data[] = {
  TEST_UAC2_AC_HEADER(46),
  TEST_UAC2_CLOCK_SOURCE(PLAYBACK_CLOCK, AUDIO20_CTRL_RW << AUDIO20_CLOCK_SOURCE_CTRL_CLK_FRQ_POS),
  TEST_UAC2_INPUT_TERM(CAPTURE_INPUT_TERM, AUDIO_TERM_TYPE_IN_GENERIC_MIC, PLAYBACK_CLOCK, 1),
  TEST_UAC2_OUTPUT_TERM(CAPTURE_OUTPUT_TERM, AUDIO_TERM_TYPE_USB_STREAMING, CAPTURE_INPUT_TERM, PLAYBACK_CLOCK),
  TEST_UAC2_AS_ALT0,
  TEST_UAC2_AS_INTERFACE(1, 1),
  TEST_UAC2_AS_GENERAL(CAPTURE_OUTPUT_TERM, 1),
  TEST_UAC2_FORMAT(2, 16),
  TEST_UAC2_DATA_EP(0x81, TUSB_ISO_EP_ATT_DATA, 96, 1),
  TEST_UAC2_CS_DATA_EP,
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
  TEST_UAC1_DATA_EP(0x81, TUSB_ISO_EP_ATT_ASYNCHRONOUS | TUSB_ISO_EP_ATT_IMPLICIT_FB, 96, 1),
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

static const uint8_t playback_after_unrelated_ac_entities[] = {
  TEST_UAC1_AC_HEADER,
  TEST_UAC1_INPUT_TERM(UNRELATED_TERM_0, AUDIO_TERM_TYPE_USB_STREAMING, 2),
  TEST_UAC1_INPUT_TERM(UNRELATED_TERM_1, AUDIO_TERM_TYPE_USB_STREAMING, 2),
  TEST_UAC1_FEATURE_UNIT(UNRELATED_FU_0, UNRELATED_TERM_0),
  TEST_UAC1_FEATURE_UNIT(UNRELATED_FU_1, UNRELATED_TERM_1),
  TEST_UAC1_INPUT_TERM(PLAYBACK_INPUT_TERM, AUDIO_TERM_TYPE_USB_STREAMING, 2),
  TEST_UAC1_FEATURE_UNIT(PLAYBACK_FU, PLAYBACK_INPUT_TERM),
  TEST_UAC1_OUTPUT_TERM(PLAYBACK_OUTPUT_TERM, AUDIO_TERM_TYPE_OUT_HEADPHONES, PLAYBACK_FU),
  TEST_UAC1_AS_ALT0,
  TEST_UAC1_AS_INTERFACE(1, 1),
  TEST_UAC1_AS_GENERAL(PLAYBACK_INPUT_TERM),
  TEST_UAC1_FORMAT(2, 2, 16, 48000),
  TEST_UAC1_DATA_EP(0x01, TUSB_ISO_EP_ATT_ADAPTIVE, 192, 1),
  TEST_UAC1_CS_DATA_EP,
};

static const uint8_t playback_fu_without_mute_volume[] = {
  TEST_UAC1_AC_HEADER,
  TEST_UAC1_FEATURE_UNIT_CTRL(PLAYBACK_FU, PLAYBACK_INPUT_TERM, AUDIO10_FU_CONTROL_BM_BASS),
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

static const uint8_t playback_with_unsupported_capture[] = {
  TEST_UAC1_AC_HEADER_2,
  TEST_UAC1_INPUT_TERM(PLAYBACK_INPUT_TERM, AUDIO_TERM_TYPE_USB_STREAMING, 2),
  TEST_UAC1_OUTPUT_TERM(CAPTURE_OUTPUT_TERM, AUDIO_TERM_TYPE_USB_STREAMING, CAPTURE_INPUT_TERM),
  TEST_UAC1_AS_ALT0,
  TEST_UAC1_AS_INTERFACE(1, 1),
  TEST_UAC1_AS_GENERAL(PLAYBACK_INPUT_TERM),
  TEST_UAC1_FORMAT(2, 2, 16, 48000),
  TEST_UAC1_DATA_EP(0x01, TUSB_ISO_EP_ATT_ADAPTIVE, 192, 1),
  TEST_UAC1_CS_DATA_EP,
  TEST_UAC1_AS_INTERFACE_NUM(AUDIO_AS_IN_ITF, 0, 0),
  TEST_UAC1_AS_INTERFACE_NUM(AUDIO_AS_IN_ITF, 1, 1),
  TEST_UAC1_AS_GENERAL(CAPTURE_OUTPUT_TERM),
  TEST_UAC1_FORMAT(1, 2, 12, 48000),
  TEST_UAC1_DATA_EP(0x81, TUSB_ISO_EP_ATT_ASYNCHRONOUS, 96, 1),
  TEST_UAC1_CS_DATA_EP,
};

static const uint8_t playback_with_two_frequencies[] = {
  TEST_UAC1_AC_HEADER,
  TEST_UAC1_INPUT_TERM(PLAYBACK_INPUT_TERM, AUDIO_TERM_TYPE_USB_STREAMING, 2),
  TEST_UAC1_AS_ALT0,
  TEST_UAC1_AS_INTERFACE(1, 1),
  TEST_UAC1_AS_GENERAL(PLAYBACK_INPUT_TERM),
  TEST_UAC1_FORMAT(2, 2, 16, 44100, 44100, 48000),
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

static const uint8_t malformed_zero_length_ac_descriptor[] = {
  TEST_UAC1_AC_HEADER,
  0,
  TUSB_DESC_CS_INTERFACE,
};

static const uint8_t malformed_sampling_frequency_list[] = {
  TEST_UAC1_AC_HEADER,
  TEST_UAC1_INPUT_TERM(PLAYBACK_INPUT_TERM, AUDIO_TERM_TYPE_USB_STREAMING, 2),
  TEST_UAC1_AS_ALT0,
  TEST_UAC1_AS_INTERFACE(1, 1),
  TEST_UAC1_AS_GENERAL(PLAYBACK_INPUT_TERM),
  11,
  TUSB_DESC_CS_INTERFACE,
  AUDIO10_CS_AS_INTERFACE_FORMAT_TYPE,
  AUDIO10_FORMAT_TYPE_I,
  2,
  2,
  16,
  2,
  U24_TO_U8S_LE(48000),
};

static const uint8_t malformed_short_endpoint[] = {
  TEST_UAC1_AC_HEADER,
  TEST_UAC1_INPUT_TERM(PLAYBACK_INPUT_TERM, AUDIO_TERM_TYPE_USB_STREAMING, 2),
  TEST_UAC1_AS_ALT0,
  TEST_UAC1_AS_INTERFACE(1, 1),
  TEST_UAC1_AS_GENERAL(PLAYBACK_INPUT_TERM),
  TEST_UAC1_FORMAT(2, 2, 16, 48000),
  6,
  TUSB_DESC_ENDPOINT,
  0x01,
  (TUSB_XFER_ISOCHRONOUS | TUSB_ISO_EP_ATT_ADAPTIVE),
  U16_TO_U8S_LE(192),
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
  TEST_UAC1_FORMAT(2, 2, 16, 48000, 96000),
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

static const uint8_t playback_44100_with_feedback_10_14[] = {
  TEST_UAC1_AC_HEADER,
  TEST_UAC1_INPUT_TERM(PLAYBACK_INPUT_TERM, AUDIO_TERM_TYPE_USB_STREAMING, 2),
  TEST_UAC1_AS_ALT0,
  TEST_UAC1_AS_INTERFACE(1, 2),
  TEST_UAC1_AS_GENERAL(PLAYBACK_INPUT_TERM),
  TEST_UAC1_FORMAT(2, 2, 16, 44100),
  TEST_UAC1_DATA_EP(0x01, TUSB_ISO_EP_ATT_ADAPTIVE, 180, 1),
  TEST_UAC1_CS_DATA_EP_ATTR(AUDIO10_CS_AS_ISO_DATA_EP_ATT_MAX_PACKETS_ONLY),
  TEST_UAC1_DATA_EP(0x81, TUSB_ISO_EP_ATT_EXPLICIT_FB, 3, 1),
};

static const uint8_t playback_44100_with_feedback_16_16[] = {
  TEST_UAC1_AC_HEADER,
  TEST_UAC1_INPUT_TERM(PLAYBACK_INPUT_TERM, AUDIO_TERM_TYPE_USB_STREAMING, 2),
  TEST_UAC1_AS_ALT0,
  TEST_UAC1_AS_INTERFACE(1, 2),
  TEST_UAC1_AS_GENERAL(PLAYBACK_INPUT_TERM),
  TEST_UAC1_FORMAT(2, 2, 16, 44100),
  TEST_UAC1_DATA_EP(0x01, TUSB_ISO_EP_ATT_ADAPTIVE, 180, 1),
  TEST_UAC1_CS_DATA_EP_ATTR(AUDIO10_CS_AS_ISO_DATA_EP_ATT_MAX_PACKETS_ONLY),
  TEST_UAC1_DATA_EP(0x81, TUSB_ISO_EP_ATT_EXPLICIT_FB, 4, 1),
};

static const uint8_t playback_11025_interval4[] = {
  TEST_UAC1_AC_HEADER,
  TEST_UAC1_INPUT_TERM(PLAYBACK_INPUT_TERM, AUDIO_TERM_TYPE_USB_STREAMING, 2),
  TEST_UAC1_AS_ALT0,
  TEST_UAC1_AS_INTERFACE(1, 1),
  TEST_UAC1_AS_GENERAL(PLAYBACK_INPUT_TERM),
  TEST_UAC1_FORMAT(2, 2, 16, 11025),
  TEST_UAC1_DATA_EP(0x01, TUSB_ISO_EP_ATT_ADAPTIVE, 180, 3),
  TEST_UAC1_CS_DATA_EP,
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

static void complete_control_xfer_with_u16(uint16_t value) {
  TEST_ASSERT_NOT_NULL(control_xfer.buffer);
  tu_unaligned_write16(control_xfer.buffer, tu_htole16(value));
  control_xfer.actual_len = 2;
  complete_control_xfer(XFER_RESULT_SUCCESS);
}

static void complete_control_xfer_with_u32(uint32_t value) {
  TEST_ASSERT_NOT_NULL(control_xfer.buffer);
  tu_unaligned_write32(control_xfer.buffer, tu_htole32(value));
  control_xfer.actual_len = 4;
  complete_control_xfer(XFER_RESULT_SUCCESS);
}

static void complete_uac2_clock_range(uint32_t rate0, uint32_t rate1) {
  TEST_ASSERT_NOT_NULL(control_xfer.buffer);
  tu_unaligned_write16(control_xfer.buffer, tu_htole16(2));
  tu_unaligned_write32(&control_xfer.buffer[2], tu_htole32(rate0));
  tu_unaligned_write32(&control_xfer.buffer[6], tu_htole32(rate0));
  tu_unaligned_write32(&control_xfer.buffer[10], 0);
  tu_unaligned_write32(&control_xfer.buffer[14], tu_htole32(rate1));
  tu_unaligned_write32(&control_xfer.buffer[18], tu_htole32(rate1));
  tu_unaligned_write32(&control_xfer.buffer[22], 0);
  control_xfer.actual_len = 26;
  complete_control_xfer(XFER_RESULT_SUCCESS);
}

static void complete_uac2_volume_range(int16_t min, int16_t max, uint16_t res) {
  TEST_ASSERT_NOT_NULL(control_xfer.buffer);
  tu_unaligned_write16(control_xfer.buffer, tu_htole16(1));
  tu_unaligned_write16(&control_xfer.buffer[2], tu_htole16((uint16_t)min));
  tu_unaligned_write16(&control_xfer.buffer[4], tu_htole16((uint16_t)max));
  tu_unaligned_write16(&control_xfer.buffer[6], tu_htole16(res));
  control_xfer.actual_len = 8;
  complete_control_xfer(XFER_RESULT_SUCCESS);
}

static uint16_t edpt_xfer_bytes_at(uint8_t ep_addr, uint8_t ep_xfer_idx) {
  uint8_t found = 0;
  for (uint8_t i = 0; i < edpt_xfer_count; i++) {
    if (edpt_xfer_ep[i] == ep_addr && found++ == ep_xfer_idx) {
      return edpt_xfer_bytes[i];
    }
  }
  TEST_FAIL_MESSAGE("Endpoint transfer not found");
  return 0;
}

static uint8_t *edpt_xfer_buffer_at(uint8_t ep_addr, uint8_t ep_xfer_idx) {
  uint8_t found = 0;
  for (uint8_t i = 0; i < edpt_xfer_count; i++) {
    if (edpt_xfer_ep[i] == ep_addr && found++ == ep_xfer_idx) {
      return edpt_xfer_buffer[i];
    }
  }
  TEST_FAIL_MESSAGE("Endpoint transfer not found");
  return NULL;
}

static void complete_playback_xfer(void) {
  complete_edpt(0x01);
  TEST_ASSERT_TRUE(audioh_xfer_cb(AUDIO_DEV_ADDR, 0x01, XFER_RESULT_SUCCESS, 0));
}

static void complete_feedback_xfer(uint32_t feedback_q16, uint8_t length) {
  uint8_t *buffer = edpt_xfer_buffer_at(0x81, 0);
  if (length == 3) {
    const uint32_t feedback_q14 = feedback_q16 >> 2;
    buffer[0]                   = (uint8_t)feedback_q14;
    buffer[1]                   = (uint8_t)(feedback_q14 >> 8);
    buffer[2]                   = (uint8_t)(feedback_q14 >> 16);
  } else {
    buffer[0] = (uint8_t)feedback_q16;
    buffer[1] = (uint8_t)(feedback_q16 >> 8);
    buffer[2] = (uint8_t)(feedback_q16 >> 16);
    buffer[3] = (uint8_t)(feedback_q16 >> 24);
  }
  complete_edpt(0x81);
  TEST_ASSERT_TRUE(audioh_xfer_cb(AUDIO_DEV_ADDR, 0x81, XFER_RESULT_SUCCESS, length));
}

void setUp(void) {
  test_speed = TUSB_SPEED_FULL;

  interface_set_result = true;
  memset(&interface_xfer, 0, sizeof(interface_xfer));
  interface_alt       = 0;
  interface_set_count = 0;

  control_xfer_result = true;
  memset(&control_xfer, 0, sizeof(control_xfer));
  memset(&control_request, 0, sizeof(control_request));
  memset(control_buffer, 0, sizeof(control_buffer));
  control_xfer_count      = 0;
  control_sync_actual_len = 0;

  edpt_open_result = true;
  memset(opened_ep, 0, sizeof(opened_ep));
  edpt_open_count = 0;

  edpt_close_result = true;
  memset(closed_ep, 0, sizeof(closed_ep));
  edpt_close_count = 0;

  edpt_busy_mask   = 0;
  edpt_xfer_result = true;
  memset(edpt_xfer_ep, 0, sizeof(edpt_xfer_ep));
  memset(edpt_xfer_bytes, 0, sizeof(edpt_xfer_bytes));
  memset(edpt_xfer_data, 0, sizeof(edpt_xfer_data));
  memset(edpt_xfer_buffer, 0, sizeof(edpt_xfer_buffer));
  edpt_xfer_count = 0;

  event_cb_count      = 0;
  event_cb_idx        = TUSB_INDEX_INVALID_8;
  event_cb_stream_idx = TUSB_INDEX_INVALID_8;
  event_cb_event      = TUH_AUDIO_EVENT_START_COMPLETE;
  event_cb_result     = XFER_RESULT_INVALID;

  descriptor_cb_count           = 0;
  descriptor_cb_idx             = TUSB_INDEX_INVALID_8;
  descriptor_cb_protocol        = 0;
  descriptor_cb_ac_itf          = TUSB_INDEX_INVALID_8;
  descriptor_cb_cs_len          = 0;
  descriptor_cb_has_playback_fu = false;

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

static void complete_mount(void) {
  while (!tuh_audio_mounted(0)) {
    switch (control_request.bRequest) {
      case AUDIO10_CS_REQ_GET_MIN:
        complete_control_xfer_with_u16((uint16_t)(-90 * 256));
        break;
      case AUDIO10_CS_REQ_GET_MAX:
        complete_control_xfer_with_u16(6 * 256);
        break;
      case AUDIO10_CS_REQ_GET_RES:
        complete_control_xfer_with_u16(256);
        break;
      default:
        TEST_FAIL_MESSAGE("Unexpected Feature Unit mount request");
        break;
    }
  }
  TEST_ASSERT_TRUE(tuh_audio_mounted(0));
  control_xfer_count = 0;
}

static void mount_descriptors(const uint8_t *desc, uint16_t desc_len) {
  open_descriptors(desc, desc_len);
  TEST_ASSERT_TRUE(audioh_set_config(AUDIO_DEV_ADDR, AUDIO_AC_ITF));
  complete_mount();
}

void test_audio_host_rejects_midi1_collection_without_consuming_instance(void) {
  TEST_ASSERT_EQUAL_UINT16(0, audioh_open(0, AUDIO_DEV_ADDR, (const tusb_desc_interface_t *)midi1_only_collection,
                                          sizeof(midi1_only_collection)));
  TEST_ASSERT_EQUAL_UINT8(0, tuh_audio_get_dev_addr(0));
}

void test_audio_host_rejects_midi2_collection_without_consuming_instance(void) {
  TEST_ASSERT_EQUAL_UINT16(0, audioh_open(0, AUDIO_DEV_ADDR, (const tusb_desc_interface_t *)midi2_only_collection,
                                          sizeof(midi2_only_collection)));
  TEST_ASSERT_EQUAL_UINT8(0, tuh_audio_get_dev_addr(0));
}

void test_audio_host_exposes_audio_control_descriptors_during_enumeration(void) {
  open_descriptors(playback_fu_before_terminal, sizeof(playback_fu_before_terminal));

  TEST_ASSERT_EQUAL_UINT8(1, descriptor_cb_count);
  TEST_ASSERT_EQUAL_UINT8(0, descriptor_cb_idx);
  TEST_ASSERT_EQUAL_UINT8(AUDIO_INT_PROTOCOL_CODE_V1, descriptor_cb_protocol);
  TEST_ASSERT_EQUAL_UINT8(AUDIO_AC_ITF, descriptor_cb_ac_itf);
  TEST_ASSERT_GREATER_THAN_UINT16(0, descriptor_cb_cs_len);
  TEST_ASSERT_TRUE(descriptor_cb_has_playback_fu);
  TEST_ASSERT_FALSE(tuh_audio_mounted(0));
}

void test_audio_host_saves_and_opens_explicit_feedback_endpoint(void) {
  mount_descriptors(playback_with_explicit_feedback, sizeof(playback_with_explicit_feedback));

  TEST_ASSERT_EQUAL_UINT8(1, tuh_audio_stream_count(0));
  TEST_ASSERT_EQUAL(TUH_AUDIO_STREAM_PLAYBACK, tuh_audio_stream_direction(0, 0));
  TEST_ASSERT_EQUAL_UINT8(1, tuh_audio_config_count(0, 0));
  TEST_ASSERT_TRUE(tuh_audio_configure(0, 0, 0));
  TEST_ASSERT_EQUAL_UINT8(2, edpt_open_count);
  TEST_ASSERT_EQUAL_HEX8(0x01, opened_ep[0].bEndpointAddress);
  TEST_ASSERT_EQUAL_HEX8(0x81, opened_ep[1].bEndpointAddress);
  TEST_ASSERT_EQUAL_UINT16(3, tu_edpt_packet_size(&opened_ep[1]));
}

void test_audio_host_rejects_uac2_interface_without_consuming_instance(void) {
  TEST_ASSERT_EQUAL_UINT16(0, audioh_open(0, AUDIO_DEV_ADDR, (const tusb_desc_interface_t *)uac2_control_interface,
                                          sizeof(uac2_control_interface)));
  TEST_ASSERT_EQUAL_UINT8(0, tuh_audio_get_dev_addr(0));

  open_descriptors(playback_with_explicit_feedback, sizeof(playback_with_explicit_feedback));
  TEST_ASSERT_EQUAL_UINT8(1, tuh_audio_stream_count(0));
}

void test_audio_host_mounts_uac2_and_sets_clock_before_activating_stream(void) {
  open_descriptors(uac2_playback, sizeof(uac2_playback));
  TEST_ASSERT_EQUAL_UINT8(AUDIO_INT_PROTOCOL_CODE_V2, descriptor_cb_protocol);
  TEST_ASSERT_TRUE(descriptor_cb_has_playback_fu);
  TEST_ASSERT_TRUE(audioh_set_config(AUDIO_DEV_ADDR, AUDIO_AC_ITF));

  TEST_ASSERT_FALSE(tuh_audio_mounted(0));
  TEST_ASSERT_EQUAL_UINT8(1, control_xfer_count);
  TEST_ASSERT_EQUAL_UINT8(AUDIO20_CS_REQ_RANGE, control_request.bRequest);
  TEST_ASSERT_EQUAL_HEX16(tu_u16(AUDIO20_CS_CTRL_SAM_FREQ, 0), tu_le16toh(control_request.wValue));
  TEST_ASSERT_EQUAL_HEX16(tu_u16(PLAYBACK_CLOCK, AUDIO_AC_ITF), tu_le16toh(control_request.wIndex));
  complete_uac2_clock_range(44100, 48000);

  TEST_ASSERT_FALSE(tuh_audio_mounted(0));
  TEST_ASSERT_EQUAL_UINT8(2, control_xfer_count);
  TEST_ASSERT_EQUAL_UINT8(AUDIO20_CS_REQ_RANGE, control_request.bRequest);
  TEST_ASSERT_EQUAL_HEX16(tu_u16(AUDIO20_FU_CTRL_VOLUME, 0), tu_le16toh(control_request.wValue));
  TEST_ASSERT_EQUAL_HEX16(tu_u16(PLAYBACK_FU, AUDIO_AC_ITF), tu_le16toh(control_request.wIndex));
  complete_uac2_volume_range((int16_t)(-90 * 256), (int16_t)(6 * 256), 256);

  TEST_ASSERT_TRUE(tuh_audio_mounted(0));
  TEST_ASSERT_EQUAL_UINT8(1, tuh_audio_stream_count(0));
  TEST_ASSERT_EQUAL_UINT8(2, tuh_audio_config_count(0, 0));
  TEST_ASSERT_TRUE(tuh_audio_mute_supported(0, 0));
  tuh_audio_volume_range_t range;
  TEST_ASSERT_TRUE(tuh_audio_volume_range_get(0, 0, &range));
  TEST_ASSERT_EQUAL_INT16(-90 * 256, range.min);
  TEST_ASSERT_EQUAL_INT16(6 * 256, range.max);
  TEST_ASSERT_EQUAL_UINT16(256, range.res);

  control_xfer_count = 0;
  TEST_ASSERT_TRUE(tuh_audio_volume_set(0, 0, (int16_t)(-6 * 256), feature_unit_complete, 42));
  TEST_ASSERT_EQUAL_UINT8(1, control_xfer_count);
  TEST_ASSERT_EQUAL_UINT8(AUDIO20_CS_REQ_CUR, control_request.bRequest);
  TEST_ASSERT_EQUAL_HEX16(tu_u16(AUDIO20_FU_CTRL_VOLUME, 0), tu_le16toh(control_request.wValue));
  TEST_ASSERT_EQUAL_UINT16((uint16_t)(-6 * 256), tu_le16toh(tu_unaligned_read16(control_buffer)));
  complete_control_xfer(XFER_RESULT_SUCCESS);
  TEST_ASSERT_EQUAL_UINT8(1, fu_cb_count);
  TEST_ASSERT_EQUAL_UINT32(42, fu_cb_user_data);

  tuh_audio_stream_config_t config;
  TEST_ASSERT_TRUE(tuh_audio_config_get(0, 0, 1, &config));
  TEST_ASSERT_EQUAL_UINT32(48000, config.sample_rate);
  TEST_ASSERT_TRUE(tuh_audio_configure(0, 0, 1));

  control_xfer_count = 0;
  TEST_ASSERT_TRUE(tuh_audio_start(0, 0));
  TEST_ASSERT_EQUAL_UINT8(1, control_xfer_count);
  TEST_ASSERT_EQUAL_UINT8(0, interface_set_count);
  TEST_ASSERT_EQUAL_UINT8(AUDIO20_CS_REQ_CUR, control_request.bRequest);
  TEST_ASSERT_EQUAL_UINT16(4, tu_le16toh(control_request.wLength));
  TEST_ASSERT_EQUAL_UINT32(48000, tu_le32toh(tu_unaligned_read32(control_buffer)));

  complete_control_xfer(XFER_RESULT_SUCCESS);
  TEST_ASSERT_EQUAL_UINT8(1, interface_set_count);
  TEST_ASSERT_EQUAL_UINT8(1, interface_alt);
  TEST_ASSERT_EQUAL_UINT8(0, edpt_xfer_count);
  complete_interface_set(XFER_RESULT_SUCCESS);
  TEST_ASSERT_EQUAL_UINT8(1, edpt_xfer_count);
  TEST_ASSERT_EQUAL_UINT16(192, edpt_xfer_bytes[0]);
}

void test_audio_host_generic_control_request_supports_uac2_clock_entity(void) {
  uint8_t clock_valid = 0;
  open_descriptors(uac2_playback, sizeof(uac2_playback));
  TEST_ASSERT_TRUE(audioh_set_config(AUDIO_DEV_ADDR, AUDIO_AC_ITF));
  complete_uac2_clock_range(44100, 48000);
  complete_uac2_volume_range((int16_t)(-90 * 256), (int16_t)(6 * 256), 256);

  control_xfer_count = 0;
  TEST_ASSERT_TRUE(tuh_audio_control_xfer(0, PLAYBACK_CLOCK, TUSB_DIR_IN, AUDIO20_CS_REQ_CUR, AUDIO20_CS_CTRL_CLK_VALID,
                                          0, &clock_valid, sizeof(clock_valid), feature_unit_complete, 77));
  TEST_ASSERT_EQUAL_UINT8(1, control_xfer_count);
  TEST_ASSERT_EQUAL(TUSB_DIR_IN, control_request.bmRequestType_bit.direction);
  TEST_ASSERT_EQUAL_UINT8(AUDIO20_CS_REQ_CUR, control_request.bRequest);
  TEST_ASSERT_EQUAL_HEX16(tu_u16(AUDIO20_CS_CTRL_CLK_VALID, 0), tu_le16toh(control_request.wValue));
  TEST_ASSERT_EQUAL_HEX16(tu_u16(PLAYBACK_CLOCK, AUDIO_AC_ITF), tu_le16toh(control_request.wIndex));
  TEST_ASSERT_EQUAL_UINT16(1, tu_le16toh(control_request.wLength));

  control_xfer.buffer[0]  = 1;
  control_xfer.actual_len = 1;
  complete_control_xfer(XFER_RESULT_SUCCESS);
  TEST_ASSERT_EQUAL_UINT8(1, fu_cb_count);
  TEST_ASSERT_EQUAL_UINT32(77, fu_cb_user_data);
  TEST_ASSERT_EQUAL_UINT8(1, clock_valid);
}

void test_audio_host_uac2_read_only_clock_exposes_cur_rate_without_setting_it(void) {
  open_descriptors(uac2_playback_read_only_clock, sizeof(uac2_playback_read_only_clock));
  TEST_ASSERT_TRUE(audioh_set_config(AUDIO_DEV_ADDR, AUDIO_AC_ITF));
  complete_uac2_clock_range(44100, 48000);

  TEST_ASSERT_EQUAL_UINT8(AUDIO20_CS_REQ_CUR, control_request.bRequest);
  TEST_ASSERT_EQUAL_UINT16(4, tu_le16toh(control_request.wLength));
  complete_control_xfer_with_u32(48000);
  TEST_ASSERT_TRUE(tuh_audio_mounted(0));
  TEST_ASSERT_EQUAL_UINT8(1, tuh_audio_config_count(0, 0));

  tuh_audio_stream_config_t config;
  TEST_ASSERT_TRUE(tuh_audio_config_get(0, 0, 0, &config));
  TEST_ASSERT_EQUAL_UINT32(48000, config.sample_rate);
  TEST_ASSERT_TRUE(tuh_audio_configure(0, 0, 0));

  control_xfer_count = 0;
  TEST_ASSERT_TRUE(tuh_audio_start(0, 0));
  TEST_ASSERT_EQUAL_UINT8(0, control_xfer_count);
  TEST_ASSERT_EQUAL_UINT8(1, interface_set_count);
  TEST_ASSERT_EQUAL_UINT8(1, interface_alt);
}

void test_audio_host_uac2_read_only_feature_controls_reject_set_and_allow_get(void) {
  open_descriptors(uac2_playback_read_only_feature_unit, sizeof(uac2_playback_read_only_feature_unit));
  TEST_ASSERT_TRUE(audioh_set_config(AUDIO_DEV_ADDR, AUDIO_AC_ITF));
  complete_uac2_clock_range(44100, 48000);
  complete_uac2_volume_range((int16_t)(-90 * 256), (int16_t)(6 * 256), 256);
  TEST_ASSERT_TRUE(tuh_audio_mounted(0));

  TEST_ASSERT_FALSE(tuh_audio_mute_set(0, 0, false, feature_unit_complete, 0));
  TEST_ASSERT_FALSE(tuh_audio_volume_set(0, 0, 0, feature_unit_complete, 0));

  int16_t volume = 0;
  TEST_ASSERT_TRUE(tuh_audio_volume_get(0, 0, &volume, feature_unit_complete, 7));
  TEST_ASSERT_EQUAL_UINT8(AUDIO20_CS_REQ_CUR, control_request.bRequest);
  TEST_ASSERT_EQUAL(TUSB_DIR_IN, control_request.bmRequestType_bit.direction);
  complete_control_xfer_with_u16((uint16_t)(-12 * 256));
  TEST_ASSERT_EQUAL_INT16(-12 * 256, volume);
  TEST_ASSERT_EQUAL_UINT32(7, fu_cb_user_data);
}

void test_audio_host_uac2_shared_clock_is_discovered_once_for_both_streams(void) {
  open_descriptors(uac2_duplex_shared_clock, sizeof(uac2_duplex_shared_clock));
  TEST_ASSERT_TRUE(audioh_set_config(AUDIO_DEV_ADDR, AUDIO_AC_ITF));
  TEST_ASSERT_EQUAL_UINT8(1, control_xfer_count);
  complete_uac2_clock_range(44100, 48000);

  TEST_ASSERT_TRUE(tuh_audio_mounted(0));
  TEST_ASSERT_EQUAL_UINT8(1, control_xfer_count);
  TEST_ASSERT_EQUAL_UINT8(2, tuh_audio_stream_count(0));
  TEST_ASSERT_EQUAL(TUH_AUDIO_STREAM_PLAYBACK, tuh_audio_stream_direction(0, 0));
  TEST_ASSERT_EQUAL(TUH_AUDIO_STREAM_CAPTURE, tuh_audio_stream_direction(0, 1));
  TEST_ASSERT_EQUAL_UINT8(2, tuh_audio_config_count(0, 0));
  TEST_ASSERT_EQUAL_UINT8(2, tuh_audio_config_count(0, 1));
}

void test_audio_host_uac2_finds_clock_source_after_unrelated_clocks(void) {
  open_descriptors(uac2_playback_after_unrelated_clocks, sizeof(uac2_playback_after_unrelated_clocks));
  TEST_ASSERT_TRUE(audioh_set_config(AUDIO_DEV_ADDR, AUDIO_AC_ITF));

  TEST_ASSERT_EQUAL_UINT8(AUDIO20_CS_REQ_RANGE, control_request.bRequest);
  TEST_ASSERT_EQUAL_HEX16(tu_u16(PLAYBACK_CLOCK, AUDIO_AC_ITF), tu_le16toh(control_request.wIndex));
  complete_uac2_clock_range(44100, 48000);

  TEST_ASSERT_TRUE(tuh_audio_mounted(0));
  TEST_ASSERT_EQUAL_UINT8(2, tuh_audio_config_count(0, 0));
}

void test_audio_host_reconfigures_stopped_duplex_streams_to_a_new_common_rate(void) {
  open_descriptors(uac2_duplex_shared_clock, sizeof(uac2_duplex_shared_clock));
  TEST_ASSERT_TRUE(audioh_set_config(AUDIO_DEV_ADDR, AUDIO_AC_ITF));
  complete_uac2_clock_range(44100, 48000);

  TEST_ASSERT_TRUE(tuh_audio_configure(0, 0, 1));
  TEST_ASSERT_TRUE(tuh_audio_configure(0, 1, 1));
  TEST_ASSERT_TRUE(tuh_audio_configure(0, 0, 0));
  TEST_ASSERT_EQUAL_UINT8(0, tuh_audio_active_config(0, 0));
  TEST_ASSERT_EQUAL_UINT8(1, tuh_audio_active_config(0, 1));
  TEST_ASSERT_TRUE(tuh_audio_configure(0, 1, 0));
  TEST_ASSERT_EQUAL_UINT8(0, tuh_audio_active_config(0, 1));
}

void test_audio_host_rejects_start_at_different_rate_from_running_peer(void) {
  open_descriptors(uac2_duplex_shared_clock, sizeof(uac2_duplex_shared_clock));
  TEST_ASSERT_TRUE(audioh_set_config(AUDIO_DEV_ADDR, AUDIO_AC_ITF));
  complete_uac2_clock_range(44100, 48000);

  TEST_ASSERT_TRUE(tuh_audio_configure(0, 0, 0));
  TEST_ASSERT_TRUE(tuh_audio_configure(0, 1, 1));
  TEST_ASSERT_TRUE(tuh_audio_start(0, 0));
  complete_control_xfer(XFER_RESULT_SUCCESS);
  complete_interface_set(XFER_RESULT_SUCCESS);

  TEST_ASSERT_FALSE(tuh_audio_start(0, 1));
  TEST_ASSERT_EQUAL_UINT8(2, control_xfer_count);
  TEST_ASSERT_EQUAL_UINT8(1, interface_set_count);
}

void test_audio_host_rejects_malformed_uac2_clock_range_and_releases_instance(void) {
  open_descriptors(uac2_playback, sizeof(uac2_playback));
  TEST_ASSERT_TRUE(audioh_set_config(AUDIO_DEV_ADDR, AUDIO_AC_ITF));
  tu_unaligned_write16(control_xfer.buffer, tu_htole16(1));
  control_xfer.actual_len = 2;
  complete_control_xfer(XFER_RESULT_SUCCESS);

  TEST_ASSERT_FALSE(tuh_audio_mounted(0));
  TEST_ASSERT_EQUAL_UINT8(0, tuh_audio_get_dev_addr(0));
}

void test_audio_host_treats_implicit_feedback_as_audio_in_endpoint(void) {
  open_descriptors(playback_with_implicit_feedback, sizeof(playback_with_implicit_feedback));

  TEST_ASSERT_EQUAL_UINT8(2, tuh_audio_stream_count(0));
  TEST_ASSERT_EQUAL(TUH_AUDIO_STREAM_PLAYBACK, tuh_audio_stream_direction(0, 0));
  TEST_ASSERT_EQUAL(TUH_AUDIO_STREAM_CAPTURE, tuh_audio_stream_direction(0, 1));
}

void test_audio_host_uac2_treats_no_sync_data_usage_as_audio_in_endpoint(void) {
  open_descriptors(uac2_capture_no_sync_data, sizeof(uac2_capture_no_sync_data));
  TEST_ASSERT_TRUE(audioh_set_config(AUDIO_DEV_ADDR, AUDIO_AC_ITF));
  complete_uac2_clock_range(44100, 48000);

  TEST_ASSERT_TRUE(tuh_audio_mounted(0));
  TEST_ASSERT_EQUAL_UINT8(1, tuh_audio_stream_count(0));
  TEST_ASSERT_EQUAL(TUH_AUDIO_STREAM_CAPTURE, tuh_audio_stream_direction(0, 0));
  TEST_ASSERT_EQUAL_UINT8(2, tuh_audio_config_count(0, 0));
}

void test_audio_host_ignores_second_data_endpoint_in_same_as_interface(void) {
  open_descriptors(playback_with_extra_data_endpoint, sizeof(playback_with_extra_data_endpoint));

  TEST_ASSERT_EQUAL_UINT8(1, tuh_audio_stream_count(0));
  TEST_ASSERT_EQUAL(TUH_AUDIO_STREAM_PLAYBACK, tuh_audio_stream_direction(0, 0));
}

void test_audio_host_maps_playback_fu_declared_before_usb_input_terminal(void) {
  open_descriptors(playback_fu_before_terminal, sizeof(playback_fu_before_terminal));

  TEST_ASSERT_TRUE(audioh_set_config(AUDIO_DEV_ADDR, AUDIO_AC_ITF));
  TEST_ASSERT_EQUAL_HEX16(tu_u16(PLAYBACK_FU, AUDIO_AC_ITF), tu_le16toh(control_request.wIndex));
}

void test_audio_host_maps_relevant_terminal_and_fu_after_unrelated_entities(void) {
  open_descriptors(playback_after_unrelated_ac_entities, sizeof(playback_after_unrelated_ac_entities));

  TEST_ASSERT_EQUAL_UINT8(1, tuh_audio_stream_count(0));
  TEST_ASSERT_TRUE(audioh_set_config(AUDIO_DEV_ADDR, AUDIO_AC_ITF));
  TEST_ASSERT_EQUAL_HEX16(tu_u16(PLAYBACK_FU, AUDIO_AC_ITF), tu_le16toh(control_request.wIndex));
}

void test_audio_host_maps_capture_fu_declared_before_usb_output_terminal(void) {
  uint8_t        captured[CFG_TUH_AUDIO_STREAM_BUFSIZE];
  const uint16_t fifo_depth = CFG_TUH_AUDIO_STREAM_BUFSIZE - (CFG_TUH_AUDIO_STREAM_BUFSIZE % 3);
  open_descriptors(capture_fu_before_usb_output, sizeof(capture_fu_before_usb_output));
  TEST_ASSERT_TRUE(audioh_set_config(AUDIO_DEV_ADDR, AUDIO_AC_ITF));
  TEST_ASSERT_EQUAL_HEX16(tu_u16(CAPTURE_FU, AUDIO_AC_ITF), tu_le16toh(control_request.wIndex));
  complete_mount();

  TEST_ASSERT_EQUAL(TUH_AUDIO_STREAM_CAPTURE, tuh_audio_stream_direction(0, 0));
  TEST_ASSERT_TRUE(tuh_audio_mute_supported(0, 0));

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
    complete_edpt(0x81);
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
  TEST_ASSERT_TRUE(audioh_set_config(AUDIO_DEV_ADDR, AUDIO_AC_ITF));
  TEST_ASSERT_EQUAL_HEX16(tu_u16(PLAYBACK_FU, AUDIO_AC_ITF), tu_le16toh(control_request.wIndex));
  complete_control_xfer_with_u16((uint16_t)(-90 * 256));
  complete_control_xfer_with_u16(6 * 256);
  complete_control_xfer_with_u16(256);
  TEST_ASSERT_EQUAL_HEX16(tu_u16(CAPTURE_FU, AUDIO_AC_ITF), tu_le16toh(control_request.wIndex));
}

void test_audio_host_reads_volume_ranges_for_both_streams_before_mount(void) {
  const uint8_t feature_units[] = {PLAYBACK_FU, CAPTURE_FU};
  open_descriptors(duplex_fus_before_usb_terminals, sizeof(duplex_fus_before_usb_terminals));

  TEST_ASSERT_TRUE(audioh_set_config(AUDIO_DEV_ADDR, AUDIO_AC_ITF));
  for (uint8_t i = 0; i < TU_ARRAY_SIZE(feature_units); i++) {
    TEST_ASSERT_EQUAL_HEX8(AUDIO10_CS_REQ_GET_MIN, control_request.bRequest);
    TEST_ASSERT_EQUAL_HEX16(tu_u16(feature_units[i], AUDIO_AC_ITF), tu_le16toh(control_request.wIndex));
    complete_control_xfer_with_u16((uint16_t)(-90 * 256));
    complete_control_xfer_with_u16(6 * 256);
    complete_control_xfer_with_u16(256);
    TEST_ASSERT_EQUAL(i == TU_ARRAY_SIZE(feature_units) - 1, tuh_audio_mounted(0));
  }
  TEST_ASSERT_EQUAL_UINT8(6, control_xfer_count);
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
  complete_edpt(0x81);
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
  TEST_ASSERT_EQUAL_UINT8(1, event_cb_count);
  TEST_ASSERT_EQUAL_UINT8(0, event_cb_idx);
  TEST_ASSERT_EQUAL_UINT8(0, event_cb_stream_idx);
  TEST_ASSERT_EQUAL(TUH_AUDIO_EVENT_START_COMPLETE, event_cb_event);
  TEST_ASSERT_EQUAL(XFER_RESULT_FAILED, event_cb_result);

  TEST_ASSERT_TRUE(tuh_audio_start(0, 0));
  complete_interface_set(XFER_RESULT_SUCCESS);
  complete_control_xfer(XFER_RESULT_STALLED);
  TEST_ASSERT_EQUAL_UINT8(2, event_cb_count);
  TEST_ASSERT_EQUAL(TUH_AUDIO_EVENT_START_COMPLETE, event_cb_event);
  TEST_ASSERT_EQUAL(XFER_RESULT_STALLED, event_cb_result);

  control_xfer_result = false;
  TEST_ASSERT_TRUE(tuh_audio_start(0, 0));
  complete_interface_set(XFER_RESULT_SUCCESS);
  TEST_ASSERT_EQUAL_UINT8(3, event_cb_count);
  TEST_ASSERT_EQUAL(TUH_AUDIO_EVENT_START_COMPLETE, event_cb_event);
  TEST_ASSERT_EQUAL(XFER_RESULT_FAILED, event_cb_result);
}

void test_audio_host_reports_asynchronous_start_and_stop_completion(void) {
  mount_descriptors(capture_fu_before_usb_output, sizeof(capture_fu_before_usb_output));
  TEST_ASSERT_TRUE(tuh_audio_configure(0, 0, 0));

  TEST_ASSERT_TRUE(tuh_audio_start(0, 0));
  TEST_ASSERT_EQUAL_UINT8(0, event_cb_count);
  complete_interface_set(XFER_RESULT_SUCCESS);
  complete_control_xfer(XFER_RESULT_SUCCESS);
  TEST_ASSERT_EQUAL_UINT8(1, event_cb_count);
  TEST_ASSERT_EQUAL(TUH_AUDIO_EVENT_START_COMPLETE, event_cb_event);
  TEST_ASSERT_EQUAL(XFER_RESULT_SUCCESS, event_cb_result);

  TEST_ASSERT_TRUE(tuh_audio_stop(0, 0));
  TEST_ASSERT_EQUAL_UINT8(1, event_cb_count);
  complete_interface_set(XFER_RESULT_STALLED);
  TEST_ASSERT_EQUAL_UINT8(2, event_cb_count);
  TEST_ASSERT_EQUAL(TUH_AUDIO_EVENT_STOP_COMPLETE, event_cb_event);
  TEST_ASSERT_EQUAL(XFER_RESULT_STALLED, event_cb_result);
}

void test_audio_host_reports_capture_submission_failure(void) {
  mount_descriptors(capture_fu_before_usb_output, sizeof(capture_fu_before_usb_output));
  TEST_ASSERT_TRUE(tuh_audio_configure(0, 0, 0));

  edpt_xfer_result = false;
  TEST_ASSERT_TRUE(tuh_audio_start(0, 0));
  complete_interface_set(XFER_RESULT_SUCCESS);
  complete_control_xfer(XFER_RESULT_SUCCESS);

  TEST_ASSERT_EQUAL_UINT8(1, event_cb_count);
  TEST_ASSERT_EQUAL_UINT8(0, event_cb_idx);
  TEST_ASSERT_EQUAL_UINT8(0, event_cb_stream_idx);
  TEST_ASSERT_EQUAL(TUH_AUDIO_EVENT_START_COMPLETE, event_cb_event);
  TEST_ASSERT_EQUAL(XFER_RESULT_FAILED, event_cb_result);

  edpt_xfer_result = true;
  TEST_ASSERT_TRUE(tuh_audio_start(0, 0));
}

void test_audio_host_reports_playback_submission_failure(void) {
  mount_descriptors(playback_44100_max_packets_only, sizeof(playback_44100_max_packets_only));
  TEST_ASSERT_TRUE(tuh_audio_configure(0, 0, 0));

  edpt_xfer_result = false;
  TEST_ASSERT_TRUE(tuh_audio_start(0, 0));
  complete_interface_set(XFER_RESULT_SUCCESS);

  TEST_ASSERT_EQUAL_UINT8(1, event_cb_count);
  TEST_ASSERT_EQUAL_UINT8(0, event_cb_idx);
  TEST_ASSERT_EQUAL_UINT8(0, event_cb_stream_idx);
  TEST_ASSERT_EQUAL(TUH_AUDIO_EVENT_START_COMPLETE, event_cb_event);
  TEST_ASSERT_EQUAL(XFER_RESULT_FAILED, event_cb_result);

  edpt_xfer_result = true;
  TEST_ASSERT_TRUE(tuh_audio_start(0, 0));
}

void test_audio_host_reports_runtime_transfer_failure_without_byte_count(void) {
  mount_descriptors(capture_fu_before_usb_output, sizeof(capture_fu_before_usb_output));
  TEST_ASSERT_TRUE(tuh_audio_configure(0, 0, 0));
  TEST_ASSERT_TRUE(tuh_audio_start(0, 0));
  complete_interface_set(XFER_RESULT_SUCCESS);
  complete_control_xfer(XFER_RESULT_SUCCESS);
  TEST_ASSERT_EQUAL_UINT8(1, event_cb_count);

  complete_edpt(0x81);
  TEST_ASSERT_TRUE(audioh_xfer_cb(AUDIO_DEV_ADDR, 0x81, XFER_RESULT_TIMEOUT, 37));
  TEST_ASSERT_EQUAL_UINT8(2, event_cb_count);
  TEST_ASSERT_EQUAL_UINT8(0, event_cb_idx);
  TEST_ASSERT_EQUAL_UINT8(0, event_cb_stream_idx);
  TEST_ASSERT_EQUAL(TUH_AUDIO_EVENT_XFER_FAILED, event_cb_event);
  TEST_ASSERT_EQUAL(XFER_RESULT_TIMEOUT, event_cb_result);
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

  complete_edpt(0x81);
  TEST_ASSERT_TRUE(audioh_xfer_cb(AUDIO_DEV_ADDR, 0x81, XFER_RESULT_SUCCESS, 96));
  TEST_ASSERT_EQUAL_UINT8(2, edpt_xfer_count);
}

void test_audio_host_parses_discrete_frequencies_with_interval_greater_than_one(void) {
  tuh_audio_stream_config_t config;
  mount_descriptors(playback_with_two_frequencies, sizeof(playback_with_two_frequencies));

  TEST_ASSERT_EQUAL_UINT8(3, tuh_audio_config_count(0, 0));
  TEST_ASSERT_TRUE(tuh_audio_config_get(0, 0, 0, &config));
  TEST_ASSERT_EQUAL_UINT32(44100, config.sample_rate);
  TEST_ASSERT_TRUE(tuh_audio_config_get(0, 0, 1, &config));
  TEST_ASSERT_EQUAL_UINT32(44100, config.sample_rate);
  TEST_ASSERT_TRUE(tuh_audio_config_get(0, 0, 2, &config));
  TEST_ASSERT_EQUAL_UINT32(48000, config.sample_rate);

  // All public configurations share one AS mapping and preserve descriptor order.
  TEST_ASSERT_TRUE(tuh_audio_configure(0, 0, 2));
  TEST_ASSERT_TRUE(tuh_audio_start(0, 0));
  complete_interface_set(XFER_RESULT_SUCCESS);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(((uint8_t[]){U24_TO_U8S_LE(48000)}), control_buffer, 3);
}

void test_audio_host_retains_same_rate_in_different_alternate_settings(void) {
  tuh_audio_stream_config_t config;
  mount_descriptors(playback_with_two_alternates, sizeof(playback_with_two_alternates));

  TEST_ASSERT_EQUAL_UINT8(3, tuh_audio_config_count(0, 0));
  TEST_ASSERT_TRUE(tuh_audio_config_get(0, 0, 0, &config));
  TEST_ASSERT_EQUAL_UINT32(48000, config.sample_rate);
  TEST_ASSERT_TRUE(tuh_audio_config_get(0, 0, 1, &config));
  TEST_ASSERT_EQUAL_UINT32(48000, config.sample_rate);
  TEST_ASSERT_TRUE(tuh_audio_config_get(0, 0, 2, &config));
  TEST_ASSERT_EQUAL_UINT32(96000, config.sample_rate);
}

void test_audio_host_keeps_supported_stream_when_other_as_format_is_unsupported(void) {
  open_descriptors(playback_with_unsupported_capture, sizeof(playback_with_unsupported_capture));

  TEST_ASSERT_EQUAL_UINT8(1, tuh_audio_stream_count(0));
  TEST_ASSERT_EQUAL(TUH_AUDIO_STREAM_PLAYBACK, tuh_audio_stream_direction(0, 0));
}

void test_audio_host_rejects_sampling_frequency_range_and_releases_instance(void) {
  TEST_ASSERT_EQUAL_UINT16(0,
                           audioh_open(0, AUDIO_DEV_ADDR, (const tusb_desc_interface_t *)playback_with_frequency_range,
                                       sizeof(playback_with_frequency_range)));
  TEST_ASSERT_EQUAL_UINT8(0, tuh_audio_get_dev_addr(0));

  open_descriptors(playback_with_explicit_feedback, sizeof(playback_with_explicit_feedback));
  TEST_ASSERT_EQUAL_UINT8(1, tuh_audio_stream_count(0));
}

void test_audio_host_rejects_overflowed_frame_size_for_large_channel_count(void) {
  TEST_ASSERT_EQUAL_UINT16(0, audioh_open(0, AUDIO_DEV_ADDR,
                                          (const tusb_desc_interface_t *)capture_with_large_channel_count,
                                          sizeof(capture_with_large_channel_count)));
  TEST_ASSERT_EQUAL_UINT8(0, tuh_audio_get_dev_addr(0));
}

void test_audio_host_rejects_malformed_descriptors_and_releases_instance(void) {
  const struct {
    const uint8_t *desc;
    uint16_t       len;
  } malformed[] = {
    {malformed_zero_length_ac_descriptor, sizeof(malformed_zero_length_ac_descriptor)},
    {malformed_sampling_frequency_list, sizeof(malformed_sampling_frequency_list)},
    {malformed_short_endpoint, sizeof(malformed_short_endpoint)},
  };

  for (uint8_t i = 0; i < TU_ARRAY_SIZE(malformed); i++) {
    TEST_ASSERT_EQUAL_UINT16(0, audioh_open(0, AUDIO_DEV_ADDR, (const tusb_desc_interface_t *)malformed[i].desc,
                                            malformed[i].len));
    TEST_ASSERT_EQUAL_UINT8(0, tuh_audio_get_dev_addr(0));
  }
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

void test_audio_host_submits_generic_entity_control_request(void) {
  uint8_t payload[] = {0x34, 0x12, 0x56};
  mount_descriptors(playback_fu_before_terminal, sizeof(playback_fu_before_terminal));

  TEST_ASSERT_TRUE(tuh_audio_control_xfer(0, PLAYBACK_FU, TUSB_DIR_OUT, AUDIO10_CS_REQ_SET_CUR,
                                          AUDIO10_FU_CTRL_GRAPHIC_EQUALIZER, 2, payload, sizeof(payload),
                                          feature_unit_complete, 0x1234));
  TEST_ASSERT_EQUAL(TUSB_DIR_OUT, control_request.bmRequestType_bit.direction);
  TEST_ASSERT_EQUAL(TUSB_REQ_TYPE_CLASS, control_request.bmRequestType_bit.type);
  TEST_ASSERT_EQUAL(TUSB_REQ_RCPT_INTERFACE, control_request.bmRequestType_bit.recipient);
  TEST_ASSERT_EQUAL_HEX8(AUDIO10_CS_REQ_SET_CUR, control_request.bRequest);
  TEST_ASSERT_EQUAL_HEX16(tu_u16(AUDIO10_FU_CTRL_GRAPHIC_EQUALIZER, 2), tu_le16toh(control_request.wValue));
  TEST_ASSERT_EQUAL_HEX16(tu_u16(PLAYBACK_FU, AUDIO_AC_ITF), tu_le16toh(control_request.wIndex));
  TEST_ASSERT_EQUAL_UINT16(sizeof(payload), tu_le16toh(control_request.wLength));
  TEST_ASSERT_EQUAL_HEX8_ARRAY(payload, control_buffer, sizeof(payload));

  complete_control_xfer(XFER_RESULT_SUCCESS);
  TEST_ASSERT_EQUAL_UINT8(1, fu_cb_count);
  TEST_ASSERT_EQUAL_HEX32(0x1234, fu_cb_user_data);

  TEST_ASSERT_FALSE(tuh_audio_control_xfer(0, 0, TUSB_DIR_OUT, AUDIO10_CS_REQ_SET_CUR, AUDIO10_FU_CTRL_MUTE, 0, payload,
                                           1, NULL, 0));
  TEST_ASSERT_FALSE(tuh_audio_control_xfer(0, PLAYBACK_FU, TUSB_DIR_OUT, AUDIO10_CS_REQ_SET_CUR, AUDIO10_FU_CTRL_MUTE,
                                           0, NULL, 1, NULL, 0));
}

void test_audio_host_sync_entity_control_request_returns_actual_length(void) {
  uint8_t  payload[8] = {0};
  uint32_t actual_len = UINT32_MAX;
  mount_descriptors(playback_fu_before_terminal, sizeof(playback_fu_before_terminal));

  control_sync_actual_len = 3;
  TEST_ASSERT_EQUAL(XFER_RESULT_SUCCESS,
                    tuh_audio_control_xfer_sync(0, PLAYBACK_FU, TUSB_DIR_IN, AUDIO10_CS_REQ_GET_CUR,
                                                AUDIO10_FU_CTRL_GRAPHIC_EQUALIZER, 0, payload, sizeof(payload),
                                                &actual_len));
  TEST_ASSERT_EQUAL_UINT32(3, actual_len);

  control_xfer_result = false;
  actual_len          = UINT32_MAX;
  TEST_ASSERT_EQUAL(XFER_RESULT_TIMEOUT,
                    tuh_audio_control_xfer_sync(0, PLAYBACK_FU, TUSB_DIR_IN, AUDIO10_CS_REQ_GET_CUR,
                                                AUDIO10_FU_CTRL_GRAPHIC_EQUALIZER, 0, payload, sizeof(payload),
                                                &actual_len));
  TEST_ASSERT_EQUAL_UINT32(0, actual_len);
}

void test_audio_host_reads_and_caches_feature_unit_controls_before_mount(void) {
  tuh_audio_volume_range_t range;
  open_descriptors(playback_fu_before_terminal, sizeof(playback_fu_before_terminal));

  TEST_ASSERT_TRUE(audioh_set_config(AUDIO_DEV_ADDR, AUDIO_AC_ITF));
  TEST_ASSERT_FALSE(tuh_audio_mounted(0));
  TEST_ASSERT_EQUAL_UINT8(1, control_xfer_count);
  TEST_ASSERT_EQUAL_HEX8(AUDIO10_CS_REQ_GET_MIN, control_request.bRequest);
  TEST_ASSERT_EQUAL_HEX16(tu_u16(AUDIO10_FU_CTRL_VOLUME, 0), tu_le16toh(control_request.wValue));
  TEST_ASSERT_EQUAL_HEX16(tu_u16(PLAYBACK_FU, AUDIO_AC_ITF), tu_le16toh(control_request.wIndex));
  TEST_ASSERT_EQUAL_UINT16(2, tu_le16toh(control_request.wLength));

  complete_control_xfer_with_u16((uint16_t)(-90 * 256));
  TEST_ASSERT_EQUAL_UINT8(2, control_xfer_count);
  TEST_ASSERT_EQUAL_HEX8(AUDIO10_CS_REQ_GET_MAX, control_request.bRequest);
  TEST_ASSERT_FALSE(tuh_audio_mounted(0));

  complete_control_xfer_with_u16(6 * 256);
  TEST_ASSERT_EQUAL_UINT8(3, control_xfer_count);
  TEST_ASSERT_EQUAL_HEX8(AUDIO10_CS_REQ_GET_RES, control_request.bRequest);
  TEST_ASSERT_FALSE(tuh_audio_mounted(0));

  complete_control_xfer_with_u16(256);
  TEST_ASSERT_TRUE(tuh_audio_mounted(0));
  TEST_ASSERT_TRUE(tuh_audio_mute_supported(0, 0));
  TEST_ASSERT_TRUE(tuh_audio_volume_range_get(0, 0, &range));
  TEST_ASSERT_EQUAL_INT16(-90 * 256, range.min);
  TEST_ASSERT_EQUAL_INT16(6 * 256, range.max);
  TEST_ASSERT_EQUAL_UINT16(256, range.res);
}

void test_audio_host_mounts_with_mute_only_when_volume_range_fails(void) {
  tuh_audio_volume_range_t range;
  open_descriptors(playback_fu_before_terminal, sizeof(playback_fu_before_terminal));

  TEST_ASSERT_TRUE(audioh_set_config(AUDIO_DEV_ADDR, AUDIO_AC_ITF));
  complete_control_xfer_with_u16((uint16_t)(-40 * 256));
  complete_control_xfer(XFER_RESULT_STALLED);

  TEST_ASSERT_EQUAL_UINT8(2, control_xfer_count);
  TEST_ASSERT_TRUE(tuh_audio_mounted(0));
  TEST_ASSERT_TRUE(tuh_audio_mute_supported(0, 0));
  TEST_ASSERT_FALSE(tuh_audio_volume_range_get(0, 0, &range));
}

void test_audio_host_rejects_invalid_cached_volume_range(void) {
  tuh_audio_volume_range_t range;
  open_descriptors(playback_fu_before_terminal, sizeof(playback_fu_before_terminal));

  TEST_ASSERT_TRUE(audioh_set_config(AUDIO_DEV_ADDR, AUDIO_AC_ITF));
  complete_control_xfer_with_u16((uint16_t)(-20 * 256));
  complete_control_xfer_with_u16(0);
  complete_control_xfer_with_u16(0);

  TEST_ASSERT_TRUE(tuh_audio_mounted(0));
  TEST_ASSERT_TRUE(tuh_audio_mute_supported(0, 0));
  TEST_ASSERT_FALSE(tuh_audio_volume_range_get(0, 0, &range));
}

void test_audio_host_ignores_feature_unit_without_master_mute_or_volume(void) {
  tuh_audio_volume_range_t range;
  mount_descriptors(playback_fu_without_mute_volume, sizeof(playback_fu_without_mute_volume));

  TEST_ASSERT_FALSE(tuh_audio_mute_supported(0, 0));
  TEST_ASSERT_FALSE(tuh_audio_volume_range_get(0, 0, &range));
}

void test_audio_host_typed_mute_and_volume_controls(void) {
  bool                     mute   = false;
  int16_t                  volume = 0;
  tuh_audio_volume_range_t range;
  mount_descriptors(playback_fu_before_terminal, sizeof(playback_fu_before_terminal));

  TEST_ASSERT_TRUE(tuh_audio_volume_range_get(0, 0, &range));
  TEST_ASSERT_FALSE(tuh_audio_volume_set(0, 0, (int16_t)(range.max + 1), NULL, 0));

  TEST_ASSERT_TRUE(tuh_audio_mute_set(0, 0, true, feature_unit_complete, 0x1234));
  TEST_ASSERT_EQUAL_HEX8(AUDIO10_CS_REQ_SET_CUR, control_request.bRequest);
  TEST_ASSERT_EQUAL_HEX16(tu_u16(AUDIO10_FU_CTRL_MUTE, 0), tu_le16toh(control_request.wValue));
  TEST_ASSERT_EQUAL_UINT16(1, tu_le16toh(control_request.wLength));
  TEST_ASSERT_EQUAL_HEX8(1, control_buffer[0]);
  TEST_ASSERT_FALSE(tuh_audio_volume_set(0, 0, -6 * 256, feature_unit_complete, 0));
  complete_control_xfer(XFER_RESULT_SUCCESS);

  TEST_ASSERT_TRUE(tuh_audio_mute_get(0, 0, &mute, feature_unit_complete, 0x2345));
  TEST_ASSERT_EQUAL_HEX8(AUDIO10_CS_REQ_GET_CUR, control_request.bRequest);
  control_xfer.buffer[0]  = 1;
  control_xfer.actual_len = 1;
  complete_control_xfer(XFER_RESULT_SUCCESS);
  TEST_ASSERT_TRUE(mute);

  TEST_ASSERT_TRUE(tuh_audio_volume_set(0, 0, -6 * 256, feature_unit_complete, 0x3456));
  TEST_ASSERT_EQUAL_HEX16(tu_u16(AUDIO10_FU_CTRL_VOLUME, 0), tu_le16toh(control_request.wValue));
  TEST_ASSERT_EQUAL_UINT16(2, tu_le16toh(control_request.wLength));
  TEST_ASSERT_EQUAL_HEX8_ARRAY(((uint8_t[]){0x00, 0xFA}), control_buffer, 2);
  complete_control_xfer(XFER_RESULT_SUCCESS);

  TEST_ASSERT_TRUE(tuh_audio_volume_get(0, 0, &volume, feature_unit_complete, 0x4567));
  complete_control_xfer_with_u16((uint16_t)(-12 * 256));
  TEST_ASSERT_EQUAL_INT16(-12 * 256, volume);
  TEST_ASSERT_EQUAL_UINT8(4, fu_cb_count);
  TEST_ASSERT_EQUAL_HEX32(0x4567, fu_cb_user_data);
}

void test_audio_host_volume_set_accepts_silence_and_rounds_unaligned_values(void) {
  tuh_audio_volume_range_t range;
  mount_descriptors(playback_fu_before_terminal, sizeof(playback_fu_before_terminal));
  TEST_ASSERT_TRUE(tuh_audio_volume_range_get(0, 0, &range));

  TEST_ASSERT_TRUE(tuh_audio_volume_set(0, 0, (int16_t)(-6 * 256 + 100), feature_unit_complete, 0));
  TEST_ASSERT_EQUAL_UINT8(1, control_xfer_count);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(((uint8_t[]){0x00, 0xFA}), control_buffer, 2);
  complete_control_xfer(XFER_RESULT_SUCCESS);

  TEST_ASSERT_TRUE(tuh_audio_volume_set(0, 0, TUH_AUDIO_VOLUME_SILENCE, feature_unit_complete, 0));
  TEST_ASSERT_EQUAL_UINT8(2, control_xfer_count);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(((uint8_t[]){0x00, 0x80}), control_buffer, 2);
  complete_control_xfer(XFER_RESULT_SUCCESS);
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
    complete_edpt(0x01);
    TEST_ASSERT_TRUE(audioh_xfer_cb(AUDIO_DEV_ADDR, 0x01, XFER_RESULT_SUCCESS, edpt_xfer_bytes[edpt_xfer_count - 1]));
  }
  TEST_ASSERT_EQUAL_UINT32(185, tuh_audio_write(0, 0, samples, 185));
  while (edpt_xfer_count < 10) {
    complete_edpt(0x01);
    TEST_ASSERT_TRUE(audioh_xfer_cb(AUDIO_DEV_ADDR, 0x01, XFER_RESULT_SUCCESS, edpt_xfer_bytes[edpt_xfer_count - 1]));
  }

  for (uint8_t i = 0; i < 9; i++) {
    TEST_ASSERT_EQUAL_UINT16(176, edpt_xfer_bytes[i]);
  }
  TEST_ASSERT_EQUAL_UINT16(180, edpt_xfer_bytes[9]);
}

void test_audio_host_applies_10_14_feedback_after_fractional_scheduling_loop(void) {
  mount_descriptors(playback_44100_with_feedback_10_14, sizeof(playback_44100_with_feedback_10_14));

  TEST_ASSERT_TRUE(tuh_audio_configure(0, 0, 0));
  TEST_ASSERT_TRUE(tuh_audio_start(0, 0));
  complete_interface_set(XFER_RESULT_SUCCESS);
  TEST_ASSERT_EQUAL_UINT16(3, edpt_xfer_bytes_at(0x81, 0));
  TEST_ASSERT_EQUAL_UINT16(176, edpt_xfer_bytes_at(0x01, 0));

  // Request 44 frames/ms after the first nominal 44.1-kHz packet. The old
  // cycle must still emit its 45-frame correction packet before changing rate.
  complete_feedback_xfer(44u << 16, 3);
  for (uint8_t i = 0; i < 10; i++) {
    complete_playback_xfer();
  }

  for (uint8_t i = 0; i < 9; i++) {
    TEST_ASSERT_EQUAL_UINT16(176, edpt_xfer_bytes_at(0x01, i));
  }
  TEST_ASSERT_EQUAL_UINT16(180, edpt_xfer_bytes_at(0x01, 9));
  TEST_ASSERT_EQUAL_UINT16(176, edpt_xfer_bytes_at(0x01, 10));
}

void test_audio_host_accepts_16_16_feedback(void) {
  mount_descriptors(playback_44100_with_feedback_16_16, sizeof(playback_44100_with_feedback_16_16));

  TEST_ASSERT_TRUE(tuh_audio_configure(0, 0, 0));
  TEST_ASSERT_TRUE(tuh_audio_start(0, 0));
  complete_interface_set(XFER_RESULT_SUCCESS);
  TEST_ASSERT_EQUAL_UINT16(4, edpt_xfer_bytes_at(0x81, 0));

  complete_feedback_xfer(45u << 16, 4);
  for (uint8_t i = 0; i < 10; i++) {
    complete_playback_xfer();
  }

  TEST_ASSERT_EQUAL_UINT16(180, edpt_xfer_bytes_at(0x01, 9));
  TEST_ASSERT_EQUAL_UINT16(180, edpt_xfer_bytes_at(0x01, 10));
}

void test_audio_host_preserves_high_speed_feedback_fraction_across_updates(void) {
  test_speed = TUSB_SPEED_HIGH;
  mount_descriptors(playback_44100_with_feedback_16_16, sizeof(playback_44100_with_feedback_16_16));

  TEST_ASSERT_TRUE(tuh_audio_configure(0, 0, 0));
  TEST_ASSERT_TRUE(tuh_audio_start(0, 0));
  complete_interface_set(XFER_RESULT_SUCCESS);

  // The captured feedback is 5.557 frames per microframe. Repeating the
  // update every millisecond must not discard the scheduler's remainder.
  const uint32_t feedback_q16 = 0x00058eafu;
  for (uint8_t packet = 0; packet < 80; packet++) {
    if ((packet % 8u) == 0) {
      complete_feedback_xfer(feedback_q16, 4);
    }
    if (packet < 79) {
      complete_playback_xfer();
    }
  }

  uint16_t total_frames = 0;
  for (uint8_t packet = 0; packet < 80; packet++) {
    total_frames += edpt_xfer_bytes_at(0x01, packet) / 4u;
  }
  TEST_ASSERT_EQUAL_UINT16(444, total_frames);
}

void test_audio_host_ignores_out_of_range_feedback(void) {
  mount_descriptors(playback_44100_with_feedback_16_16, sizeof(playback_44100_with_feedback_16_16));

  TEST_ASSERT_TRUE(tuh_audio_configure(0, 0, 0));
  TEST_ASSERT_TRUE(tuh_audio_start(0, 0));
  complete_interface_set(XFER_RESULT_SUCCESS);

  complete_feedback_xfer(50u << 16, 4);
  for (uint8_t i = 0; i < 10; i++) {
    complete_playback_xfer();
  }

  TEST_ASSERT_EQUAL_UINT16(180, edpt_xfer_bytes_at(0x01, 9));
  TEST_ASSERT_EQUAL_UINT16(176, edpt_xfer_bytes_at(0x01, 10));
}

void test_audio_host_uses_exponential_full_speed_iso_interval(void) {
  uint8_t samples[441 * 4] = {0};
  mount_descriptors(playback_11025_interval4, sizeof(playback_11025_interval4));

  TEST_ASSERT_TRUE(tuh_audio_configure(0, 0, 0));
  TEST_ASSERT_TRUE(tuh_audio_start(0, 0));
  TEST_ASSERT_EQUAL_UINT32(256, tuh_audio_write(0, 0, samples, 256));
  complete_interface_set(XFER_RESULT_SUCCESS);

  while (edpt_xfer_count < 5) {
    complete_edpt(0x01);
    TEST_ASSERT_TRUE(audioh_xfer_cb(AUDIO_DEV_ADDR, 0x01, XFER_RESULT_SUCCESS, edpt_xfer_bytes[edpt_xfer_count - 1]));
  }
  TEST_ASSERT_EQUAL_UINT32(185, tuh_audio_write(0, 0, samples, 185));
  while (edpt_xfer_count < 10) {
    complete_edpt(0x01);
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
  complete_edpt(0x01);
  TEST_ASSERT_TRUE(audioh_xfer_cb(AUDIO_DEV_ADDR, 0x01, XFER_RESULT_SUCCESS, edpt_xfer_bytes[0]));

  TEST_ASSERT_EQUAL_UINT8(2, edpt_xfer_count);
  TEST_ASSERT_EQUAL_UINT16(176, edpt_xfer_bytes[1]);
  TEST_ASSERT_EACH_EQUAL_HEX8(0, edpt_xfer_data[1], sizeof(edpt_xfer_data[1]));

  TEST_ASSERT_EQUAL_UINT32(43, tuh_audio_write(0, 0, more_samples, 43));
  complete_edpt(0x01);
  TEST_ASSERT_TRUE(audioh_xfer_cb(AUDIO_DEV_ADDR, 0x01, XFER_RESULT_SUCCESS, edpt_xfer_bytes[1]));

  TEST_ASSERT_EQUAL_UINT8(3, edpt_xfer_count);
  TEST_ASSERT_EQUAL_UINT16(176, edpt_xfer_bytes[2]);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(sample, edpt_xfer_data[2], sizeof(sample));
}

/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 Zhenjiang Zhang
 * SPDX-License-Identifier: MIT
 *
 * This file is part of the TinyUSB stack.
 */

/*
 * This driver implements a USB Audio Host (UAC 1.0) class driver with a
 * WASAPI/ALSA-like high-level streaming API. The USB Audio topology (Audio
 * Control interface, Audio Streaming interfaces, alternate settings, and
 * endpoints) is kept private to the driver.
 *
 * Each instance (Audio Control interface) provides at most one logical stream
 * per direction:
 * - capture stream (TUSB_DIR_IN): device -> host, filled by isochronous IN
 *   transfers scheduled by the driver into a FIFO, drained by the application
 *   with tuh_audio_read()
 * - playback stream (TUSB_DIR_OUT): host -> device, drained by isochronous
 *   OUT transfers from a FIFO filled by the application with tuh_audio_write()
 *
 * While a stream is running, the driver keeps one isochronous transfer in
 * flight at the endpoint's polling cadence and re-submits on completion. The
 * FIFO + endpoint-claim pattern is modeled after the tu_edpt_stream helper
 * used by the MIDI host driver: the application's frame-based read/write is
 * decoupled from the USB transfer cadence, and only whole frames are ever
 * queued or transferred. Completion of each transfer is reported through
 * tuh_audio_capture_cb()/tuh_audio_playback_cb(), failures through
 * tuh_audio_err_cb().
 *
 * The supported configurations of all Audio Streaming interfaces and alternate
 * settings in one direction are combined into a flat list of discrete
 * {format, sample_rate, channels} tuples. The driver keeps the mapping from
 * each configuration to its interface, alternate setting, and endpoint, and
 * applies it when the application calls tuh_audio_configure().
 *
 * Non-PCM formats and sampling-frequency ranges are not registered as
 * supported configurations during enumeration.
 *
 * The driver owns:
 * 1. Endpoint selection and opening; only the alternate setting selected by
 *    tuh_audio_configure() is activated by tuh_audio_start().
 * 2. Endpoint sampling-frequency control (SET_CUR, 3 bytes little-endian).
 */

#include "tusb_option.h"

#if (CFG_TUH_ENABLED && CFG_TUH_AUDIO)

#include "host/usbh.h"
#include "host/usbh_pvt.h"
#include "audio_host.h"

// Level where CFG_TUSB_DEBUG must be at least for this driver is logged
#ifndef CFG_TUH_AUDIO_LOG_LEVEL
  #define CFG_TUH_AUDIO_LOG_LEVEL CFG_TUH_LOG_LEVEL
#endif

#define TU_LOG_DRV(...) TU_LOG(CFG_TUH_AUDIO_LOG_LEVEL, __VA_ARGS__)


//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+

// Maximum number of supported configurations per stream (per direction)
#define AUDIOH_MAX_CONFIGS (CFG_TUH_AUDIO_MAX_AS * CFG_TUH_AUDIO_MAX_SAM_FREQ)

//--------------------------------------------------------------------+
// Weak stubs: invoked if no strong implementation is available
//--------------------------------------------------------------------+

TU_ATTR_WEAK void tuh_audio_mount_cb(uint8_t idx) {
  (void)idx;
}

TU_ATTR_WEAK void tuh_audio_umount_cb(uint8_t idx) {
  (void)idx;
}

TU_ATTR_WEAK void tuh_audio_capture_cb(uint8_t idx, uint8_t stream_idx, uint16_t xferred_bytes) {
  (void)idx;
  (void)stream_idx;
  (void)xferred_bytes;
}

TU_ATTR_WEAK void tuh_audio_playback_cb(uint8_t idx, uint8_t stream_idx, uint16_t xferred_bytes) {
  (void)idx;
  (void)stream_idx;
  (void)xferred_bytes;
}

TU_ATTR_WEAK void tuh_audio_err_cb(uint8_t idx, uint8_t stream_idx, uint16_t xferred_bytes) {
  (void)idx;
  (void)stream_idx;
  (void)xferred_bytes;
}

// Stream state machine
enum {
  STREAM_STATE_IDLE = 0, // not configured
  STREAM_STATE_READY     // configured, ready to start/stop
};

// Hardware mapping of one supported configuration
typedef struct {
  uint8_t  itf_num;       // Audio Streaming interface number
  uint8_t  alt_setting;   // alternate setting that provides this configuration
  uint8_t  ep_addr;       // isochronous endpoint address
  uint16_t ep_size;       // endpoint max packet size
  uint8_t  ep_interval;   // endpoint bInterval
  uint8_t  ep_sync;       // bmAttributes sync type
  uint8_t  ep_usage;      // bmAttributes usage type
  bool     sam_freq_ctrl; // endpoint supports sampling-frequency control
} audioh_stream_map_t;

// One logical stream (capture or playback)
typedef struct {
  // instance info (set at init, preserved across close/open)
  uint8_t    idx;        // instance index
  uint8_t    stream_idx; // logical stream index within the instance
  tusb_dir_t dir;        // TUSB_DIR_IN = capture, TUSB_DIR_OUT = playback

  // device owning this stream (0 = no device)
  uint8_t daddr;

  // Supported configurations (parsed during enumeration)
  uint8_t                   config_count;
  tuh_audio_stream_config_t config[AUDIOH_MAX_CONFIGS];
  audioh_stream_map_t       map[AUDIOH_MAX_CONFIGS];

  // Active stream state
  uint8_t active_config; // index into config[]/map[], TUSB_INDEX_INVALID_8 when not configured
  uint8_t state;         // STREAM_STATE_*
  bool    running;       // tuh_audio_start() called, transfers may be submitted

  // One Feature Unit associated with this logical stream (0 = none)
  uint8_t feature_unit_id;

  // Size in bytes of one frame (all channels) of the active configuration
  uint16_t frame_bytes;

  // Playback pacing per endpoint poll interval. Fractional frames are
  // accumulated in millionths because the interval is expressed in us.
  uint16_t frames_per_interval;
  uint32_t frames_rem;
  uint32_t rem_acc;

  // FIFO + endpoint transfer helper (see tu_edpt_stream, used by the MIDI
  // host driver): the FIFO decouples the application's frame-based read/write
  // from the endpoint's isochronous transfer cadence. ep_buf is bound at init from
  // _audioh_epbuf[], the endpoint is bound by tu_edpt_stream_open() when the
  // stream is configured.
  tu_edpt_stream_t edpt;
  uint8_t          ff_buf[CFG_TUH_AUDIO_STREAM_BUFSIZE];
} tuh_audio_stream_t;

// Per-instance (Audio device) storage
typedef struct {
  uint8_t daddr;      // device address (0 = free slot)
  uint8_t ac_itf_num; // Audio Control interface number

  // Logical streams: playback first, then capture (stream index order)
  tuh_audio_stream_t out_stream;
  tuh_audio_stream_t in_stream;
  uint8_t            stream_count; // number of streams with supported configurations

  bool mounted;
} audioh_interface_t;

typedef struct {
  TUH_EPBUF_DEF(sam_freq, 4);                        // shared sampling-frequency SET data
  TUH_EPBUF_DEF(fu_ctrl, 8);                         // feature-unit SET data
  TUH_EPBUF_DEF(epin, CFG_TUH_AUDIO_EPIN_BUFSIZE);   // capture transfer buffer
  TUH_EPBUF_DEF(epout, CFG_TUH_AUDIO_EPOUT_BUFSIZE); // playback transfer buffer
  // Feature-unit request state: only one GET or SET in flight per device
  tuh_xfer_cb_t complete_cb;
  uintptr_t     user_data;
  uint16_t     *value;
  uint8_t       width;
  bool          fu_busy;
} audioh_epbuf_t;

static audioh_interface_t _audioh_itf[CFG_TUH_AUDIO_MAX];

CFG_TUH_MEM_SECTION static audioh_epbuf_t _audioh_epbuf[CFG_TUH_AUDIO_MAX];

//--------------------------------------------------------------------+
// Helper
//--------------------------------------------------------------------+
TU_ATTR_ALWAYS_INLINE static inline uint8_t find_new_audio_index(void) {
  for (uint8_t idx = 0; idx < CFG_TUH_AUDIO_MAX; idx++) {
    if (_audioh_itf[idx].daddr == 0) {
      return idx;
    }
  }
  return TUSB_INDEX_INVALID_8;
}

static tuh_audio_stream_t *audioh_get_stream(audioh_interface_t *p_audio, tusb_dir_t direction) {
  switch (direction) {
    case TUSB_DIR_IN:
      return &p_audio->in_stream;
    case TUSB_DIR_OUT:
      return &p_audio->out_stream;
    default:
      return NULL;
  }
}

// Look up a stream by its logical index within the instance
static tuh_audio_stream_t *audioh_get_stream_by_idx(audioh_interface_t *p_audio, uint8_t stream_idx) {
  for (uint8_t i = 0; i < 2; i++) {
    tuh_audio_stream_t *s = (i == 0) ? &p_audio->out_stream : &p_audio->in_stream;
    if (s->config_count > 0 && s->stream_idx == stream_idx) {
      return s;
    }
  }
  return NULL;
}

// Map a UAC 1.0 (subframe size, bit resolution) pair to a supported format
static bool audioh_format_from_uac1(uint8_t subframe_size, uint8_t bit_resolution, tuh_audio_format_t *format) {
  if (subframe_size == 1 && bit_resolution == 8) {
    *format = TUH_AUDIO_FORMAT_S8;
  } else if (subframe_size == 2 && bit_resolution == 16) {
    *format = TUH_AUDIO_FORMAT_S16_LE;
  } else if (subframe_size == 3 && bit_resolution == 24) {
    *format = TUH_AUDIO_FORMAT_S24_3LE;
  } else if (subframe_size == 4 && bit_resolution == 24) {
    *format = TUH_AUDIO_FORMAT_S24_LE;
  } else if (subframe_size == 4 && bit_resolution == 32) {
    *format = TUH_AUDIO_FORMAT_S32_LE;
  } else {
    return false;
  }
  return true;
}

// Endpoint poll interval in microseconds: full-speed bInterval is in 1 ms
// frames, high-speed isochronous bInterval is a power-of-2 exponent of
// 125 us microframes
static uint32_t audioh_interval_us(uint8_t ep_interval, uint8_t daddr) {
  if (tuh_speed_get(daddr) == TUSB_SPEED_HIGH) {
    return ((uint32_t)1u << (ep_interval - 1)) * 125u;
  }
  return (uint32_t)ep_interval * 1000u;
}

// UAC 1.0 feature-unit control value width (0 = unsupported variable/unknown width)
static uint8_t audioh_fu_control_width(uint8_t control_selector) {
  switch (control_selector) {
    case AUDIO10_FU_CTRL_MUTE:
    case AUDIO10_FU_CTRL_BASS:
    case AUDIO10_FU_CTRL_MID:
    case AUDIO10_FU_CTRL_TREBLE:
    case AUDIO10_FU_CTRL_AGC:
    case AUDIO10_FU_CTRL_BASS_BOOST:
    case AUDIO10_FU_CTRL_LOUDNESS:
      return 1;
    case AUDIO10_FU_CTRL_VOLUME:
    case AUDIO10_FU_CTRL_DELAY:
      return 2;
    default:
      return 0;
  }
}

// Reset a stream to its unconfigured state (keeps idx, dir, and FIFO configuration)
static void audioh_stream_reset(tuh_audio_stream_t *s) {
  s->daddr               = 0;
  s->stream_idx          = TUSB_INDEX_INVALID_8;
  s->config_count        = 0;
  s->active_config       = TUSB_INDEX_INVALID_8;
  s->state               = STREAM_STATE_IDLE;
  s->running             = false;
  s->feature_unit_id     = 0;
  s->frame_bytes         = 0;
  s->frames_per_interval = 0;
  s->frames_rem          = 0;
  s->rem_acc             = 0;
  tu_edpt_stream_close(&s->edpt);
  tu_edpt_stream_clear(&s->edpt);
}

// Find the stream owning an endpoint (used to dispatch transfer completion)
static tuh_audio_stream_t *audioh_find_stream(uint8_t dev_addr, uint8_t ep_addr) {
  for (uint8_t idx = 0; idx < CFG_TUH_AUDIO_MAX; idx++) {
    audioh_interface_t *p_audio = &_audioh_itf[idx];
    for (uint8_t s = 0; s < 2; s++) {
      tuh_audio_stream_t *stream = (s == 0) ? &p_audio->in_stream : &p_audio->out_stream;
      if (stream->daddr == dev_addr && stream->active_config != TUSB_INDEX_INVALID_8 &&
          stream->map[stream->active_config].ep_addr == ep_addr) {
        return stream;
      }
    }
  }
  return NULL;
}

//--------------------------------------------------------------------+
// Packet scheduler
//--------------------------------------------------------------------+

// Re-arm the capture endpoint: request one full packet (the device sends at
// most its max packet size per poll interval). The overwritable FIFO retains
// the newest capture frames when the application cannot drain it in time.
static void audioh_stream_capture_xfer(tuh_audio_stream_t *s) {
  TU_VERIFY(s->state == STREAM_STATE_READY && s->running, );

  const audioh_stream_map_t *map = &s->map[s->active_config];
  TU_VERIFY(usbh_edpt_claim(s->daddr, map->ep_addr), ); // one transfer in flight

  // ep_size is guaranteed <= CFG_TUH_AUDIO_EPIN_BUFSIZE by enumeration
  TU_ASSERT(usbh_edpt_xfer(s->daddr, map->ep_addr, s->edpt.ep_buf, map->ep_size), );
}

// Submit the next queued playback packet. Fractional frames per endpoint poll
// interval are accumulated on each successful submission, keeping the average
// data rate exactly at the sample rate.
static void audioh_stream_playback_xfer(tuh_audio_stream_t *s) {
  TU_VERIFY(s->state == STREAM_STATE_READY && s->running, );

  const audioh_stream_map_t *map = &s->map[s->active_config];
  TU_VERIFY(usbh_edpt_claim(s->daddr, map->ep_addr), ); // one transfer in flight

  uint32_t frames       = s->frames_per_interval;
  uint32_t next_rem_acc = s->rem_acc + s->frames_rem;
  if (next_rem_acc >= 1000000) {
    next_rem_acc -= 1000000;
    frames++;
  }

  const uint64_t bytes_64 = (uint64_t)frames * s->frame_bytes;
  TU_ASSERT(bytes_64 <= map->ep_size && bytes_64 <= CFG_TUH_AUDIO_EPOUT_BUFSIZE &&
              bytes_64 <= CFG_TUH_AUDIO_STREAM_BUFSIZE, );
  const uint16_t bytes = (uint16_t)bytes_64;
  if (tu_fifo_count(&s->edpt.ff) < bytes) {
    // Keep the isochronous stream active without consuming a partial frame.
    // The queued audio is sent once a complete poll interval is available.
    tu_memclr(s->edpt.ep_buf, bytes);
  } else {
    tu_fifo_read_n(&s->edpt.ff, s->edpt.ep_buf, bytes);
  }

  TU_ASSERT(usbh_edpt_xfer(s->daddr, map->ep_addr, s->edpt.ep_buf, bytes), );
  s->rem_acc = next_rem_acc;
}

//--------------------------------------------------------------------+
// Configure state machine
//--------------------------------------------------------------------+

static bool audioh_stream_close_ep(tuh_audio_stream_t *s) {
  if (!tu_edpt_stream_is_opened(&s->edpt)) {
    return true;
  }

  const uint8_t ep_addr = s->edpt.ep_addr;
  if (!tuh_edpt_close(s->daddr, ep_addr)) {
    TU_LOG_DRV("  AUDIO close endpoint failed: addr=%u ep=%02x\r\n", s->daddr, ep_addr);
    return false;
  }

  tu_edpt_stream_close(&s->edpt);
  return true;
}

static void audioh_stream_fail(tuh_audio_stream_t *s) {
  (void)audioh_stream_close_ep(s);
  s->state         = STREAM_STATE_IDLE;
  s->active_config = TUSB_INDEX_INVALID_8;
  s->running       = false;
}

// Set the endpoint sampling frequency (3 bytes little-endian)
static bool audioh_stream_set_freq(tuh_audio_stream_t *s, tuh_xfer_cb_t complete_cb) {
  const audioh_stream_map_t       *map  = &s->map[s->active_config];
  const tuh_audio_stream_config_t *cfg  = &s->config[s->active_config];
  uint8_t                         *ctrl = _audioh_epbuf[s->idx].sam_freq;

  ctrl[0] = (uint8_t)(cfg->sample_rate & 0xFF);
  ctrl[1] = (uint8_t)((cfg->sample_rate >> 8) & 0xFF);
  ctrl[2] = (uint8_t)((cfg->sample_rate >> 16) & 0xFF);

  const tusb_control_request_t request =
    {.bmRequestType_bit = {.recipient = TUSB_REQ_RCPT_ENDPOINT, .type = TUSB_REQ_TYPE_CLASS, .direction = TUSB_DIR_OUT},
     .bRequest          = AUDIO10_CS_REQ_SET_CUR,
     .wValue            = tu_htole16(tu_u16(AUDIO10_EP_CTRL_SAMPLING_FREQ, 0)), // control selector, channel 0
     .wIndex            = tu_htole16(map->ep_addr),
     .wLength           = 3};

  tuh_xfer_t xfer = {.daddr       = s->daddr,
                     .ep_addr     = 0,
                     .setup       = &request,
                     .buffer      = ctrl,
                     .complete_cb = complete_cb,
                     .user_data   = (uintptr_t)s};
  return tuh_control_xfer(&xfer);
}

// Reconstruct the endpoint descriptor of the selected configuration and open it
static bool audioh_stream_open_ep(tuh_audio_stream_t *s) {
  const audioh_stream_map_t *map = &s->map[s->active_config];

  const tusb_desc_endpoint_t desc_ep = {.bLength          = sizeof(tusb_desc_endpoint_t),
                                        .bDescriptorType  = TUSB_DESC_ENDPOINT,
                                        .bEndpointAddress = map->ep_addr,
                                        .bmAttributes     = {.xfer  = TUSB_XFER_ISOCHRONOUS,
                                                             .sync  = map->ep_sync,
                                                             .usage = map->ep_usage},
                                        .wMaxPacketSize   = tu_htole16(map->ep_size),
                                        .bInterval        = map->ep_interval};

  if (!tuh_edpt_open(s->daddr, &desc_ep)) {
    TU_LOG_DRV("  AUDIO open endpoint failed: addr=%u ep=%02x\r\n", s->daddr, map->ep_addr);
    audioh_stream_fail(s);
    return false;
  }

  // Bind the transfer helper to the endpoint and start with an empty FIFO
  const uint16_t xfer_len = (s->dir == TUSB_DIR_IN) ? CFG_TUH_AUDIO_EPIN_BUFSIZE : CFG_TUH_AUDIO_EPOUT_BUFSIZE;
  tu_edpt_stream_open(&s->edpt, s->daddr, &desc_ep, xfer_len);
  tu_edpt_stream_clear(&s->edpt);
  s->state = STREAM_STATE_READY;
  return true;
}

//--------------------------------------------------------------------+
// USBH API
//--------------------------------------------------------------------+
bool audioh_init(void) {
  tu_memclr(&_audioh_itf, sizeof(_audioh_itf));

  for (uint8_t idx = 0; idx < CFG_TUH_AUDIO_MAX; idx++) {
    tuh_audio_stream_t *in  = &_audioh_itf[idx].in_stream;
    tuh_audio_stream_t *out = &_audioh_itf[idx].out_stream;

    in->idx  = idx;
    in->dir  = TUSB_DIR_IN;
    out->idx = idx;
    out->dir = TUSB_DIR_OUT;

    // Bind FIFO buffer and transfer buffer (see tu_edpt_stream_init)
    TU_VERIFY(tu_edpt_stream_init(&in->edpt, true, false, true, in->ff_buf, CFG_TUH_AUDIO_STREAM_BUFSIZE,
                                  _audioh_epbuf[idx].epin));
    TU_VERIFY(tu_edpt_stream_init(&out->edpt, true, true, false, out->ff_buf, CFG_TUH_AUDIO_STREAM_BUFSIZE,
                                  _audioh_epbuf[idx].epout));

    audioh_stream_reset(in);
    audioh_stream_reset(out);
  }
  return true;
}

bool audioh_deinit(void) {
  for (uint8_t idx = 0; idx < CFG_TUH_AUDIO_MAX; idx++) {
    tu_edpt_stream_deinit(&_audioh_itf[idx].in_stream.edpt);
    tu_edpt_stream_deinit(&_audioh_itf[idx].out_stream.edpt);
  }
  return true;
}

void audioh_close(uint8_t daddr) {
  for (uint8_t idx = 0; idx < CFG_TUH_AUDIO_MAX; idx++) {
    audioh_interface_t *p_audio = &_audioh_itf[idx];
    if (p_audio->daddr != daddr) {
      continue;
    }

    TU_LOG_DRV("  AUDIO close addr = %u index = %u\r\n", daddr, idx);
    if (p_audio->mounted) {
      tuh_audio_umount_cb(idx);
    }

    for (uint8_t s = 0; s < 2; s++) {
      tuh_audio_stream_t *stream = (s == 0) ? &p_audio->in_stream : &p_audio->out_stream;
      audioh_stream_reset(stream);
    }

    _audioh_epbuf[idx].complete_cb = NULL; // drop a pending feature-unit request
    _audioh_epbuf[idx].fu_busy     = false;

    p_audio->stream_count = 0;
    p_audio->daddr        = 0;
    p_audio->mounted      = false;
  }
}

bool audioh_xfer_cb(uint8_t dev_addr, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes) {
  tuh_audio_stream_t *s = audioh_find_stream(dev_addr, ep_addr);
  if (s == NULL) {
    return false;
  }

  // Failed, stalled, or aborted transfers never carry valid audio data
  if (result != XFER_RESULT_SUCCESS) {
    TU_LOG_DRV("  AUDIO transfer failed: addr=%u ep=%02x result=%u\r\n", dev_addr, ep_addr, result);
    s->running = false;
    tu_edpt_stream_clear(&s->edpt); // discard queued data
    tuh_audio_err_cb(s->idx, s->stream_idx, (uint16_t)xferred_bytes);
    return true;
  }

  // Stopped stream: the in-flight transfer completes and its data is discarded
  if (!s->running) {
    return true;
  }

  if (s->dir == TUSB_DIR_IN) {
    // Capture: move the received bytes into the FIFO (whole frames only),
    // notify, then re-arm for the next packet
    const uint16_t bytes = (uint16_t)(xferred_bytes - (xferred_bytes % s->frame_bytes));
    if (bytes > 0) {
      tu_fifo_write_n(&s->edpt.ff, s->edpt.ep_buf, bytes);
    }
    tuh_audio_capture_cb(s->idx, s->stream_idx, (uint16_t)xferred_bytes);
    audioh_stream_capture_xfer(s);
  } else {
    // Playback: notify, then submit the next queued packet
    tuh_audio_playback_cb(s->idx, s->stream_idx, (uint16_t)xferred_bytes);
    audioh_stream_playback_xfer(s);
  }
  return true;
}

//--------------------------------------------------------------------+
// Enumeration
//--------------------------------------------------------------------+

// Parse one Audio Streaming interface alternate setting and register its
// supported configurations into the matching stream. Returns the descriptor
// pointer of the next interface.
static const uint8_t *audioh_parse_as(audioh_interface_t *p_audio, const tusb_desc_interface_t *desc_itf,
                                      const uint8_t *p_desc, const uint8_t *desc_end) {
  const uint8_t itf_num = desc_itf->bInterfaceNumber;
  const uint8_t alt     = desc_itf->bAlternateSetting;

  p_desc = tu_desc_next(p_desc);

  // Alternate setting 0 has no endpoints: nothing to stream
  if (alt == 0 || desc_itf->bNumEndpoints == 0) {
    while (tu_desc_in_bounds(p_desc, desc_end) && tu_desc_type(p_desc) != TUSB_DESC_INTERFACE) {
      p_desc = tu_desc_next(p_desc);
    }
    return p_desc;
  }

  // Parse the class-specific and endpoint descriptors of this alternate setting
  uint16_t format_tag                           = 0;
  uint8_t  num_channels                         = 0;
  uint8_t  subframe_size                        = 0;
  uint8_t  bit_res                              = 0;
  uint8_t  sam_freq_count                       = 0;
  uint32_t sam_freq[CFG_TUH_AUDIO_MAX_SAM_FREQ] = {0};

  // An AS alternate setting has one audio data endpoint and may have one
  // explicit feedback endpoint. Implicit-feedback endpoints are data endpoints
  // and are handled normally when they are the AS interface's data endpoint.
  typedef struct {
    uint8_t  ep_addr;
    uint16_t ep_size;
    uint8_t  ep_interval;
    uint8_t  ep_sync;
    uint8_t  ep_usage;
    bool     sam_freq_ctrl;
  } audioh_ep_info_t;
  audioh_ep_info_t ep_info     = {0};
  bool             has_data_ep = false;
  // The CS_ENDPOINT descriptor carries the sampling-frequency control bit of
  // its endpoint. Devices differ in whether it precedes or follows the
  // standard endpoint descriptor, so attribute it in either order.
  bool pending_sam_freq_ctrl = false; // CS_ENDPOINT seen, applies to the next endpoint
  bool unassigned_ep         = false; // endpoint seen, applies to the next CS_ENDPOINT

  while (tu_desc_in_bounds(p_desc, desc_end) && tu_desc_type(p_desc) != TUSB_DESC_INTERFACE) {
    switch (tu_desc_type(p_desc)) {
      case TUSB_DESC_CS_INTERFACE: {
        switch (tu_desc_subtype(p_desc)) {
          case AUDIO10_CS_AS_INTERFACE_AS_GENERAL: {
            const audio10_desc_cs_as_interface_t *desc_as_general = (const audio10_desc_cs_as_interface_t *)p_desc;
            if (desc_as_general->bLength >= sizeof(audio10_desc_cs_as_interface_t)) {
              format_tag = tu_le16toh(desc_as_general->wFormatTag);
            }
            break;
          }
          case AUDIO10_CS_AS_INTERFACE_FORMAT_TYPE: {
            TU_ASSERT(p_desc[0] >= 8, p_desc);
            if (p_desc[3] != AUDIO10_FORMAT_TYPE_I) {
              break; // only Type I (PCM) is supported
            }
            num_channels  = p_desc[4];
            subframe_size = p_desc[5];
            bit_res       = p_desc[6];
            if (p_desc[7] > 0) {
              sam_freq_count = TU_MIN(p_desc[7], CFG_TUH_AUDIO_MAX_SAM_FREQ);
              for (uint8_t i = 0; i < sam_freq_count && (8 + i * 3 + 2) < p_desc[0]; i++) {
                sam_freq[i] = ((uint32_t)p_desc[8 + i * 3] | ((uint32_t)p_desc[9 + i * 3] << 8) |
                               ((uint32_t)p_desc[10 + i * 3] << 16));
              }
            }
            break;
          }
          default:
            break;
        }
        break;
      }
      case TUSB_DESC_CS_ENDPOINT: {
        if (tu_desc_subtype(p_desc) == AUDIO10_CS_EP_SUBTYPE_GENERAL && p_desc[0] >= 4) {
          const audio10_desc_cs_as_iso_data_ep_t *desc_ep       = (const audio10_desc_cs_as_iso_data_ep_t *)p_desc;
          const bool                              sam_freq_ctrl = (desc_ep->bmAttributes & 0x01) != 0;
          if (unassigned_ep) {
            // Standard order: the CS_ENDPOINT follows its endpoint descriptor
            ep_info.sam_freq_ctrl = sam_freq_ctrl;
            unassigned_ep         = false;
          } else {
            // Non-standard order: the CS_ENDPOINT precedes its endpoint descriptor
            pending_sam_freq_ctrl = sam_freq_ctrl;
          }
        }
        break;
      }
      case TUSB_DESC_ENDPOINT: {
        const tusb_desc_endpoint_t *desc_endpoint = (const tusb_desc_endpoint_t *)p_desc;
        if (desc_endpoint->bmAttributes.xfer != TUSB_XFER_ISOCHRONOUS) {
          break;
        }

        const uint8_t usage = desc_endpoint->bmAttributes.usage;
        const bool    implicit_feedback =
          usage == (TUSB_ISO_EP_ATT_IMPLICIT_FB >> 4) && tu_edpt_dir(desc_endpoint->bEndpointAddress) == TUSB_DIR_IN;
        const bool explicit_feedback =
          usage == (TUSB_ISO_EP_ATT_EXPLICIT_FB >> 4) ||
          (usage == (TUSB_ISO_EP_ATT_DATA >> 4) && desc_endpoint->bmAttributes.sync == TUSB_ISO_EP_ATT_NO_SYNC);

        if (explicit_feedback) {
          TU_LOG_DRV("  AUDIO AS itf %u alt %u: explicit feedback ep %02x ignored\r\n", itf_num, alt,
                     desc_endpoint->bEndpointAddress);
          break;
        }

        if (usage == (TUSB_ISO_EP_ATT_DATA >> 4) || implicit_feedback) {
          if (has_data_ep) {
            TU_LOG_DRV("  AUDIO AS itf %u alt %u: extra data ep %02x ignored\r\n", itf_num, alt,
                       desc_endpoint->bEndpointAddress);
            break;
          }

          ep_info.ep_addr     = desc_endpoint->bEndpointAddress;
          ep_info.ep_size     = tu_edpt_packet_size(desc_endpoint);
          ep_info.ep_interval = desc_endpoint->bInterval;
          // bInterval must be in [1, 16] for isochronous endpoints
          if (ep_info.ep_interval == 0 || ep_info.ep_interval > 16) {
            ep_info.ep_interval = 1;
          }
          ep_info.ep_sync       = desc_endpoint->bmAttributes.sync;
          ep_info.ep_usage      = desc_endpoint->bmAttributes.usage;
          ep_info.sam_freq_ctrl = pending_sam_freq_ctrl;
          pending_sam_freq_ctrl = false;
          unassigned_ep         = !ep_info.sam_freq_ctrl;
          has_data_ep           = true;
        }
        break;
      }
      default:
        break;
    }
    p_desc = tu_desc_next(p_desc);
  }

  if (!has_data_ep) {
    return p_desc;
  }

  // Reject unsupported formats explicitly
  if (format_tag != AUDIO10_DATA_FORMAT_TYPE_I_PCM) {
    TU_LOG_DRV("  AUDIO AS itf %u: format tag 0x%04x not supported\r\n", itf_num, format_tag);
    return p_desc;
  }
  tuh_audio_format_t format;
  if (!audioh_format_from_uac1(subframe_size, bit_res, &format)) {
    TU_LOG_DRV("  AUDIO AS itf %u: subframe %u bits %u not supported\r\n", itf_num, subframe_size, bit_res);
    return p_desc;
  }
  if (num_channels == 0) {
    TU_LOG_DRV("  AUDIO AS itf %u: zero channels not supported\r\n", itf_num);
    return p_desc;
  }

  const uint16_t iso_xfer_size =
    (tuh_speed_get(p_audio->daddr) == TUSB_SPEED_HIGH) ? TUSB_EPSIZE_ISO_HS_MAX : TUSB_EPSIZE_ISO_FS_MAX;
  const uint32_t frame_bytes_32 = (uint32_t)num_channels * tuh_audio_format_bytes(format);
  if (frame_bytes_32 == 0 || frame_bytes_32 > iso_xfer_size) {
    TU_LOG_DRV("  AUDIO AS itf %u: frame size %lu not supported\r\n", itf_num, (unsigned long)frame_bytes_32);
    return p_desc;
  }
  const uint16_t frame_bytes = (uint16_t)frame_bytes_32;

  // Register one configuration per discrete sampling frequency
  const audioh_ep_info_t *ep     = &ep_info;
  tuh_audio_stream_t     *stream = audioh_get_stream(p_audio, tu_edpt_dir(ep->ep_addr));
  TU_ASSERT(stream != NULL, p_desc);

  const uint16_t epbuf_size = (stream->dir == TUSB_DIR_IN) ? CFG_TUH_AUDIO_EPIN_BUFSIZE : CFG_TUH_AUDIO_EPOUT_BUFSIZE;

  if (ep->ep_size == 0 || ep->ep_size > iso_xfer_size) {
    TU_LOG_DRV("  AUDIO AS itf %u alt %u: invalid isochronous ep size %u\r\n", itf_num, alt, ep->ep_size);
    return p_desc;
  }

  // Capture: the device can deliver up to its max packet size per poll
  // interval, the transfer buffer must fit it
  if (stream->dir == TUSB_DIR_IN && (ep->ep_size > epbuf_size || ep->ep_size > CFG_TUH_AUDIO_STREAM_BUFSIZE)) {
    TU_LOG_DRV("  AUDIO AS itf %u alt %u: capture ep size %u exceeds buffer capacity\r\n", itf_num, alt, ep->ep_size);
    return p_desc;
  }

  for (uint8_t i = 0; i < sam_freq_count; i++) {
    if (sam_freq[i] == 0) {
      continue;
    }

    // The largest whole-frame packet for one poll interval must fit the
    // endpoint. Playback must also stage it in the transfer buffer and FIFO.
    const uint64_t frames_numerator = (uint64_t)sam_freq[i] * audioh_interval_us(ep->ep_interval, p_audio->daddr);
    const uint64_t max_frames       = (frames_numerator + 999999u) / 1000000u;
    const uint64_t packet_bytes     = max_frames * frame_bytes;
    if (packet_bytes == 0 || packet_bytes > ep->ep_size ||
        (stream->dir == TUSB_DIR_OUT && (packet_bytes > epbuf_size || packet_bytes > CFG_TUH_AUDIO_STREAM_BUFSIZE))) {
      TU_LOG_DRV("  AUDIO AS itf %u alt %u: packet per interval does not fit endpoint/buffers (ep size %u)\r\n",
                 itf_num, alt, ep->ep_size);
      continue;
    }
    // Skip duplicate configurations
    bool duplicate = false;
    for (uint8_t j = 0; j < stream->config_count; j++) {
      if (stream->config[j].format == format && stream->config[j].sample_rate == sam_freq[i] &&
          stream->config[j].channels == num_channels) {
        duplicate = true;
        break;
      }
    }
    if (duplicate) {
      continue;
    }

    if (stream->config_count >= AUDIOH_MAX_CONFIGS) {
      TU_LOG_DRV("  AUDIO AS itf %u alt %u: reach max configurations %u\r\n", itf_num, alt, AUDIOH_MAX_CONFIGS);
      return p_desc;
    }

    stream->config[stream->config_count].dir =
      (stream->dir == TUSB_DIR_IN) ? TUH_AUDIO_STREAM_CAPTURE : TUH_AUDIO_STREAM_PLAYBACK;
    stream->config[stream->config_count].format      = format;
    stream->config[stream->config_count].sample_rate = sam_freq[i];
    stream->config[stream->config_count].channels    = num_channels;
    stream->map[stream->config_count].itf_num        = itf_num;
    stream->map[stream->config_count].alt_setting    = alt;
    stream->map[stream->config_count].ep_addr        = ep->ep_addr;
    stream->map[stream->config_count].ep_size        = ep->ep_size;
    stream->map[stream->config_count].ep_interval    = ep->ep_interval;
    stream->map[stream->config_count].ep_sync        = ep->ep_sync;
    stream->map[stream->config_count].ep_usage       = ep->ep_usage;
    stream->map[stream->config_count].sam_freq_ctrl  = ep->sam_freq_ctrl;
    stream->config_count++;
  }

  return p_desc;
}

uint16_t audioh_open(uint8_t rhport, uint8_t dev_addr, const tusb_desc_interface_t *desc_itf, uint16_t max_len) {
  (void)rhport;

  TU_VERIFY(TUSB_CLASS_AUDIO == desc_itf->bInterfaceClass, 0);
  TU_VERIFY(AUDIO_SUBCLASS_CONTROL == desc_itf->bInterfaceSubClass, 0);

  const uint8_t *desc_start = (const uint8_t *)desc_itf;
  const uint8_t *p_desc     = desc_start;
  const uint8_t *desc_end   = desc_start + max_len;

  const uint8_t idx = find_new_audio_index();
  TU_VERIFY(idx < CFG_TUH_AUDIO_MAX, 0);
  audioh_interface_t *p_audio = &_audioh_itf[idx];
  p_audio->daddr              = dev_addr;
  p_audio->ac_itf_num         = desc_itf->bInterfaceNumber;
  audioh_stream_reset(&p_audio->in_stream);
  audioh_stream_reset(&p_audio->out_stream);
  p_audio->in_stream.daddr  = dev_addr;
  p_audio->out_stream.daddr = dev_addr;

  TU_LOG_DRV("AUDIO opening AC Interface %u (addr = %u)\r\n", desc_itf->bInterfaceNumber, dev_addr);

  uint8_t usb_input_terminal_id = 0;
  uint8_t usb_output_source_id  = 0;
  bool    found_as_interface    = false;
  // A Feature Unit may precede the USB terminal that identifies its stream.
  uint8_t pending_fu_id        = 0;
  uint8_t pending_fu_source_id = 0;

  p_desc = tu_desc_next(p_desc);
  while (tu_desc_in_bounds(p_desc, desc_end) && tu_desc_type(p_desc) != TUSB_DESC_INTERFACE) {
    if (tu_desc_type(p_desc) == TUSB_DESC_CS_INTERFACE) {
      switch (tu_desc_subtype(p_desc)) {
        case AUDIO10_CS_AC_INTERFACE_INPUT_TERMINAL: {
          const audio10_desc_input_terminal_t *terminal = (const audio10_desc_input_terminal_t *)p_desc;
          if (terminal->bLength >= sizeof(audio10_desc_input_terminal_t) &&
              tu_le16toh(terminal->wTerminalType) == AUDIO_TERM_TYPE_USB_STREAMING && usb_input_terminal_id == 0) {
            usb_input_terminal_id = terminal->bTerminalID;
            if (pending_fu_source_id == usb_input_terminal_id) {
              p_audio->out_stream.feature_unit_id = pending_fu_id;
              pending_fu_id                       = 0;
              pending_fu_source_id                = 0;
            }
          }
          break;
        }
        case AUDIO10_CS_AC_INTERFACE_OUTPUT_TERMINAL: {
          const audio10_desc_output_terminal_t *terminal = (const audio10_desc_output_terminal_t *)p_desc;
          if (terminal->bLength >= sizeof(audio10_desc_output_terminal_t) &&
              tu_le16toh(terminal->wTerminalType) == AUDIO_TERM_TYPE_USB_STREAMING && usb_output_source_id == 0) {
            usb_output_source_id = terminal->bSourceID;
            if (pending_fu_id == usb_output_source_id) {
              p_audio->in_stream.feature_unit_id = pending_fu_id;
              pending_fu_id                      = 0;
              pending_fu_source_id               = 0;
            }
          }
          break;
        }
        case AUDIO10_CS_AC_INTERFACE_FEATURE_UNIT: {
          if (p_desc[0] >= 5) {
            bool mapped = false;
            if (usb_input_terminal_id != 0 && p_audio->out_stream.feature_unit_id == 0 &&
                p_desc[4] == usb_input_terminal_id) {
              p_audio->out_stream.feature_unit_id = p_desc[3];
              mapped = true;
            }
            if (usb_output_source_id != 0 && p_audio->in_stream.feature_unit_id == 0 &&
                p_desc[3] == usb_output_source_id) {
              p_audio->in_stream.feature_unit_id = p_desc[3];
              mapped = true;
            }
            if (!mapped) {
              pending_fu_id        = p_desc[3];
              pending_fu_source_id = p_desc[4];
            }
          }
          break;
        }
        default:
          break;
      }
    }
    p_desc = tu_desc_next(p_desc);
  }

  // Parse the contiguous Audio Streaming interfaces of this audio function.
  while (tu_desc_in_bounds(p_desc, desc_end)) {
    if (tu_desc_type(p_desc) != TUSB_DESC_INTERFACE) {
      p_desc = tu_desc_next(p_desc);
      continue;
    }

    const tusb_desc_interface_t *desc_interface = (const tusb_desc_interface_t *)p_desc;
    if (desc_interface->bInterfaceClass != TUSB_CLASS_AUDIO ||
        desc_interface->bInterfaceSubClass != AUDIO_SUBCLASS_STREAMING) {
      break;
    }

    found_as_interface = true;
    TU_LOG_DRV("  Found AS Interface %u (alt = %u)\r\n", desc_interface->bInterfaceNumber,
               desc_interface->bAlternateSetting);
    p_desc = audioh_parse_as(p_audio, desc_interface, p_desc, desc_end);
  }

  // This AC interface belongs to MIDI or another Audio subclass. Release the
  // tentative instance and let the next class driver claim the interface.
  if (!found_as_interface) {
    audioh_stream_reset(&p_audio->in_stream);
    audioh_stream_reset(&p_audio->out_stream);
    p_audio->daddr        = 0;
    p_audio->ac_itf_num   = 0;
    p_audio->stream_count = 0;
    p_audio->mounted      = false;
    return 0;
  }

  // Assign stream indices: playback first, then capture, so the application
  // can iterate [0, stream_count) without gaps
  uint8_t stream_idx = 0;
  if (p_audio->out_stream.config_count > 0) {
    p_audio->out_stream.stream_idx = stream_idx++;
  }
  if (p_audio->in_stream.config_count > 0) {
    p_audio->in_stream.stream_idx = stream_idx++;
  }
  p_audio->stream_count = stream_idx;

  return (uint16_t)((uintptr_t)p_desc - (uintptr_t)desc_start);
}

//--------------------------------------------------------------------+
// Set Configuration
//--------------------------------------------------------------------+
bool audioh_set_config(uint8_t dev_addr, uint8_t itf_num) {
  uint8_t idx = TUSB_INDEX_INVALID_8;
  for (uint8_t i = 0; i < CFG_TUH_AUDIO_MAX; i++) {
    if (_audioh_itf[i].daddr == dev_addr && _audioh_itf[i].ac_itf_num == itf_num) {
      idx = i;
      break;
    }
  }

  if (idx == TUSB_INDEX_INVALID_8) {
    // Audio Streaming interface (or another driver's interface): nothing to do at mount.
    // Alternate settings are activated by tuh_audio_start().
    usbh_driver_set_config_complete(dev_addr, itf_num);
    return true;
  }

  audioh_interface_t *p_audio = &_audioh_itf[idx];
  p_audio->mounted            = true;
  TU_LOG_DRV("  AUDIO mounted: addr = %u index = %u\r\n", dev_addr, idx);

  tuh_audio_mount_cb(idx);

  usbh_driver_set_config_complete(dev_addr, itf_num);
  return true;
}

//--------------------------------------------------------------------+
// Application API
//--------------------------------------------------------------------+
bool tuh_audio_mounted(uint8_t idx) {
  TU_VERIFY(idx < CFG_TUH_AUDIO_MAX);
  return _audioh_itf[idx].mounted;
}

uint8_t tuh_audio_get_dev_addr(uint8_t idx) {
  TU_VERIFY(idx < CFG_TUH_AUDIO_MAX, 0);
  return _audioh_itf[idx].daddr;
}

uint8_t tuh_audio_get_feature_unit_id(uint8_t idx, uint8_t stream_idx) {
  TU_VERIFY(idx < CFG_TUH_AUDIO_MAX, 0);
  audioh_interface_t *p_audio = &_audioh_itf[idx];
  TU_VERIFY(p_audio->daddr != 0, 0);

  tuh_audio_stream_t *s = audioh_get_stream_by_idx(p_audio, stream_idx);
  TU_VERIFY(s, 0);
  return s->feature_unit_id;
}

uint8_t tuh_audio_stream_count(uint8_t dev_idx) {
  TU_VERIFY(dev_idx < CFG_TUH_AUDIO_MAX, 0);
  audioh_interface_t *p_audio = &_audioh_itf[dev_idx];
  TU_VERIFY(p_audio->daddr != 0, 0);
  return p_audio->stream_count;
}

bool tuh_audio_stream_exists(uint8_t dev_idx, uint8_t stream_idx) {
  TU_VERIFY(dev_idx < CFG_TUH_AUDIO_MAX, false);
  audioh_interface_t *p_audio = &_audioh_itf[dev_idx];
  TU_VERIFY(p_audio->daddr != 0, false);
  return audioh_get_stream_by_idx(p_audio, stream_idx) != NULL;
}

tuh_audio_direction_t tuh_audio_stream_direction(uint8_t dev_idx, uint8_t stream_idx) {
  TU_VERIFY(dev_idx < CFG_TUH_AUDIO_MAX, TUH_AUDIO_STREAM_DIRECTION_COUNT);
  audioh_interface_t *p_audio = &_audioh_itf[dev_idx];
  TU_VERIFY(p_audio->daddr != 0, TUH_AUDIO_STREAM_DIRECTION_COUNT);

  tuh_audio_stream_t *s = audioh_get_stream_by_idx(p_audio, stream_idx);
  TU_VERIFY(s, TUH_AUDIO_STREAM_DIRECTION_COUNT);
  return (s->dir == TUSB_DIR_IN) ? TUH_AUDIO_STREAM_CAPTURE : TUH_AUDIO_STREAM_PLAYBACK;
}

uint8_t tuh_audio_config_count(uint8_t dev_idx, uint8_t stream_idx) {
  TU_VERIFY(dev_idx < CFG_TUH_AUDIO_MAX, 0);
  audioh_interface_t *p_audio = &_audioh_itf[dev_idx];
  TU_VERIFY(p_audio->daddr != 0, 0);

  tuh_audio_stream_t *s = audioh_get_stream_by_idx(p_audio, stream_idx);
  TU_VERIFY(s, 0);
  return s->config_count;
}
uint8_t tuh_audio_active_config(uint8_t dev_idx, uint8_t stream_idx) {
  TU_VERIFY(dev_idx < CFG_TUH_AUDIO_MAX, TUSB_INDEX_INVALID_8);
  audioh_interface_t *p_audio = &_audioh_itf[dev_idx];
  TU_VERIFY(p_audio->daddr != 0, TUSB_INDEX_INVALID_8);

  tuh_audio_stream_t *s = audioh_get_stream_by_idx(p_audio, stream_idx);
  TU_VERIFY(s, TUSB_INDEX_INVALID_8);
  return s->active_config;
}
bool tuh_audio_config_get(uint8_t dev_idx, uint8_t stream_idx, uint8_t config_idx, tuh_audio_stream_config_t *config) {
  TU_VERIFY(dev_idx < CFG_TUH_AUDIO_MAX, false);
  audioh_interface_t *p_audio = &_audioh_itf[dev_idx];
  TU_VERIFY(p_audio->daddr != 0, false);

  tuh_audio_stream_t *s = audioh_get_stream_by_idx(p_audio, stream_idx);
  TU_VERIFY(s && config, false);
  TU_VERIFY(config_idx < s->config_count, false);

  *config = s->config[config_idx];
  return true;
}

bool tuh_audio_configure(uint8_t dev_idx, uint8_t stream_idx, uint8_t config_idx) {
  TU_VERIFY(dev_idx < CFG_TUH_AUDIO_MAX, false);
  audioh_interface_t *p_audio = &_audioh_itf[dev_idx];
  TU_VERIFY(p_audio->mounted, false);

  tuh_audio_stream_t *s = audioh_get_stream_by_idx(p_audio, stream_idx);
  TU_VERIFY(s, false);
  TU_VERIFY(config_idx < s->config_count, false);
  const tuh_audio_stream_config_t *cfg = &s->config[config_idx];
  // Reconfiguration is allowed from a stopped stream.
  TU_VERIFY(!s->running, false);
  if (s->state == STREAM_STATE_READY) {
    // Wait for any in-flight transfer to complete and be discarded
    TU_VERIFY(!usbh_edpt_busy(s->daddr, s->edpt.ep_addr), false);
  }

  tuh_audio_stream_t *other = (s == &p_audio->out_stream) ? &p_audio->in_stream : &p_audio->out_stream;
  if (other->active_config != TUSB_INDEX_INVALID_8) {
    const tuh_audio_stream_config_t *other_cfg = &other->config[other->active_config];
    if (cfg->sample_rate != other_cfg->sample_rate) {
      TU_LOG_DRV("  AUDIO configure failed: capture/playback sample rates must match (%lu != %lu)\r\n",
                 (unsigned long)cfg->sample_rate, (unsigned long)other_cfg->sample_rate);
      return false;
    }
  }

  // The HCD endpoint must be reopened even when the new configuration uses
  // the same address, since its packet size and interval may have changed.
  TU_VERIFY(audioh_stream_close_ep(s), false);

  const audioh_stream_map_t *map  = &s->map[config_idx];
  const uint64_t frames_numerator = (uint64_t)cfg->sample_rate * audioh_interval_us(map->ep_interval, s->daddr);
  s->active_config                = config_idx;
  s->frame_bytes                  = (uint16_t)tuh_audio_config_frame_size(cfg);
  s->frames_per_interval          = (uint16_t)(frames_numerator / 1000000u);
  s->frames_rem                   = (uint32_t)(frames_numerator % 1000000u);
  s->rem_acc                      = 0;
  s->state                        = STREAM_STATE_IDLE;
  if (s->dir == TUSB_DIR_IN) {
    // A byte FIFO can overwrite only complete audio frames when its depth is
    // an exact multiple of the configured frame size.
    const uint16_t fifo_depth = CFG_TUH_AUDIO_STREAM_BUFSIZE - (CFG_TUH_AUDIO_STREAM_BUFSIZE % s->frame_bytes);
    if (!tu_fifo_config(&s->edpt.ff, s->ff_buf, fifo_depth, true)) {
      audioh_stream_fail(s);
      return false;
    }
  }

  TU_LOG_DRV("  AUDIO configure %s stream %u: itf %u alt %u ep %02x\r\n",
             (s->dir == TUSB_DIR_IN) ? "capture" : "playback", s->stream_idx, map->itf_num, map->alt_setting,
             map->ep_addr);

  return audioh_stream_open_ep(s);
}

// Invoked when the SET_INTERFACE activating the stream's interface completes:
// the interface is active, set its sampling frequency before submitting
// transfers
static void audioh_stream_start_xfer(tuh_audio_stream_t *s) {
  if (s->dir == TUSB_DIR_IN) {
    audioh_stream_capture_xfer(s);  // feed the capture endpoint
  } else {
    audioh_stream_playback_xfer(s); // start the continuous playback transfer chain
  }
}

static void audioh_stream_start_set_freq_complete(tuh_xfer_t *xfer) {
  tuh_audio_stream_t *s = (tuh_audio_stream_t *)xfer->user_data;
  if (s->daddr != xfer->daddr || s->state != STREAM_STATE_READY || !s->running) {
    return; // device is gone or the stream was stopped meanwhile
  }
  if (xfer->result != XFER_RESULT_SUCCESS) {
    TU_LOG_DRV("  AUDIO set sampling frequency failed: result=%u\r\n", xfer->result);
    s->running = false;
    return;
  }
  audioh_stream_start_xfer(s);
}

static bool audioh_stream_start_active(tuh_audio_stream_t *s) {
  const audioh_stream_map_t *map = &s->map[s->active_config];
  if (map->sam_freq_ctrl) {
    if (!audioh_stream_set_freq(s, audioh_stream_start_set_freq_complete)) {
      s->running = false;
      return false;
    }
  } else {
    audioh_stream_start_xfer(s);
  }
  return true;
}

static void audioh_stream_start_complete(tuh_xfer_t *xfer) {
  tuh_audio_stream_t *s = (tuh_audio_stream_t *)xfer->user_data;
  if (s->daddr != xfer->daddr || s->state != STREAM_STATE_READY || !s->running) {
    return; // device is gone or the stream was stopped meanwhile
  }
  if (xfer->result != XFER_RESULT_SUCCESS) {
    TU_LOG_DRV("  AUDIO SET_INTERFACE activate failed: result=%u\r\n", xfer->result);
    s->running = false;
    return;
  }
  (void)audioh_stream_start_active(s);
}

bool tuh_audio_start(uint8_t dev_idx, uint8_t stream_idx) {
  TU_VERIFY(dev_idx < CFG_TUH_AUDIO_MAX, false);
  audioh_interface_t *p_audio = &_audioh_itf[dev_idx];
  TU_VERIFY(p_audio->mounted, false);

  tuh_audio_stream_t *s = audioh_get_stream_by_idx(p_audio, stream_idx);
  TU_VERIFY(s, false);
  TU_VERIFY(s->state == STREAM_STATE_READY && !s->running, false);
  // Wait for any in-flight transfer to complete and be discarded
  TU_VERIFY(!usbh_edpt_busy(s->daddr, s->map[s->active_config].ep_addr), false);

  s->running = true;
  // Activate the interface's alternate setting asynchronously: transfers
  // begin once SET_INTERFACE and sampling-frequency control complete.
  const audioh_stream_map_t *map = &s->map[s->active_config];
  if (!tuh_interface_set(s->daddr, map->itf_num, map->alt_setting, audioh_stream_start_complete, (uintptr_t)s)) {
    s->running = false;
    return false;
  }
  return true;
}

// Invoked when the SET_INTERFACE deactivating the stream's interface (alt 0)
// completes
static void audioh_stream_stop_complete(tuh_xfer_t *xfer) {
  tuh_audio_stream_t *s = (tuh_audio_stream_t *)xfer->user_data;
  if (s->daddr != xfer->daddr) {
    return;
  }
  TU_LOG_DRV("  AUDIO SET_INTERFACE deactivate done: result=%u\r\n", xfer->result);
}

bool tuh_audio_stop(uint8_t dev_idx, uint8_t stream_idx) {
  TU_VERIFY(dev_idx < CFG_TUH_AUDIO_MAX, false);
  audioh_interface_t *p_audio = &_audioh_itf[dev_idx];
  TU_VERIFY(p_audio->mounted, false);

  tuh_audio_stream_t *s = audioh_get_stream_by_idx(p_audio, stream_idx);
  TU_VERIFY(s && s->state == STREAM_STATE_READY, false);

  // The in-flight transfer (if any) completes and its data is discarded;
  // queued frames are dropped as well. The interface is deactivated (alt 0)
  // so the device stops transferring.
  s->running = false;
  tu_edpt_stream_clear(&s->edpt);
  s->rem_acc = 0; // restart the pacing accumulator on the next tuh_audio_start()

  // Keep stop retryable if EP0 is busy and SET_INTERFACE cannot be submitted.
  const audioh_stream_map_t *map = &s->map[s->active_config];
  return tuh_interface_set(s->daddr, map->itf_num, 0, audioh_stream_stop_complete, (uintptr_t)s);
}

uint32_t tuh_audio_write(uint8_t dev_idx, uint8_t stream_idx, const void *buffer, uint32_t frame_count) {
  TU_VERIFY(dev_idx < CFG_TUH_AUDIO_MAX, 0);
  audioh_interface_t *p_audio = &_audioh_itf[dev_idx];
  TU_VERIFY(p_audio->mounted && buffer, 0);

  tuh_audio_stream_t *s = audioh_get_stream_by_idx(p_audio, stream_idx);
  // Writes are only accepted by the playback stream
  TU_VERIFY(s && s->dir == TUSB_DIR_OUT, 0);
  TU_VERIFY(s->state == STREAM_STATE_READY && s->running, 0);
  TU_VERIFY(frame_count > 0, 0);

  // Queue as many whole frames as the FIFO can hold
  const uint32_t frames = TU_MIN(frame_count, tu_fifo_remaining(&s->edpt.ff) / s->frame_bytes);
  if (frames == 0) {
    return 0;
  }
  tu_fifo_write_n(&s->edpt.ff, buffer, (uint16_t)(frames * s->frame_bytes));

  return frames;
}

uint32_t tuh_audio_read(uint8_t dev_idx, uint8_t stream_idx, void *buffer, uint32_t frame_count) {
  TU_VERIFY(dev_idx < CFG_TUH_AUDIO_MAX, 0);
  audioh_interface_t *p_audio = &_audioh_itf[dev_idx];
  TU_VERIFY(p_audio->mounted && buffer, 0);

  tuh_audio_stream_t *s = audioh_get_stream_by_idx(p_audio, stream_idx);
  // Reads are only accepted by the capture stream
  TU_VERIFY(s && s->dir == TUSB_DIR_IN, 0);
  TU_VERIFY(s->state == STREAM_STATE_READY && s->running, 0);
  TU_VERIFY(frame_count > 0, 0);

  // Drain as many whole frames as are queued
  const uint32_t frames = TU_MIN(frame_count, tu_fifo_count(&s->edpt.ff) / s->frame_bytes);
  if (frames > 0) {
    tu_fifo_read_n(&s->edpt.ff, buffer, (uint16_t)(frames * s->frame_bytes));
  }
  return frames;
}

uint32_t tuh_audio_write_available(uint8_t dev_idx, uint8_t stream_idx) {
  TU_VERIFY(dev_idx < CFG_TUH_AUDIO_MAX, 0);
  audioh_interface_t *p_audio = &_audioh_itf[dev_idx];
  TU_VERIFY(p_audio->daddr != 0, 0);

  tuh_audio_stream_t *s = audioh_get_stream_by_idx(p_audio, stream_idx);
  TU_VERIFY(s && s->dir == TUSB_DIR_OUT, 0);
  TU_VERIFY(s->state == STREAM_STATE_READY && s->running, 0);
  return tu_edpt_stream_write_available(&s->edpt) / s->frame_bytes;
}

uint32_t tuh_audio_read_available(uint8_t dev_idx, uint8_t stream_idx) {
  TU_VERIFY(dev_idx < CFG_TUH_AUDIO_MAX, 0);
  audioh_interface_t *p_audio = &_audioh_itf[dev_idx];
  TU_VERIFY(p_audio->daddr != 0, 0);

  tuh_audio_stream_t *s = audioh_get_stream_by_idx(p_audio, stream_idx);
  TU_VERIFY(s && s->dir == TUSB_DIR_IN, 0);
  TU_VERIFY(s->state == STREAM_STATE_READY && s->running, 0);
  return tu_edpt_stream_read_available(&s->edpt) / s->frame_bytes;
}

//--------------------------------------------------------------------+
// Feature Unit Control API
//--------------------------------------------------------------------+

// Release the stable SET buffer and chain to the application callback
static void audioh_fu_set_complete(tuh_xfer_t *xfer) {
  const uint8_t   idx       = (uint8_t)xfer->user_data;
  audioh_epbuf_t *epbuf     = &_audioh_epbuf[idx];
  tuh_xfer_cb_t   app_cb    = epbuf->complete_cb;
  uintptr_t       user_data = epbuf->user_data;
  epbuf->complete_cb        = NULL;
  epbuf->fu_busy            = false;

  xfer->user_data = user_data;
  if (app_cb != NULL) {
    app_cb(xfer);
  }
}

// Convert the raw control value to host order and chain to the application callback
static void audioh_fu_get_complete(tuh_xfer_t *xfer) {
  const uint8_t   idx       = (uint8_t)xfer->user_data;
  audioh_epbuf_t *epbuf     = &_audioh_epbuf[idx];
  tuh_xfer_cb_t   app_cb    = epbuf->complete_cb;
  uintptr_t       user_data = epbuf->user_data;
  uint16_t       *value     = epbuf->value;
  const uint8_t   width     = epbuf->width;
  epbuf->complete_cb        = NULL;
  epbuf->fu_busy            = false;

  if (app_cb != NULL && value != NULL && xfer->result == XFER_RESULT_SUCCESS) {
    const uint8_t *raw = (const uint8_t *)value;
    // The raw bytes are little-endian on the wire: rebuild the host-order value
    *value = (width == 1) ? (uint16_t)raw[0] : (uint16_t)((uint16_t)raw[0] | ((uint16_t)raw[1] << 8));
  }

  xfer->user_data = user_data;
  if (app_cb != NULL) {
    app_cb(xfer);
  }
}

bool tuh_audio_feature_unit_set(uint8_t idx, uint8_t stream_idx, uint8_t control_selector, uint8_t channel,
                                uint16_t value, tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  TU_VERIFY(idx < CFG_TUH_AUDIO_MAX, false);
  audioh_interface_t *p_audio = &_audioh_itf[idx];
  TU_VERIFY(p_audio->mounted, false);
  tuh_audio_stream_t *s = audioh_get_stream_by_idx(p_audio, stream_idx);
  TU_VERIFY(s && s->feature_unit_id != 0, false);

  const uint8_t width = audioh_fu_control_width(control_selector);
  TU_VERIFY(width != 0, false);

  audioh_epbuf_t *epbuf = &_audioh_epbuf[idx];
  TU_VERIFY(!epbuf->fu_busy, false);
  epbuf->fu_busy = true; // reserve the request state and fu_ctrl before writing

  const tusb_control_request_t request = {.bmRequestType_bit = {.recipient = TUSB_REQ_RCPT_INTERFACE,
                                                                .type      = TUSB_REQ_TYPE_CLASS,
                                                                .direction = TUSB_DIR_OUT},
                                          .bRequest          = AUDIO10_CS_REQ_SET_CUR,
                                          .wValue            = tu_htole16(tu_u16(control_selector, channel)),
                                          .wIndex  = tu_htole16(tu_u16(s->feature_unit_id, p_audio->ac_itf_num)),
                                          .wLength = width};

  uint8_t *val_buf = epbuf->fu_ctrl;
  val_buf[0]       = (uint8_t)(value & 0xFF);
  if (width == 2) {
    val_buf[1] = (uint8_t)((value >> 8) & 0xFF);
  }

  tuh_xfer_t xfer = {.daddr       = p_audio->daddr,
                     .ep_addr     = 0,
                     .setup       = &request,
                     .buffer      = val_buf,
                     .complete_cb = complete_cb,
                     .user_data   = user_data};

  if (complete_cb == NULL) {
    const bool result = tuh_control_xfer(&xfer);
    epbuf->fu_busy    = false;
    return result;
  }

  epbuf->complete_cb = complete_cb;
  epbuf->user_data   = user_data;
  xfer.complete_cb   = audioh_fu_set_complete;
  xfer.user_data     = (uintptr_t)idx;

  if (!tuh_control_xfer(&xfer)) {
    epbuf->complete_cb = NULL;
    epbuf->fu_busy     = false;
    return false;
  }
  return true;
}

bool tuh_audio_feature_unit_get(uint8_t idx, uint8_t stream_idx, uint8_t control_selector, uint8_t channel,
                                uint16_t *value, tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  TU_VERIFY(idx < CFG_TUH_AUDIO_MAX, false);
  audioh_interface_t *p_audio = &_audioh_itf[idx];
  TU_VERIFY(p_audio->mounted && value, false);
  tuh_audio_stream_t *s = audioh_get_stream_by_idx(p_audio, stream_idx);
  TU_VERIFY(s && s->feature_unit_id != 0, false);

  const uint8_t width = audioh_fu_control_width(control_selector);
  TU_VERIFY(width != 0, false);

  audioh_epbuf_t *epbuf = &_audioh_epbuf[idx];
  TU_VERIFY(!epbuf->fu_busy, false);
  epbuf->fu_busy = true;

  const tusb_control_request_t request = {.bmRequestType_bit = {.recipient = TUSB_REQ_RCPT_INTERFACE,
                                                                .type      = TUSB_REQ_TYPE_CLASS,
                                                                .direction = TUSB_DIR_IN},
                                          .bRequest          = AUDIO10_CS_REQ_GET_CUR,
                                          .wValue            = tu_htole16(tu_u16(control_selector, channel)),
                                          .wIndex  = tu_htole16(tu_u16(s->feature_unit_id, p_audio->ac_itf_num)),
                                          .wLength = width};

  if (complete_cb == NULL) {
    // Sync (blocking) path: user_data points to a tusb_xfer_result_t, the raw
    // bytes are converted to host order after the transfer completes
    tuh_xfer_t xfer = {.daddr       = p_audio->daddr,
                       .ep_addr     = 0,
                       .setup       = &request,
                       .buffer      = (uint8_t *)value,
                       .complete_cb = NULL,
                       .user_data   = user_data};
    if (!tuh_control_xfer(&xfer)) {
      epbuf->fu_busy = false;
      return false;
    }
    if (xfer.result == XFER_RESULT_SUCCESS) {
      const uint8_t *raw = (const uint8_t *)value;
      *value             = (width == 1) ? (uint16_t)raw[0] : (uint16_t)((uint16_t)raw[0] | ((uint16_t)raw[1] << 8));
    }
    epbuf->fu_busy = false;
    return true;
  }

  // Async path: chain the host-order conversion to the application callback
  epbuf->complete_cb = complete_cb;
  epbuf->user_data   = user_data;
  epbuf->value       = value;
  epbuf->width       = width;

  tuh_xfer_t xfer = {.daddr       = p_audio->daddr,
                     .ep_addr     = 0,
                     .setup       = &request,
                     .buffer      = (uint8_t *)value, // raw bytes, converted in audioh_fu_get_complete()
                     .complete_cb = audioh_fu_get_complete,
                     .user_data   = (uintptr_t)idx};

  if (!tuh_control_xfer(&xfer)) {
    epbuf->complete_cb = NULL;
    epbuf->fu_busy     = false;
    return false;
  }
  return true;
}

#endif

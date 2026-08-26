/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 Zhenjiang Zhang
 * SPDX-FileCopyrightText: Copyright (c) 2026 HiFiPhile (Zixun LI)
 * SPDX-License-Identifier: MIT
 *
 * This file is part of the TinyUSB stack.
 */

/*
 * This driver implements a USB Audio Host (UAC1/UAC2) class driver with a
 * WASAPI/ALSA-like high-level streaming API.
 * The USB Audio topology (Audio Control interface, Audio Streaming interfaces, alternate settings, and endpoints) is
 * kept private to the driver.
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
 * settings in one direction are presented as a flat list of discrete
 * {format, sample_rate, channels} tuples. Internally, configurations are
 * grouped by alternate setting so their format and endpoint properties are
 * stored only once. The selected mapping is applied by tuh_audio_configure().
 *
 * Non-PCM formats are not registered as supported configurations during
 * enumeration. UAC1 continuous
 * sampling-frequency ranges are unsupported.
 * The driver owns:
 * 1. Endpoint selection and opening; only the alternate setting selected by
 *    tuh_audio_configure() is activated by tuh_audio_start().
 * 2. Protocol-specific sampling-frequency control.
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

enum {
  AUDIOH_CTRL_NONE       = 0,
  AUDIOH_CTRL_READ       = 1,
  AUDIOH_CTRL_READ_WRITE = 3
};

#if CFG_TUH_AUDIO_PROTOCOLS & TUH_AUDIO_PROTOCOL_UAC1
  #define AUDIOH_MAX_RATE_SOURCES (2 * CFG_TUH_AUDIO_MAX_AS)
#else
  #define AUDIOH_MAX_RATE_SOURCES TUH_AUDIO_STREAM_DIRECTION_COUNT
#endif

// A UAC1 alternate setting owns one descriptor-provided rate source. UAC2
// alternate settings attached to the same Clock Source share one rate source.
typedef struct {
  uint32_t sample_rate[CFG_TUH_AUDIO_MAX_SAM_FREQ];
  uint8_t  control_id; // UAC1 endpoint address or UAC2 Clock Source ID
  uint8_t  sample_rate_count;
  uint8_t  frequency_access;
} audioh_rate_source_t;

// One Audio Streaming alternate setting. Format, channels, and endpoint
// properties are shared by all of its discrete sampling frequencies.
typedef struct {
  uint16_t ep_size; // endpoint max packet size
  uint8_t  itf_num;
  uint8_t  alt_setting;
  uint8_t  ep_addr;
  uint8_t  ep_interval;
  uint8_t  ep_attr; // bmAttributes synchronization and usage bits
  uint8_t  format;
  uint8_t  channels;
  uint8_t  terminal_id;
  uint8_t  rate_source_idx;
  uint8_t  rate_count;
} audioh_as_config_t;

// Explicit feedback exists only for playback alternate settings.
typedef struct {
  uint8_t ep_addr;
  uint8_t ep_size; // feedback endpoint max packet size (3 or 4)
  uint8_t ep_interval;
  uint8_t ep_attr;
} audioh_feedback_ep_t;

typedef struct {
  audioh_feedback_ep_t feedback[CFG_TUH_AUDIO_MAX_AS];

  // Playback pacing in Q16.16 frames per data-endpoint poll interval.
  // Feedback is latched only when the current fractional scheduling cycle
  // wraps, so one cycle is never generated from two different rates.
  uint32_t nominal_frames_q16;
  uint32_t target_frames_q16;
  uint32_t pending_frames_q16;
  uint16_t feedback_min_frames;
  uint16_t feedback_max_frames;
  uint16_t rem_acc;
  bool     feedback_pending;
  bool     feedback_opened;
} audioh_playback_t;

// Control-transfer state does not require USB-accessible memory.
typedef struct {
  tuh_xfer_cb_t complete_cb;
  uintptr_t     user_data;
  void         *value;
  union {
    struct {
      uint8_t width;
      uint8_t value_type;
    } control;
    struct {
      uint8_t stream_idx;
      uint8_t range_step;
    } mount;
  } fu;
  #if CFG_TUH_AUDIO_PROTOCOLS & TUH_AUDIO_PROTOCOL_UAC2
  struct {
    uint8_t rate_source_idx;
    bool    read_cur;
  } clock;
  #endif
  bool fu_busy;
} audioh_ctrl_state_t;

// One logical stream (capture or playback)
typedef struct {
  // instance info (set at init, preserved across close/open)
  uint8_t    idx;        // instance index
  uint8_t    stream_idx; // logical stream index within the instance
  tusb_dir_t dir;        // TUSB_DIR_IN = capture, TUSB_DIR_OUT = playback

  // device owning this stream (0 = no device)
  uint8_t daddr;

  // Supported configurations (parsed during enumeration)
  uint8_t            as_count;
  uint8_t            config_count;
  audioh_as_config_t as[CFG_TUH_AUDIO_MAX_AS];

  // Active stream state
  uint8_t active_config; // flattened public configuration index
  uint8_t active_as;
  uint8_t active_rate;
  uint8_t state;         // STREAM_STATE_*
  bool    running;       // tuh_audio_start() called, transfers may be submitted

  // One Feature Unit associated with this logical stream (0 = none)
  uint8_t                  feature_unit_id;
  uint8_t                  mute_access;
  uint8_t                  volume_access;
  tuh_audio_volume_range_t volume_range;

  // Size in bytes of one frame (all channels) of the active configuration
  uint16_t frame_bytes;

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
  uint8_t protocol;   // AUDIO_INT_PROTOCOL_CODE_V1/V2
  uint8_t stream_count;
  uint8_t rate_source_count;
  bool    mounted;

  audioh_rate_source_t rate_source[AUDIOH_MAX_RATE_SOURCES];

  // Logical streams: playback first, then capture (stream index order)
  tuh_audio_stream_t  out_stream;
  tuh_audio_stream_t  in_stream;
  audioh_playback_t   playback;
  audioh_ctrl_state_t ctrl;
} audioh_interface_t;

  #if CFG_TUH_AUDIO_PROTOCOLS & TUH_AUDIO_PROTOCOL_UAC2
    #define AUDIOH_CLOCK_RANGE_BUFSIZE (2 + 12 * CFG_TUH_AUDIO_MAX_SAM_FREQ)
  #endif

typedef struct {
  // Clock discovery completes before mount. Afterwards its storage is reused
  // by independently cache-aligned sampling-frequency and Feature Unit buffers.
  union {
  #if CFG_TUH_AUDIO_PROTOCOLS & TUH_AUDIO_PROTOCOL_UAC2
    TUH_EPBUF_DEF(clock_range, AUDIOH_CLOCK_RANGE_BUFSIZE);
  #endif
    struct {
      TUH_EPBUF_DEF(rate_ctrl, 4);
      TUH_EPBUF_DEF(fu_ctrl, 8);
    } runtime;
  } control;
  // Explicit feedback can overlap both runtime control transfers.
  TUH_EPBUF_DEF(feedback, 4);
  TUH_EPBUF_DEF(epin, CFG_TUH_AUDIO_EPIN_BUFSIZE);   // capture transfer buffer
  TUH_EPBUF_DEF(epout, CFG_TUH_AUDIO_EPOUT_BUFSIZE); // playback transfer buffer
} audioh_epbuf_t;

static audioh_interface_t _audioh_itf[CFG_TUH_AUDIO_MAX];

CFG_TUH_MEM_SECTION static audioh_epbuf_t _audioh_epbuf[CFG_TUH_AUDIO_MAX];

TU_ATTR_ALWAYS_INLINE static inline uint8_t *audioh_rate_ctrl(audioh_epbuf_t *epbuf) {
  return epbuf->control.runtime.rate_ctrl;
}

TU_ATTR_ALWAYS_INLINE static inline uint8_t *audioh_fu_ctrl(audioh_epbuf_t *epbuf) {
  return epbuf->control.runtime.fu_ctrl;
}

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

static bool audioh_desc_valid(const uint8_t *p_desc, const uint8_t *desc_end, uint8_t min_len) {
  if (p_desc >= desc_end) {
    return false;
  }

  const size_t remaining = (size_t)(desc_end - p_desc);
  return TUH_VALIDATE_BASIC(remaining >= min_len) && TUH_VALIDATE_BASIC(tu_desc_len(p_desc) >= min_len) &&
         TUH_VALIDATE_BASIC(tu_desc_len(p_desc) <= remaining);
}

static bool audioh_protocol_enabled(uint8_t protocol) {
  switch (protocol) {
    case AUDIO_INT_PROTOCOL_CODE_V1:
      return (CFG_TUH_AUDIO_PROTOCOLS & TUH_AUDIO_PROTOCOL_UAC1) != 0;
    case AUDIO_INT_PROTOCOL_CODE_V2:
      return (CFG_TUH_AUDIO_PROTOCOLS & TUH_AUDIO_PROTOCOL_UAC2) != 0;
    default:
      return false;
  }
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
    if (s->as_count > 0 && s->stream_idx == stream_idx) {
      return s;
    }
  }
  return NULL;
}

TU_ATTR_ALWAYS_INLINE static inline audioh_playback_t *audioh_get_playback(const tuh_audio_stream_t *s) {
  return &_audioh_itf[s->idx].playback;
}

TU_ATTR_ALWAYS_INLINE static inline audioh_as_config_t *audioh_stream_active_as(tuh_audio_stream_t *s) {
  return &s->as[s->active_as];
}

TU_ATTR_ALWAYS_INLINE static inline audioh_rate_source_t *audioh_as_rate_source(const tuh_audio_stream_t *s,
                                                                                const audioh_as_config_t *as) {
  return &_audioh_itf[s->idx].rate_source[as->rate_source_idx];
}

static bool audioh_as_rate_fits(const audioh_interface_t *p_audio, const tuh_audio_stream_t *stream,
                                const audioh_as_config_t *as, uint32_t sample_rate);

static bool audioh_stream_resolve_config(const tuh_audio_stream_t *s, uint8_t config_idx, uint8_t *as_idx,
                                         uint8_t *rate_idx) {
  TU_VERIFY(config_idx < s->config_count, false);

  for (uint8_t i = 0; i < s->as_count; i++) {
    if (config_idx < s->as[i].rate_count) {
      const audioh_as_config_t   *as          = &s->as[i];
      const audioh_rate_source_t *rate_source = audioh_as_rate_source(s, as);
      for (uint8_t source_rate_idx = 0; source_rate_idx < rate_source->sample_rate_count; source_rate_idx++) {
        if (audioh_as_rate_fits(&_audioh_itf[s->idx], s, as, rate_source->sample_rate[source_rate_idx])) {
          if (config_idx == 0) {
            *as_idx   = i;
            *rate_idx = source_rate_idx;
            return true;
          }
          config_idx--;
        }
      }
      return false;
    }
    config_idx -= s->as[i].rate_count;
  }
  return false;
}

static void audioh_stream_config_fill(const tuh_audio_stream_t *s, uint8_t as_idx, uint8_t rate_idx,
                                      tuh_audio_stream_config_t *config) {
  const audioh_as_config_t   *as          = &s->as[as_idx];
  const audioh_rate_source_t *rate_source = audioh_as_rate_source(s, as);
  config->dir         = (s->dir == TUSB_DIR_IN) ? TUH_AUDIO_STREAM_CAPTURE : TUH_AUDIO_STREAM_PLAYBACK;
  config->format      = (tuh_audio_format_t)as->format;
  config->sample_rate = rate_source->sample_rate[rate_idx];
  config->channels    = as->channels;
}

static bool audioh_stream_config_get(const tuh_audio_stream_t *s, uint8_t config_idx,
                                     tuh_audio_stream_config_t *config) {
  uint8_t as_idx;
  uint8_t rate_idx;
  TU_VERIFY(audioh_stream_resolve_config(s, config_idx, &as_idx, &rate_idx), false);

  audioh_stream_config_fill(s, as_idx, rate_idx, config);
  return true;
}

static void audioh_stream_set_feature_unit(tuh_audio_stream_t *s, uint8_t unit_id, uint8_t mute_access,
                                           uint8_t volume_access) {
  s->feature_unit_id = unit_id;
  s->mute_access     = mute_access;
  s->volume_access   = volume_access;
}

// Map a Type-I PCM (subslot size, bit resolution) pair to a supported format.
static bool audioh_format_from_pcm(uint8_t subslot_size, uint8_t bit_resolution, tuh_audio_format_t *format) {
  if (subslot_size == 1 && bit_resolution == 8) {
    *format = TUH_AUDIO_FORMAT_S8;
  } else if (subslot_size == 2 && bit_resolution == 16) {
    *format = TUH_AUDIO_FORMAT_S16_LE;
  } else if (subslot_size == 3 && bit_resolution == 24) {
    *format = TUH_AUDIO_FORMAT_S24_3LE;
  } else if (subslot_size == 4 && bit_resolution == 24) {
    *format = TUH_AUDIO_FORMAT_S24_LE;
  } else if (subslot_size == 4 && bit_resolution == 32) {
    *format = TUH_AUDIO_FORMAT_S32_LE;
  } else {
    return false;
  }
  return true;
}

// Isochronous bInterval is a power-of-2 exponent in 1 ms full-speed frames
// or 125 us high-speed microframes.
static uint32_t audioh_interval_us(uint8_t ep_interval, uint8_t daddr) {
  const uint32_t unit_us = (tuh_speed_get(daddr) == TUSB_SPEED_HIGH) ? 125u : 1000u;
  return ((uint32_t)1u << (ep_interval - 1)) * unit_us;
}

// Convert a nominal sample rate to Q16.16 frames per data endpoint poll
// interval. Round to the nearest representable value to preserve common
// fractional rates such as 44.1 frames/ms.
static uint32_t audioh_nominal_frames_q16(uint32_t sample_rate, uint8_t ep_interval, uint8_t daddr) {
  const uint64_t numerator = (uint64_t)sample_rate * audioh_interval_us(ep_interval, daddr) * 65536u;
  return (uint32_t)((numerator + 500000u) / 1000000u);
}

// Supported Feature Unit control widths (0 = unsupported variable/unknown width).
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
  s->daddr           = 0;
  s->stream_idx      = TUSB_INDEX_INVALID_8;
  s->as_count        = 0;
  s->config_count    = 0;
  s->active_config   = TUSB_INDEX_INVALID_8;
  s->active_as       = TUSB_INDEX_INVALID_8;
  s->active_rate     = TUSB_INDEX_INVALID_8;
  s->state           = STREAM_STATE_IDLE;
  s->running         = false;
  s->feature_unit_id = 0;
  s->mute_access     = AUDIOH_CTRL_NONE;
  s->volume_access   = AUDIOH_CTRL_NONE;
  s->volume_range    = (tuh_audio_volume_range_t){0};
  s->frame_bytes     = 0;
  tu_edpt_stream_close(&s->edpt);
  tu_edpt_stream_clear(&s->edpt);
}

static void audioh_playback_reset(audioh_playback_t *playback) {
  tu_memclr(playback, sizeof(*playback));
}

// Find the stream owning an endpoint (used to dispatch transfer completion)
static tuh_audio_stream_t *audioh_find_stream(uint8_t dev_addr, uint8_t ep_addr) {
  for (uint8_t idx = 0; idx < CFG_TUH_AUDIO_MAX; idx++) {
    audioh_interface_t *p_audio = &_audioh_itf[idx];
    for (uint8_t s = 0; s < 2; s++) {
      tuh_audio_stream_t *stream = (s == 0) ? &p_audio->in_stream : &p_audio->out_stream;
      if (stream->daddr == dev_addr && stream->active_config != TUSB_INDEX_INVALID_8) {
        const audioh_as_config_t *as = audioh_stream_active_as(stream);
        if (as->ep_addr == ep_addr) {
          return stream;
        }
        const uint8_t feedback_ep = p_audio->playback.feedback[stream->active_as].ep_addr;
        if (stream->dir == TUSB_DIR_OUT && feedback_ep != 0 && feedback_ep == ep_addr) {
          return stream;
        }
      }
    }
  }
  return NULL;
}

//--------------------------------------------------------------------+
// Packet scheduler
//--------------------------------------------------------------------+

static void audioh_stream_error(tuh_audio_stream_t *s, uint16_t xferred_bytes);

static void audioh_stream_feedback_xfer(tuh_audio_stream_t *s) {
  TU_VERIFY(s->state == STREAM_STATE_READY && s->running, );

  const audioh_feedback_ep_t *feedback = &audioh_get_playback(s)->feedback[s->active_as];
  TU_VERIFY(feedback->ep_addr != 0, );
  TU_VERIFY(usbh_edpt_claim(s->daddr, feedback->ep_addr), );
  if (!usbh_edpt_xfer(s->daddr, feedback->ep_addr, _audioh_epbuf[s->idx].feedback, feedback->ep_size)) {
    audioh_stream_error(s, 0);
  }
}

// Re-arm the capture endpoint: request one full packet (the device sends at
// most its max packet size per poll interval). The overwritable FIFO retains
// the newest capture frames when the application cannot drain it in time.
static void audioh_stream_capture_xfer(tuh_audio_stream_t *s) {
  TU_VERIFY(s->state == STREAM_STATE_READY && s->running, );

  const audioh_as_config_t *as = audioh_stream_active_as(s);
  TU_VERIFY(usbh_edpt_claim(s->daddr, as->ep_addr), ); // one transfer in flight

  // ep_size is guaranteed <= CFG_TUH_AUDIO_EPIN_BUFSIZE by enumeration
  if (!usbh_edpt_xfer(s->daddr, as->ep_addr, s->edpt.ep_buf, as->ep_size)) {
    audioh_stream_error(s, 0);
  }
}

// Submit the next queued playback packet. Fractional frames per endpoint poll
// interval are accumulated on each successful submission, keeping the average
// data rate at the active nominal or feedback target.
static void audioh_stream_playback_xfer(tuh_audio_stream_t *s) {
  TU_VERIFY(s->state == STREAM_STATE_READY && s->running, );

  const audioh_as_config_t *as       = audioh_stream_active_as(s);
  audioh_playback_t        *playback = audioh_get_playback(s);
  TU_VERIFY(usbh_edpt_claim(s->daddr, as->ep_addr), ); // one transfer in flight

  uint32_t       frames       = playback->target_frames_q16 >> 16;
  const uint32_t fraction     = playback->target_frames_q16 & 0xFFFFu;
  uint32_t       next_rem_acc = playback->rem_acc + fraction;
  bool           loop_done    = (fraction == 0);
  if (next_rem_acc >= 65536u) {
    next_rem_acc -= 65536u;
    frames++;
    loop_done = true;
  }

  const uint64_t bytes_64 = (uint64_t)frames * s->frame_bytes;
  TU_ASSERT(bytes_64 <= as->ep_size && bytes_64 <= CFG_TUH_AUDIO_EPOUT_BUFSIZE &&
              bytes_64 <= CFG_TUH_AUDIO_STREAM_BUFSIZE, );
  const uint16_t bytes = (uint16_t)bytes_64;
  if (tu_fifo_count(&s->edpt.ff) < bytes) {
    // Keep the isochronous stream active without consuming a partial frame.
    // The queued audio is sent once a complete poll interval is available.
    tu_memclr(s->edpt.ep_buf, bytes);
  } else {
    tu_fifo_read_n(&s->edpt.ff, s->edpt.ep_buf, bytes);
  }

  if (!usbh_edpt_xfer(s->daddr, as->ep_addr, s->edpt.ep_buf, bytes)) {
    audioh_stream_error(s, 0);
    return;
  }
  playback->rem_acc = (uint16_t)next_rem_acc;
  if (loop_done && playback->feedback_pending) {
    playback->target_frames_q16 = playback->pending_frames_q16;
    playback->feedback_pending  = false;
    // Keep the remainder from the completed cycle. Clearing it for every
    // feedback update biases the average toward the integer packet sizes.
  }
}

//--------------------------------------------------------------------+
// Configure state machine
//--------------------------------------------------------------------+

static bool audioh_stream_close_ep(tuh_audio_stream_t *s) {
  audioh_playback_t *playback = (s->dir == TUSB_DIR_OUT) ? audioh_get_playback(s) : NULL;
  if (playback != NULL && playback->feedback_opened) {
    const uint8_t fb_ep_addr = playback->feedback[s->active_as].ep_addr;
    if (!tuh_edpt_close(s->daddr, fb_ep_addr)) {
      TU_LOG_DRV("  AUDIO close feedback endpoint failed: addr=%u ep=%02x\r\n", s->daddr, fb_ep_addr);
      return false;
    }
    playback->feedback_opened = false;
  }

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
  s->active_as     = TUSB_INDEX_INVALID_8;
  s->active_rate   = TUSB_INDEX_INVALID_8;
  s->running       = false;
}

static void audioh_stream_error(tuh_audio_stream_t *s, uint16_t xferred_bytes) {
  s->running = false;
  if (s->dir == TUSB_DIR_OUT) {
    audioh_playback_t *playback = audioh_get_playback(s);
    playback->target_frames_q16 = playback->nominal_frames_q16;
    playback->feedback_pending  = false;
    playback->rem_acc           = 0;
  }
  tu_edpt_stream_clear(&s->edpt);
  tuh_audio_err_cb(s->idx, s->stream_idx, xferred_bytes);
}

// Submit the protocol-specific sampling-frequency control request.
static bool audioh_stream_set_freq(tuh_audio_stream_t *s, tuh_xfer_cb_t complete_cb) {
  const audioh_as_config_t   *as          = audioh_stream_active_as(s);
  const audioh_rate_source_t *rate_source = audioh_as_rate_source(s, as);
  const uint32_t              sample_rate = rate_source->sample_rate[s->active_rate];
  uint8_t                    *ctrl        = audioh_rate_ctrl(&_audioh_epbuf[s->idx]);
  tusb_control_request_t      request     = {0};

  ctrl[0] = (uint8_t)(sample_rate & 0xFF);
  ctrl[1] = (uint8_t)((sample_rate >> 8) & 0xFF);
  ctrl[2] = (uint8_t)((sample_rate >> 16) & 0xFF);
  switch (_audioh_itf[s->idx].protocol) {
  #if CFG_TUH_AUDIO_PROTOCOLS & TUH_AUDIO_PROTOCOL_UAC1
    case AUDIO_INT_PROTOCOL_CODE_V1:
      request.bmRequestType_bit.recipient = TUSB_REQ_RCPT_ENDPOINT;
      request.bmRequestType_bit.type      = TUSB_REQ_TYPE_CLASS;
      request.bmRequestType_bit.direction = TUSB_DIR_OUT;
      request.bRequest                    = AUDIO10_CS_REQ_SET_CUR;
      request.wValue                      = tu_htole16(tu_u16(AUDIO10_EP_CTRL_SAMPLING_FREQ, 0));
      request.wIndex                      = tu_htole16(rate_source->control_id);
      request.wLength                     = 3;
      break;
  #endif
  #if CFG_TUH_AUDIO_PROTOCOLS & TUH_AUDIO_PROTOCOL_UAC2
    case AUDIO_INT_PROTOCOL_CODE_V2:
      request.bmRequestType_bit.recipient = TUSB_REQ_RCPT_INTERFACE;
      request.bmRequestType_bit.type      = TUSB_REQ_TYPE_CLASS;
      request.bmRequestType_bit.direction = TUSB_DIR_OUT;
      request.bRequest                    = AUDIO20_CS_REQ_CUR;
      request.wValue                      = tu_htole16(tu_u16(AUDIO20_CS_CTRL_SAM_FREQ, 0));
      request.wIndex                      = tu_htole16(tu_u16(rate_source->control_id, _audioh_itf[s->idx].ac_itf_num));
      request.wLength                     = 4;
      ctrl[3]                             = (uint8_t)(sample_rate >> 24);
      break;
  #endif
    default:
      return false;
  }

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
  const audioh_as_config_t *as = audioh_stream_active_as(s);

  const tusb_desc_endpoint_t desc_ep = {.bLength          = sizeof(tusb_desc_endpoint_t),
                                        .bDescriptorType  = TUSB_DESC_ENDPOINT,
                                        .bEndpointAddress = as->ep_addr,
                                        .bmAttributes     = {.xfer  = TUSB_XFER_ISOCHRONOUS,
                                                             .sync  = (as->ep_attr >> 2) & 0x03u,
                                                             .usage = (as->ep_attr >> 4) & 0x03u},
                                        .wMaxPacketSize   = tu_htole16(as->ep_size),
                                        .bInterval        = as->ep_interval};

  if (!tuh_edpt_open(s->daddr, &desc_ep)) {
    TU_LOG_DRV("  AUDIO open endpoint failed: addr=%u ep=%02x\r\n", s->daddr, as->ep_addr);
    audioh_stream_fail(s);
    return false;
  }

  // Bind the transfer helper to the endpoint and start with an empty FIFO
  const uint16_t xfer_len = (s->dir == TUSB_DIR_IN) ? CFG_TUH_AUDIO_EPIN_BUFSIZE : CFG_TUH_AUDIO_EPOUT_BUFSIZE;
  tu_edpt_stream_open(&s->edpt, s->daddr, &desc_ep, xfer_len);
  tu_edpt_stream_clear(&s->edpt);

  if (s->dir == TUSB_DIR_OUT) {
    audioh_playback_t          *playback = audioh_get_playback(s);
    const audioh_feedback_ep_t *feedback = &playback->feedback[s->active_as];
    if (feedback->ep_addr != 0) {
      const tusb_desc_endpoint_t desc_fb = {.bLength          = sizeof(tusb_desc_endpoint_t),
                                            .bDescriptorType  = TUSB_DESC_ENDPOINT,
                                            .bEndpointAddress = feedback->ep_addr,
                                            .bmAttributes     = {.xfer  = TUSB_XFER_ISOCHRONOUS,
                                                                 .sync  = (feedback->ep_attr >> 2) & 0x03u,
                                                                 .usage = (feedback->ep_attr >> 4) & 0x03u},
                                            .wMaxPacketSize   = tu_htole16(feedback->ep_size),
                                            .bInterval        = feedback->ep_interval};
      if (!tuh_edpt_open(s->daddr, &desc_fb)) {
        TU_LOG_DRV("  AUDIO open feedback endpoint failed: addr=%u ep=%02x\r\n", s->daddr, feedback->ep_addr);
        audioh_stream_fail(s);
        return false;
      }
      playback->feedback_opened = true;
    }
  }

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
    audioh_playback_reset(&_audioh_itf[idx].playback);
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
    audioh_playback_reset(&p_audio->playback);

    tu_memclr(&p_audio->ctrl, sizeof(p_audio->ctrl)); // drop pending control state

    p_audio->stream_count      = 0;
    p_audio->daddr             = 0;
    p_audio->protocol          = 0;
    p_audio->rate_source_count = 0;
    p_audio->mounted           = false;
  }
}

static void audioh_feedback_received(tuh_audio_stream_t *s, uint32_t xferred_bytes) {
  const uint8_t     *fb       = _audioh_epbuf[s->idx].feedback;
  audioh_playback_t *playback = audioh_get_playback(s);
  uint32_t           feedback_q16;
  if (xferred_bytes == 3) {
    // Full-speed feedback is normally Q10.14. Keep the scheduler in Q16.16.
    feedback_q16 = ((uint32_t)fb[0] | ((uint32_t)fb[1] << 8) | ((uint32_t)fb[2] << 16)) << 2;
  } else if (xferred_bytes == 4) {
    feedback_q16 = (uint32_t)fb[0] | ((uint32_t)fb[1] << 8) | ((uint32_t)fb[2] << 16) | ((uint32_t)fb[3] << 24);
  } else {
    TU_LOG_DRV("  AUDIO invalid feedback length: %lu\r\n", (unsigned long)xferred_bytes);
    return;
  }

  const uint32_t feedback_min_q16 = (uint32_t)playback->feedback_min_frames << 16;
  const uint32_t feedback_max_q16 = (uint32_t)playback->feedback_max_frames << 16;
  if (feedback_q16 < feedback_min_q16 || feedback_q16 > feedback_max_q16) {
    TU_LOG_DRV("  AUDIO feedback out of range: 0x%08lx\r\n", (unsigned long)feedback_q16);
    return;
  }

  // Feedback is expressed per USB frame/microframe. Scale it to the data
  // endpoint's polling interval before handing it to the packet scheduler.
  const audioh_as_config_t *as            = audioh_stream_active_as(s);
  const uint64_t            target_q16_64 = (uint64_t)feedback_q16 << (as->ep_interval - 1u);
  if (target_q16_64 > UINT32_MAX) {
    return;
  }

  const uint32_t target_q16 = (uint32_t)target_q16_64;
  const uint64_t max_bytes  = (((uint64_t)target_q16 + 0xFFFFu) >> 16) * s->frame_bytes;
  if (max_bytes == 0 || max_bytes > as->ep_size || max_bytes > CFG_TUH_AUDIO_EPOUT_BUFSIZE ||
      max_bytes > CFG_TUH_AUDIO_STREAM_BUFSIZE) {
    TU_LOG_DRV("  AUDIO feedback exceeds playback packet capacity: 0x%08lx\r\n", (unsigned long)feedback_q16);
    return;
  }

  // Keep only the newest feedback sample. The packet scheduler promotes it at
  // the end of its current fractional cycle.
  playback->pending_frames_q16 = target_q16;
  playback->feedback_pending   = true;
}

bool audioh_xfer_cb(uint8_t dev_addr, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes) {
  tuh_audio_stream_t *s = audioh_find_stream(dev_addr, ep_addr);
  if (s == NULL) {
    return false;
  }

  // Failed, stalled, or aborted transfers never carry valid audio data
  if (result != XFER_RESULT_SUCCESS) {
    TU_LOG_DRV("  AUDIO transfer failed: addr=%u ep=%02x result=%u\r\n", dev_addr, ep_addr, result);
    audioh_stream_error(s, (uint16_t)xferred_bytes);
    return true;
  }

  // Stopped stream: the in-flight transfer completes and its data is discarded
  if (!s->running) {
    return true;
  }

  const uint8_t feedback_ep = audioh_get_playback(s)->feedback[s->active_as].ep_addr;
  if (s->dir == TUSB_DIR_OUT && feedback_ep != 0 && ep_addr == feedback_ep) {
    audioh_feedback_received(s, xferred_bytes);
    audioh_stream_feedback_xfer(s);
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

  #define AUDIOH_MAX_AC_ENTITIES TUH_AUDIO_STREAM_DIRECTION_COUNT

typedef struct {
  uint8_t id;
  uint8_t source_id;
  uint8_t clock_id;
  uint8_t stream_dir;
} audioh_terminal_info_t;

typedef struct {
  uint8_t id;
  uint8_t source_id;
  uint8_t mute_access;
  uint8_t volume_access;
} audioh_fu_info_t;

typedef struct {
  uint8_t id;
  uint8_t frequency_access;
} audioh_clock_info_t;

typedef struct {
  audioh_terminal_info_t terminal[AUDIOH_MAX_AC_ENTITIES];
  audioh_fu_info_t       feature_unit[AUDIOH_MAX_AC_ENTITIES];
  audioh_clock_info_t    clock[AUDIOH_MAX_AC_ENTITIES];
  uint8_t                terminal_count;
  uint8_t                feature_unit_count;
  uint8_t                clock_count;
} audioh_ac_map_t;

  #if CFG_TUH_AUDIO_PROTOCOLS & TUH_AUDIO_PROTOCOL_UAC2
static uint8_t audioh_uac2_control_access(uint32_t controls, uint8_t position) {
  const uint8_t access = (uint8_t)((controls >> position) & 0x03u);
  return (access == AUDIOH_CTRL_READ || access == AUDIOH_CTRL_READ_WRITE) ? access : AUDIOH_CTRL_NONE;
}
  #endif

static bool audioh_as_rate_fits(const audioh_interface_t *p_audio, const tuh_audio_stream_t *stream,
                                const audioh_as_config_t *as, uint32_t sample_rate) {
  const uint32_t frame_bytes = (uint32_t)as->channels * tuh_audio_format_bytes((tuh_audio_format_t)as->format);
  const uint16_t epbuf_size  = (stream->dir == TUSB_DIR_IN) ? CFG_TUH_AUDIO_EPIN_BUFSIZE : CFG_TUH_AUDIO_EPOUT_BUFSIZE;
  const uint64_t frames_numerator = (uint64_t)sample_rate * audioh_interval_us(as->ep_interval, p_audio->daddr);
  const uint64_t max_frames       = (frames_numerator + 999999u) / 1000000u;
  const uint64_t packet_bytes     = max_frames * frame_bytes;

  return packet_bytes > 0 && packet_bytes <= as->ep_size &&
         (stream->dir != TUSB_DIR_OUT || (packet_bytes <= epbuf_size && packet_bytes <= CFG_TUH_AUDIO_STREAM_BUFSIZE));
}

static void audioh_ac_terminal_add(audioh_ac_map_t *map, uint8_t id, uint8_t source_id, uint8_t clock_id,
                                   tusb_dir_t stream_dir) {
  if (map->terminal_count < TU_ARRAY_SIZE(map->terminal)) {
    map->terminal[map->terminal_count++] =
      (audioh_terminal_info_t){.id = id, .source_id = source_id, .clock_id = clock_id, .stream_dir = stream_dir};
  }
}

static void audioh_ac_feature_unit_add(audioh_ac_map_t *map, uint8_t id, uint8_t source_id, uint8_t mute_access,
                                       uint8_t volume_access) {
  if ((mute_access != AUDIOH_CTRL_NONE || volume_access != AUDIOH_CTRL_NONE) &&
      map->feature_unit_count < TU_ARRAY_SIZE(map->feature_unit)) {
    map->feature_unit[map->feature_unit_count++] =
      (audioh_fu_info_t){.id = id, .source_id = source_id, .mute_access = mute_access, .volume_access = volume_access};
  }
}

static const audioh_terminal_info_t *audioh_ac_terminal_find(const audioh_ac_map_t *map, uint8_t id) {
  for (uint8_t i = 0; i < map->terminal_count; i++) {
    if (map->terminal[i].id == id) {
      return &map->terminal[i];
    }
  }
  return NULL;
}

  #if CFG_TUH_AUDIO_PROTOCOLS & TUH_AUDIO_PROTOCOL_UAC2
static const audioh_clock_info_t *audioh_ac_clock_find(const audioh_ac_map_t *map, uint8_t id) {
  for (uint8_t i = 0; i < map->clock_count; i++) {
    if (map->clock[i].id == id) {
      return &map->clock[i];
    }
  }
  return NULL;
}
  #endif

  #if CFG_TUH_AUDIO_PROTOCOLS & TUH_AUDIO_PROTOCOL_UAC1
static bool audioh_uac1_parse_ac_entity(audioh_ac_map_t *map, const uint8_t *p_desc) {
  switch (tu_desc_subtype(p_desc)) {
    case AUDIO10_CS_AC_INTERFACE_INPUT_TERMINAL: {
      TU_VERIFY(TUH_VALIDATE_BASIC(tu_desc_len(p_desc) >= sizeof(audio10_desc_input_terminal_t)), false);
      const audio10_desc_input_terminal_t *terminal = (const audio10_desc_input_terminal_t *)p_desc;
      if (tu_le16toh(terminal->wTerminalType) == AUDIO_TERM_TYPE_USB_STREAMING) {
        audioh_ac_terminal_add(map, terminal->bTerminalID, 0, 0, TUSB_DIR_OUT);
      }
      break;
    }
    case AUDIO10_CS_AC_INTERFACE_OUTPUT_TERMINAL: {
      TU_VERIFY(TUH_VALIDATE_BASIC(tu_desc_len(p_desc) >= sizeof(audio10_desc_output_terminal_t)), false);
      const audio10_desc_output_terminal_t *terminal = (const audio10_desc_output_terminal_t *)p_desc;
      if (tu_le16toh(terminal->wTerminalType) == AUDIO_TERM_TYPE_USB_STREAMING) {
        audioh_ac_terminal_add(map, terminal->bTerminalID, terminal->bSourceID, 0, TUSB_DIR_IN);
      }
      break;
    }
    case AUDIO10_CS_AC_INTERFACE_FEATURE_UNIT: {
      TU_VERIFY(TUH_VALIDATE_BASIC(tu_desc_len(p_desc) >= 7), false);
      const uint8_t control_size = p_desc[5];
      TU_VERIFY(TUH_VALIDATE_BASIC(control_size > 0), false);
      TU_VERIFY(TUH_VALIDATE_BASIC(control_size <= (uint8_t)(tu_desc_len(p_desc) - 7)), false);
      const uint8_t controls = p_desc[6];
      audioh_ac_feature_unit_add(map, p_desc[3], p_desc[4],
                                 (controls & AUDIO10_FU_CONTROL_BM_MUTE) ? AUDIOH_CTRL_READ_WRITE : AUDIOH_CTRL_NONE,
                                 (controls & AUDIO10_FU_CONTROL_BM_VOLUME) ? AUDIOH_CTRL_READ_WRITE : AUDIOH_CTRL_NONE);
      break;
    }
    default:
      break;
  }
  return true;
}
  #endif

  #if CFG_TUH_AUDIO_PROTOCOLS & TUH_AUDIO_PROTOCOL_UAC2
static bool audioh_uac2_parse_ac_entity(audioh_ac_map_t *map, const uint8_t *p_desc) {
  switch (tu_desc_subtype(p_desc)) {
    case AUDIO20_CS_AC_INTERFACE_INPUT_TERMINAL: {
      TU_VERIFY(TUH_VALIDATE_BASIC(tu_desc_len(p_desc) >= sizeof(audio20_desc_input_terminal_t)), false);
      const audio20_desc_input_terminal_t *terminal = (const audio20_desc_input_terminal_t *)p_desc;
      if (tu_le16toh(terminal->wTerminalType) == AUDIO_TERM_TYPE_USB_STREAMING) {
        audioh_ac_terminal_add(map, terminal->bTerminalID, 0, terminal->bCSourceID, TUSB_DIR_OUT);
      }
      break;
    }
    case AUDIO20_CS_AC_INTERFACE_OUTPUT_TERMINAL: {
      TU_VERIFY(TUH_VALIDATE_BASIC(tu_desc_len(p_desc) >= sizeof(audio20_desc_output_terminal_t)), false);
      const audio20_desc_output_terminal_t *terminal = (const audio20_desc_output_terminal_t *)p_desc;
      if (tu_le16toh(terminal->wTerminalType) == AUDIO_TERM_TYPE_USB_STREAMING) {
        audioh_ac_terminal_add(map, terminal->bTerminalID, terminal->bSourceID, terminal->bCSourceID, TUSB_DIR_IN);
      }
      break;
    }
    case AUDIO20_CS_AC_INTERFACE_FEATURE_UNIT: {
      TU_VERIFY(TUH_VALIDATE_BASIC(tu_desc_len(p_desc) >= 10), false);
      const uint32_t controls = tu_le32toh(tu_unaligned_read32(&p_desc[5]));
      audioh_ac_feature_unit_add(map, p_desc[3], p_desc[4],
                                 audioh_uac2_control_access(controls, AUDIO20_FEATURE_UNIT_CTRL_MUTE_POS),
                                 audioh_uac2_control_access(controls, AUDIO20_FEATURE_UNIT_CTRL_VOLUME_POS));
      break;
    }
    case AUDIO20_CS_AC_INTERFACE_CLOCK_SOURCE: {
      TU_VERIFY(TUH_VALIDATE_BASIC(tu_desc_len(p_desc) >= sizeof(audio20_desc_clock_source_t)), false);
      const audio20_desc_clock_source_t *clock = (const audio20_desc_clock_source_t *)p_desc;
      if (map->clock_count < TU_ARRAY_SIZE(map->clock)) {
        map->clock[map->clock_count++] =
          (audioh_clock_info_t){.id = clock->bClockID,
                                .frequency_access =
                                  audioh_uac2_control_access(clock->bmControls, AUDIO20_CLOCK_SOURCE_CTRL_CLK_FRQ_POS)};
      }
      break;
    }
    default:
      break;
  }
  return true;
}
  #endif

static bool audioh_parse_ac_entity(audioh_interface_t *p_audio, audioh_ac_map_t *map, const uint8_t *p_desc) {
  switch (p_audio->protocol) {
  #if CFG_TUH_AUDIO_PROTOCOLS & TUH_AUDIO_PROTOCOL_UAC1
    case AUDIO_INT_PROTOCOL_CODE_V1:
      return audioh_uac1_parse_ac_entity(map, p_desc);
  #endif
  #if CFG_TUH_AUDIO_PROTOCOLS & TUH_AUDIO_PROTOCOL_UAC2
    case AUDIO_INT_PROTOCOL_CODE_V2:
      return audioh_uac2_parse_ac_entity(map, p_desc);
  #endif
    default:
      return false;
  }
}

static void audioh_link_feature_units(audioh_interface_t *p_audio, const audioh_ac_map_t *map) {
  for (uint8_t direction = TUSB_DIR_OUT; direction <= TUSB_DIR_IN; direction++) {
    tuh_audio_stream_t *stream = audioh_get_stream(p_audio, (tusb_dir_t)direction);
    if (stream == NULL || stream->as_count == 0) {
      continue;
    }
    const audioh_terminal_info_t *terminal = audioh_ac_terminal_find(map, stream->as[0].terminal_id);
    if (terminal == NULL || terminal->stream_dir != direction) {
      continue;
    }
    for (uint8_t i = 0; i < map->feature_unit_count; i++) {
      const audioh_fu_info_t *fu = &map->feature_unit[i];
      const bool              linked =
        (direction == TUSB_DIR_OUT) ? (fu->source_id == terminal->id) : (fu->id == terminal->source_id);
      if (linked) {
        audioh_stream_set_feature_unit(stream, fu->id, fu->mute_access, fu->volume_access);
        break;
      }
    }
  }
}

typedef struct {
  uint32_t       format_bitmap;
  uint16_t       format_tag;
  const uint8_t *sample_rate_data;
  uint8_t        terminal_id;
  uint8_t        format_type;
  uint8_t        channels;
  uint8_t        subslot_size;
  uint8_t        bit_resolution;
  uint8_t        sample_rate_count;
} audioh_as_class_info_t;

  #if CFG_TUH_AUDIO_PROTOCOLS & TUH_AUDIO_PROTOCOL_UAC1
static bool audioh_uac1_parse_as_interface(const uint8_t *p_desc, audioh_as_class_info_t *info) {
  switch (tu_desc_subtype(p_desc)) {
    case AUDIO10_CS_AS_INTERFACE_AS_GENERAL: {
      TU_VERIFY(TUH_VALIDATE_BASIC(tu_desc_len(p_desc) >= sizeof(audio10_desc_cs_as_interface_t)), false);
      const audio10_desc_cs_as_interface_t *general = (const audio10_desc_cs_as_interface_t *)p_desc;
      info->terminal_id                             = general->bTerminalLink;
      info->format_tag                              = tu_le16toh(general->wFormatTag);
      break;
    }
    case AUDIO10_CS_AS_INTERFACE_FORMAT_TYPE:
      TU_VERIFY(TUH_VALIDATE_BASIC(tu_desc_len(p_desc) >= 8), false);
      info->format_type = p_desc[3];
      if (info->format_type != AUDIO10_FORMAT_TYPE_I) {
        break;
      }
      info->channels          = p_desc[4];
      info->subslot_size      = p_desc[5];
      info->bit_resolution    = p_desc[6];
      info->sample_rate_count = 0;
      info->sample_rate_data  = NULL;
      if (p_desc[7] > 0) {
        TU_VERIFY(TUH_VALIDATE_BASIC(p_desc[7] <= (tu_desc_len(p_desc) - 8u) / 3u), false);
        info->sample_rate_count = TU_MIN(p_desc[7], CFG_TUH_AUDIO_MAX_SAM_FREQ);
        info->sample_rate_data  = &p_desc[8];
      }
      break;
    default:
      break;
  }
  return true;
}
  #endif

  #if CFG_TUH_AUDIO_PROTOCOLS & TUH_AUDIO_PROTOCOL_UAC2
static bool audioh_uac2_parse_as_interface(const uint8_t *p_desc, audioh_as_class_info_t *info) {
  switch (tu_desc_subtype(p_desc)) {
    case AUDIO20_CS_AS_INTERFACE_AS_GENERAL: {
      TU_VERIFY(TUH_VALIDATE_BASIC(tu_desc_len(p_desc) >= sizeof(audio20_desc_cs_as_interface_t)), false);
      const audio20_desc_cs_as_interface_t *general = (const audio20_desc_cs_as_interface_t *)p_desc;
      info->terminal_id                             = general->bTerminalLink;
      info->format_type                             = general->bFormatType;
      info->format_bitmap                           = tu_le32toh(general->bmFormats);
      info->channels                                = general->bNrChannels;
      break;
    }
    case AUDIO20_CS_AS_INTERFACE_FORMAT_TYPE: {
      TU_VERIFY(TUH_VALIDATE_BASIC(tu_desc_len(p_desc) >= sizeof(audio20_desc_type_I_format_t)), false);
      const audio20_desc_type_I_format_t *format = (const audio20_desc_type_I_format_t *)p_desc;
      if (format->bFormatType == AUDIO20_FORMAT_TYPE_I) {
        info->subslot_size   = format->bSubslotSize;
        info->bit_resolution = format->bBitResolution;
      }
      break;
    }
    default:
      break;
  }
  return true;
}
  #endif

static bool audioh_parse_as_interface(audioh_interface_t *p_audio, const uint8_t *p_desc,
                                      audioh_as_class_info_t *info) {
  switch (p_audio->protocol) {
  #if CFG_TUH_AUDIO_PROTOCOLS & TUH_AUDIO_PROTOCOL_UAC1
    case AUDIO_INT_PROTOCOL_CODE_V1:
      return audioh_uac1_parse_as_interface(p_desc, info);
  #endif
  #if CFG_TUH_AUDIO_PROTOCOLS & TUH_AUDIO_PROTOCOL_UAC2
    case AUDIO_INT_PROTOCOL_CODE_V2:
      return audioh_uac2_parse_as_interface(p_desc, info);
  #endif
    default:
      return false;
  }
}

  #if CFG_TUH_AUDIO_PROTOCOLS & TUH_AUDIO_PROTOCOL_UAC2
static int8_t audioh_uac2_rate_source_get(audioh_interface_t *p_audio, const audioh_ac_map_t *map,
                                          const audioh_terminal_info_t *terminal) {
  if (terminal->clock_id == 0) {
    return -1;
  }
  const audioh_clock_info_t *clock = audioh_ac_clock_find(map, terminal->clock_id);
  if (clock == NULL || clock->frequency_access == AUDIOH_CTRL_NONE) {
    return -1;
  }
  for (uint8_t i = 0; i < p_audio->rate_source_count; i++) {
    if (p_audio->rate_source[i].control_id == clock->id) {
      return (int8_t)i;
    }
  }
  if (p_audio->rate_source_count >= AUDIOH_MAX_RATE_SOURCES) {
    return -1;
  }
  const uint8_t idx = p_audio->rate_source_count++;
  p_audio->rate_source[idx] =
    (audioh_rate_source_t){.control_id = clock->id, .frequency_access = clock->frequency_access};
  return (int8_t)idx;
}
  #endif

  #if CFG_TUH_AUDIO_PROTOCOLS & TUH_AUDIO_PROTOCOL_UAC1
static bool audioh_uac1_rates_store(const audioh_interface_t *p_audio, const tuh_audio_stream_t *stream,
                                    audioh_as_config_t *as, const audioh_as_class_info_t *info,
                                    audioh_rate_source_t *rate_source) {
  for (uint8_t i = 0; i < info->sample_rate_count; i++) {
    const uint8_t *rate_data = &info->sample_rate_data[i * 3u];
    const uint32_t sample_rate =
      (uint32_t)rate_data[0] | ((uint32_t)rate_data[1] << 8) | ((uint32_t)rate_data[2] << 16);
    if (sample_rate == 0 || !audioh_as_rate_fits(p_audio, stream, as, sample_rate)) {
      continue;
    }

    const uint8_t rate_idx             = rate_source->sample_rate_count++;
    rate_source->sample_rate[rate_idx] = sample_rate;
    as->rate_count++;
  }
  return as->rate_count > 0;
}
  #endif

// Parse one Audio Streaming interface alternate setting and register its
// supported configurations into the matching stream. Returns the descriptor
// pointer of the next interface.
static const uint8_t *audioh_parse_as(audioh_interface_t *p_audio, const audioh_ac_map_t *ac_map,
                                      const tusb_desc_interface_t *desc_itf, const uint8_t *p_desc,
                                      const uint8_t *desc_end) {
  #if !(CFG_TUH_AUDIO_PROTOCOLS & TUH_AUDIO_PROTOCOL_UAC2)
  (void)ac_map;
  #endif
  TU_VERIFY(audioh_desc_valid(p_desc, desc_end, sizeof(tusb_desc_interface_t)), NULL);

  const uint8_t itf_num = desc_itf->bInterfaceNumber;
  const uint8_t alt     = desc_itf->bAlternateSetting;

  p_desc = tu_desc_next(p_desc);

  // Alternate setting 0 has no endpoints: nothing to stream
  if (alt == 0 || desc_itf->bNumEndpoints == 0) {
    while (p_desc < desc_end) {
      TU_VERIFY(audioh_desc_valid(p_desc, desc_end, 2), NULL);
      if (tu_desc_type(p_desc) == TUSB_DESC_INTERFACE) {
        break;
      }
      p_desc = tu_desc_next(p_desc);
    }
    return p_desc;
  }

  // Parse the class-specific and endpoint descriptors of this alternate setting
  audioh_as_class_info_t class_info = {0};

  // An AS alternate setting has one audio data endpoint and may have one
  // explicit feedback endpoint. Implicit-feedback endpoints are data endpoints
  // and are handled normally when they are the AS interface's data endpoint.
  typedef struct {
    uint8_t  ep_addr;
    uint16_t ep_size;
    uint8_t  ep_interval;
    uint8_t  ep_attr;
    bool     sam_freq_ctrl;
  } audioh_ep_info_t;
  audioh_ep_info_t ep_info         = {0};
  audioh_ep_info_t fb_info         = {0};
  bool             has_data_ep     = false;
  bool             has_feedback_ep = false;

  while (p_desc < desc_end) {
    TU_VERIFY(audioh_desc_valid(p_desc, desc_end, 2), NULL);
    if (tu_desc_type(p_desc) == TUSB_DESC_INTERFACE) {
      break;
    }

    switch (tu_desc_type(p_desc)) {
      case TUSB_DESC_CS_INTERFACE: {
        TU_VERIFY(audioh_desc_valid(p_desc, desc_end, 3), NULL);
        TU_VERIFY(audioh_parse_as_interface(p_audio, p_desc, &class_info), NULL);
        break;
      }
      case TUSB_DESC_CS_ENDPOINT: {
        TU_VERIFY(audioh_desc_valid(p_desc, desc_end, 3), NULL);
        if (p_audio->protocol == AUDIO_INT_PROTOCOL_CODE_V1 &&
            tu_desc_subtype(p_desc) == AUDIO10_CS_EP_SUBTYPE_GENERAL) {
          TU_VERIFY(TUH_VALIDATE_BASIC(tu_desc_len(p_desc) >= 4), NULL);
          const audio10_desc_cs_as_iso_data_ep_t *desc_ep = (const audio10_desc_cs_as_iso_data_ep_t *)p_desc;
          ep_info.sam_freq_ctrl = (desc_ep->bmAttributes & AUDIO10_CS_AS_ISO_DATA_EP_ATT_SAMPLING_FRQ) != 0;
        }
        break;
      }
      case TUSB_DESC_ENDPOINT: {
        TU_VERIFY(TUH_VALIDATE_BASIC(tu_desc_len(p_desc) >= sizeof(tusb_desc_endpoint_t)), NULL);
        const tusb_desc_endpoint_t *desc_endpoint = (const tusb_desc_endpoint_t *)p_desc;
        if (desc_endpoint->bmAttributes.xfer != TUSB_XFER_ISOCHRONOUS) {
          break;
        }

        const uint8_t usage = desc_endpoint->bmAttributes.usage;
        const bool    implicit_feedback =
          usage == (TUSB_ISO_EP_ATT_IMPLICIT_FB >> 4) && tu_edpt_dir(desc_endpoint->bEndpointAddress) == TUSB_DIR_IN;
        const bool explicit_feedback =
          tu_edpt_dir(desc_endpoint->bEndpointAddress) == TUSB_DIR_IN &&
          (usage == (TUSB_ISO_EP_ATT_EXPLICIT_FB >> 4) ||
           (usage == (TUSB_ISO_EP_ATT_DATA >> 4) && desc_endpoint->bmAttributes.sync == TUSB_ISO_EP_ATT_NO_SYNC));

        if (explicit_feedback) {
          const uint16_t fb_ep_size = tu_edpt_packet_size(desc_endpoint);
          if (has_feedback_ep || (fb_ep_size != 3 && fb_ep_size != 4)) {
            TU_LOG_DRV("  AUDIO AS itf %u alt %u: invalid/extra feedback ep %02x ignored\r\n", itf_num, alt,
                       desc_endpoint->bEndpointAddress);
            break;
          }

          fb_info.ep_addr     = desc_endpoint->bEndpointAddress;
          fb_info.ep_size     = fb_ep_size;
          fb_info.ep_interval = desc_endpoint->bInterval;
          if (fb_info.ep_interval == 0 || fb_info.ep_interval > 16) {
            fb_info.ep_interval = 1;
          }
          fb_info.ep_attr =
            (uint8_t)((desc_endpoint->bmAttributes.sync << 2) | (desc_endpoint->bmAttributes.usage << 4));
          has_feedback_ep = true;
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
          ep_info.ep_attr =
            (uint8_t)((desc_endpoint->bmAttributes.sync << 2) | (desc_endpoint->bmAttributes.usage << 4));
          has_data_ep = true;
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

  // Reject unsupported formats explicitly.
  bool pcm_supported = false;
  switch (p_audio->protocol) {
  #if CFG_TUH_AUDIO_PROTOCOLS & TUH_AUDIO_PROTOCOL_UAC1
    case AUDIO_INT_PROTOCOL_CODE_V1:
      pcm_supported =
        class_info.format_type == AUDIO10_FORMAT_TYPE_I && class_info.format_tag == AUDIO10_DATA_FORMAT_TYPE_I_PCM;
      break;
  #endif
  #if CFG_TUH_AUDIO_PROTOCOLS & TUH_AUDIO_PROTOCOL_UAC2
    case AUDIO_INT_PROTOCOL_CODE_V2:
      pcm_supported = class_info.format_type == AUDIO20_FORMAT_TYPE_I &&
                      (class_info.format_bitmap & AUDIO20_DATA_FORMAT_TYPE_I_PCM) != 0;
      break;
  #endif
    default:
      break;
  }
  if (!pcm_supported) {
    TU_LOG_DRV("  AUDIO AS itf %u: Type-I PCM format not supported\r\n", itf_num);
    return p_desc;
  }
  tuh_audio_format_t format;
  if (!audioh_format_from_pcm(class_info.subslot_size, class_info.bit_resolution, &format)) {
    TU_LOG_DRV("  AUDIO AS itf %u: subslot %u bits %u not supported\r\n", itf_num, class_info.subslot_size,
               class_info.bit_resolution);
    return p_desc;
  }
  if (class_info.channels == 0) {
    TU_LOG_DRV("  AUDIO AS itf %u: zero channels not supported\r\n", itf_num);
    return p_desc;
  }

  const uint16_t iso_xfer_size =
    (tuh_speed_get(p_audio->daddr) == TUSB_SPEED_HIGH) ? TUSB_EPSIZE_ISO_HS_MAX : TUSB_EPSIZE_ISO_FS_MAX;
  const uint32_t frame_bytes_32 = (uint32_t)class_info.channels * tuh_audio_format_bytes(format);
  if (frame_bytes_32 == 0 || frame_bytes_32 > iso_xfer_size) {
    TU_LOG_DRV("  AUDIO AS itf %u: frame size %lu not supported\r\n", itf_num, (unsigned long)frame_bytes_32);
    return p_desc;
  }
  // Register one AS alternate setting containing its discrete sampling
  // frequencies. The public API flattens these entries when requested.
  const audioh_ep_info_t *ep     = &ep_info;
  tuh_audio_stream_t     *stream = audioh_get_stream(p_audio, tu_edpt_dir(ep->ep_addr));
  TU_ASSERT(stream != NULL, p_desc);
  const audioh_terminal_info_t *terminal = audioh_ac_terminal_find(ac_map, class_info.terminal_id);
  if (terminal == NULL || terminal->stream_dir != stream->dir) {
    TU_LOG_DRV("  AUDIO AS itf %u alt %u: terminal %u does not match endpoint direction\r\n", itf_num, alt,
               class_info.terminal_id);
    return p_desc;
  }

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

  audioh_as_config_t as_config = {.ep_size     = ep->ep_size,
                                  .itf_num     = itf_num,
                                  .alt_setting = alt,
                                  .ep_addr     = ep->ep_addr,
                                  .ep_interval = ep->ep_interval,
                                  .ep_attr     = ep->ep_attr,
                                  .format      = (uint8_t)format,
                                  .channels    = class_info.channels,
                                  .terminal_id = class_info.terminal_id};
  #if CFG_TUH_AUDIO_PROTOCOLS & TUH_AUDIO_PROTOCOL_UAC1
  audioh_rate_source_t rate_source = {0};
  #endif

  switch (p_audio->protocol) {
  #if CFG_TUH_AUDIO_PROTOCOLS & TUH_AUDIO_PROTOCOL_UAC1
    case AUDIO_INT_PROTOCOL_CODE_V1:
      rate_source.control_id       = ep->ep_addr;
      rate_source.frequency_access = ep->sam_freq_ctrl ? AUDIOH_CTRL_READ_WRITE : AUDIOH_CTRL_NONE;
      if (!audioh_uac1_rates_store(p_audio, stream, &as_config, &class_info, &rate_source)) {
        TU_LOG_DRV("  AUDIO AS itf %u alt %u: no supported sampling frequency\r\n", itf_num, alt);
        return p_desc;
      }
      break;
  #endif
  #if CFG_TUH_AUDIO_PROTOCOLS & TUH_AUDIO_PROTOCOL_UAC2
    case AUDIO_INT_PROTOCOL_CODE_V2: {
      const int8_t rate_source_idx = audioh_uac2_rate_source_get(p_audio, ac_map, terminal);
      if (rate_source_idx < 0) {
        TU_LOG_DRV("  AUDIO AS itf %u alt %u: direct Clock Source not found\r\n", itf_num, alt);
        return p_desc;
      }
      as_config.rate_source_idx = (uint8_t)rate_source_idx;
      break;
    }
  #endif
    default:
      return p_desc;
  }
  if (stream->as_count >= CFG_TUH_AUDIO_MAX_AS) {
    TU_LOG_DRV("  AUDIO AS itf %u alt %u: reach max alternate settings %u\r\n", itf_num, alt, CFG_TUH_AUDIO_MAX_AS);
    return p_desc;
  }
  #if CFG_TUH_AUDIO_PROTOCOLS & TUH_AUDIO_PROTOCOL_UAC1
  if (p_audio->protocol == AUDIO_INT_PROTOCOL_CODE_V1 && p_audio->rate_source_count >= AUDIOH_MAX_RATE_SOURCES) {
    TU_LOG_DRV("  AUDIO AS itf %u alt %u: reach max rate sources %u\r\n", itf_num, alt, AUDIOH_MAX_RATE_SOURCES);
    return p_desc;
  }
  #endif

  const uint8_t as_idx = stream->as_count;
  #if CFG_TUH_AUDIO_PROTOCOLS & TUH_AUDIO_PROTOCOL_UAC1
  if (p_audio->protocol == AUDIO_INT_PROTOCOL_CODE_V1) {
    as_config.rate_source_idx                          = p_audio->rate_source_count;
    p_audio->rate_source[p_audio->rate_source_count++] = rate_source;
  }
  #endif
  stream->as[as_idx] = as_config;
  if (stream->dir == TUSB_DIR_OUT && has_feedback_ep) {
    audioh_feedback_ep_t *feedback = &p_audio->playback.feedback[as_idx];
    feedback->ep_addr              = fb_info.ep_addr;
    feedback->ep_size              = (uint8_t)fb_info.ep_size;
    feedback->ep_interval          = fb_info.ep_interval;
    feedback->ep_attr              = fb_info.ep_attr;
  }
  stream->as_count++;
  stream->config_count += as_config.rate_count;

  return p_desc;
}

uint16_t audioh_open(uint8_t rhport, uint8_t dev_addr, const tusb_desc_interface_t *desc_itf, uint16_t max_len) {
  (void)rhport;

  TU_VERIFY(TUH_VALIDATE_BASIC(max_len >= sizeof(tusb_desc_interface_t)), 0);

  const uint8_t *desc_start = (const uint8_t *)desc_itf;
  const uint8_t *p_desc     = desc_start;
  const uint8_t *desc_end   = desc_start + max_len;
  TU_VERIFY(audioh_desc_valid(p_desc, desc_end, sizeof(tusb_desc_interface_t)), 0);
  TU_VERIFY(TUH_VALIDATE_BASIC(tu_desc_type(desc_itf) == TUSB_DESC_INTERFACE), 0);
  TU_VERIFY(TUSB_CLASS_AUDIO == desc_itf->bInterfaceClass, 0);
  TU_VERIFY(AUDIO_SUBCLASS_CONTROL == desc_itf->bInterfaceSubClass, 0);
  TU_VERIFY(audioh_protocol_enabled(desc_itf->bInterfaceProtocol), 0);

  const uint8_t idx = find_new_audio_index();
  TU_VERIFY(idx < CFG_TUH_AUDIO_MAX, 0);
  audioh_interface_t *p_audio = &_audioh_itf[idx];
  p_audio->daddr              = dev_addr;
  p_audio->ac_itf_num         = desc_itf->bInterfaceNumber;
  p_audio->protocol           = desc_itf->bInterfaceProtocol;
  p_audio->rate_source_count  = 0;
  tu_memclr(p_audio->rate_source, sizeof(p_audio->rate_source));
  tu_memclr(&p_audio->ctrl, sizeof(p_audio->ctrl));
  audioh_stream_reset(&p_audio->in_stream);
  audioh_stream_reset(&p_audio->out_stream);
  audioh_playback_reset(&p_audio->playback);
  p_audio->in_stream.daddr  = dev_addr;
  p_audio->out_stream.daddr = dev_addr;

  TU_LOG_DRV("AUDIO opening AC Interface %u (addr = %u)\r\n", desc_itf->bInterfaceNumber, dev_addr);

  audioh_ac_map_t ac_map = {0};

  p_desc = tu_desc_next(p_desc);
  while (p_desc < desc_end) {
    if (!audioh_desc_valid(p_desc, desc_end, 2)) {
      goto open_failed;
    }
    if (tu_desc_type(p_desc) == TUSB_DESC_INTERFACE) {
      break;
    }

    if (tu_desc_type(p_desc) == TUSB_DESC_CS_INTERFACE) {
      if (!audioh_desc_valid(p_desc, desc_end, 3)) {
        goto open_failed;
      }
      if (!audioh_parse_ac_entity(p_audio, &ac_map, p_desc)) {
        goto open_failed;
      }
    }
    p_desc = tu_desc_next(p_desc);
  }

  // Parse the contiguous Audio Streaming interfaces of this audio function.
  while (p_desc < desc_end) {
    if (!audioh_desc_valid(p_desc, desc_end, 2)) {
      goto open_failed;
    }
    if (tu_desc_type(p_desc) != TUSB_DESC_INTERFACE) {
      p_desc = tu_desc_next(p_desc);
      continue;
    }

    if (!audioh_desc_valid(p_desc, desc_end, sizeof(tusb_desc_interface_t))) {
      goto open_failed;
    }
    const tusb_desc_interface_t *desc_interface = (const tusb_desc_interface_t *)p_desc;
    if (desc_interface->bInterfaceClass != TUSB_CLASS_AUDIO ||
        desc_interface->bInterfaceSubClass != AUDIO_SUBCLASS_STREAMING ||
        desc_interface->bInterfaceProtocol != p_audio->protocol) {
      break;
    }

    TU_LOG_DRV("  Found AS Interface %u (alt = %u)\r\n", desc_interface->bInterfaceNumber,
               desc_interface->bAlternateSetting);
    p_desc = audioh_parse_as(p_audio, &ac_map, desc_interface, p_desc, desc_end);
    if (p_desc == NULL) {
      goto open_failed;
    }
  }

  audioh_link_feature_units(p_audio, &ac_map);

  // UAC2 configurations receive their rates asynchronously during mount.
  // Release the tentative instance when no supported AS alternate was found.
  if (p_audio->in_stream.as_count == 0 && p_audio->out_stream.as_count == 0) {
    goto open_failed;
  }

  // Assign stream indices: playback first, then capture, so the application
  // can iterate [0, stream_count) without gaps
  uint8_t stream_idx = 0;
  if (p_audio->out_stream.as_count > 0) {
    p_audio->out_stream.stream_idx = stream_idx++;
  }
  if (p_audio->in_stream.as_count > 0) {
    p_audio->in_stream.stream_idx = stream_idx++;
  }
  p_audio->stream_count = stream_idx;

  return (uint16_t)((uintptr_t)p_desc - (uintptr_t)desc_start);

open_failed:
  audioh_stream_reset(&p_audio->in_stream);
  audioh_stream_reset(&p_audio->out_stream);
  audioh_playback_reset(&p_audio->playback);
  p_audio->daddr             = 0;
  p_audio->ac_itf_num        = 0;
  p_audio->protocol          = 0;
  p_audio->stream_count      = 0;
  p_audio->rate_source_count = 0;
  p_audio->mounted           = false;
  return 0;
}

//--------------------------------------------------------------------+
// Set Configuration
//--------------------------------------------------------------------+
static void audioh_mount_feature_unit_next(uint8_t idx);

  #if CFG_TUH_AUDIO_PROTOCOLS & TUH_AUDIO_PROTOCOL_UAC2
static void audioh_mount_clock_complete(tuh_xfer_t *xfer);

static void audioh_uac2_configs_rebuild(audioh_interface_t *p_audio) {
  p_audio->in_stream.config_count  = 0;
  p_audio->out_stream.config_count = 0;

  for (uint8_t direction = TUSB_DIR_OUT; direction <= TUSB_DIR_IN; direction++) {
    tuh_audio_stream_t *stream = audioh_get_stream(p_audio, (tusb_dir_t)direction);
    TU_ASSERT(stream != NULL, );
    for (uint8_t as_idx = 0; as_idx < stream->as_count; as_idx++) {
      audioh_as_config_t   *as          = &stream->as[as_idx];
      audioh_rate_source_t *rate_source = audioh_as_rate_source(stream, as);
      as->rate_count                    = 0;
      for (uint8_t rate_idx = 0; rate_idx < rate_source->sample_rate_count; rate_idx++) {
        if (audioh_as_rate_fits(p_audio, stream, as, rate_source->sample_rate[rate_idx])) {
          as->rate_count++;
        }
      }
      stream->config_count += as->rate_count;
    }
  }

  uint8_t stream_idx             = 0;
  p_audio->out_stream.stream_idx = TUSB_INDEX_INVALID_8;
  p_audio->in_stream.stream_idx  = TUSB_INDEX_INVALID_8;
  if (p_audio->out_stream.config_count > 0) {
    p_audio->out_stream.stream_idx = stream_idx++;
  }
  if (p_audio->in_stream.config_count > 0) {
    p_audio->in_stream.stream_idx = stream_idx++;
  }
  p_audio->stream_count = stream_idx;
}

static bool audioh_uac2_clock_range_store(audioh_rate_source_t *rate_source, const uint8_t *buffer, uint16_t length) {
  TU_VERIFY(length >= 2, false);
  const uint16_t subrange_count = tu_le16toh(tu_unaligned_read16(buffer));
  const uint16_t available      = (uint16_t)((length - 2u) / 12u);
  TU_VERIFY(subrange_count > 0 && available > 0, false);

  rate_source->sample_rate_count = 0;
  const uint16_t parsed_count    = TU_MIN(subrange_count, available);
  for (uint16_t i = 0; i < parsed_count && rate_source->sample_rate_count < CFG_TUH_AUDIO_MAX_SAM_FREQ; i++) {
    const uint8_t *subrange = &buffer[2u + 12u * i];
    const uint32_t min      = tu_le32toh(tu_unaligned_read32(&subrange[0]));
    const uint32_t max      = tu_le32toh(tu_unaligned_read32(&subrange[4]));
    const uint32_t res      = tu_le32toh(tu_unaligned_read32(&subrange[8]));
    if (min == 0 || min > max || (min != max && res == 0)) {
      continue;
    }
    if (min == max) {
      rate_source->sample_rate[rate_source->sample_rate_count++] = min;
      continue;
    }
    for (uint32_t rate = min; rate <= max && rate_source->sample_rate_count < CFG_TUH_AUDIO_MAX_SAM_FREQ;) {
      rate_source->sample_rate[rate_source->sample_rate_count++] = rate;
      if (max - rate < res) {
        break;
      }
      rate += res;
    }
  }
  return rate_source->sample_rate_count > 0;
}

static bool audioh_mount_clock_submit(uint8_t idx) {
  audioh_interface_t   *p_audio     = &_audioh_itf[idx];
  audioh_epbuf_t       *epbuf       = &_audioh_epbuf[idx];
  audioh_ctrl_state_t  *ctrl        = &p_audio->ctrl;
  audioh_rate_source_t *rate_source = &p_audio->rate_source[ctrl->clock.rate_source_idx];
  const uint16_t        length      = ctrl->clock.read_cur ? 4u : (uint16_t)sizeof(epbuf->control.clock_range);

  const tusb_control_request_t request = {
    .bmRequestType_bit = {.recipient = TUSB_REQ_RCPT_INTERFACE, .type = TUSB_REQ_TYPE_CLASS, .direction = TUSB_DIR_IN},
    .bRequest          = ctrl->clock.read_cur ? AUDIO20_CS_REQ_CUR : AUDIO20_CS_REQ_RANGE,
    .wValue            = tu_htole16(tu_u16(AUDIO20_CS_CTRL_SAM_FREQ, 0)),
    .wIndex            = tu_htole16(tu_u16(rate_source->control_id, p_audio->ac_itf_num)),
    .wLength           = tu_htole16(length),
  };
  tuh_xfer_t xfer = {.daddr       = p_audio->daddr,
                     .ep_addr     = 0,
                     .setup       = &request,
                     .buffer      = epbuf->control.clock_range,
                     .complete_cb = audioh_mount_clock_complete,
                     .user_data   = (uintptr_t)idx};
  return tuh_control_xfer(&xfer);
}

static void audioh_mount_clock_finish(uint8_t idx) {
  audioh_interface_t  *p_audio = &_audioh_itf[idx];
  audioh_ctrl_state_t *ctrl    = &p_audio->ctrl;
  ctrl->fu_busy                = false;
  audioh_uac2_configs_rebuild(p_audio);

  if (p_audio->stream_count == 0) {
    const uint8_t daddr   = p_audio->daddr;
    const uint8_t itf_num = p_audio->ac_itf_num;
    audioh_stream_reset(&p_audio->in_stream);
    audioh_stream_reset(&p_audio->out_stream);
    audioh_playback_reset(&p_audio->playback);
    p_audio->daddr             = 0;
    p_audio->ac_itf_num        = 0;
    p_audio->protocol          = 0;
    p_audio->rate_source_count = 0;
    usbh_driver_set_config_complete(daddr, itf_num);
    return;
  }

  ctrl->fu.mount.stream_idx = 0;
  audioh_mount_feature_unit_next(idx);
}

static void audioh_mount_clock_next(uint8_t idx) {
  audioh_interface_t  *p_audio = &_audioh_itf[idx];
  audioh_ctrl_state_t *ctrl    = &p_audio->ctrl;

  while (ctrl->clock.rate_source_idx < p_audio->rate_source_count) {
    ctrl->clock.read_cur = false;
    ctrl->fu_busy        = true;
    if (audioh_mount_clock_submit(idx)) {
      return;
    }
    p_audio->rate_source[ctrl->clock.rate_source_idx].sample_rate_count = 0;
    ctrl->clock.rate_source_idx++;
  }
  audioh_mount_clock_finish(idx);
}

static void audioh_mount_clock_complete(tuh_xfer_t *xfer) {
  const uint8_t        idx     = (uint8_t)xfer->user_data;
  audioh_interface_t  *p_audio = &_audioh_itf[idx];
  audioh_ctrl_state_t *ctrl    = &p_audio->ctrl;
  audioh_epbuf_t      *epbuf   = &_audioh_epbuf[idx];
  if (!ctrl->fu_busy) {
    return;
  }
  audioh_rate_source_t *rate_source = &p_audio->rate_source[ctrl->clock.rate_source_idx];

  bool success = xfer->result == XFER_RESULT_SUCCESS;
  if (success && ctrl->clock.read_cur) {
    success = xfer->actual_len == 4;
    if (success) {
      const uint32_t current = tu_le32toh(tu_unaligned_read32(epbuf->control.clock_range));
      success                = current > 0;
      if (success) {
        rate_source->sample_rate[0]    = current;
        rate_source->sample_rate_count = 1;
      }
    }
  } else if (success) {
    success = audioh_uac2_clock_range_store(rate_source, epbuf->control.clock_range, (uint16_t)xfer->actual_len);
    if (success && rate_source->frequency_access == AUDIOH_CTRL_READ) {
      ctrl->clock.read_cur = true;
      if (audioh_mount_clock_submit(idx)) {
        return;
      }
      success = false;
    }
  }

  if (!success) {
    rate_source->sample_rate_count = 0;
  }
  ctrl->fu_busy = false;
  ctrl->clock.rate_source_idx++;
  audioh_mount_clock_next(idx);
}
  #endif

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

  audioh_ctrl_state_t *ctrl = &_audioh_itf[idx].ctrl;
  #if CFG_TUH_AUDIO_PROTOCOLS & TUH_AUDIO_PROTOCOL_UAC2
  if (_audioh_itf[idx].protocol == AUDIO_INT_PROTOCOL_CODE_V2) {
    ctrl->clock.rate_source_idx = 0;
    audioh_mount_clock_next(idx);
  } else
  #endif
  {
    ctrl->fu.mount.stream_idx = 0;
    audioh_mount_feature_unit_next(idx);
  }
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

bool tuh_audio_mute_supported(uint8_t idx, uint8_t stream_idx) {
  TU_VERIFY(idx < CFG_TUH_AUDIO_MAX, false);
  audioh_interface_t *p_audio = &_audioh_itf[idx];
  TU_VERIFY(p_audio->mounted, false);
  tuh_audio_stream_t *s = audioh_get_stream_by_idx(p_audio, stream_idx);
  return s != NULL && s->mute_access != AUDIOH_CTRL_NONE;
}

bool tuh_audio_volume_range_get(uint8_t idx, uint8_t stream_idx, tuh_audio_volume_range_t *range) {
  TU_VERIFY(idx < CFG_TUH_AUDIO_MAX && range != NULL, false);
  audioh_interface_t *p_audio = &_audioh_itf[idx];
  TU_VERIFY(p_audio->mounted, false);
  tuh_audio_stream_t *s = audioh_get_stream_by_idx(p_audio, stream_idx);
  TU_VERIFY(s != NULL && s->volume_access != AUDIOH_CTRL_NONE, false);
  *range = s->volume_range;
  return true;
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

  return audioh_stream_config_get(s, config_idx, config);
}

bool tuh_audio_configure(uint8_t dev_idx, uint8_t stream_idx, uint8_t config_idx) {
  TU_VERIFY(dev_idx < CFG_TUH_AUDIO_MAX, false);
  audioh_interface_t *p_audio = &_audioh_itf[dev_idx];
  TU_VERIFY(p_audio->mounted, false);

  tuh_audio_stream_t *s = audioh_get_stream_by_idx(p_audio, stream_idx);
  TU_VERIFY(s, false);
  TU_VERIFY(config_idx < s->config_count, false);
  tuh_audio_stream_config_t cfg;
  uint8_t                   as_idx;
  uint8_t                   rate_idx;
  TU_VERIFY(audioh_stream_resolve_config(s, config_idx, &as_idx, &rate_idx), false);
  audioh_stream_config_fill(s, as_idx, rate_idx, &cfg);
  // Reconfiguration is allowed from a stopped stream.
  TU_VERIFY(!s->running, false);
  if (s->state == STREAM_STATE_READY) {
    // Wait for any in-flight transfer to complete and be discarded
    TU_VERIFY(!usbh_edpt_busy(s->daddr, s->edpt.ep_addr), false);
    if (s->dir == TUSB_DIR_OUT && p_audio->playback.feedback_opened) {
      TU_VERIFY(!usbh_edpt_busy(s->daddr, p_audio->playback.feedback[s->active_as].ep_addr), false);
    }
  }

  tuh_audio_stream_t *other = (s == &p_audio->out_stream) ? &p_audio->in_stream : &p_audio->out_stream;
  if (other->active_config != TUSB_INDEX_INVALID_8) {
    tuh_audio_stream_config_t other_cfg;
    audioh_stream_config_fill(other, other->active_as, other->active_rate, &other_cfg);
    if (cfg.sample_rate != other_cfg.sample_rate) {
      TU_LOG_DRV("  AUDIO configure failed: capture/playback sample rates must match (%lu != %lu)\r\n",
                 (unsigned long)cfg.sample_rate, (unsigned long)other_cfg.sample_rate);
      return false;
    }
  }

  // The HCD endpoint must be reopened even when the new configuration uses
  // the same address, since its packet size and interval may have changed.
  TU_VERIFY(audioh_stream_close_ep(s), false);

  const audioh_as_config_t *as = &s->as[as_idx];
  s->active_config             = config_idx;
  s->active_as                 = as_idx;
  s->active_rate               = rate_idx;
  s->frame_bytes               = (uint16_t)tuh_audio_config_frame_size(&cfg);
  s->state                     = STREAM_STATE_IDLE;
  if (s->dir == TUSB_DIR_OUT) {
    const uint32_t     frame_div  = (tuh_speed_get(s->daddr) == TUSB_SPEED_HIGH) ? 8000u : 1000u;
    audioh_playback_t *playback   = &p_audio->playback;
    playback->nominal_frames_q16  = audioh_nominal_frames_q16(cfg.sample_rate, as->ep_interval, s->daddr);
    playback->target_frames_q16   = playback->nominal_frames_q16;
    playback->pending_frames_q16  = 0;
    playback->feedback_min_frames = (uint16_t)((cfg.sample_rate - 1u) / frame_div);
    playback->feedback_max_frames = (uint16_t)(cfg.sample_rate / frame_div + 1u);
    playback->feedback_pending    = false;
    playback->rem_acc             = 0;
  }
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
             (s->dir == TUSB_DIR_IN) ? "capture" : "playback", s->stream_idx, as->itf_num, as->alt_setting,
             as->ep_addr);

  return audioh_stream_open_ep(s);
}

// Invoked when the SET_INTERFACE activating the stream's interface completes:
// the interface is active, set its sampling frequency before submitting
// transfers
static void audioh_stream_start_xfer(tuh_audio_stream_t *s) {
  if (s->dir == TUSB_DIR_IN) {
    audioh_stream_capture_xfer(s); // feed the capture endpoint
  } else {
    if (audioh_get_playback(s)->feedback[s->active_as].ep_addr != 0) {
      audioh_stream_feedback_xfer(s);
    }
    if (!s->running) {
      return;
    }
    audioh_stream_playback_xfer(s); // start the continuous playback transfer chain
  }
}

static void audioh_stream_start_complete(tuh_xfer_t *xfer);

static bool audioh_stream_activate(tuh_audio_stream_t *s) {
  const audioh_as_config_t *as = audioh_stream_active_as(s);
  return tuh_interface_set(s->daddr, as->itf_num, as->alt_setting, audioh_stream_start_complete, (uintptr_t)s);
}

static void audioh_stream_start_set_freq_complete(tuh_xfer_t *xfer) {
  tuh_audio_stream_t *s = (tuh_audio_stream_t *)xfer->user_data;
  if (s->daddr != xfer->daddr || s->state != STREAM_STATE_READY || !s->running) {
    return; // device is gone or the stream was stopped meanwhile
  }
  if (xfer->result != XFER_RESULT_SUCCESS) {
    TU_LOG_DRV("  AUDIO set sampling frequency failed: result=%u\r\n", xfer->result);
    audioh_stream_error(s, 0);
    return;
  }
  #if CFG_TUH_AUDIO_PROTOCOLS & TUH_AUDIO_PROTOCOL_UAC2
  if (_audioh_itf[s->idx].protocol == AUDIO_INT_PROTOCOL_CODE_V2) {
    if (!audioh_stream_activate(s)) {
      audioh_stream_error(s, 0);
    }
  } else
  #endif
  {
    audioh_stream_start_xfer(s);
  }
}

static bool audioh_stream_start_active(tuh_audio_stream_t *s) {
  #if CFG_TUH_AUDIO_PROTOCOLS & TUH_AUDIO_PROTOCOL_UAC1
  const audioh_as_config_t   *as          = audioh_stream_active_as(s);
  const audioh_rate_source_t *rate_source = audioh_as_rate_source(s, as);
  if (_audioh_itf[s->idx].protocol == AUDIO_INT_PROTOCOL_CODE_V1 &&
      rate_source->frequency_access == AUDIOH_CTRL_READ_WRITE) {
    if (!audioh_stream_set_freq(s, audioh_stream_start_set_freq_complete)) {
      audioh_stream_error(s, 0);
      return false;
    }
  } else
  #endif
  {
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
    audioh_stream_error(s, 0);
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
  const audioh_as_config_t *as = audioh_stream_active_as(s);
  TU_VERIFY(!usbh_edpt_busy(s->daddr, as->ep_addr), false);
  if (s->dir == TUSB_DIR_OUT && p_audio->playback.feedback_opened) {
    TU_VERIFY(!usbh_edpt_busy(s->daddr, p_audio->playback.feedback[s->active_as].ep_addr), false);
  }

  if (s->dir == TUSB_DIR_OUT) {
    p_audio->playback.target_frames_q16 = p_audio->playback.nominal_frames_q16;
    p_audio->playback.feedback_pending  = false;
    p_audio->playback.rem_acc           = 0;
  }
  s->running = true;
  // UAC2 changes the Clock Source before selecting the alternate setting;
  // UAC1 selects the alternate first because its control targets the endpoint.
  bool submitted = false;
  #if CFG_TUH_AUDIO_PROTOCOLS & TUH_AUDIO_PROTOCOL_UAC2
  const audioh_rate_source_t *rate_source = audioh_as_rate_source(s, as);
  if (p_audio->protocol == AUDIO_INT_PROTOCOL_CODE_V2 && rate_source->frequency_access == AUDIOH_CTRL_READ_WRITE) {
    submitted = audioh_stream_set_freq(s, audioh_stream_start_set_freq_complete);
  } else
  #endif
  {
    submitted = audioh_stream_activate(s);
  }
  if (!submitted) {
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

  const audioh_as_config_t *as = audioh_stream_active_as(s);
  // Leave the stream running if SET_INTERFACE cannot be submitted, so the
  // caller can retry without the host and device states diverging.
  TU_VERIFY(tuh_interface_set(s->daddr, as->itf_num, 0, audioh_stream_stop_complete, (uintptr_t)s), false);

  // The in-flight transfer (if any) completes and its data is discarded;
  // queued frames are dropped as well. The interface is being deactivated so
  // the device stops transferring.
  s->running = false;
  if (s->dir == TUSB_DIR_OUT) {
    p_audio->playback.target_frames_q16 = p_audio->playback.nominal_frames_q16;
    p_audio->playback.feedback_pending  = false;
    p_audio->playback.rem_acc           = 0;
  }
  tu_edpt_stream_clear(&s->edpt);
  return true;
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
  const uint8_t        idx       = (uint8_t)xfer->user_data;
  audioh_ctrl_state_t *ctrl      = &_audioh_itf[idx].ctrl;
  tuh_xfer_cb_t        app_cb    = ctrl->complete_cb;
  uintptr_t            user_data = ctrl->user_data;
  ctrl->complete_cb              = NULL;
  ctrl->fu_busy                  = false;

  xfer->user_data = user_data;
  if (app_cb != NULL) {
    app_cb(xfer);
  }
}

enum {
  AUDIOH_FU_VALUE_U16,
  AUDIOH_FU_VALUE_BOOL,
  AUDIOH_FU_VALUE_I16
};

static uint8_t audioh_fu_cur_request(uint8_t protocol, tusb_dir_t direction) {
  #if !(CFG_TUH_AUDIO_PROTOCOLS & TUH_AUDIO_PROTOCOL_UAC1)
  (void)direction;
  #endif
  switch (protocol) {
  #if CFG_TUH_AUDIO_PROTOCOLS & TUH_AUDIO_PROTOCOL_UAC1
    case AUDIO_INT_PROTOCOL_CODE_V1:
      return (direction == TUSB_DIR_IN) ? AUDIO10_CS_REQ_GET_CUR : AUDIO10_CS_REQ_SET_CUR;
  #endif
  #if CFG_TUH_AUDIO_PROTOCOLS & TUH_AUDIO_PROTOCOL_UAC2
    case AUDIO_INT_PROTOCOL_CODE_V2:
      return AUDIO20_CS_REQ_CUR;
  #endif
    default:
      return 0;
  }
}

static bool audioh_fu_selector_supported(uint8_t protocol, uint8_t control_selector) {
  #if CFG_TUH_AUDIO_PROTOCOLS & TUH_AUDIO_PROTOCOL_UAC2
  if (protocol == AUDIO_INT_PROTOCOL_CODE_V2) {
    return control_selector == AUDIO20_FU_CTRL_MUTE || control_selector == AUDIO20_FU_CTRL_VOLUME;
  }
  #else
  (void)protocol;
  (void)control_selector;
  #endif
  return true;
}

static void audioh_fu_value_store(audioh_ctrl_state_t *ctrl, audioh_epbuf_t *epbuf) {
  if (ctrl->fu.control.value_type == AUDIOH_FU_VALUE_BOOL) {
    *((bool *)ctrl->value) = audioh_fu_ctrl(epbuf)[0] != 0;
  } else if (ctrl->fu.control.width == 1) {
    *((uint16_t *)ctrl->value) = audioh_fu_ctrl(epbuf)[0];
  } else {
    const uint16_t value = tu_le16toh(tu_unaligned_read16(audioh_fu_ctrl(epbuf)));
    if (ctrl->fu.control.value_type == AUDIOH_FU_VALUE_I16) {
      *((int16_t *)ctrl->value) = (int16_t)value;
    } else {
      *((uint16_t *)ctrl->value) = value;
    }
  }
}

// Convert the raw control value to host order and chain to the application callback
static void audioh_fu_get_complete(tuh_xfer_t *xfer) {
  const uint8_t        idx       = (uint8_t)xfer->user_data;
  audioh_ctrl_state_t *ctrl      = &_audioh_itf[idx].ctrl;
  audioh_epbuf_t      *epbuf     = &_audioh_epbuf[idx];
  tuh_xfer_cb_t        app_cb    = ctrl->complete_cb;
  uintptr_t            user_data = ctrl->user_data;
  ctrl->complete_cb              = NULL;
  ctrl->fu_busy                  = false;

  if (ctrl->value != NULL && xfer->result == XFER_RESULT_SUCCESS) {
    if (xfer->actual_len == ctrl->fu.control.width) {
      audioh_fu_value_store(ctrl, epbuf);
    } else {
      xfer->result = XFER_RESULT_FAILED;
    }
  }

  xfer->user_data = user_data;
  if (app_cb != NULL) {
    app_cb(xfer);
  }
}

enum {
  AUDIOH_VOLUME_RANGE_MIN,
  AUDIOH_VOLUME_RANGE_MAX,
  AUDIOH_VOLUME_RANGE_RES,
  AUDIOH_VOLUME_RANGE_COUNT
};

static uint8_t audioh_fu_volume_range_request(uint8_t step) {
  switch (step) {
    case AUDIOH_VOLUME_RANGE_MIN:
      return AUDIO10_CS_REQ_GET_MIN;
    case AUDIOH_VOLUME_RANGE_MAX:
      return AUDIO10_CS_REQ_GET_MAX;
    case AUDIOH_VOLUME_RANGE_RES:
      return AUDIO10_CS_REQ_GET_RES;
    default:
      return AUDIO10_CS_REQ_UNDEF;
  }
}

static void audioh_fu_volume_range_store(tuh_audio_stream_t *s, audioh_ctrl_state_t *ctrl, audioh_epbuf_t *epbuf) {
  const uint16_t value = tu_le16toh(tu_unaligned_read16(audioh_fu_ctrl(epbuf)));
  switch (ctrl->fu.mount.range_step) {
    case AUDIOH_VOLUME_RANGE_MIN:
      s->volume_range.min = (int16_t)value;
      break;
    case AUDIOH_VOLUME_RANGE_MAX:
      s->volume_range.max = (int16_t)value;
      break;
    case AUDIOH_VOLUME_RANGE_RES:
      s->volume_range.res = value;
      break;
    default:
      break;
  }
}

static void audioh_mount_feature_unit_complete(tuh_xfer_t *xfer);

static bool audioh_mount_feature_unit_submit(uint8_t idx) {
  audioh_interface_t  *p_audio = &_audioh_itf[idx];
  audioh_ctrl_state_t *ctrl    = &p_audio->ctrl;
  audioh_epbuf_t      *epbuf   = &_audioh_epbuf[idx];
  tuh_audio_stream_t  *s       = audioh_get_stream_by_idx(p_audio, ctrl->fu.mount.stream_idx);
  TU_ASSERT(s != NULL);

  const bool                   uac2    = p_audio->protocol == AUDIO_INT_PROTOCOL_CODE_V2;
  const tusb_control_request_t request = {
    .bmRequestType_bit = {.recipient = TUSB_REQ_RCPT_INTERFACE, .type = TUSB_REQ_TYPE_CLASS, .direction = TUSB_DIR_IN},
    .bRequest          = uac2 ? AUDIO20_CS_REQ_RANGE : audioh_fu_volume_range_request(ctrl->fu.mount.range_step),
    .wValue            = tu_htole16(tu_u16(AUDIO10_FU_CTRL_VOLUME, 0)),
    .wIndex            = tu_htole16(tu_u16(s->feature_unit_id, p_audio->ac_itf_num)),
    .wLength           = tu_htole16(uac2 ? 8u : 2u),
  };
  tuh_xfer_t xfer = {.daddr       = p_audio->daddr,
                     .ep_addr     = 0,
                     .setup       = &request,
                     .buffer      = audioh_fu_ctrl(epbuf),
                     .complete_cb = audioh_mount_feature_unit_complete,
                     .user_data   = (uintptr_t)idx};
  return tuh_control_xfer(&xfer);
}

static void audioh_mount_feature_unit_next(uint8_t idx) {
  audioh_interface_t  *p_audio = &_audioh_itf[idx];
  audioh_ctrl_state_t *ctrl    = &p_audio->ctrl;

  while (ctrl->fu.mount.stream_idx < p_audio->stream_count) {
    tuh_audio_stream_t *s = audioh_get_stream_by_idx(p_audio, ctrl->fu.mount.stream_idx);
    TU_ASSERT(s != NULL, );
    if (s->volume_access != AUDIOH_CTRL_NONE) {
      s->volume_range           = (tuh_audio_volume_range_t){0};
      ctrl->fu.mount.range_step = AUDIOH_VOLUME_RANGE_MIN;
      ctrl->fu_busy             = true;
      if (audioh_mount_feature_unit_submit(idx)) {
        return;
      }
      s->volume_access = AUDIOH_CTRL_NONE;
      if (s->mute_access == AUDIOH_CTRL_NONE) {
        s->feature_unit_id = 0;
      }
      ctrl->fu_busy = false;
    }
    ctrl->fu.mount.stream_idx++;
  }

  p_audio->mounted = true;
  TU_LOG_DRV("  AUDIO mounted: addr = %u index = %u\r\n", p_audio->daddr, idx);
  tuh_audio_mount_cb(idx);
  usbh_driver_set_config_complete(p_audio->daddr, p_audio->ac_itf_num);
}

static void audioh_mount_feature_unit_complete(tuh_xfer_t *xfer) {
  const uint8_t        idx     = (uint8_t)xfer->user_data;
  audioh_interface_t  *p_audio = &_audioh_itf[idx];
  audioh_ctrl_state_t *ctrl    = &p_audio->ctrl;
  audioh_epbuf_t      *epbuf   = &_audioh_epbuf[idx];
  if (!ctrl->fu_busy) {
    return;
  }
  tuh_audio_stream_t *s = audioh_get_stream_by_idx(p_audio, ctrl->fu.mount.stream_idx);
  TU_ASSERT(s != NULL, );

  const bool uac2 = p_audio->protocol == AUDIO_INT_PROTOCOL_CODE_V2;
  if (uac2 && xfer->result == XFER_RESULT_SUCCESS && xfer->actual_len == 8 &&
      tu_le16toh(tu_unaligned_read16(audioh_fu_ctrl(epbuf))) == 1) {
    uint8_t *fu_ctrl    = audioh_fu_ctrl(epbuf);
    s->volume_range.min = (int16_t)tu_le16toh(tu_unaligned_read16(&fu_ctrl[2]));
    s->volume_range.max = (int16_t)tu_le16toh(tu_unaligned_read16(&fu_ctrl[4]));
    s->volume_range.res = tu_le16toh(tu_unaligned_read16(&fu_ctrl[6]));
    if (s->volume_range.min > s->volume_range.max || s->volume_range.res == 0) {
      xfer->result = XFER_RESULT_FAILED;
    }
  } else if (!uac2 && xfer->result == XFER_RESULT_SUCCESS && xfer->actual_len == 2) {
    audioh_fu_volume_range_store(s, ctrl, epbuf);
    ctrl->fu.mount.range_step++;

    if (ctrl->fu.mount.range_step < AUDIOH_VOLUME_RANGE_COUNT) {
      if (audioh_mount_feature_unit_submit(idx)) {
        return;
      }
      xfer->result = XFER_RESULT_FAILED;
    } else if (s->volume_range.min > s->volume_range.max || s->volume_range.res == 0) {
      xfer->result = XFER_RESULT_FAILED;
    }
  }

  const uint32_t expected_len = uac2 ? 8u : 2u;
  if (xfer->result != XFER_RESULT_SUCCESS || xfer->actual_len != expected_len) {
    s->volume_access = AUDIOH_CTRL_NONE;
    s->volume_range  = (tuh_audio_volume_range_t){0};
    if (s->mute_access == AUDIOH_CTRL_NONE) {
      s->feature_unit_id = 0;
    }
  }
  ctrl->fu_busy = false;
  ctrl->fu.mount.stream_idx++;
  audioh_mount_feature_unit_next(idx);
}

bool tuh_audio_feature_unit_set(uint8_t idx, uint8_t stream_idx, uint8_t control_selector, uint8_t channel,
                                uint16_t value, tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  TU_VERIFY(idx < CFG_TUH_AUDIO_MAX, false);
  audioh_interface_t *p_audio = &_audioh_itf[idx];
  TU_VERIFY(p_audio->mounted, false);
  tuh_audio_stream_t *s = audioh_get_stream_by_idx(p_audio, stream_idx);
  TU_VERIFY(s && s->feature_unit_id != 0, false);
  TU_VERIFY(audioh_fu_selector_supported(p_audio->protocol, control_selector), false);

  const uint8_t width = audioh_fu_control_width(control_selector);
  TU_VERIFY(width != 0, false);
  if (control_selector == AUDIO10_FU_CTRL_MUTE) {
    TU_VERIFY(s->mute_access == AUDIOH_CTRL_READ_WRITE, false);
  } else if (control_selector == AUDIO10_FU_CTRL_VOLUME) {
    TU_VERIFY(s->volume_access == AUDIOH_CTRL_READ_WRITE, false);
  }

  const uint8_t request_code = audioh_fu_cur_request(p_audio->protocol, TUSB_DIR_OUT);
  TU_VERIFY(request_code != 0, false);
  audioh_ctrl_state_t *ctrl  = &p_audio->ctrl;
  audioh_epbuf_t      *epbuf = &_audioh_epbuf[idx];
  TU_VERIFY(!ctrl->fu_busy, false);
  ctrl->fu_busy = true; // reserve the request state and fu_ctrl before writing

  const tusb_control_request_t request = {.bmRequestType_bit = {.recipient = TUSB_REQ_RCPT_INTERFACE,
                                                                .type      = TUSB_REQ_TYPE_CLASS,
                                                                .direction = TUSB_DIR_OUT},
                                          .bRequest          = request_code,
                                          .wValue            = tu_htole16(tu_u16(control_selector, channel)),
                                          .wIndex  = tu_htole16(tu_u16(s->feature_unit_id, p_audio->ac_itf_num)),
                                          .wLength = width};

  uint8_t *val_buf = audioh_fu_ctrl(epbuf);
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
    ctrl->fu_busy     = false;
    return result;
  }

  ctrl->complete_cb = complete_cb;
  ctrl->user_data   = user_data;
  xfer.complete_cb  = audioh_fu_set_complete;
  xfer.user_data    = (uintptr_t)idx;

  if (!tuh_control_xfer(&xfer)) {
    ctrl->complete_cb = NULL;
    ctrl->fu_busy     = false;
    return false;
  }
  return true;
}

static bool audioh_feature_unit_get(uint8_t idx, uint8_t stream_idx, uint8_t control_selector, uint8_t channel,
                                    void *value, uint8_t value_type, tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  TU_VERIFY(idx < CFG_TUH_AUDIO_MAX, false);
  audioh_interface_t *p_audio = &_audioh_itf[idx];
  TU_VERIFY(p_audio->mounted && value, false);
  tuh_audio_stream_t *s = audioh_get_stream_by_idx(p_audio, stream_idx);
  TU_VERIFY(s && s->feature_unit_id != 0, false);
  TU_VERIFY(audioh_fu_selector_supported(p_audio->protocol, control_selector), false);

  const uint8_t width = audioh_fu_control_width(control_selector);
  TU_VERIFY(width != 0, false);
  if (control_selector == AUDIO10_FU_CTRL_MUTE) {
    TU_VERIFY(s->mute_access != AUDIOH_CTRL_NONE, false);
  } else if (control_selector == AUDIO10_FU_CTRL_VOLUME) {
    TU_VERIFY(s->volume_access != AUDIOH_CTRL_NONE, false);
  }

  const uint8_t request_code = audioh_fu_cur_request(p_audio->protocol, TUSB_DIR_IN);
  TU_VERIFY(request_code != 0, false);
  audioh_ctrl_state_t *ctrl  = &p_audio->ctrl;
  audioh_epbuf_t      *epbuf = &_audioh_epbuf[idx];
  TU_VERIFY(!ctrl->fu_busy, false);
  ctrl->fu_busy               = true;
  ctrl->value                 = value;
  ctrl->fu.control.width      = width;
  ctrl->fu.control.value_type = value_type;

  const tusb_control_request_t request = {.bmRequestType_bit = {.recipient = TUSB_REQ_RCPT_INTERFACE,
                                                                .type      = TUSB_REQ_TYPE_CLASS,
                                                                .direction = TUSB_DIR_IN},
                                          .bRequest          = request_code,
                                          .wValue            = tu_htole16(tu_u16(control_selector, channel)),
                                          .wIndex  = tu_htole16(tu_u16(s->feature_unit_id, p_audio->ac_itf_num)),
                                          .wLength = width};

  if (complete_cb == NULL) {
    // Sync (blocking) path: user_data points to a tusb_xfer_result_t, the raw
    // bytes are converted to host order after the transfer completes
    tuh_xfer_t xfer = {.daddr       = p_audio->daddr,
                       .ep_addr     = 0,
                       .setup       = &request,
                       .buffer      = audioh_fu_ctrl(epbuf),
                       .complete_cb = NULL,
                       .user_data   = user_data};
    if (!tuh_control_xfer(&xfer)) {
      ctrl->fu_busy = false;
      return false;
    }
    if (xfer.result == XFER_RESULT_SUCCESS && xfer.actual_len == width) {
      audioh_fu_value_store(ctrl, epbuf);
    } else if (xfer.result == XFER_RESULT_SUCCESS && user_data != 0) {
      *((tusb_xfer_result_t *)user_data) = XFER_RESULT_FAILED;
    }
    ctrl->fu_busy = false;
    return true;
  }

  // Async path: chain the host-order conversion to the application callback
  ctrl->complete_cb = complete_cb;
  ctrl->user_data   = user_data;
  tuh_xfer_t xfer   = {.daddr       = p_audio->daddr,
                       .ep_addr     = 0,
                       .setup       = &request,
                       .buffer      = audioh_fu_ctrl(epbuf),
                       .complete_cb = audioh_fu_get_complete,
                       .user_data   = (uintptr_t)idx};

  if (!tuh_control_xfer(&xfer)) {
    ctrl->complete_cb = NULL;
    ctrl->fu_busy     = false;
    return false;
  }
  return true;
}

bool tuh_audio_feature_unit_get(uint8_t idx, uint8_t stream_idx, uint8_t control_selector, uint8_t channel,
                                uint16_t *value, tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  return audioh_feature_unit_get(idx, stream_idx, control_selector, channel, value, AUDIOH_FU_VALUE_U16, complete_cb,
                                 user_data);
}

bool tuh_audio_mute_set(uint8_t idx, uint8_t stream_idx, bool mute, tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  TU_VERIFY(tuh_audio_mute_supported(idx, stream_idx), false);
  return tuh_audio_feature_unit_set(idx, stream_idx, AUDIO10_FU_CTRL_MUTE, 0, mute ? 1 : 0, complete_cb, user_data);
}

bool tuh_audio_mute_get(uint8_t idx, uint8_t stream_idx, bool *mute, tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  TU_VERIFY(mute != NULL && tuh_audio_mute_supported(idx, stream_idx), false);
  return audioh_feature_unit_get(idx, stream_idx, AUDIO10_FU_CTRL_MUTE, 0, mute, AUDIOH_FU_VALUE_BOOL, complete_cb,
                                 user_data);
}

bool tuh_audio_volume_set(uint8_t idx, uint8_t stream_idx, int16_t volume, tuh_xfer_cb_t complete_cb,
                          uintptr_t user_data) {
  tuh_audio_volume_range_t range;
  TU_VERIFY(tuh_audio_volume_range_get(idx, stream_idx, &range), false);
  TU_VERIFY(volume >= range.min && volume <= range.max, false);
  return tuh_audio_feature_unit_set(idx, stream_idx, AUDIO10_FU_CTRL_VOLUME, 0, (uint16_t)volume, complete_cb,
                                    user_data);
}

bool tuh_audio_volume_get(uint8_t idx, uint8_t stream_idx, int16_t *volume, tuh_xfer_cb_t complete_cb,
                          uintptr_t user_data) {
  tuh_audio_volume_range_t range;
  TU_VERIFY(volume != NULL && tuh_audio_volume_range_get(idx, stream_idx, &range), false);
  return audioh_feature_unit_get(idx, stream_idx, AUDIO10_FU_CTRL_VOLUME, 0, volume, AUDIOH_FU_VALUE_I16, complete_cb,
                                 user_data);
}

#endif

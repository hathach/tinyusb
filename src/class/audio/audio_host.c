/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 Zhenjiang Zhang
 * SPDX-FileCopyrightText: Copyright (c) 2026 HiFiPhile (Zixun LI)
 * SPDX-License-Identifier: MIT
 *
 * This file is part of the TinyUSB stack.
 */

// clang-format off
/*
 * USB Audio Host driver architecture
 * ==================================
 *
 * One audioh_interface_t represents an Audio Control (AC) interface and owns
 * at most one logical stream in each direction. A capture stream receives
 * isochronous IN data from the device; a playback stream sends isochronous OUT
 * data to the device. Audio topology remains private, while applications see
 * each stream as a flat list of format, sample-rate, and channel-count tuples.
 * Internally, tuples using the same Audio Streaming (AS) alternate setting
 * share one audioh_as_config_t and refer to a rate source by index.
 * Only Type-I PCM configurations are exposed. UAC1 requires a discrete
 * sampling-frequency list; UAC2 Clock Source ranges are expanded into the
 * bounded public list.
 *
 * Mounting discovers the topology and completes any control requests needed
 * to describe the public configurations:
 *
 *   USB enumeration
 *     audioh_open()
 *       +-- validate and retain the AC descriptor range
 *       +-- audioh_parse_as() for each consecutive AS interface
 *       |     +-- parse the protocol-specific AS and format descriptors
 *       |     +-- associate the data and optional feedback endpoints
 *       |     `-- store UAC1 rates or a UAC2 Clock Source reference
 *       +-- audioh_link_feature_units()
 *       `-- tuh_audio_descriptor_cb()
 *
 *     audioh_set_config()
 *       +-- UAC2: audioh_mount_clock_next()
 *       |     `-- RANGE/CUR completion -> next Clock Source
 *       |                              -> rebuild public configurations
 *       `-- audioh_mount_feature_unit_next()
 *             `-- volume RANGE completion -> next logical stream
 *                                          -> tuh_audio_mount_cb()
 *                                          -> usbh_driver_set_config_complete()
 *
 * UAC1 rates come from each Format Type descriptor, whereas UAC2 rates are
 * queried from the Clock Sources referenced by the parsed topology. Feature
 * Unit parsing records master mute and master/logical-channel volume access.
 * Mount probing reads the volume range from the master or first controlled
 * logical channel. A device is reported as mounted only after these
 * asynchronous probes finish.
 *
 * Stream configuration is local; stream activation is asynchronous:
 *
 *   tuh_audio_configure(stream, configuration)
 *     +-- resolve the public tuple to AS and rate-source indices
 *     +-- close endpoints from the previous configuration
 *     +-- initialize frame size, FIFO, and playback scheduler state
 *     `-- audioh_stream_open_ep()          (data and optional feedback EP)
 *
 *   tuh_audio_start(stream)
 *     +-- UAC1: SET_INTERFACE(non-zero alt)
 *     |           `-- optional endpoint SET_CUR(sample rate)
 *     +-- UAC2: optional Clock Source CUR(sample rate)
 *     |           `-- SET_INTERFACE(non-zero alt)
 *     `-- audioh_stream_start_xfer()
 *           `-- tuh_audio_event_cb(START_COMPLETE)
 *
 *   tuh_audio_stop(stream)
 *     +-- SET_INTERFACE(alt 0) and stop local transfer resubmission
 *     `-- completion -> tuh_audio_event_cb(STOP_COMPLETE)
 *
 * A successful start/stop API return means that the first control request was
 * submitted. The corresponding event reports completion of the entire chain.
 * UAC1 sets the rate after activating the endpoint because its control targets
 * that endpoint; UAC2 sets the Clock Source before activating the AS interface.
 *
 * Once started, each endpoint completion prepares and submits its successor:
 *
 *   host controller -> audioh_xfer_cb()
 *     +-- capture data
 *     |     +-- copy whole audio frames to the overwrite FIFO
 *     |     +-- tuh_audio_capture_cb()
 *     |     `-- audioh_stream_capture_xfer()
 *     +-- playback data
 *     |     +-- tuh_audio_playback_cb()
 *     |     `-- audioh_stream_playback_xfer()
 *     |           +-- calculate the next fractional packet size
 *     |           +-- read a complete packet from the FIFO, or send silence
 *     |           `-- submit the next OUT transfer
 *     `-- explicit feedback
 *           +-- validate and stage the Q10.14 or Q16.16 rate
 *           `-- audioh_stream_feedback_xfer()
 *
 * tuh_audio_read() and tuh_audio_write() access only the stream FIFOs and do
 * not need to run from transfer callbacks. The FIFOs decouple application I/O
 * from USB polling cadence and never expose partial interleaved audio frames.
 * A transfer failure stops resubmission and is reported through
 * tuh_audio_event_cb(XFER_FAILED).
 */
// clang-format on

#include "tusb_option.h"

#if (CFG_TUH_ENABLED && CFG_TUH_AUDIO)

  #include "host/usbh.h"
  #include "host/usbh_pvt.h"
  #include "audio_host.h"

  // Driver-specific log level; defaults to the host-stack log level.
  #ifndef CFG_TUH_AUDIO_LOG_LEVEL
    #define CFG_TUH_AUDIO_LOG_LEVEL CFG_TUH_LOG_LEVEL
  #endif

  #define TU_LOG_DRV(...) TU_LOG(CFG_TUH_AUDIO_LOG_LEVEL, __VA_ARGS__)


//--------------------------------------------------------------------+
// MACROS, CONSTANTS, AND TYPES
//--------------------------------------------------------------------+

enum {
  STREAM_STATE_IDLE = 0, // No active configuration.
  STREAM_STATE_READY     // Configured and ready to start.
};

enum {
  AUDIOH_STREAM_OP_NONE = 0,
  AUDIOH_STREAM_OP_START,
  AUDIOH_STREAM_OP_STOP
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

// UAC1 stores one rate source per alternate setting. UAC2 alternate settings
// that reference the same Clock Source share one rate source.
typedef struct {
  uint32_t sample_rate[CFG_TUH_AUDIO_MAX_SAM_FREQ];
  uint8_t  control_id; // UAC1 endpoint address or UAC2 Clock Source ID.
  uint8_t  sample_rate_count;
  uint8_t  frequency_access;
} audioh_rate_source_t;

// Properties shared by every sampling frequency of one AS alternate setting.
typedef struct {
  uint16_t ep_size;
  uint8_t  itf_num;
  uint8_t  alt_setting;
  uint8_t  ep_addr;
  uint8_t  ep_interval;
  uint8_t  ep_attr; // Synchronization and usage fields from bmAttributes.
  uint8_t  format;
  uint8_t  channels;
  uint8_t  terminal_id;
  uint8_t  rate_source_idx;
  uint8_t  rate_count;
} audioh_as_config_t;

// Explicit-feedback endpoint associated with a playback alternate setting.
typedef struct {
  uint8_t ep_addr;
  uint8_t ep_size;
  uint8_t ep_interval;
  uint8_t ep_attr;
} audioh_feedback_ep_t;

typedef struct {
  audioh_feedback_ep_t feedback[CFG_TUH_AUDIO_MAX_AS];

  // Packet rates use Q16.16 audio frames per data-endpoint poll interval. The
  // scheduler snapshots target_frames_q16 once per packet and retains rem_acc,
  // which integrates fractional frames across feedback updates.
  uint32_t nominal_frames_q16;
  uint32_t target_frames_q16;
  uint16_t feedback_min_frames;
  uint16_t feedback_max_frames;
  uint16_t rem_acc;
  bool     feedback_opened;
} audioh_playback_t;

// Control-transfer bookkeeping; transfer payloads are stored in audioh_epbuf_t.
typedef struct {
  tuh_xfer_cb_t complete_cb;
  uintptr_t     user_data;
  void         *value;
  union {
    struct {
      uint8_t width;
      uint8_t value_type;
      uint8_t channel;
      uint8_t last_channel;
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

// One logical capture or playback stream.
typedef struct {
  // Identity is initialized once and preserved when the stream is reset.
  uint8_t    idx;
  uint8_t    stream_idx;
  tusb_dir_t dir; // TUSB_DIR_IN is capture; TUSB_DIR_OUT is playback.

  // Device address, or zero while this stream slot is unused.
  uint8_t daddr;

  // Configurations discovered during enumeration.
  uint8_t            as_count;
  uint8_t            config_count;
  audioh_as_config_t as[CFG_TUH_AUDIO_MAX_AS];

  // Selected configuration and runtime state.
  uint8_t active_config; // Index in the flattened public configuration list.
  uint8_t active_as;
  uint8_t active_rate;
  uint8_t state;
  uint8_t operation;
  bool    running;

  // Directly associated Feature Unit, or zero when none is usable.
  uint8_t                  feature_unit_id;
  uint8_t                  mute_access;
  uint8_t                  volume_master_access;
  uint8_t                  feature_unit_channels;
  uint8_t                  volume_range_channel;
  bool                     volume_all_channels_writable;
  tuh_audio_volume_range_t volume_range;

  // Bytes in one interleaved audio frame across all channels.
  uint16_t frame_bytes;

  // The FIFO decouples application I/O from isochronous transfers. ep_buf is
  // assigned during driver initialization and the endpoint during configure.
  tu_edpt_stream_t edpt;
  uint8_t          ff_buf[CFG_TUH_AUDIO_STREAM_BUFSIZE];
} tuh_audio_stream_t;

// State owned by one Audio Control interface.
typedef struct {
  uint8_t daddr; // Device address, or zero for a free instance.
  uint8_t ac_itf_num;
  uint8_t protocol;
  uint8_t stream_count;
  uint8_t rate_source_count;
  bool    mounted;

  audioh_rate_source_t rate_source[AUDIOH_MAX_RATE_SOURCES];

  // Public stream indices are assigned in playback-then-capture order.
  tuh_audio_stream_t  out_stream;
  tuh_audio_stream_t  in_stream;
  audioh_playback_t   playback;
  audioh_ctrl_state_t ctrl;
} audioh_interface_t;

  #if CFG_TUH_AUDIO_PROTOCOLS & TUH_AUDIO_PROTOCOL_UAC2
    #define AUDIOH_CLOCK_RANGE_BUFSIZE (2 + 12 * CFG_TUH_AUDIO_MAX_SAM_FREQ)
  #endif

typedef struct {
  // Clock discovery finishes before mount, so its buffer can be reused by
  // runtime sampling-frequency and Feature Unit requests.
  union {
  #if CFG_TUH_AUDIO_PROTOCOLS & TUH_AUDIO_PROTOCOL_UAC2
    TUH_EPBUF_DEF(clock_range, AUDIOH_CLOCK_RANGE_BUFSIZE);
  #endif
    struct {
      TUH_EPBUF_DEF(rate_ctrl, 4);
      TUH_EPBUF_DEF(fu_ctrl, 8);
    } runtime;
  } control;
  // Feedback transfers may overlap runtime control transfers, so the feedback buffer is separate.
  TUH_EPBUF_DEF(feedback, 4);
  TUH_EPBUF_DEF(epin, CFG_TUH_AUDIO_EPIN_BUFSIZE);
  TUH_EPBUF_DEF(epout, CFG_TUH_AUDIO_EPOUT_BUFSIZE);
} audioh_epbuf_t;

static audioh_interface_t _audioh_itf[CFG_TUH_AUDIO_MAX];

CFG_TUH_MEM_SECTION static audioh_epbuf_t _audioh_epbuf[CFG_TUH_AUDIO_MAX];

//--------------------------------------------------------------------+
// WEAK APPLICATION CALLBACKS
//--------------------------------------------------------------------+

TU_ATTR_WEAK void tuh_audio_descriptor_cb(uint8_t idx, const tuh_audio_descriptor_cb_t *desc_cb_data) {
  (void)idx;
  (void)desc_cb_data;
}

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

TU_ATTR_WEAK void tuh_audio_event_cb(uint8_t idx, uint8_t stream_idx, tuh_audio_event_t event,
                                     tusb_xfer_result_t result) {
  (void)idx;
  (void)stream_idx;
  (void)event;
  (void)result;
}

//--------------------------------------------------------------------+
// HELPERS
//--------------------------------------------------------------------+

TU_ATTR_ALWAYS_INLINE static inline uint8_t *audioh_rate_ctrl(audioh_epbuf_t *epbuf) {
  return epbuf->control.runtime.rate_ctrl;
}

TU_ATTR_ALWAYS_INLINE static inline uint8_t *audioh_fu_ctrl(audioh_epbuf_t *epbuf) {
  return epbuf->control.runtime.fu_ctrl;
}

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
  return (direction == TUSB_DIR_IN) ? &p_audio->in_stream : &p_audio->out_stream;
}

static tuh_audio_stream_t *audioh_get_stream_by_idx(audioh_interface_t *p_audio, uint8_t stream_idx) {
  for (uint8_t i = 0; i < 2; i++) {
    tuh_audio_stream_t *s = (i == 0) ? &p_audio->out_stream : &p_audio->in_stream;
    if (s->as_count > 0 && s->stream_idx == stream_idx) {
      return s;
    }
  }
  return NULL;
}

TU_ATTR_ALWAYS_INLINE static inline tuh_audio_stream_t *audioh_get_stream_by_idx_unchecked(
  audioh_interface_t *p_audio, uint8_t stream_idx) {
  return (p_audio->out_stream.stream_idx == stream_idx) ? &p_audio->out_stream : &p_audio->in_stream;
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
                                           uint8_t volume_master_access, uint8_t channels, uint8_t volume_range_channel,
                                           bool volume_all_channels_writable) {
  s->feature_unit_id              = unit_id;
  s->mute_access                  = mute_access;
  s->volume_master_access         = volume_master_access;
  s->feature_unit_channels        = channels;
  s->volume_range_channel         = volume_range_channel;
  s->volume_all_channels_writable = volume_all_channels_writable;
}

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

// bInterval encodes 2^(bInterval-1) full-speed frames or high-speed microframes.
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

// Preserve the stream identity and FIFO allocation while clearing device state.
static void audioh_stream_reset(tuh_audio_stream_t *s) {
  s->daddr                        = 0;
  s->stream_idx                   = TUSB_INDEX_INVALID_8;
  s->as_count                     = 0;
  s->config_count                 = 0;
  s->active_config                = TUSB_INDEX_INVALID_8;
  s->active_as                    = TUSB_INDEX_INVALID_8;
  s->active_rate                  = TUSB_INDEX_INVALID_8;
  s->state                        = STREAM_STATE_IDLE;
  s->running                      = false;
  s->feature_unit_id              = 0;
  s->mute_access                  = AUDIOH_CTRL_NONE;
  s->volume_master_access         = AUDIOH_CTRL_NONE;
  s->feature_unit_channels        = 0;
  s->volume_range_channel         = TUSB_INDEX_INVALID_8;
  s->volume_all_channels_writable = false;
  s->volume_range                 = (tuh_audio_volume_range_t){0};
  s->frame_bytes                  = 0;
  tu_edpt_stream_close(&s->edpt);
  tu_edpt_stream_clear(&s->edpt);
}

static void audioh_playback_reset(audioh_playback_t *playback) {
  tu_memclr(playback, sizeof(*playback));
}

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
// PACKET SCHEDULER
//--------------------------------------------------------------------+

static void audioh_stream_xfer_failed(tuh_audio_stream_t *s, tusb_xfer_result_t result);

static bool audioh_stream_feedback_xfer(tuh_audio_stream_t *s) {
  const audioh_feedback_ep_t *feedback = &audioh_get_playback(s)->feedback[s->active_as];
  TU_VERIFY(usbh_edpt_claim(s->daddr, feedback->ep_addr), false);
  return usbh_edpt_xfer(s->daddr, feedback->ep_addr, _audioh_epbuf[s->idx].feedback, feedback->ep_size);
}

static bool audioh_stream_capture_xfer(tuh_audio_stream_t *s) {
  const audioh_as_config_t *as = audioh_stream_active_as(s);
  TU_VERIFY(usbh_edpt_claim(s->daddr, as->ep_addr), false);
  return usbh_edpt_xfer(s->daddr, as->ep_addr, s->edpt.ep_buf, as->ep_size);
}

static bool audioh_stream_playback_xfer(tuh_audio_stream_t *s) {
  const audioh_as_config_t *as       = audioh_stream_active_as(s);
  audioh_playback_t        *playback = audioh_get_playback(s);
  TU_VERIFY(usbh_edpt_claim(s->daddr, as->ep_addr), false);

  // Use one target for the entire packet calculation. Retaining the fractional
  // remainder makes the scheduled total follow the sum of changing feedback
  // values with less than one frame of quantization error.
  const uint32_t target_q16   = playback->target_frames_q16;
  uint32_t       frames       = target_q16 >> 16;
  const uint32_t fraction     = target_q16 & 0xFFFFu;
  uint32_t       next_rem_acc = playback->rem_acc + fraction;
  if (next_rem_acc >= 65536u) {
    next_rem_acc -= 65536u;
    frames++;
  }

  const uint16_t bytes = (uint16_t)(frames * s->frame_bytes);
  if (tu_fifo_count(&s->edpt.ff) < bytes) {
    // Isochronous OUT must continue at every interval. Send silence until a
    // complete packet is queued, leaving any partial packet in the FIFO.
    tu_memclr(s->edpt.ep_buf, bytes);
  } else {
    tu_fifo_read_n(&s->edpt.ff, s->edpt.ep_buf, bytes);
  }

  if (!usbh_edpt_xfer(s->daddr, as->ep_addr, s->edpt.ep_buf, bytes)) {
    return false;
  }
  playback->rem_acc = (uint16_t)next_rem_acc;
  return true;
}

//--------------------------------------------------------------------+
// STREAM CONFIGURATION
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
  s->operation     = AUDIOH_STREAM_OP_NONE;
  s->running       = false;
}

static void audioh_stream_stop_xfers(tuh_audio_stream_t *s) {
  s->running = false;
  if (s->dir == TUSB_DIR_OUT) {
    audioh_playback_t *playback = audioh_get_playback(s);
    playback->target_frames_q16 = playback->nominal_frames_q16;
    playback->rem_acc           = 0;
  }
  tu_edpt_stream_clear(&s->edpt);
}

static void audioh_stream_xfer_failed(tuh_audio_stream_t *s, tusb_xfer_result_t result) {
  audioh_stream_stop_xfers(s);
  tuh_audio_event_cb(s->idx, s->stream_idx, TUH_AUDIO_EVENT_XFER_FAILED, result);
}

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

  // Bind the transfer helper to the selected endpoint and empty its FIFO.
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
// USB HOST CLASS DRIVER
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

    // A disconnected device cannot complete its pending control request.
    tu_memclr(&p_audio->ctrl, sizeof(p_audio->ctrl));

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
    // Three-byte feedback is Q10.14; the scheduler uses Q16.16 throughout.
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

  // Feedback is measured per USB frame or microframe. Scale it to the data
  // endpoint's polling interval.
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

  // Host-class callbacks run serially. The playback scheduler snapshots this
  // target before calculating a packet, so an update cannot split a packet
  // calculation across two rates.
  playback->target_frames_q16 = target_q16;
}

bool audioh_xfer_cb(uint8_t dev_addr, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes) {
  tuh_audio_stream_t *s = audioh_find_stream(dev_addr, ep_addr);
  if (s == NULL) {
    return false;
  }

  // Stopping one endpoint does not cancel every transfer that may already be
  // in flight (for example, a playback data and feedback pair). Ignore those
  // completions after the stream has stopped.
  if (!s->running) {
    return true;
  }

  // Failed, stalled, and aborted transfers do not carry valid audio data.
  if (result != XFER_RESULT_SUCCESS) {
    TU_LOG_DRV("  AUDIO transfer failed: addr=%u ep=%02x result=%u\r\n", dev_addr, ep_addr, result);
    audioh_stream_xfer_failed(s, (tusb_xfer_result_t)result);
    return true;
  }

  const uint8_t feedback_ep = audioh_get_playback(s)->feedback[s->active_as].ep_addr;
  if (s->dir == TUSB_DIR_OUT && feedback_ep != 0 && ep_addr == feedback_ep) {
    audioh_feedback_received(s, xferred_bytes);
    if (!audioh_stream_feedback_xfer(s)) {
      audioh_stream_xfer_failed(s, XFER_RESULT_FAILED);
    }
    return true;
  }

  if (s->dir == TUSB_DIR_IN) {
    // Queue whole capture frames, notify the application, then re-arm.
    const uint16_t bytes = (uint16_t)(xferred_bytes - (xferred_bytes % s->frame_bytes));
    if (bytes > 0) {
      tu_fifo_write_n(&s->edpt.ff, s->edpt.ep_buf, bytes);
    }
    tuh_audio_capture_cb(s->idx, s->stream_idx, (uint16_t)xferred_bytes);
    if (s->running && !audioh_stream_capture_xfer(s)) {
      audioh_stream_xfer_failed(s, XFER_RESULT_FAILED);
    }
  } else {
    // Notify the application before requesting the next playback packet.
    tuh_audio_playback_cb(s->idx, s->stream_idx, (uint16_t)xferred_bytes);
    if (s->running && !audioh_stream_playback_xfer(s)) {
      audioh_stream_xfer_failed(s, XFER_RESULT_FAILED);
    }
  }
  return true;
}

//--------------------------------------------------------------------+
// ENUMERATION
//--------------------------------------------------------------------+

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
  uint8_t volume_master_access;
  uint8_t channels;
  uint8_t volume_range_channel;
  bool    volume_all_channels_writable;
} audioh_fu_info_t;

typedef struct {
  uint8_t id;
  uint8_t frequency_access;
} audioh_clock_info_t;

typedef struct {
  const uint8_t *desc_start;
  const uint8_t *desc_end;
} audioh_ac_desc_range_t;

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

static bool audioh_ac_entity_valid(const audioh_interface_t *p_audio, const uint8_t *p_desc) {
  switch (p_audio->protocol) {
  #if CFG_TUH_AUDIO_PROTOCOLS & TUH_AUDIO_PROTOCOL_UAC1
    case AUDIO_INT_PROTOCOL_CODE_V1: {
      switch (tu_desc_subtype(p_desc)) {
        case AUDIO10_CS_AC_INTERFACE_INPUT_TERMINAL:
          return TUH_VALIDATE_BASIC(tu_desc_len(p_desc) >= sizeof(audio10_desc_input_terminal_t));
        case AUDIO10_CS_AC_INTERFACE_OUTPUT_TERMINAL:
          return TUH_VALIDATE_BASIC(tu_desc_len(p_desc) >= sizeof(audio10_desc_output_terminal_t));
        case AUDIO10_CS_AC_INTERFACE_FEATURE_UNIT: {
          TU_VERIFY(TUH_VALIDATE_BASIC(tu_desc_len(p_desc) >= 7), false);
          const uint8_t control_size = p_desc[5];
          const uint8_t control_bytes = (uint8_t)(tu_desc_len(p_desc) - 7);
          return TUH_VALIDATE_BASIC(control_size > 0) && TUH_VALIDATE_BASIC(control_size <= control_bytes) &&
                 TUH_VALIDATE_BASIC(control_bytes % control_size == 0);
        }
        default:
          return true;
      }
    }
  #endif
  #if CFG_TUH_AUDIO_PROTOCOLS & TUH_AUDIO_PROTOCOL_UAC2
    case AUDIO_INT_PROTOCOL_CODE_V2:
      switch (tu_desc_subtype(p_desc)) {
        case AUDIO20_CS_AC_INTERFACE_INPUT_TERMINAL:
          return TUH_VALIDATE_BASIC(tu_desc_len(p_desc) >= sizeof(audio20_desc_input_terminal_t));
        case AUDIO20_CS_AC_INTERFACE_OUTPUT_TERMINAL:
          return TUH_VALIDATE_BASIC(tu_desc_len(p_desc) >= sizeof(audio20_desc_output_terminal_t));
        case AUDIO20_CS_AC_INTERFACE_FEATURE_UNIT:
          return TUH_VALIDATE_BASIC(tu_desc_len(p_desc) >= 10) &&
                 TUH_VALIDATE_BASIC((tu_desc_len(p_desc) - 6u) % 4u == 0);
        case AUDIO20_CS_AC_INTERFACE_CLOCK_SOURCE:
          return TUH_VALIDATE_BASIC(tu_desc_len(p_desc) >= sizeof(audio20_desc_clock_source_t));
        default:
          return true;
      }
  #endif
    default:
      return false;
  }
}

static bool audioh_ac_terminal_find(const audioh_interface_t *p_audio, const audioh_ac_desc_range_t *range, uint8_t id,
                                    audioh_terminal_info_t *info) {
  for (const uint8_t *p_desc = range->desc_start; p_desc < range->desc_end; p_desc = tu_desc_next(p_desc)) {
    if (tu_desc_type(p_desc) != TUSB_DESC_CS_INTERFACE || tu_desc_len(p_desc) < 4 || p_desc[3] != id) {
      continue;
    }
    switch (p_audio->protocol) {
  #if CFG_TUH_AUDIO_PROTOCOLS & TUH_AUDIO_PROTOCOL_UAC1
      case AUDIO_INT_PROTOCOL_CODE_V1:
        if (tu_desc_subtype(p_desc) == AUDIO10_CS_AC_INTERFACE_INPUT_TERMINAL) {
          const audio10_desc_input_terminal_t *terminal = (const audio10_desc_input_terminal_t *)p_desc;
          if (tu_le16toh(terminal->wTerminalType) == AUDIO_TERM_TYPE_USB_STREAMING) {
            *info = (audioh_terminal_info_t){.id = id, .stream_dir = TUSB_DIR_OUT};
            return true;
          }
        } else if (tu_desc_subtype(p_desc) == AUDIO10_CS_AC_INTERFACE_OUTPUT_TERMINAL) {
          const audio10_desc_output_terminal_t *terminal = (const audio10_desc_output_terminal_t *)p_desc;
          if (tu_le16toh(terminal->wTerminalType) == AUDIO_TERM_TYPE_USB_STREAMING) {
            *info = (audioh_terminal_info_t){.id = id, .source_id = terminal->bSourceID, .stream_dir = TUSB_DIR_IN};
            return true;
          }
        }
        break;
  #endif
  #if CFG_TUH_AUDIO_PROTOCOLS & TUH_AUDIO_PROTOCOL_UAC2
      case AUDIO_INT_PROTOCOL_CODE_V2:
        if (tu_desc_subtype(p_desc) == AUDIO20_CS_AC_INTERFACE_INPUT_TERMINAL) {
          const audio20_desc_input_terminal_t *terminal = (const audio20_desc_input_terminal_t *)p_desc;
          if (tu_le16toh(terminal->wTerminalType) == AUDIO_TERM_TYPE_USB_STREAMING) {
            *info = (audioh_terminal_info_t){.id = id, .clock_id = terminal->bCSourceID, .stream_dir = TUSB_DIR_OUT};
            return true;
          }
        } else if (tu_desc_subtype(p_desc) == AUDIO20_CS_AC_INTERFACE_OUTPUT_TERMINAL) {
          const audio20_desc_output_terminal_t *terminal = (const audio20_desc_output_terminal_t *)p_desc;
          if (tu_le16toh(terminal->wTerminalType) == AUDIO_TERM_TYPE_USB_STREAMING) {
            *info = (audioh_terminal_info_t){.id         = id,
                                             .source_id  = terminal->bSourceID,
                                             .clock_id   = terminal->bCSourceID,
                                             .stream_dir = TUSB_DIR_IN};
            return true;
          }
        }
        break;
  #endif
      default:
        return false;
    }
  }
  return false;
}

static bool audioh_ac_feature_unit_parse(const audioh_interface_t *p_audio, const uint8_t *p_desc,
                                         audioh_fu_info_t *info) {
  if (tu_desc_type(p_desc) != TUSB_DESC_CS_INTERFACE) {
    return false;
  }

  uint8_t control_offset;
  uint8_t control_size;
  uint8_t channels;
  uint8_t mute_access;
  switch (p_audio->protocol) {
  #if CFG_TUH_AUDIO_PROTOCOLS & TUH_AUDIO_PROTOCOL_UAC1
    case AUDIO_INT_PROTOCOL_CODE_V1:
      if (tu_desc_subtype(p_desc) != AUDIO10_CS_AC_INTERFACE_FEATURE_UNIT) {
        return false;
      }
      control_offset = 6;
      control_size   = p_desc[5];
      channels       = (uint8_t)((tu_desc_len(p_desc) - 7u) / control_size - 1u);
      mute_access = (p_desc[control_offset] & AUDIO10_FU_CONTROL_BM_MUTE) ? AUDIOH_CTRL_READ_WRITE : AUDIOH_CTRL_NONE;
      break;
  #endif
  #if CFG_TUH_AUDIO_PROTOCOLS & TUH_AUDIO_PROTOCOL_UAC2
    case AUDIO_INT_PROTOCOL_CODE_V2:
      if (tu_desc_subtype(p_desc) != AUDIO20_CS_AC_INTERFACE_FEATURE_UNIT) {
        return false;
      }
      control_offset = 5;
      control_size   = 4;
      channels       = (uint8_t)((tu_desc_len(p_desc) - 6u) / control_size - 1u);
      mute_access    = audioh_uac2_control_access(tu_le32toh(tu_unaligned_read32(&p_desc[control_offset])),
                                                  AUDIO20_FEATURE_UNIT_CTRL_MUTE_POS);
      break;
  #endif
    default:
      return false;
  }

  uint8_t volume_master_access         = AUDIOH_CTRL_NONE;
  uint8_t volume_range_channel         = TUSB_INDEX_INVALID_8;
  bool    volume_all_channels_writable = channels > 0;
  for (uint8_t channel = 0; channel <= channels; channel++) {
    uint8_t access;
  #if CFG_TUH_AUDIO_PROTOCOLS & TUH_AUDIO_PROTOCOL_UAC2
    if (p_audio->protocol == AUDIO_INT_PROTOCOL_CODE_V2) {
      const uint32_t controls = tu_le32toh(tu_unaligned_read32(&p_desc[control_offset + channel * control_size]));
      access                  = audioh_uac2_control_access(controls, AUDIO20_FEATURE_UNIT_CTRL_VOLUME_POS);
    } else
  #endif
    {
      access = (p_desc[control_offset + channel * control_size] & AUDIO10_FU_CONTROL_BM_VOLUME) ? AUDIOH_CTRL_READ_WRITE
                                                                                                : AUDIOH_CTRL_NONE;
    }
    if (channel == 0) {
      volume_master_access = access;
    } else {
      volume_all_channels_writable &= access == AUDIOH_CTRL_READ_WRITE;
    }
    if (access != AUDIOH_CTRL_NONE && volume_range_channel == TUSB_INDEX_INVALID_8) {
      volume_range_channel = channel;
    }
  }

  *info = (audioh_fu_info_t){.id                           = p_desc[3],
                             .source_id                    = p_desc[4],
                             .mute_access                  = mute_access,
                             .volume_master_access         = volume_master_access,
                             .channels                     = channels,
                             .volume_range_channel         = volume_range_channel,
                             .volume_all_channels_writable = volume_all_channels_writable};
  return mute_access != AUDIOH_CTRL_NONE || volume_range_channel != TUSB_INDEX_INVALID_8;
}

  #if CFG_TUH_AUDIO_PROTOCOLS & TUH_AUDIO_PROTOCOL_UAC2
static bool audioh_ac_clock_find(const audioh_ac_desc_range_t *range, uint8_t id, audioh_clock_info_t *info) {
  for (const uint8_t *p_desc = range->desc_start; p_desc < range->desc_end; p_desc = tu_desc_next(p_desc)) {
    if (tu_desc_type(p_desc) == TUSB_DESC_CS_INTERFACE &&
        tu_desc_subtype(p_desc) == AUDIO20_CS_AC_INTERFACE_CLOCK_SOURCE && p_desc[3] == id) {
      const audio20_desc_clock_source_t *clock = (const audio20_desc_clock_source_t *)p_desc;
      *info =
        (audioh_clock_info_t){.id = id,
                              .frequency_access =
                                audioh_uac2_control_access(clock->bmControls, AUDIO20_CLOCK_SOURCE_CTRL_CLK_FRQ_POS)};
      return true;
    }
  }
  return false;
}
  #endif

static void audioh_link_feature_units(audioh_interface_t *p_audio, const audioh_ac_desc_range_t *range) {
  for (uint8_t direction = TUSB_DIR_OUT; direction <= TUSB_DIR_IN; direction++) {
    tuh_audio_stream_t *stream = audioh_get_stream(p_audio, (tusb_dir_t)direction);
    if (stream->as_count == 0) {
      continue;
    }
    audioh_terminal_info_t terminal;
    if (!audioh_ac_terminal_find(p_audio, range, stream->as[0].terminal_id, &terminal) ||
        terminal.stream_dir != direction) {
      continue;
    }
    for (const uint8_t *p_desc = range->desc_start; p_desc < range->desc_end; p_desc = tu_desc_next(p_desc)) {
      audioh_fu_info_t fu;
      if (!audioh_ac_feature_unit_parse(p_audio, p_desc, &fu)) {
        continue;
      }
      const bool linked = (direction == TUSB_DIR_OUT) ? (fu.source_id == terminal.id) : (fu.id == terminal.source_id);
      if (linked) {
        audioh_stream_set_feature_unit(stream, fu.id, fu.mute_access, fu.volume_master_access, fu.channels,
                                       fu.volume_range_channel, fu.volume_all_channels_writable);
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
static int8_t audioh_uac2_rate_source_get(audioh_interface_t *p_audio, const audioh_ac_desc_range_t *range,
                                          const audioh_terminal_info_t *terminal) {
  if (terminal->clock_id == 0) {
    return -1;
  }
  audioh_clock_info_t clock;
  if (!audioh_ac_clock_find(range, terminal->clock_id, &clock) || clock.frequency_access == AUDIOH_CTRL_NONE) {
    return -1;
  }
  for (uint8_t i = 0; i < p_audio->rate_source_count; i++) {
    if (p_audio->rate_source[i].control_id == clock.id) {
      return (int8_t)i;
    }
  }
  if (p_audio->rate_source_count >= AUDIOH_MAX_RATE_SOURCES) {
    return -1;
  }
  const uint8_t idx = p_audio->rate_source_count++;
  p_audio->rate_source[idx] =
    (audioh_rate_source_t){.control_id = clock.id, .frequency_access = clock.frequency_access};
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

// Parse one AS alternate setting and return the next interface descriptor.
// Supported configurations are appended to the stream matching its endpoint.
static const uint8_t *audioh_parse_as(audioh_interface_t *p_audio, const audioh_ac_desc_range_t *ac_desc,
                                      const tusb_desc_interface_t *desc_itf, const uint8_t *p_desc,
                                      const uint8_t *desc_end) {
  const uint8_t itf_num = desc_itf->bInterfaceNumber;
  const uint8_t alt     = desc_itf->bAlternateSetting;

  p_desc = tu_desc_next(p_desc);

  // Alternate setting zero is the zero-bandwidth setting, not a configuration.
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

  audioh_as_class_info_t class_info = {0};

  // Retain one data endpoint and, for playback, one explicit-feedback endpoint.
  // An implicit-feedback IN endpoint remains the data endpoint of its own AS
  // interface and is therefore exposed as a capture stream.
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

        bool is_data_ep           = false;
        bool is_explicit_feedback = false;
        // UAC1 distinguishes feedback by synchronization type; UAC2 uses the
        // endpoint usage field.
        switch (p_audio->protocol) {
  #if CFG_TUH_AUDIO_PROTOCOLS & TUH_AUDIO_PROTOCOL_UAC1
          case AUDIO_INT_PROTOCOL_CODE_V1:
            is_data_ep           = desc_endpoint->bmAttributes.sync != TUSB_ISO_EP_ATT_NO_SYNC;
            is_explicit_feedback = tu_edpt_dir(desc_endpoint->bEndpointAddress) == TUSB_DIR_IN && !is_data_ep;
            break;
  #endif
  #if CFG_TUH_AUDIO_PROTOCOLS & TUH_AUDIO_PROTOCOL_UAC2
          case AUDIO_INT_PROTOCOL_CODE_V2:
            is_data_ep           = desc_endpoint->bmAttributes.usage == (TUSB_ISO_EP_ATT_DATA >> 4) ||
                                   desc_endpoint->bmAttributes.usage == (TUSB_ISO_EP_ATT_IMPLICIT_FB >> 4);
            is_explicit_feedback = tu_edpt_dir(desc_endpoint->bEndpointAddress) == TUSB_DIR_IN &&
                                   desc_endpoint->bmAttributes.usage == (TUSB_ISO_EP_ATT_EXPLICIT_FB >> 4);
            break;
  #endif
          default:
            break;
        }

        if (is_explicit_feedback) {
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

        if (is_data_ep) {
          if (has_data_ep) {
            TU_LOG_DRV("  AUDIO AS itf %u alt %u: extra data ep %02x ignored\r\n", itf_num, alt,
                       desc_endpoint->bEndpointAddress);
            break;
          }

          ep_info.ep_addr     = desc_endpoint->bEndpointAddress;
          ep_info.ep_size     = tu_edpt_packet_size(desc_endpoint);
          ep_info.ep_interval = desc_endpoint->bInterval;
          // Isochronous bInterval is an exponent in the inclusive range 1..16.
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
  // Store the alternate setting once; the public API expands its sampling
  // frequencies into separate configurations.
  const audioh_ep_info_t *ep     = &ep_info;
  tuh_audio_stream_t     *stream = audioh_get_stream(p_audio, tu_edpt_dir(ep->ep_addr));
  audioh_terminal_info_t terminal;
  if (!audioh_ac_terminal_find(p_audio, ac_desc, class_info.terminal_id, &terminal) ||
      terminal.stream_dir != stream->dir) {
    TU_LOG_DRV("  AUDIO AS itf %u alt %u: terminal %u does not match endpoint direction\r\n", itf_num, alt,
               class_info.terminal_id);
    return p_desc;
  }

  const uint16_t epbuf_size = (stream->dir == TUSB_DIR_IN) ? CFG_TUH_AUDIO_EPIN_BUFSIZE : CFG_TUH_AUDIO_EPOUT_BUFSIZE;

  if (ep->ep_size == 0 || ep->ep_size > iso_xfer_size) {
    TU_LOG_DRV("  AUDIO AS itf %u alt %u: invalid isochronous ep size %u\r\n", itf_num, alt, ep->ep_size);
    return p_desc;
  }

  // Capture always requests the endpoint's maximum packet size, so both the
  // transfer buffer and FIFO must hold it.
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
      const int8_t rate_source_idx = audioh_uac2_rate_source_get(p_audio, ac_desc, &terminal);
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

  p_desc                         = tu_desc_next(p_desc);
  audioh_ac_desc_range_t ac_desc = {.desc_start = p_desc};
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
      if (!audioh_ac_entity_valid(p_audio, p_desc)) {
        goto open_failed;
      }
    }
    p_desc = tu_desc_next(p_desc);
  }
  ac_desc.desc_end = p_desc;

  // Audio Streaming interfaces belonging to this function immediately follow
  // its Audio Control descriptor block.
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
    p_desc = audioh_parse_as(p_audio, &ac_desc, desc_interface, p_desc, desc_end);
    if (p_desc == NULL) {
      goto open_failed;
    }
  }

  audioh_link_feature_units(p_audio, &ac_desc);

  if (p_audio->in_stream.as_count == 0 && p_audio->out_stream.as_count == 0) {
    goto open_failed;
  }

  // Assign contiguous public indices in playback-then-capture order.
  uint8_t stream_idx = 0;
  if (p_audio->out_stream.as_count > 0) {
    p_audio->out_stream.stream_idx = stream_idx++;
  }
  if (p_audio->in_stream.as_count > 0) {
    p_audio->in_stream.stream_idx = stream_idx++;
  }
  p_audio->stream_count = stream_idx;

  const tuh_audio_descriptor_cb_t desc_cb_data = {
    .desc_audio_control        = desc_itf,
    .desc_cs_audio_control     = ac_desc.desc_start,
    .desc_cs_audio_control_len = (uint16_t)(ac_desc.desc_end - ac_desc.desc_start),
  };
  tuh_audio_descriptor_cb(idx, &desc_cb_data);

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
// SET CONFIGURATION
//--------------------------------------------------------------------+
static void audioh_mount_feature_unit_next(uint8_t idx);

  #if CFG_TUH_AUDIO_PROTOCOLS & TUH_AUDIO_PROTOCOL_UAC2
static void audioh_mount_clock_complete(tuh_xfer_t *xfer);

static void audioh_uac2_configs_rebuild(audioh_interface_t *p_audio) {
  p_audio->in_stream.config_count  = 0;
  p_audio->out_stream.config_count = 0;

  for (uint8_t direction = TUSB_DIR_OUT; direction <= TUSB_DIR_IN; direction++) {
    tuh_audio_stream_t *stream = audioh_get_stream(p_audio, (tusb_dir_t)direction);
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
    // Only the Audio Control interface drives mounting. Streaming alternate
    // settings are selected later by tuh_audio_start().
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
// APPLICATION API
//--------------------------------------------------------------------+
bool tuh_audio_mounted(uint8_t idx) {
  TU_VERIFY(idx < CFG_TUH_AUDIO_MAX);
  return _audioh_itf[idx].mounted;
}

uint8_t tuh_audio_get_dev_addr(uint8_t idx) {
  TU_VERIFY(idx < CFG_TUH_AUDIO_MAX, 0);
  return _audioh_itf[idx].daddr;
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
  TU_VERIFY(s != NULL && s->volume_range_channel != TUSB_INDEX_INVALID_8, false);
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

  return audioh_stream_config_get(s, config_idx, config);
}

bool tuh_audio_configure(uint8_t dev_idx, uint8_t stream_idx, uint8_t config_idx) {
  TU_VERIFY(dev_idx < CFG_TUH_AUDIO_MAX, false);
  audioh_interface_t *p_audio = &_audioh_itf[dev_idx];
  TU_VERIFY(p_audio->mounted, false);

  tuh_audio_stream_t *s = audioh_get_stream_by_idx(p_audio, stream_idx);
  TU_VERIFY(s, false);
  tuh_audio_stream_config_t cfg;
  uint8_t                   as_idx;
  uint8_t                   rate_idx;
  TU_VERIFY(audioh_stream_resolve_config(s, config_idx, &as_idx, &rate_idx), false);
  audioh_stream_config_fill(s, as_idx, rate_idx, &cfg);
  TU_VERIFY(!s->running, false);
  if (s->state == STREAM_STATE_READY) {
    // Configuration cannot close an endpoint while its final transfer drains.
    TU_VERIFY(!usbh_edpt_busy(s->daddr, s->edpt.ep_addr), false);
    if (s->dir == TUSB_DIR_OUT && p_audio->playback.feedback_opened) {
      TU_VERIFY(!usbh_edpt_busy(s->daddr, p_audio->playback.feedback[s->active_as].ep_addr), false);
    }
  }

  // Reopen even when the address is unchanged: packet size and interval belong
  // to the alternate setting and may differ.
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
    playback->feedback_min_frames = (uint16_t)((cfg.sample_rate - 1u) / frame_div);
    playback->feedback_max_frames = (uint16_t)(cfg.sample_rate / frame_div + 1u);
    playback->rem_acc             = 0;
  }
  if (s->dir == TUSB_DIR_IN) {
    // Overwrite mode is frame-safe only when FIFO depth is a whole-frame multiple.
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

// Start endpoint transfers after the alternate setting and sampling frequency
// are both active.
static bool audioh_stream_start_xfer(tuh_audio_stream_t *s) {
  if (s->dir == TUSB_DIR_IN) {
    return audioh_stream_capture_xfer(s);
  } else {
    if (audioh_get_playback(s)->feedback[s->active_as].ep_addr != 0) {
      TU_VERIFY(audioh_stream_feedback_xfer(s), false);
    }
    return audioh_stream_playback_xfer(s);
  }
}

static void audioh_stream_start_done(tuh_audio_stream_t *s, tusb_xfer_result_t result) {
  if (result != XFER_RESULT_SUCCESS) {
    audioh_stream_stop_xfers(s);
  }
  s->operation = AUDIOH_STREAM_OP_NONE;
  tuh_audio_event_cb(s->idx, s->stream_idx, TUH_AUDIO_EVENT_START_COMPLETE, result);
}

static void audioh_stream_start_xfers(tuh_audio_stream_t *s) {
  const tusb_xfer_result_t result = audioh_stream_start_xfer(s) ? XFER_RESULT_SUCCESS : XFER_RESULT_FAILED;
  audioh_stream_start_done(s, result);
}

static void audioh_stream_start_complete(tuh_xfer_t *xfer);

static bool audioh_stream_activate(tuh_audio_stream_t *s) {
  const audioh_as_config_t *as = audioh_stream_active_as(s);
  return tuh_interface_set(s->daddr, as->itf_num, as->alt_setting, audioh_stream_start_complete, (uintptr_t)s);
}

static void audioh_stream_start_set_freq_complete(tuh_xfer_t *xfer) {
  tuh_audio_stream_t *s = (tuh_audio_stream_t *)xfer->user_data;
  if (s->daddr != xfer->daddr || s->state != STREAM_STATE_READY || !s->running) {
    // Ignore a completion delivered after disconnect or stop.
    return;
  }
  if (xfer->result != XFER_RESULT_SUCCESS) {
    TU_LOG_DRV("  AUDIO set sampling frequency failed: result=%u\r\n", xfer->result);
    audioh_stream_start_done(s, xfer->result);
    return;
  }
  #if CFG_TUH_AUDIO_PROTOCOLS & TUH_AUDIO_PROTOCOL_UAC2
  if (_audioh_itf[s->idx].protocol == AUDIO_INT_PROTOCOL_CODE_V2) {
    if (!audioh_stream_activate(s)) {
      audioh_stream_start_done(s, XFER_RESULT_FAILED);
    }
  } else
  #endif
  {
    audioh_stream_start_xfers(s);
  }
}

static void audioh_stream_start_active(tuh_audio_stream_t *s) {
  #if CFG_TUH_AUDIO_PROTOCOLS & TUH_AUDIO_PROTOCOL_UAC1
  const audioh_as_config_t   *as          = audioh_stream_active_as(s);
  const audioh_rate_source_t *rate_source = audioh_as_rate_source(s, as);
  if (_audioh_itf[s->idx].protocol == AUDIO_INT_PROTOCOL_CODE_V1 &&
      rate_source->frequency_access == AUDIOH_CTRL_READ_WRITE) {
    if (!audioh_stream_set_freq(s, audioh_stream_start_set_freq_complete)) {
      audioh_stream_start_done(s, XFER_RESULT_FAILED);
    }
    return;
  } else
  #endif
  {
    audioh_stream_start_xfers(s);
  }
}

static void audioh_stream_start_complete(tuh_xfer_t *xfer) {
  tuh_audio_stream_t *s = (tuh_audio_stream_t *)xfer->user_data;
  if (s->daddr != xfer->daddr || s->state != STREAM_STATE_READY || !s->running) {
    // Ignore a completion delivered after disconnect or stop.
    return;
  }
  if (xfer->result != XFER_RESULT_SUCCESS) {
    TU_LOG_DRV("  AUDIO SET_INTERFACE activate failed: result=%u\r\n", xfer->result);
    audioh_stream_start_done(s, xfer->result);
    return;
  }
  audioh_stream_start_active(s);
}

bool tuh_audio_start(uint8_t dev_idx, uint8_t stream_idx) {
  TU_VERIFY(dev_idx < CFG_TUH_AUDIO_MAX, false);
  audioh_interface_t *p_audio = &_audioh_itf[dev_idx];
  TU_VERIFY(p_audio->mounted, false);

  tuh_audio_stream_t *s = audioh_get_stream_by_idx(p_audio, stream_idx);
  TU_VERIFY(s, false);
  TU_VERIFY(s->state == STREAM_STATE_READY && s->operation == AUDIOH_STREAM_OP_NONE && !s->running, false);
  // A stopped transfer must drain before the endpoint can be restarted.
  const audioh_as_config_t   *as          = audioh_stream_active_as(s);
  const audioh_rate_source_t *rate_source = audioh_as_rate_source(s, as);
  TU_VERIFY(!usbh_edpt_busy(s->daddr, as->ep_addr), false);
  if (s->dir == TUSB_DIR_OUT && p_audio->playback.feedback_opened) {
    TU_VERIFY(!usbh_edpt_busy(s->daddr, p_audio->playback.feedback[s->active_as].ep_addr), false);
  }

  // Capture and playback must use the same rate while both are running.
  tuh_audio_stream_t *other = (s == &p_audio->out_stream) ? &p_audio->in_stream : &p_audio->out_stream;
  if (other->running) {
    const audioh_as_config_t   *other_as          = audioh_stream_active_as(other);
    const audioh_rate_source_t *other_rate_source = audioh_as_rate_source(other, other_as);
    const uint32_t              sample_rate       = rate_source->sample_rate[s->active_rate];
    const uint32_t              other_sample_rate = other_rate_source->sample_rate[other->active_rate];
    if (sample_rate != other_sample_rate) {
      TU_LOG_DRV("  AUDIO start failed: capture/playback sample rates must match (%lu != %lu)\r\n",
                 (unsigned long)sample_rate, (unsigned long)other_sample_rate);
      return false;
    }
  }

  if (s->dir == TUSB_DIR_OUT) {
    p_audio->playback.target_frames_q16 = p_audio->playback.nominal_frames_q16;
    p_audio->playback.rem_acc           = 0;
  }
  s->running   = true;
  s->operation = AUDIOH_STREAM_OP_START;
  // UAC2 controls a Clock Source that exists before endpoint activation. UAC1
  // controls the endpoint itself, so its alternate setting must be active first.
  bool submitted = false;
  #if CFG_TUH_AUDIO_PROTOCOLS & TUH_AUDIO_PROTOCOL_UAC2
  if (p_audio->protocol == AUDIO_INT_PROTOCOL_CODE_V2 && rate_source->frequency_access == AUDIOH_CTRL_READ_WRITE) {
    submitted = audioh_stream_set_freq(s, audioh_stream_start_set_freq_complete);
  } else
  #endif
  {
    submitted = audioh_stream_activate(s);
  }
  if (!submitted) {
    s->operation = AUDIOH_STREAM_OP_NONE;
    s->running   = false;
    return false;
  }
  return true;
}

static void audioh_stream_stop_complete(tuh_xfer_t *xfer) {
  tuh_audio_stream_t *s = (tuh_audio_stream_t *)xfer->user_data;
  if (s->daddr != xfer->daddr || s->operation != AUDIOH_STREAM_OP_STOP) {
    return;
  }
  TU_LOG_DRV("  AUDIO SET_INTERFACE deactivate done: result=%u\r\n", xfer->result);
  s->operation = AUDIOH_STREAM_OP_NONE;
  tuh_audio_event_cb(s->idx, s->stream_idx, TUH_AUDIO_EVENT_STOP_COMPLETE, xfer->result);
}

bool tuh_audio_stop(uint8_t dev_idx, uint8_t stream_idx) {
  TU_VERIFY(dev_idx < CFG_TUH_AUDIO_MAX, false);
  audioh_interface_t *p_audio = &_audioh_itf[dev_idx];
  TU_VERIFY(p_audio->mounted, false);

  tuh_audio_stream_t *s = audioh_get_stream_by_idx(p_audio, stream_idx);
  TU_VERIFY(s && s->state == STREAM_STATE_READY && s->operation == AUDIOH_STREAM_OP_NONE && s->running, false);

  const audioh_as_config_t *as = audioh_stream_active_as(s);
  // Preserve running state when submission fails so the caller can retry.
  TU_VERIFY(tuh_interface_set(s->daddr, as->itf_num, 0, audioh_stream_stop_complete, (uintptr_t)s), false);

  // SET_INTERFACE stops future traffic. The current transfer drains, while its
  // data and all queued frames are discarded.
  s->operation = AUDIOH_STREAM_OP_STOP;
  audioh_stream_stop_xfers(s);
  return true;
}

uint32_t tuh_audio_write(uint8_t dev_idx, uint8_t stream_idx, const void *buffer, uint32_t frame_count) {
  TU_VERIFY(dev_idx < CFG_TUH_AUDIO_MAX, 0);
  audioh_interface_t *p_audio = &_audioh_itf[dev_idx];
  TU_VERIFY(p_audio->mounted && buffer, 0);

  tuh_audio_stream_t *s = audioh_get_stream_by_idx(p_audio, stream_idx);
  TU_VERIFY(s && s->dir == TUSB_DIR_OUT, 0);
  TU_VERIFY(s->state == STREAM_STATE_READY && s->running, 0);

  // Never split an audio frame at the FIFO boundary.
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
  TU_VERIFY(s && s->dir == TUSB_DIR_IN, 0);
  TU_VERIFY(s->state == STREAM_STATE_READY && s->running, 0);

  // Never return a partial audio frame.
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
// AUDIO CONTROL REQUESTS
//--------------------------------------------------------------------+

static void audioh_fu_set_complete(tuh_xfer_t *xfer);

enum {
  AUDIOH_FU_VALUE_BOOL,
  AUDIOH_FU_VALUE_I16
};

static uint8_t audioh_control_cur_request(uint8_t protocol, tusb_dir_t direction) {
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

static bool audioh_control_submit(uint8_t idx, uint8_t entity_id, tusb_dir_t direction, uint8_t request,
                                  uint8_t control_selector, uint8_t channel, void *buffer, uint16_t length,
                                  tuh_xfer_t *xfer) {
  TU_VERIFY(idx < CFG_TUH_AUDIO_MAX && entity_id != 0 && request != 0, false);
  TU_VERIFY(direction == TUSB_DIR_OUT || direction == TUSB_DIR_IN, false);
  TU_VERIFY(buffer != NULL || length == 0, false);
  audioh_interface_t *p_audio = &_audioh_itf[idx];
  TU_VERIFY(p_audio->mounted, false);

  const tusb_control_request_t setup = {
    .bmRequestType_bit = {.recipient = TUSB_REQ_RCPT_INTERFACE, .type = TUSB_REQ_TYPE_CLASS, .direction = direction},
    .bRequest          = request,
    .wValue            = tu_htole16(tu_u16(control_selector, channel)),
    .wIndex            = tu_htole16(tu_u16(entity_id, p_audio->ac_itf_num)),
    .wLength           = tu_htole16(length),
  };
  xfer->daddr   = p_audio->daddr;
  xfer->ep_addr = 0;
  xfer->setup   = &setup;
  xfer->buffer  = buffer;
  return tuh_control_xfer(xfer);
}

bool tuh_audio_control_xfer(uint8_t idx, uint8_t entity_id, tusb_dir_t direction, uint8_t request,
                            uint8_t control_selector, uint8_t channel, void *buffer, uint16_t length,
                            tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  tuh_xfer_t xfer = {.complete_cb = complete_cb, .user_data = user_data};
  return audioh_control_submit(idx, entity_id, direction, request, control_selector, channel, buffer, length, &xfer);
}

tusb_xfer_result_t tuh_audio_control_xfer_sync(uint8_t idx, uint8_t entity_id, tusb_dir_t direction, uint8_t request,
                                               uint8_t control_selector, uint8_t channel, void *buffer, uint16_t length,
                                               uint32_t *actual_len) {
  if (actual_len != NULL) {
    *actual_len = 0;
  }

  tuh_xfer_t xfer = {0};
  if (!audioh_control_submit(idx, entity_id, direction, request, control_selector, channel, buffer, length, &xfer)) {
    return XFER_RESULT_TIMEOUT;
  }

  if (actual_len != NULL) {
    *actual_len = xfer.actual_len;
  }
  return xfer.result;
}

// Continue a master-volume request across logical channels. Driver-owned
// request state is released before the final application callback so another
// Feature Unit request can be submitted from that callback.
static void audioh_fu_set_complete(tuh_xfer_t *xfer) {
  const uint8_t        idx     = (uint8_t)xfer->user_data;
  audioh_interface_t  *p_audio = &_audioh_itf[idx];
  audioh_ctrl_state_t *ctrl    = &p_audio->ctrl;

  if (xfer->result == XFER_RESULT_SUCCESS && ctrl->fu.control.channel < ctrl->fu.control.last_channel) {
    tuh_audio_stream_t *s = (tuh_audio_stream_t *)ctrl->value;
    ctrl->fu.control.channel++;
    const uint8_t request_code = audioh_control_cur_request(p_audio->protocol, TUSB_DIR_OUT);
    tuh_xfer_t    next_xfer    = {.complete_cb = audioh_fu_set_complete, .user_data = (uintptr_t)idx};
    if (audioh_control_submit(idx, s->feature_unit_id, TUSB_DIR_OUT, request_code, AUDIO10_FU_CTRL_VOLUME,
                              ctrl->fu.control.channel, audioh_fu_ctrl(&_audioh_epbuf[idx]), 2, &next_xfer)) {
      return;
    }
    xfer->result = XFER_RESULT_FAILED;
  }

  tuh_xfer_cb_t app_cb    = ctrl->complete_cb;
  uintptr_t     user_data = ctrl->user_data;
  ctrl->complete_cb       = NULL;
  ctrl->fu_busy           = false;
  ctrl->value             = NULL;

  xfer->user_data = user_data;
  if (app_cb != NULL) {
    app_cb(xfer);
  }
}

static void audioh_fu_value_store(audioh_ctrl_state_t *ctrl, audioh_epbuf_t *epbuf) {
  if (ctrl->fu.control.value_type == AUDIOH_FU_VALUE_BOOL) {
    *((bool *)ctrl->value) = audioh_fu_ctrl(epbuf)[0] != 0;
  } else {
    const uint16_t value      = tu_le16toh(tu_unaligned_read16(audioh_fu_ctrl(epbuf)));
    *((int16_t *)ctrl->value) = (int16_t)value;
  }
}

// Convert the driver-owned response before releasing the request state and
// invoking the application callback.
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
  tuh_audio_stream_t  *s       = audioh_get_stream_by_idx_unchecked(p_audio, ctrl->fu.mount.stream_idx);

  const bool                   uac2     = p_audio->protocol == AUDIO_INT_PROTOCOL_CODE_V2;
  const uint8_t                selector = uac2 ? AUDIO20_FU_CTRL_VOLUME : AUDIO10_FU_CTRL_VOLUME;
  const tusb_control_request_t request  = {
    .bmRequestType_bit = {.recipient = TUSB_REQ_RCPT_INTERFACE, .type = TUSB_REQ_TYPE_CLASS, .direction = TUSB_DIR_IN},
    .bRequest          = uac2 ? AUDIO20_CS_REQ_RANGE : audioh_fu_volume_range_request(ctrl->fu.mount.range_step),
    .wValue            = tu_htole16(tu_u16(selector, s->volume_range_channel)),
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
    tuh_audio_stream_t *s = audioh_get_stream_by_idx_unchecked(p_audio, ctrl->fu.mount.stream_idx);
    if (s->volume_range_channel != TUSB_INDEX_INVALID_8) {
      s->volume_range           = (tuh_audio_volume_range_t){0};
      ctrl->fu.mount.range_step = AUDIOH_VOLUME_RANGE_MIN;
      ctrl->fu_busy             = true;
      if (audioh_mount_feature_unit_submit(idx)) {
        return;
      }
      s->volume_master_access         = AUDIOH_CTRL_NONE;
      s->volume_range_channel         = TUSB_INDEX_INVALID_8;
      s->volume_all_channels_writable = false;
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
  tuh_audio_stream_t *s = audioh_get_stream_by_idx_unchecked(p_audio, ctrl->fu.mount.stream_idx);

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
    s->volume_master_access         = AUDIOH_CTRL_NONE;
    s->volume_range_channel         = TUSB_INDEX_INVALID_8;
    s->volume_all_channels_writable = false;
    s->volume_range                 = (tuh_audio_volume_range_t){0};
    if (s->mute_access == AUDIOH_CTRL_NONE) {
      s->feature_unit_id = 0;
    }
  }
  ctrl->fu_busy = false;
  ctrl->fu.mount.stream_idx++;
  audioh_mount_feature_unit_next(idx);
}

static bool audioh_fu_set(uint8_t idx, uint8_t stream_idx, uint8_t control_selector, uint8_t channel,
                          uint8_t last_channel, uint16_t value, uint8_t width, tuh_xfer_cb_t complete_cb,
                          uintptr_t user_data) {
  TU_VERIFY(idx < CFG_TUH_AUDIO_MAX, false);
  audioh_interface_t *p_audio = &_audioh_itf[idx];
  TU_VERIFY(p_audio->mounted, false);
  tuh_audio_stream_t *s = audioh_get_stream_by_idx(p_audio, stream_idx);
  TU_VERIFY(s && s->feature_unit_id != 0, false);
  if (control_selector == AUDIO10_FU_CTRL_MUTE) {
    TU_VERIFY(channel == 0 && last_channel == 0 && s->mute_access == AUDIOH_CTRL_READ_WRITE, false);
  } else if (control_selector == AUDIO10_FU_CTRL_VOLUME) {
    TU_VERIFY(s->volume_range_channel != TUSB_INDEX_INVALID_8 && channel <= last_channel, false);
    if (channel == 0) {
      TU_VERIFY(last_channel == 0 && s->volume_master_access == AUDIOH_CTRL_READ_WRITE, false);
    } else {
      TU_VERIFY(last_channel <= s->feature_unit_channels, false);
      TU_VERIFY(channel == last_channel || s->volume_all_channels_writable, false);
    }
  }

  const uint8_t request_code = audioh_control_cur_request(p_audio->protocol, TUSB_DIR_OUT);
  audioh_ctrl_state_t *ctrl  = &p_audio->ctrl;
  audioh_epbuf_t      *epbuf = &_audioh_epbuf[idx];
  TU_VERIFY(!ctrl->fu_busy, false);
  // Reserve both bookkeeping and payload storage before populating the request.
  ctrl->fu_busy = true;

  uint8_t *val_buf = audioh_fu_ctrl(epbuf);
  val_buf[0]       = (uint8_t)(value & 0xFF);
  if (width == 2) {
    val_buf[1] = (uint8_t)((value >> 8) & 0xFF);
  }

  if (complete_cb == NULL) {
    bool result = true;
    for (uint8_t current_channel = channel; current_channel <= last_channel; current_channel++) {
      tuh_xfer_t xfer = {.complete_cb = NULL, .user_data = user_data};
      result          = audioh_control_submit(idx, s->feature_unit_id, TUSB_DIR_OUT, request_code, control_selector,
                                              current_channel, val_buf, width, &xfer);
      if (!result || xfer.result != XFER_RESULT_SUCCESS) {
        break;
      }
    }
    ctrl->fu_busy = false;
    return result;
  }

  ctrl->complete_cb             = complete_cb;
  ctrl->user_data               = user_data;
  ctrl->value                   = s;
  ctrl->fu.control.channel      = channel;
  ctrl->fu.control.last_channel = last_channel;
  tuh_xfer_t xfer               = {.complete_cb = audioh_fu_set_complete, .user_data = (uintptr_t)idx};

  if (!audioh_control_submit(idx, s->feature_unit_id, TUSB_DIR_OUT, request_code, control_selector, channel, val_buf,
                             width, &xfer)) {
    ctrl->complete_cb = NULL;
    ctrl->value       = NULL;
    ctrl->fu_busy     = false;
    return false;
  }
  return true;
}

static bool audioh_fu_get(uint8_t idx, uint8_t stream_idx, uint8_t control_selector, uint8_t channel, void *value,
                          uint8_t width, uint8_t value_type, tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  TU_VERIFY(idx < CFG_TUH_AUDIO_MAX, false);
  audioh_interface_t *p_audio = &_audioh_itf[idx];
  TU_VERIFY(p_audio->mounted && value, false);
  tuh_audio_stream_t *s = audioh_get_stream_by_idx(p_audio, stream_idx);
  TU_VERIFY(s && s->feature_unit_id != 0, false);
  if (control_selector == AUDIO10_FU_CTRL_MUTE) {
    TU_VERIFY(channel == 0 && s->mute_access != AUDIOH_CTRL_NONE, false);
  } else if (control_selector == AUDIO10_FU_CTRL_VOLUME) {
    TU_VERIFY(s->volume_range_channel != TUSB_INDEX_INVALID_8, false);
    if (channel == 0) {
      TU_VERIFY(s->volume_master_access != AUDIOH_CTRL_NONE, false);
    } else {
      TU_VERIFY(channel <= s->feature_unit_channels, false);
    }
  }

  const uint8_t request_code = audioh_control_cur_request(p_audio->protocol, TUSB_DIR_IN);
  audioh_ctrl_state_t *ctrl  = &p_audio->ctrl;
  audioh_epbuf_t      *epbuf = &_audioh_epbuf[idx];
  TU_VERIFY(!ctrl->fu_busy, false);
  ctrl->fu_busy               = true;
  ctrl->value                 = value;
  ctrl->fu.control.width      = width;
  ctrl->fu.control.value_type = value_type;

  if (complete_cb == NULL) {
    // The synchronous transfer completes before its driver-owned response is
    // converted to host order.
    tuh_xfer_t xfer = {.complete_cb = NULL, .user_data = user_data};
    if (!audioh_control_submit(idx, s->feature_unit_id, TUSB_DIR_IN, request_code, control_selector, channel,
                               audioh_fu_ctrl(epbuf), width, &xfer)) {
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

  // The asynchronous wrapper converts the response before calling the application.
  ctrl->complete_cb = complete_cb;
  ctrl->user_data   = user_data;
  tuh_xfer_t xfer   = {.complete_cb = audioh_fu_get_complete, .user_data = (uintptr_t)idx};

  if (!audioh_control_submit(idx, s->feature_unit_id, TUSB_DIR_IN, request_code, control_selector, channel,
                             audioh_fu_ctrl(epbuf), width, &xfer)) {
    ctrl->complete_cb = NULL;
    ctrl->fu_busy     = false;
    return false;
  }
  return true;
}

bool tuh_audio_mute_set(uint8_t idx, uint8_t stream_idx, bool mute, tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  return audioh_fu_set(idx, stream_idx, AUDIO10_FU_CTRL_MUTE, 0, 0, mute ? 1 : 0, 1, complete_cb, user_data);
}

bool tuh_audio_mute_get(uint8_t idx, uint8_t stream_idx, bool *mute, tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  return audioh_fu_get(idx, stream_idx, AUDIO10_FU_CTRL_MUTE, 0, mute, 1, AUDIOH_FU_VALUE_BOOL, complete_cb, user_data);
}

static bool audioh_volume_normalize(uint8_t idx, uint8_t stream_idx, int16_t *volume) {
  tuh_audio_volume_range_t range;
  TU_VERIFY(tuh_audio_volume_range_get(idx, stream_idx, &range), false);
  if (*volume != TUH_AUDIO_VOLUME_SILENCE) {
    TU_VERIFY(*volume >= range.min && *volume <= range.max && range.res != 0, false);
    const uint32_t offset  = (uint32_t)((int32_t)*volume - range.min);
    const uint32_t steps   = (offset + range.res / 2u) / range.res;
    int32_t        rounded = (int32_t)range.min + (int32_t)(steps * range.res);
    if (rounded > range.max) {
      rounded -= range.res;
    }
    *volume = (int16_t)rounded;
  }
  return true;
}

bool tuh_audio_volume_set(uint8_t idx, uint8_t stream_idx, uint8_t channel, int16_t volume, tuh_xfer_cb_t complete_cb,
                          uintptr_t user_data) {
  TU_VERIFY(audioh_volume_normalize(idx, stream_idx, &volume), false);
  if (channel > 0) {
    return audioh_fu_set(idx, stream_idx, AUDIO10_FU_CTRL_VOLUME, channel, channel, (uint16_t)volume, 2, complete_cb,
                         user_data);
  }

  audioh_interface_t *p_audio = &_audioh_itf[idx];
  tuh_audio_stream_t *s       = audioh_get_stream_by_idx(p_audio, stream_idx);
  TU_VERIFY(s != NULL, false);
  if (s->volume_master_access == AUDIOH_CTRL_READ_WRITE) {
    return audioh_fu_set(idx, stream_idx, AUDIO10_FU_CTRL_VOLUME, 0, 0, (uint16_t)volume, 2, complete_cb, user_data);
  }
  TU_VERIFY(s->feature_unit_channels > 0 && s->volume_all_channels_writable, false);
  return audioh_fu_set(idx, stream_idx, AUDIO10_FU_CTRL_VOLUME, 1, s->feature_unit_channels, (uint16_t)volume, 2,
                       complete_cb, user_data);
}

bool tuh_audio_volume_get(uint8_t idx, uint8_t stream_idx, uint8_t channel, int16_t *volume, tuh_xfer_cb_t complete_cb,
                          uintptr_t user_data) {
  return audioh_fu_get(idx, stream_idx, AUDIO10_FU_CTRL_VOLUME, channel, volume, 2, AUDIOH_FU_VALUE_I16, complete_cb,
                       user_data);
}

#endif

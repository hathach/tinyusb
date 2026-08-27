/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 Zhenjiang Zhang
 * SPDX-FileCopyrightText: Copyright (c) 2026 HiFiPhile (Zixun LI)
 * SPDX-License-Identifier: MIT
 *
 * This file is part of the TinyUSB stack.
 */

#ifndef TUSB_AUDIO_HOST_H_
#define TUSB_AUDIO_HOST_H_

#include "audio.h"

#ifdef __cplusplus
extern "C" {
#endif

//--------------------------------------------------------------------+
// Class Driver Configuration
//--------------------------------------------------------------------+

// Audio Class protocol versions compiled into the host driver. Multiple
// versions can be enabled so UAC1 and UAC2 devices can be mounted together.
#define TUH_AUDIO_PROTOCOL_UAC1 TU_BIT(0)
#define TUH_AUDIO_PROTOCOL_UAC2 TU_BIT(1)

#ifndef CFG_TUH_AUDIO_PROTOCOLS
  #define CFG_TUH_AUDIO_PROTOCOLS TUH_AUDIO_PROTOCOL_UAC1
#endif

#if !(CFG_TUH_AUDIO_PROTOCOLS & (TUH_AUDIO_PROTOCOL_UAC1 | TUH_AUDIO_PROTOCOL_UAC2))
  #error CFG_TUH_AUDIO_PROTOCOLS must enable UAC1 and/or UAC2
#endif

#if CFG_TUH_AUDIO_PROTOCOLS & ~(TUH_AUDIO_PROTOCOL_UAC1 | TUH_AUDIO_PROTOCOL_UAC2)
  #error CFG_TUH_AUDIO_PROTOCOLS contains an unsupported protocol bit
#endif

// Maximum number of Audio devices
#ifndef CFG_TUH_AUDIO_MAX
  #define CFG_TUH_AUDIO_MAX 1
#endif
// Maximum number of discrete sampling frequencies per Audio Streaming interface
#ifndef CFG_TUH_AUDIO_MAX_SAM_FREQ
  #define CFG_TUH_AUDIO_MAX_SAM_FREQ 5
#endif
// Maximum number of Audio Streaming interfaces per Audio device
#ifndef CFG_TUH_AUDIO_MAX_AS
  #define CFG_TUH_AUDIO_MAX_AS 4
#endif

// Maximum size of one capture (IN) isochronous transfer the driver submits.
// Configurations needing a larger per-poll-interval packet are rejected.
// 256 covers 2-ch 48 kHz S16_LE (192 B) and common endpoint padding (208 B).
#ifndef CFG_TUH_AUDIO_EPIN_BUFSIZE
  #define CFG_TUH_AUDIO_EPIN_BUFSIZE 256
#endif

// Maximum size of one playback (OUT) isochronous transfer the driver submits.
// Configurations needing a larger per-poll-interval packet are rejected.
#ifndef CFG_TUH_AUDIO_EPOUT_BUFSIZE
  #define CFG_TUH_AUDIO_EPOUT_BUFSIZE 256
#endif

// Depth in bytes of the per-stream data FIFO. The FIFO decouples the
// application's read/write calls from the endpoint's isochronous polling cadence
// and absorbs rate differences. Capture overwrites the oldest frames when full.
// 1024 bytes hold 4 default (256 B) packets.
#ifndef CFG_TUH_AUDIO_STREAM_BUFSIZE
  #define CFG_TUH_AUDIO_STREAM_BUFSIZE 1024
#endif

//--------------------------------------------------------------------+
// Types
//--------------------------------------------------------------------+

// Fixed transfer direction of a logical stream.
typedef enum {
  TUH_AUDIO_STREAM_PLAYBACK = 0, // Host -> Device (OUT)
  TUH_AUDIO_STREAM_CAPTURE  = 1, // Device -> Host (IN)
  TUH_AUDIO_STREAM_DIRECTION_COUNT
} tuh_audio_direction_t;

// Discrete Type-I PCM sample format. UAC1 requires bSamFreqType > 0; UAC2
// configurations are built from the directly connected Clock Source RANGE.
typedef enum {
  TUH_AUDIO_FORMAT_S8 = 0,  // signed 8-bit
  TUH_AUDIO_FORMAT_S16_LE,  // signed 16-bit little-endian
  TUH_AUDIO_FORMAT_S24_3LE, // signed 24-bit packed in 3 bytes, LE
  TUH_AUDIO_FORMAT_S24_LE,  // signed 24-bit in 32-bit container, LE
  TUH_AUDIO_FORMAT_S32_LE,  // signed 32-bit little-endian
  TUH_AUDIO_FORMAT_COUNT
} tuh_audio_format_t;

// One complete supported discrete configuration tuple.
// Each entry is a full (format, sample_rate, channels) combination,
// avoiding invalid mixes between independent format/rate/channel lists.
// dir is constant for all configs of a given (dev_idx, stream_idx) and
// equals the result of tuh_audio_stream_direction().
typedef struct {
  uint32_t              sample_rate;
  tuh_audio_direction_t dir;
  tuh_audio_format_t    format;
  uint8_t               channels;
} tuh_audio_stream_config_t;

// Volume values are signed 1/256 dB. INT16_MIN represents silence.
#define TUH_AUDIO_VOLUME_SILENCE INT16_MIN

// One continuous volume range. This matches UAC1 MIN/MAX/RES and the common
// UAC2 RANGE response containing one subrange.
typedef struct {
  int16_t  min;
  int16_t  max;
  uint16_t res;
} tuh_audio_volume_range_t;

//--------------------------------------------------------------------+
// Stream Enumeration
//--------------------------------------------------------------------+

// Number of logical audio streams exposed by one mounted device. The
// application iterates stream indices [0, tuh_audio_stream_count()) and
// inspects each with tuh_audio_stream_exists()/tuh_audio_stream_direction().
uint8_t tuh_audio_stream_count(uint8_t dev_idx);

// True if (dev_idx, stream_idx) identifies an existing stream.
bool tuh_audio_stream_exists(uint8_t dev_idx, uint8_t stream_idx);

// Fixed transfer direction of the stream.
tuh_audio_direction_t tuh_audio_stream_direction(uint8_t dev_idx, uint8_t stream_idx);

//--------------------------------------------------------------------+
// Configuration Enumeration
//--------------------------------------------------------------------+

// Number of supported discrete configurations of the stream.
uint8_t tuh_audio_config_count(uint8_t dev_idx, uint8_t stream_idx);

// Active configuration index of the stream, or TUSB_INDEX_INVALID_8 if none.
uint8_t tuh_audio_active_config(uint8_t dev_idx, uint8_t stream_idx);

// Retrieve one discrete configuration tuple into *config.
bool tuh_audio_config_get(uint8_t dev_idx, uint8_t stream_idx, uint8_t config_idx, tuh_audio_stream_config_t *config);

//--------------------------------------------------------------------+
// Configuration (ALSA hw_params analogue)
//--------------------------------------------------------------------+

// Synchronously configure the stream with the discrete configuration identified by
// config_idx. The driver:
//   1. resolves the AS interface and alternate setting,
//   2. initializes the FIFO and packet scheduler,
//   3. opens / reconfigures only the selected endpoint.
bool tuh_audio_configure(uint8_t dev_idx, uint8_t stream_idx, uint8_t config_idx);

//--------------------------------------------------------------------+
// Stream Control / Frame-based Data
//--------------------------------------------------------------------+

// Start transferring data with the configuration selected by configure().
// UAC1 activates the alternate setting before setting an endpoint frequency;
// UAC2 sets a writable Clock Source before activating the alternate setting.
bool tuh_audio_start(uint8_t dev_idx, uint8_t stream_idx);
// Stop transferring and deactivate the Audio Streaming interface (alt 0).
bool tuh_audio_stop(uint8_t dev_idx, uint8_t stream_idx);

// Frame-based transfer. One frame = channels * bytes per sample.
// tuh_audio_write() is valid only for TUH_AUDIO_STREAM_PLAYBACK streams,
// tuh_audio_read() only for TUH_AUDIO_STREAM_CAPTURE streams.
// Returns the number of frames actually written/read (0 on any error,
// including wrong direction, unconfigured/stopped stream, or full/empty FIFO).
uint32_t tuh_audio_write(uint8_t dev_idx, uint8_t stream_idx, const void *buffer, uint32_t frame_count);
uint32_t tuh_audio_read(uint8_t dev_idx, uint8_t stream_idx, void *buffer, uint32_t frame_count);

// FIFO occupancy in frames available for a non-blocking write/read.
uint32_t tuh_audio_write_available(uint8_t dev_idx, uint8_t stream_idx);
uint32_t tuh_audio_read_available(uint8_t dev_idx, uint8_t stream_idx);

//--------------------------------------------------------------------+
// Helpers
//--------------------------------------------------------------------+

// Container size in bytes of one sample for a given format.
static inline uint8_t tuh_audio_format_bytes(tuh_audio_format_t format) {
  switch (format) {
    case TUH_AUDIO_FORMAT_S8:
      return 1;
    case TUH_AUDIO_FORMAT_S16_LE:
      return 2;
    case TUH_AUDIO_FORMAT_S24_3LE:
      return 3;
    case TUH_AUDIO_FORMAT_S24_LE:
    case TUH_AUDIO_FORMAT_S32_LE:
      return 4;
    default:
      return 0;
  }
}

// Size in bytes of one frame (all channels) for a configuration.
static inline uint32_t tuh_audio_config_frame_size(const tuh_audio_stream_config_t *config) {
  TU_ASSERT(config != NULL);
  return (uint32_t)tuh_audio_format_bytes(config->format) * config->channels;
}

//--------------------------------------------------------------------+
// Device Info
//--------------------------------------------------------------------+

// Check if Audio device is mounted
bool tuh_audio_mounted(uint8_t idx);
// Get device address of Audio device
uint8_t tuh_audio_get_dev_addr(uint8_t idx);
// Get the Feature Unit ID associated with a stream (0 = none)
uint8_t tuh_audio_get_feature_unit_id(uint8_t idx, uint8_t stream_idx);
// True when the stream's Feature Unit supports master mute control.
bool tuh_audio_mute_supported(uint8_t idx, uint8_t stream_idx);
// Get the cached master volume range. Returns false when volume is unsupported.
bool tuh_audio_volume_range_get(uint8_t idx, uint8_t stream_idx, tuh_audio_volume_range_t *range);

//--------------------------------------------------------------------+
// Control Request API
//--------------------------------------------------------------------+

// Set a Feature Unit control associated with an Audio stream. UAC2 supports
// mute and volume through this low-level API; the other fixed-width selectors
// below are UAC1-only.
// Mute/bass/mid/treble/AGC/bass boost/loudness use one byte; volume/delay use two.
// Graphic EQ and unknown selectors are unsupported.
bool tuh_audio_feature_unit_set(uint8_t idx, uint8_t stream_idx, uint8_t control_selector, uint8_t channel,
                                uint16_t value, tuh_xfer_cb_t complete_cb, uintptr_t user_data);

// Get a Feature Unit control associated with an Audio stream. UAC2 supports
// mute and volume through this low-level API; the other fixed-width selectors
// below are UAC1-only.
// The value is converted to host byte order before complete_cb is invoked.
// Graphic EQ and unknown selectors are unsupported.
// Only one Feature Unit operation may be in flight per device.
bool tuh_audio_feature_unit_get(uint8_t idx, uint8_t stream_idx, uint8_t control_selector, uint8_t channel,
                                uint16_t *value, tuh_xfer_cb_t complete_cb, uintptr_t user_data);

// Master mute and volume controls. Capability and range information is cached
// before tuh_audio_mount_cb() is invoked. Volume SET accepts
// TUH_AUDIO_VOLUME_SILENCE or a value within the cached range; finite values
// are rounded to the nearest resolution step measured from the range minimum.
bool tuh_audio_mute_set(uint8_t idx, uint8_t stream_idx, bool mute, tuh_xfer_cb_t complete_cb, uintptr_t user_data);
bool tuh_audio_mute_get(uint8_t idx, uint8_t stream_idx, bool *mute, tuh_xfer_cb_t complete_cb, uintptr_t user_data);
bool tuh_audio_volume_set(uint8_t idx, uint8_t stream_idx, int16_t volume, tuh_xfer_cb_t complete_cb,
                          uintptr_t user_data);
bool tuh_audio_volume_get(uint8_t idx, uint8_t stream_idx, int16_t *volume, tuh_xfer_cb_t complete_cb,
                          uintptr_t user_data);

//--------------------------------------------------------------------+
// Control Request Sync API
// Each Function will make a USB control transfer request to/from device the function will block until request is
// complete. The function will return the transfer request result
//--------------------------------------------------------------------+
TU_ATTR_ALWAYS_INLINE static inline tusb_xfer_result_t tuh_audio_feature_unit_set_sync(uint8_t idx, uint8_t stream_idx,
                                                                                       uint8_t  control_selector,
                                                                                       uint8_t  channel,
                                                                                       uint16_t value) {
  TU_API_SYNC(tuh_audio_feature_unit_set, idx, stream_idx, control_selector, channel, value);
}

TU_ATTR_ALWAYS_INLINE static inline tusb_xfer_result_t tuh_audio_feature_unit_get_sync(uint8_t idx, uint8_t stream_idx,
                                                                                       uint8_t   control_selector,
                                                                                       uint8_t   channel,
                                                                                       uint16_t *value) {
  TU_API_SYNC(tuh_audio_feature_unit_get, idx, stream_idx, control_selector, channel, value);
}

TU_ATTR_ALWAYS_INLINE static inline tusb_xfer_result_t tuh_audio_mute_set_sync(uint8_t idx, uint8_t stream_idx,
                                                                               bool mute) {
  TU_API_SYNC(tuh_audio_mute_set, idx, stream_idx, mute);
}

TU_ATTR_ALWAYS_INLINE static inline tusb_xfer_result_t tuh_audio_mute_get_sync(uint8_t idx, uint8_t stream_idx,
                                                                               bool *mute) {
  TU_API_SYNC(tuh_audio_mute_get, idx, stream_idx, mute);
}

TU_ATTR_ALWAYS_INLINE static inline tusb_xfer_result_t tuh_audio_volume_set_sync(uint8_t idx, uint8_t stream_idx,
                                                                                 int16_t volume) {
  TU_API_SYNC(tuh_audio_volume_set, idx, stream_idx, volume);
}

TU_ATTR_ALWAYS_INLINE static inline tusb_xfer_result_t tuh_audio_volume_get_sync(uint8_t idx, uint8_t stream_idx,
                                                                                 int16_t *volume) {
  TU_API_SYNC(tuh_audio_volume_get, idx, stream_idx, volume);
}

//--------------------------------------------------------------------+
// Callbacks (Weak is optional)
//--------------------------------------------------------------------+

// Invoked when device with Audio interface is mounted
void tuh_audio_mount_cb(uint8_t idx);

// Invoked when device with Audio interface is un-mounted
void tuh_audio_umount_cb(uint8_t idx);

// Invoked when an isochronous IN transfer completes successfully: the
// received data is already queued into the stream's capture FIFO.
void tuh_audio_capture_cb(uint8_t idx, uint8_t stream_idx, uint16_t xferred_bytes);

// Invoked when an isochronous OUT transfer completes successfully: the
// next playback packet is submitted from the stream FIFO, or as silence when
// a complete packet is not queued.
void tuh_audio_playback_cb(uint8_t idx, uint8_t stream_idx, uint16_t xferred_bytes);

// Invoked when asynchronous stream activation or an isochronous transfer
// fails. The stream is stopped (tuh_audio_start() must be called again to
// resume). xferred_bytes is zero for an activation failure.
void tuh_audio_err_cb(uint8_t idx, uint8_t stream_idx, uint16_t xferred_bytes);

//--------------------------------------------------------------------+
// Internal Class Driver API
//--------------------------------------------------------------------+
bool     audioh_init(void);
bool     audioh_deinit(void);
uint16_t audioh_open(uint8_t rhport, uint8_t dev_addr, const tusb_desc_interface_t *desc_itf, uint16_t max_len);
bool     audioh_set_config(uint8_t dev_addr, uint8_t itf_num);
bool     audioh_xfer_cb(uint8_t dev_addr, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes);
void     audioh_close(uint8_t daddr);

#ifdef __cplusplus
}
#endif

#endif /* TUSB_AUDIO_HOST_H_ */

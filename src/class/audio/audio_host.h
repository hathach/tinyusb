/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 Zhenjiang Zhang
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
// and absorbs rate differences. 1024 bytes hold 4 default (256 B) packets.
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

// Discrete sample format. Only discrete configurations are supported
// initially; continuous sample-rate ranges are ignored by the driver.
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
  tuh_audio_direction_t dir;
  tuh_audio_format_t    format;
  uint32_t              sample_rate;
  uint8_t               channels;
} tuh_audio_stream_config_t;

// Asynchronous completion callback of tuh_audio_configure().
typedef void (*tuh_audio_configure_cb_t)(uint8_t dev_idx, uint8_t stream_idx, tusb_xfer_result_t result,
                                         uintptr_t user_data);

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
// Configuration (ALSA hw_params analogue, asynchronous)
//--------------------------------------------------------------------+

// Configure the stream with the discrete configuration identified by
// config_idx. The driver asynchronously:
//   1. resolves the AS interface and alternate setting,
//   2. issues SET_INTERFACE (checking submission and transfer result),
//   3. opens / reconfigures only the selected endpoint,
//   4. sets the endpoint sampling frequency when supported,
//   5. initializes the FIFO and packet scheduler.
// complete_cb is invoked with the final XFER_RESULT_* status.
bool tuh_audio_configure(uint8_t dev_idx, uint8_t stream_idx, uint8_t config_idx, tuh_audio_configure_cb_t complete_cb,
                         uintptr_t user_data);

//--------------------------------------------------------------------+
// Stream Control / Frame-based Data
//--------------------------------------------------------------------+

// Start/stop transferring data on a configured stream.
bool tuh_audio_start(uint8_t dev_idx, uint8_t stream_idx);
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

//--------------------------------------------------------------------+
// Control Request API
//--------------------------------------------------------------------+

// Set a Feature Unit control (mute, volume, ...) associated with an Audio stream (UAC 1.0)
// Mute/bass/mid/treble/AGC/bass boost/loudness use one byte; volume/delay use two.
// Graphic EQ and unknown selectors are unsupported.
bool tuh_audio_feature_unit_set(uint8_t idx, uint8_t stream_idx, uint8_t control_selector, uint8_t channel,
                                uint16_t value, tuh_xfer_cb_t complete_cb, uintptr_t user_data);

// Get a Feature Unit control (mute, volume, ...) associated with an Audio stream (UAC 1.0)
// The value is converted to host byte order before complete_cb is invoked.
// Graphic EQ and unknown selectors are unsupported.
// Only one Feature Unit GET or SET request may be in flight per device.
bool tuh_audio_feature_unit_get(uint8_t idx, uint8_t stream_idx, uint8_t control_selector, uint8_t channel,
                                uint16_t *value, tuh_xfer_cb_t complete_cb, uintptr_t user_data);

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
// next queued packet is submitted from the stream's playback FIFO.
void tuh_audio_playback_cb(uint8_t idx, uint8_t stream_idx, uint16_t xferred_bytes);

// Invoked when an isochronous transfer fails. The stream is stopped
// (tuh_audio_start() must be called again to resume).
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

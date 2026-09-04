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
// Maximum discrete sampling frequencies retained per rate source. UAC1 uses
// one rate source per alternate setting; UAC2 alternate settings may share a
// Clock Source.
#ifndef CFG_TUH_AUDIO_MAX_SAM_FREQ
  #define CFG_TUH_AUDIO_MAX_SAM_FREQ 5
#endif
// Maximum supported nonzero-bandwidth Audio Streaming alternate settings per
// logical stream.
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

// Asynchronous stream operation and transport events.
typedef enum {
  TUH_AUDIO_EVENT_START_COMPLETE = 0,
  TUH_AUDIO_EVENT_STOP_COMPLETE,
  TUH_AUDIO_EVENT_XFER_FAILED
} tuh_audio_event_t;

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

// Feature Unit channel zero selects the master channel.
#define TUH_AUDIO_CHANNEL_MASTER 0

// Volume values are signed 1/256 dB. INT16_MIN represents silence.
#define TUH_AUDIO_VOLUME_SILENCE INT16_MIN

// One continuous volume range. This matches UAC1 MIN/MAX/RES and the common
// UAC2 RANGE response containing one subrange.
typedef struct {
  int16_t  min;
  int16_t  max;
  uint16_t res;
} tuh_audio_volume_range_t;

// Audio Control descriptors reported during enumeration. Descriptor pointers
// are valid only for the duration of tuh_audio_descriptor_cb().
typedef struct {
  const tusb_desc_interface_t *desc_audio_control;
  const uint8_t               *desc_cs_audio_control;
  uint16_t                     desc_cs_audio_control_len;
} tuh_audio_descriptor_cb_t;

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
// Startup is asynchronous: true means that the first request was submitted.
// Completion is reported through tuh_audio_event_cb(); no event is emitted
// when this function returns false.
bool tuh_audio_start(uint8_t dev_idx, uint8_t stream_idx);
// Stop transferring and asynchronously deactivate the Audio Streaming
// interface (alt 0). true means that the deactivation request was submitted.
// Completion is reported through tuh_audio_event_cb(); no event is emitted
// when this function returns false.
bool tuh_audio_stop(uint8_t dev_idx, uint8_t stream_idx);

// Frame-based transfer. One frame = channels * bytes per sample.
// tuh_audio_write() is valid only for TUH_AUDIO_STREAM_PLAYBACK streams,
// tuh_audio_read() only for TUH_AUDIO_STREAM_CAPTURE streams.
// Both functions are non-blocking and return immediately.
// Returns the number of frames actually written/read (0 on any error,
// including wrong direction, unconfigured/stopped stream, or full/empty FIFO).
uint32_t tuh_audio_write(uint8_t dev_idx, uint8_t stream_idx, const void *buffer, uint32_t frame_count);
uint32_t tuh_audio_read(uint8_t dev_idx, uint8_t stream_idx, void *buffer, uint32_t frame_count);

// Number of frames that can be queued immediately for playback.
uint32_t tuh_audio_write_available(uint8_t dev_idx, uint8_t stream_idx);
// Number of captured frames that can be read immediately.
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
// True when the stream's Feature Unit supports master mute control.
bool tuh_audio_mute_supported(uint8_t idx, uint8_t stream_idx);
// Get the cached volume range. The driver reads the master channel when it
// supports volume, otherwise the first logical channel with volume control.
// This typed API assumes logical channels use the same range; applications
// needing per-channel ranges can use tuh_audio_control_xfer().
bool tuh_audio_volume_range_get(uint8_t idx, uint8_t stream_idx, tuh_audio_volume_range_t *range);

//--------------------------------------------------------------------+
// Control Request API
//--------------------------------------------------------------------+

// Submit a class-specific request to an entity on the Audio Control interface.
// request is the protocol-specific UAC request code. buffer contains the raw
// little-endian control payload. For an asynchronous transfer, buffer must
// remain valid until complete_cb is invoked.
bool tuh_audio_control_xfer(uint8_t idx, uint8_t entity_id, tusb_dir_t direction, uint8_t request,
                            uint8_t control_selector, uint8_t channel, void *buffer, uint16_t length,
                            tuh_xfer_cb_t complete_cb, uintptr_t user_data);

// Master mute and volume controls. Capability and range information is cached
// before tuh_audio_mount_cb() is invoked. Volume channel 0 selects the master;
// a SET falls back to writing every logical channel when the master is not
// writable and all logical channels advertise write access. The completion
// callback is invoked once after the entire operation. A nonzero volume
// channel directly selects that 1-based Feature Unit logical channel.
// Per-channel capability is not cached; an unsupported channel is reported by
// the control transfer.
//
// Volume SET accepts TUH_AUDIO_VOLUME_SILENCE or a value within the cached
// range; finite values are rounded to the nearest resolution step measured
// from the range minimum.
bool tuh_audio_mute_set(uint8_t idx, uint8_t stream_idx, bool mute, tuh_xfer_cb_t complete_cb, uintptr_t user_data);
bool tuh_audio_mute_get(uint8_t idx, uint8_t stream_idx, bool *mute, tuh_xfer_cb_t complete_cb, uintptr_t user_data);
bool tuh_audio_volume_set(uint8_t idx, uint8_t stream_idx, uint8_t channel, int16_t volume, tuh_xfer_cb_t complete_cb,
                          uintptr_t user_data);
bool tuh_audio_volume_get(uint8_t idx, uint8_t stream_idx, uint8_t channel, int16_t *volume, tuh_xfer_cb_t complete_cb,
                          uintptr_t user_data);

//--------------------------------------------------------------------+
// Synchronous control requests block until the transfer completes and return
// its result. actual_len may be NULL when the received length is not needed.
// Only use when audio streaming is stopped, otherwise the stream's isochronous
// transfers may be disrupted and creating audible artifacts !
//--------------------------------------------------------------------+
tusb_xfer_result_t tuh_audio_control_xfer_sync(uint8_t idx, uint8_t entity_id, tusb_dir_t direction, uint8_t request,
                                               uint8_t control_selector, uint8_t channel, void *buffer, uint16_t length,
                                               uint32_t *actual_len);

TU_ATTR_ALWAYS_INLINE static inline tusb_xfer_result_t tuh_audio_mute_set_sync(uint8_t idx, uint8_t stream_idx,
                                                                               bool mute) {
  TU_API_SYNC(tuh_audio_mute_set, idx, stream_idx, mute);
}

TU_ATTR_ALWAYS_INLINE static inline tusb_xfer_result_t tuh_audio_mute_get_sync(uint8_t idx, uint8_t stream_idx,
                                                                               bool *mute) {
  TU_API_SYNC(tuh_audio_mute_get, idx, stream_idx, mute);
}

TU_ATTR_ALWAYS_INLINE static inline tusb_xfer_result_t tuh_audio_volume_set_sync(uint8_t idx, uint8_t stream_idx,
                                                                                 uint8_t channel, int16_t volume) {
  TU_API_SYNC(tuh_audio_volume_set, idx, stream_idx, channel, volume);
}

TU_ATTR_ALWAYS_INLINE static inline tusb_xfer_result_t tuh_audio_volume_get_sync(uint8_t idx, uint8_t stream_idx,
                                                                                 uint8_t channel, int16_t *volume) {
  TU_API_SYNC(tuh_audio_volume_get, idx, stream_idx, channel, volume);
}

//--------------------------------------------------------------------+
// Callbacks (Weak is optional)
//--------------------------------------------------------------------+

// Invoked after the Audio Control and Streaming descriptors have been
// validated during enumeration, before tuh_audio_mount_cb(). The interface is
// not mounted yet and control requests must not be submitted from this
// callback. Applications may inspect or copy descriptors needed for later raw
// entity control requests.
void tuh_audio_descriptor_cb(uint8_t idx, const tuh_audio_descriptor_cb_t *desc_cb_data);

// Invoked when device with Audio interface is mounted
void tuh_audio_mount_cb(uint8_t idx);

// Invoked when device with Audio interface is un-mounted
void tuh_audio_umount_cb(uint8_t idx);

// Invoked when an isochronous IN transfer completes successfully: the
// received data is already queued into the stream's capture FIFO.
void tuh_audio_capture_cb(uint8_t idx, uint8_t stream_idx, uint16_t xferred_bytes);

// Invoked after a successful isochronous OUT transfer, before the next packet
// is prepared. After this callback returns, the driver submits queued audio
// from the stream FIFO, or silence when a complete packet is unavailable.
void tuh_audio_playback_cb(uint8_t idx, uint8_t stream_idx, uint16_t xferred_bytes);

// Reports completion of asynchronous start/stop operations and unrecoverable
// transfer failures. START_COMPLETE is emitted after the complete activation
// sequence and initial endpoint transfers are submitted. XFER_FAILED means the
// HCD could not submit a transfer or completed it unsuccessfully; it is not a
// notification for an individual dropped isochronous packet. The driver stops
// the stream before reporting START_COMPLETE failure or XFER_FAILED.
void tuh_audio_event_cb(uint8_t idx, uint8_t stream_idx, tuh_audio_event_t event, tusb_xfer_result_t result);

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

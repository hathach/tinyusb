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
// Maximum number of Audio interfaces per Audio device
#ifndef CFG_TUH_AUDIO_MAX
  #define CFG_TUH_AUDIO_MAX 1
#endif
// Maximum number of Audio Streaming interfaces per Audio device
#ifndef CFG_TUH_AUDIO_MAX_SAM_FREQ
  #define CFG_TUH_AUDIO_MAX_SAM_FREQ 5
#endif
// Maximum number of Audio Streaming interfaces per Audio device
#ifndef CFG_TUH_AUDIO_MAX_AS
  #define CFG_TUH_AUDIO_MAX_AS 4
#endif

//--------------------------------------------------------------------+
// AS Interface Info (per-interface independent storage)
//--------------------------------------------------------------------+
typedef struct {
  uint8_t  interface_num; // AS interface number
  uint8_t  alt_setting;   // Current alt setting
  uint8_t  ep_addr;       // Endpoint address
  uint16_t ep_size;       // Max packet size
  uint8_t  ep_dir;        // TUSB_DIR_IN or TUSB_DIR_OUT

  // Format info
  uint8_t  format_type;
  uint8_t  num_channels;
  uint8_t  sub_frame_size;
  uint8_t  bit_resolution;
  uint8_t  sam_freq_type;
  uint32_t sam_freq[CFG_TUH_AUDIO_MAX_SAM_FREQ];
  uint32_t sam_freq_lower;
  uint32_t sam_freq_upper;
} tuh_audio_as_info_t;

#ifndef CFG_TUH_AUDIO_EPIN_BUFSIZE
  #define CFG_TUH_AUDIO_EPIN_BUFSIZE 192
#endif

#ifndef CFG_TUH_AUDIO_EPOUT_BUFSIZE
  #define CFG_TUH_AUDIO_EPOUT_BUFSIZE 192
#endif

//--------------------------------------------------------------------+
// Application API
//--------------------------------------------------------------------+

// Check if Audio interface is mounted
bool tuh_audio_mounted(uint8_t idx);
// Get device address of Audio interface
uint8_t tuh_audio_get_dev_addr(uint8_t idx);
// Get Feature Unit ID
uint8_t tuh_audio_get_feature_unit_id(uint8_t idx);
// Get Interface index from device address + interface number
// return TUSB_INDEX_INVALID_8 (0xFF) if not found
uint8_t tuh_audio_itf_get_index(uint8_t daddr, uint8_t itf_num);

// Get Interface information
// return true if index is correct and interface is currently mounted
bool tuh_audio_itf_get_info(uint8_t idx, tuh_itf_info_t *info);

// Get number of AS interfaces for an audio device
uint8_t tuh_audio_as_get_count(uint8_t idx);

// Get AS interface info by index
// as_idx: 0 to (as_count - 1)
bool tuh_audio_as_get_info(uint8_t idx, uint8_t as_idx, tuh_audio_as_info_t *info);

// Set Audio Streaming interface alternate setting (to enable/disable endpoints)
bool tuh_audio_set_interface(uint8_t daddr, uint8_t itf_num, uint8_t alt_setting, tuh_xfer_cb_t complete_cb,
                             uintptr_t user_data);

//--------------------------------------------------------------------+
// Control Endpoint API
//--------------------------------------------------------------------+

// Set current sampling frequency on an isochronous endpoint (UAC 1.0)
// Sampling frequency is 3 bytes little-endian
// In multi-AS scenarios, pass the endpoint address from tuh_audio_as_get_info().
bool tuh_audio_set_sampling_freq(uint8_t idx, uint8_t as_idx, uint32_t sampling_freq, tuh_xfer_cb_t complete_cb,
                                 uintptr_t user_data);

// Get current sampling frequency from an isochronous endpoint (UAC 1.0)
// In multi-AS scenarios, pass the endpoint address from tuh_audio_as_get_info().
bool tuh_audio_get_sampling_freq(uint8_t idx, uint8_t as_idx, uint32_t *sampling_freq, tuh_xfer_cb_t complete_cb,
                                 uintptr_t user_data);

// Set current/mute/volume etc. for a feature unit (UAC 1.0)
bool tuh_audio_feature_unit_set(uint8_t idx, uint8_t control_selector, uint8_t channel, uint16_t value,
                                tuh_xfer_cb_t complete_cb, uintptr_t user_data);

// Get current/mute/volume etc. from a feature unit (UAC 1.0)
bool tuh_audio_feature_unit_get(uint8_t idx, uint8_t control_selector, uint8_t channel, uint16_t *value,
                                tuh_xfer_cb_t complete_cb, uintptr_t user_data);

//--------------------------------------------------------------------+
// Control Request Sync API
// Each Function will make a USB control transfer request to/from device the function will block until request is
// complete. The function will return the transfer request result
//--------------------------------------------------------------------+
TU_ATTR_ALWAYS_INLINE static inline tusb_xfer_result_t tuh_audio_get_sampling_freq_sync(uint8_t idx, uint8_t as_idx,
                                                                                        uint32_t *sampling_freq) {
  TU_API_SYNC(tuh_audio_get_sampling_freq, idx, as_idx, sampling_freq);
}

TU_ATTR_ALWAYS_INLINE static inline tusb_xfer_result_t tuh_audio_set_sampling_freq_sync(uint8_t idx, uint8_t as_idx,
                                                                                        uint32_t sampling_freq) {
  TU_API_SYNC(tuh_audio_set_sampling_freq, idx, as_idx, sampling_freq);
}

TU_ATTR_ALWAYS_INLINE static inline tusb_xfer_result_t
tuh_audio_feature_unit_set_sync(uint8_t idx, uint8_t control_selector, uint8_t channel, uint16_t value) {
  TU_API_SYNC(tuh_audio_feature_unit_set, idx, control_selector, channel, value);
}

TU_ATTR_ALWAYS_INLINE static inline tusb_xfer_result_t
tuh_audio_feature_unit_get_sync(uint8_t idx, uint8_t control_selector, uint8_t channel, uint16_t *value) {
  TU_API_SYNC(tuh_audio_feature_unit_get, idx, control_selector, channel, value);
}

//--------------------------------------------------------------------+
// Interrupt/Isochronous Endpoint API
//--------------------------------------------------------------------+

// Submit an isochronous transfer to receive audio data from a default IN endpoint.
// In multi-AS scenarios, endpoint selection is implementation-defined default behavior.
// Use tuh_audio_as_get_info() when application needs explicit per-AS endpoint control.
bool tuh_audio_receive(uint8_t idx, uint8_t as_idx, uint8_t *buffer, uint16_t len);

// Submit an isochronous transfer to send audio data to a default OUT endpoint.
// In multi-AS scenarios, endpoint selection is implementation-defined default behavior.
// Use tuh_audio_as_get_info() when application needs explicit per-AS endpoint control.
bool tuh_audio_send(uint8_t idx, uint8_t as_idx, uint8_t *buffer, uint16_t len);

//--------------------------------------------------------------------+
// Callbacks (Weak is optional)
//--------------------------------------------------------------------+

// Invoked when device with Audio interface is mounted
void tuh_audio_mount_cb(uint8_t idx);

// Invoked when device with Audio interface is un-mounted
void tuh_audio_umount_cb(uint8_t idx);

// Invoked when an isochronous IN transfer is complete
void tuh_audio_rx_cb(uint8_t dev_addr, uint8_t ep_addr, uint16_t xferred_bytes);

// Invoked when an isochronous OUT transfer is complete
void tuh_audio_tx_cb(uint8_t dev_addr, uint8_t ep_addr, uint16_t xferred_bytes);

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

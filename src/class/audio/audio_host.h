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
#ifndef CFG_TUH_AUDIO_MAX
  #define CFG_TUH_AUDIO_MAX 1
#endif

#ifndef CFG_TUH_AUDIO_MAX_SAM_FREQ
  #define CFG_TUH_AUDIO_MAX_SAM_FREQ 5
#endif

#ifndef CFG_TUH_AUDIO_MAX_AS
  #define CFG_TUH_AUDIO_MAX_AS 4
#endif

//--------------------------------------------------------------------+
// AS Interface Info (per-interface independent storage)
//--------------------------------------------------------------------+
typedef struct {
  uint8_t interface_num;      // AS interface number
  uint8_t alt_setting;        // Current alt setting
  uint8_t ep_addr;            // Endpoint address
  uint16_t ep_size;           // Max packet size
  uint8_t ep_dir;             // TUSB_DIR_IN or TUSB_DIR_OUT

  // Format info
  uint8_t format_type;
  uint8_t num_channels;
  uint8_t sub_frame_size;
  uint8_t bit_resolution;
  uint8_t sam_freq_type;
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
// Descriptor Information
//--------------------------------------------------------------------+
// Information about parsed UAC 1.0 descriptors passed to the application
// during enumeration (via tuh_audio_descriptor_cb)
typedef struct {
  // Audio Control Interface descriptor
  const tusb_desc_interface_t *desc_ac_interface;

  // Audio Streaming Interface descriptor (alt setting 0)
  const tusb_desc_interface_t *desc_as_interface;

  // Audio Streaming Interface alt setting (with endpoints)
  const tusb_desc_interface_t *desc_as_interface_alt;

  // Format Type descriptor
  const uint8_t *desc_format_type;

  // Class-Specific AS Interface (AS General) descriptor
  const uint8_t *desc_cs_as_general;

  // Standard Isochronous Endpoint descriptor (IN)
  const tusb_desc_endpoint_t *desc_ep_in;

  // Standard Isochronous Endpoint descriptor (OUT)
  const tusb_desc_endpoint_t *desc_ep_out;

  // Audio function information
  uint8_t ac_interface_num;     // Audio Control interface number
  uint8_t as_interface_num;       // Audio Streaming interface number
  uint8_t alt_setting;          // Current alt setting with endpoints
} tuh_audio_descriptor_cb_t;

typedef struct {
  uint8_t daddr;
  uint8_t bInterfaceNumber;
  uint8_t bAltSetting;

  // Terminal info (from Audio Control Interface)
  uint16_t input_terminal_type;         // wTerminalType of Input Terminal (0x0201 = Mic, etc.)
  uint8_t input_terminal_id;            // bTerminalID of Input Terminal
  uint8_t input_terminal_channels;      // bNrChannels of Input Terminal
  uint16_t output_terminal_type;        // wTerminalType of Output Terminal (0x0301 = Speaker, etc.)
  uint8_t output_terminal_id;           // bTerminalID of Output Terminal

  // Feature Unit info
  uint8_t feature_unit_id;              // bUnitID of Feature Unit (0 = none)
  uint8_t feature_unit_source_id;       // bSourceID of Feature Unit

  // Endpoint info
  uint8_t ep_in;
  uint8_t ep_out;
  uint16_t ep_in_size;
  uint16_t ep_out_size;

  // Multi-AS support (per AS interface independent storage)
  uint8_t as_count;
  tuh_audio_as_info_t as_info[CFG_TUH_AUDIO_MAX_AS];
} tuh_audio_mount_cb_t;

//--------------------------------------------------------------------+
// Application API
//--------------------------------------------------------------------+

// Check if Audio interface is mounted
bool tuh_audio_mounted(uint8_t idx);

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
bool tuh_audio_set_interface(uint8_t daddr, uint8_t itf_num, uint8_t alt_setting,
                              tuh_xfer_cb_t complete_cb, uintptr_t user_data);

//--------------------------------------------------------------------+
// Control Endpoint API
//--------------------------------------------------------------------+

// Set current sampling frequency on an isochronous endpoint (UAC 1.0)
// Sampling frequency is 3 bytes little-endian
bool tuh_audio_set_sampling_freq(uint8_t daddr, uint8_t ep_addr, uint32_t sampling_freq,
                                  tuh_xfer_cb_t complete_cb, uintptr_t user_data);

// Get current sampling frequency from an isochronous endpoint (UAC 1.0)
bool tuh_audio_get_sampling_freq(uint8_t daddr, uint8_t ep_addr, uint32_t *sampling_freq,
                                  tuh_xfer_cb_t complete_cb, uintptr_t user_data);

// Set current/mute/volume etc. for a feature unit (UAC 1.0)
bool tuh_audio_feature_unit_set(uint8_t daddr, uint8_t itf_num, uint8_t unit_id,
                                 uint8_t control_selector, uint8_t channel,
                                 uint16_t value, tuh_xfer_cb_t complete_cb, uintptr_t user_data);

// Get current/mute/volume etc. from a feature unit (UAC 1.0)
bool tuh_audio_feature_unit_get(uint8_t daddr, uint8_t itf_num, uint8_t unit_id,
                                 uint8_t control_selector, uint8_t channel,
                                 void *buffer, uint8_t len,
                                 tuh_xfer_cb_t complete_cb, uintptr_t user_data);

//--------------------------------------------------------------------+
// Interrupt/Isochronous Endpoint API
//--------------------------------------------------------------------+

// Submit an isochronous transfer to receive audio data from IN endpoint
bool tuh_audio_receive(uint8_t daddr, uint8_t idx, uint8_t *buffer, uint16_t len);

// Submit an isochronous transfer to send audio data to OUT endpoint
bool tuh_audio_send(uint8_t daddr, uint8_t idx, uint8_t *buffer, uint16_t len);

//--------------------------------------------------------------------+
// Callbacks (Weak is optional)
//--------------------------------------------------------------------+

// Invoked when Audio interface descriptor is detected during enumeration.
// Application can copy/parse descriptor if needed.
// Note: may be fired before tuh_audio_mount_cb(), therefore audio interface is not mounted/ready.
void tuh_audio_descriptor_cb(uint8_t idx, const tuh_audio_descriptor_cb_t *desc_cb_data);

// Invoked when device with Audio interface is mounted
void tuh_audio_mount_cb(uint8_t idx, const tuh_audio_mount_cb_t *mount_cb_data);

// Invoked when device with Audio interface is un-mounted
void tuh_audio_umount_cb(uint8_t idx);

// Invoked when an isochronous IN transfer is complete
void tuh_audio_rx_cb(uint8_t idx, uint8_t ep_addr, uint16_t xferred_bytes);

// Invoked when an isochronous OUT transfer is complete
void tuh_audio_tx_cb(uint8_t idx, uint8_t ep_addr, uint16_t xferred_bytes);

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

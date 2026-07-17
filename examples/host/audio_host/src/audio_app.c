/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2025 TinyUSB contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 */

#include <stdio.h>
#include "bsp/board_api.h"
#include "tusb.h"
#include "app.h"

//--------------------------------------------------------------------+
// MACRO TYPEDEF CONSTANT ENUM DECLARATION
//--------------------------------------------------------------------+

static bool          audio_mounted         = false;
static uint8_t       audio_dev_addr        = 0;
static uint8_t       audio_idx             = 0;
static uint8_t       audio_ep_in           = 0;
static uint8_t       audio_ep_out          = 0;
static uint32_t      sampling_freq         = 48000; // Default sampling frequency (Hz)
static uint8_t       audio_mic_channels    = 1;
static volatile bool audio_rx_busy         = false; // Track IN endpoint transfer state
static volatile bool audio_tx_busy         = false; // Track OUT endpoint transfer state
static volatile bool audio_ready           = false; // Wait for sampling freq set before starting isochronous transfer
static uint8_t       audio_ac_itf          = 0;     // Audio Control interface number
static uint8_t       audio_feature_unit_id = 0;     // Feature Unit ID
static uint32_t      audio_receive_total_bytes = 0; // Total bytes received


static uint8_t audio_rx_buffer[CFG_TUH_AUDIO_EPIN_BUFSIZE] __attribute__((aligned(4)));
static uint8_t audio_tx_buffer[CFG_TUH_AUDIO_EPOUT_BUFSIZE] __attribute__((aligned(4)));


//--------------------------------------------------------------------+
// Blinking Task
//--------------------------------------------------------------------+
void led_blinking_task(void) {
  const uint32_t  interval_ms = 1000;
  static uint32_t start_ms    = 0;

  static bool led_state = false;

  // Blink every interval ms
  if (tusb_time_millis_api() - start_ms < interval_ms) {
    return; // not enough time
  }
  start_ms += interval_ms;

  board_led_write(led_state);
  led_state = 1 - led_state;     // toggle
  printf("Microphone total bytes received: %lu bps\r\n", audio_receive_total_bytes*8 );
  audio_receive_total_bytes = 0; // Reset total bytes received after printing
}


//--------------------------------------------------------------------+
// Helper Functions
//--------------------------------------------------------------------+

// Mono (96 bytes, 48 samples) -> Stereo (192 bytes)
static void mono_to_stereo(const uint8_t *mono, uint8_t *stereo, uint16_t mono_samples) {
  for (uint16_t i = 0; i < mono_samples; i++) {
    // Copy 2 bytes (one int16 sample) to left channel
    stereo[i * 4]     = mono[i * 2];
    stereo[i * 4 + 1] = mono[i * 2 + 1];
    // Copy same 2 bytes to right channel
    stereo[i * 4 + 2] = mono[i * 2];
    stereo[i * 4 + 3] = mono[i * 2 + 1];
  }
}

// Print sampling frequency info for an AS interface
static void print_sampling_freq(const tuh_audio_as_info_t *as) {
  if (as->sam_freq_type == 0) {
    printf("    Sampling Freq: Continuous range %lu Hz - %lu Hz\r\n", (unsigned long)as->sam_freq_lower,
           (unsigned long)as->sam_freq_upper);
  } else {
    printf("    Sampling Freq: Discrete, count=%u\r\n", as->sam_freq_type);
    for (uint8_t j = 0; j < as->sam_freq_type && j < CFG_TUH_AUDIO_MAX_SAM_FREQ; j++) {
      printf("      Freq[%u]: %lu Hz\r\n", j, (unsigned long)as->sam_freq[j]);
    }
  }
}

// Print all AS interface info
static void print_as_interfaces(const tuh_audio_mount_cb_t *mount_cb_data) {
  for (uint8_t i = 0; i < mount_cb_data->as_count; i++) {
    const tuh_audio_as_info_t *as = &mount_cb_data->as_info[i];
    if (as->ep_dir == TUSB_DIR_IN) {
      // Save microphone channel count for mono-to-stereo conversion
      audio_mic_channels = as->num_channels;
      printf("  --- Microphone (AS %u) ---\r\n", i);
      printf("    IN EP: 0x%02x (max size: %u)\r\n", as->ep_addr, as->ep_size);
    } else {
      printf("  --- Speaker (AS %u) ---\r\n", i);
      printf("    OUT EP: 0x%02x (max size: %u)\r\n", as->ep_addr, as->ep_size);
    }
    printf("    Interface: %u, Alt: %u\r\n", as->interface_num, as->alt_setting);
    printf("    Format Type: %u, Channels: %u, SubFrameSize: %u, BitResolution: %u\r\n", as->format_type,
           as->num_channels, as->sub_frame_size, as->bit_resolution);
    print_sampling_freq(as);
  }
}

// Find IN and OUT endpoints from AS interfaces, return IN sampling freq
static uint32_t find_audio_endpoints(const tuh_audio_mount_cb_t *mount_cb_data) {
  uint32_t in_sam_freq = 0;
  audio_ep_in          = 0;
  audio_ep_out         = 0;

  for (uint8_t i = 0; i < mount_cb_data->as_count; i++) {
    const tuh_audio_as_info_t *as = &mount_cb_data->as_info[i];
    if (as->ep_dir == TUSB_DIR_IN) {
      audio_ep_in = as->ep_addr;
      if (as->sam_freq_type > 0) {
        in_sam_freq = as->sam_freq[as->sam_freq_type - 1];
      }
    } else {
      audio_ep_out = as->ep_addr;
    }
  }
  return in_sam_freq;
}

// Set Feature Unit volume to un-mute
static void set_feature_unit_volume(void) {
  if (audio_feature_unit_id == 0) {
    return;
  }
  printf("  Setting Feature Unit %u volume to 0x0600\r\n", audio_feature_unit_id);
  tuh_audio_feature_unit_set(audio_dev_addr, audio_ac_itf, audio_feature_unit_id, AUDIO10_FU_CTRL_VOLUME, 0, 0x0600,
                             NULL, 0);
}

//--------------------------------------------------------------------+
// Application Task
//--------------------------------------------------------------------+
void audio_app_task(void) {
  if (!audio_mounted || !audio_ready) {
    return;
  }

  if (!audio_rx_busy) {
    if (tuh_audio_receive(audio_dev_addr, audio_idx, audio_rx_buffer, CFG_TUH_AUDIO_EPIN_BUFSIZE)) {
      audio_rx_busy = true;
    }
  }
}

//--------------------------------------------------------------------+
// TinyUSB Callbacks
//--------------------------------------------------------------------+


// Callback after IN sampling frequency is set
static void in_sampling_freq_set_cb(tuh_xfer_t *xfer) {
  if (xfer->result != XFER_RESULT_SUCCESS) {
    printf("  Sampling frequency set FAILED: result=%u\r\n", xfer->result);
    return;
  }
  printf("  Sampling frequency set OK, ready for isochronous transfer\r\n");
  // Set Feature Unit volume to un-mute
  set_feature_unit_volume();
  // Set OUT sampling frequency then send empty packet to kick-start device
  tuh_audio_set_sampling_freq(audio_dev_addr, audio_ep_out, sampling_freq, NULL, 0);
  audio_ready = true;
}

void tuh_audio_mount_cb(uint8_t idx, const tuh_audio_mount_cb_t *mount_cb_data) {
  if (!mount_cb_data) {
    return;
  }
  audio_receive_total_bytes = 0; // Reset total bytes received on new mount
  printf("Audio device mounted: idx=%u, daddr=%u, AS count=%u\r\n", idx, mount_cb_data->daddr, mount_cb_data->as_count);

  print_as_interfaces(mount_cb_data);

  // Feature Unit
  if (mount_cb_data->feature_unit_id != 0) {
    printf("  Feature Unit: ID=%u, SourceID=%u\r\n", mount_cb_data->feature_unit_id,
           mount_cb_data->feature_unit_source_id);
  }

  // Save device info
  audio_dev_addr        = mount_cb_data->daddr;
  audio_idx             = idx;
  audio_mounted         = true;
  audio_ac_itf          = mount_cb_data->bInterfaceNumber;
  audio_feature_unit_id = mount_cb_data->feature_unit_id;

  // Find endpoints and IN sampling frequency
  uint32_t in_sam_freq = find_audio_endpoints(mount_cb_data);

  // Set IN sampling frequency before starting isochronous transfer
  if (audio_ep_in != 0 && in_sam_freq != 0) {
    sampling_freq = in_sam_freq;
    printf("  Setting IN sampling frequency to %lu Hz\r\n", (unsigned long)sampling_freq);
    tuh_audio_set_sampling_freq(mount_cb_data->daddr, audio_ep_in, sampling_freq, in_sampling_freq_set_cb, 0);
  }
}

// Invoked when device with Audio interface is un-mounted
void tuh_audio_umount_cb(uint8_t idx) {
  printf("Audio device unmounted: idx=%u\r\n", idx);
  if (audio_mounted && audio_idx == idx) {
    audio_mounted  = false;
    audio_ready    = false;
    audio_rx_busy  = false;
    audio_tx_busy  = false;
    audio_dev_addr = 0;
    audio_idx      = 0;
  }
}

// Invoked when an isochronous IN transfer is complete
void tuh_audio_rx_cb(uint8_t idx, uint8_t ep_addr, uint16_t xferred_bytes) {
  (void)idx;
  (void)ep_addr;
  audio_rx_busy = false;
  audio_receive_total_bytes += xferred_bytes; // Update total bytes received
  if (xferred_bytes > 0 && audio_ep_out != 0 && !audio_tx_busy) {
    bool ok;
    if (audio_mic_channels == 1) {
      // Mono microphone, convert to stereo and send to OUT endpoint
      uint16_t samples = xferred_bytes / 2;
      mono_to_stereo(audio_rx_buffer, audio_tx_buffer, samples);
      ok = tuh_audio_send(audio_dev_addr, audio_idx, audio_tx_buffer, xferred_bytes * 2);
    } else {
      // Stereo microphone, send directly to OUT endpoint
      ok = tuh_audio_send(audio_dev_addr, audio_idx, audio_rx_buffer, xferred_bytes);
    }

    if (ok) {
      audio_tx_busy = true;
    }
  }
}

// Invoked when an isochronous OUT transfer is complete
void tuh_audio_tx_cb(uint8_t idx, uint8_t ep_addr, uint16_t xferred_bytes) {
  (void)idx;
  (void)ep_addr;
  (void)xferred_bytes;
  audio_tx_busy = false;
}

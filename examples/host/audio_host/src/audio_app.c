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

static bool          audio_mounted       = false;
static uint8_t       audio_dev_addr      = 0xFF;
static volatile bool audio_ready         = false; // Wait for sampling freq set before starting isochronous transfer
static volatile bool audio_rx_busy       = false; // Track IN endpoint transfer state
static volatile bool audio_tx_busy       = false; // Track OUT endpoint transfer state
static uint8_t       audio_idx           = 0xFF;
static uint8_t       audiostream_in_idx  = 0xFF;
static uint8_t       audiostream_out_idx = 0xFF;
static uint32_t      sampling_freq       = 48000; // Default sampling frequency (Hz)
static uint8_t       audio_mic_channels  = 1;

static uint8_t audio_rx_buffer[CFG_TUH_AUDIO_EPIN_BUFSIZE] __attribute__((aligned(4)));
static uint8_t audio_tx_buffer[CFG_TUH_AUDIO_EPOUT_BUFSIZE] __attribute__((aligned(4)));

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
static void print_as_interfaces(uint8_t idx) {
  tuh_audio_as_info_t as       = {};
  uint8_t             as_count = tuh_audio_as_get_count(idx);
  for (uint8_t i = 0; i < as_count; i++) {
    tuh_audio_as_get_info(idx, i, &as);
    if (as.ep_dir == TUSB_DIR_IN) {
      // Save microphone channel count for mono-to-stereo conversion
      audio_mic_channels = as.num_channels;
      printf("  --- Microphone (AS %u) ---\r\n", i);
      printf("    IN EP: 0x%02x (max size: %u)\r\n", as.ep_addr, as.ep_size);
    } else {
      printf("  --- Speaker (AS %u) ---\r\n", i);
      printf("    OUT EP: 0x%02x (max size: %u)\r\n", as.ep_addr, as.ep_size);
    }
    printf("    Interface: %u, Alt: %u\r\n", as.interface_num, as.alt_setting);
    printf("    Format Type: %u, Channels: %u, SubFrameSize: %u, BitResolution: %u\r\n", as.format_type,
           as.num_channels, as.sub_frame_size, as.bit_resolution);
    print_sampling_freq(&as);
  }
}

//--------------------------------------------------------------------+
// Application Task
//--------------------------------------------------------------------+
void audio_app_task(void) {
  if (!audio_mounted || !audio_ready) {
    return;
  }

  if (!audio_rx_busy) {
    if (tuh_audio_receive(audio_idx, audiostream_in_idx, audio_rx_buffer, CFG_TUH_AUDIO_EPIN_BUFSIZE)) {
      audio_rx_busy = true;
    }
  }
}

//--------------------------------------------------------------------+
// TinyUSB Callbacks
//--------------------------------------------------------------------+


void tuh_audio_mount_cb(uint8_t idx) {
  if (idx >= CFG_TUH_AUDIO_MAX) {
    printf("Audio device mount failed: idx=%u exceeds max=%u\r\n", idx, CFG_TUH_AUDIO_MAX);
    return;
  }

  print_as_interfaces(idx);

  // Save device info
  audio_dev_addr = tuh_audio_get_dev_addr(idx);
  audio_idx      = idx;
  audio_mounted  = true;

  // Find endpoints and IN sampling frequency
  tuh_audio_as_info_t as;
  for (uint8_t i = 0; i < tuh_audio_as_get_count(idx); i++) {

    tuh_audio_as_get_info(idx, i, &as);
    if (as.ep_dir == TUSB_DIR_IN) {
      audiostream_in_idx = i;
      if (as.sam_freq_type > 0) {
        sampling_freq = as.sam_freq[0];
      }
    } else {
      audiostream_out_idx = i;
    }
  }

  // Set IN sampling frequency before starting isochronous transfer
  if (audiostream_in_idx != 0xFF && sampling_freq != 0) {
    printf("  Setting IN sampling frequency to %lu Hz\r\n", (unsigned long)sampling_freq);
    // tuh_audio_set_sampling_freq(audio_idx, audiostream_in_idx, sampling_freq, in_sampling_freq_set_cb, 0);

    tusb_xfer_result_t result;
    result = tuh_audio_set_sampling_freq_sync(audio_idx, audiostream_in_idx, sampling_freq);
    if (result == XFER_RESULT_SUCCESS) {
      tuh_audio_get_sampling_freq_sync(audio_idx, audiostream_in_idx, &sampling_freq);
      printf("  IN sampling frequency set to %lu Hz\r\n", (unsigned long)sampling_freq);
      if (audiostream_out_idx != 0xFF) {
        printf("  Setting OUT sampling frequency to %lu Hz\r\n", (unsigned long)sampling_freq);
        result = tuh_audio_set_sampling_freq_sync(audio_idx, audiostream_out_idx, sampling_freq);
        if (result == XFER_RESULT_SUCCESS) {
          tuh_audio_get_sampling_freq_sync(audio_idx, audiostream_out_idx, &sampling_freq);
          printf("  OUT sampling frequency set to %lu Hz\r\n", (unsigned long)sampling_freq);
        } else {
          printf("  Setting OUT sampling frequency FAILED: result=%u\r\n", result);
        }
      }
    } else {
      printf("  Setting IN sampling frequency FAILED: result=%u\r\n", result);
    }
    uint16_t volume = 0x0600;

    result = tuh_audio_feature_unit_set_sync(audio_idx, AUDIO10_FU_CTRL_VOLUME, 0, volume);
    if (result == XFER_RESULT_SUCCESS) {
      printf("  Feature Unit volume set:volume 0x%04x\r\n", (unsigned int)volume);
      tuh_audio_feature_unit_get_sync(audio_idx, AUDIO10_FU_CTRL_VOLUME, 0, &volume);
      printf("  Feature Unit volume get: 0x%04x\r\n", (unsigned int)volume);
    } else {
      printf("  Setting Feature Unit volume FAILED: result=%u\r\n", result);
    }
  }
  audio_ready = true;
}

// Invoked when device with Audio interface is un-mounted
void tuh_audio_umount_cb(uint8_t idx) {
  printf("Audio device unmounted: idx=%u\r\n", idx);
  if (audio_mounted && audio_idx == idx) {
    audio_mounted       = false;
    audio_ready         = false;
    audio_rx_busy       = false;
    audio_tx_busy       = false;
    audio_dev_addr      = 0;
    audio_idx           = 0;
    audiostream_in_idx  = 0xFF;
    audiostream_out_idx = 0xFF;
  }
}

// Invoked when an isochronous IN transfer is complete
void tuh_audio_rx_cb(uint8_t dev_addr, uint8_t ep_addr, uint16_t xferred_bytes) {
  (void)dev_addr;
  (void)ep_addr;
  audio_rx_busy = false;

  if (xferred_bytes > 0 && audiostream_out_idx != 0xFF && !audio_tx_busy) {
    bool ok;
    if (audio_mic_channels == 1) {
      // Mono microphone, convert to stereo and send to OUT endpoint
      uint16_t samples = xferred_bytes / 2;
      mono_to_stereo(audio_rx_buffer, audio_tx_buffer, samples);
      ok = tuh_audio_send(audio_idx, audiostream_out_idx, audio_tx_buffer, xferred_bytes * 2);
    } else {
      // Stereo microphone, send directly to OUT endpoint
      ok = tuh_audio_send(audio_idx, audiostream_out_idx, audio_rx_buffer, xferred_bytes);
    }

    if (ok) {
      audio_tx_busy = true;
    }
  }
}

// Invoked when an isochronous OUT transfer is complete
void tuh_audio_tx_cb(uint8_t dev_addr, uint8_t ep_addr, uint16_t xferred_bytes) {
  (void)dev_addr;
  (void)ep_addr;
  (void)xferred_bytes;
  audio_tx_busy = false;
}

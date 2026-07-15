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

static bool          audio_mounted  = false;
static uint8_t       audio_dev_addr = 0;
static uint8_t       audio_idx      = 0;
static volatile bool audio_rx_busy  = false; // Track IN endpoint transfer state
static volatile bool audio_tx_busy  = false; // Track OUT endpoint transfer state

static uint8_t audio_rx_buffer[CFG_TUH_AUDIO_EPIN_BUFSIZE];
void           audio_app_task(void) {
  if (!audio_mounted) {
    return;
  }

  // Request to receive audio data from IN endpoint only when previous transfer is complete
  // tuh_audio_rx_cb() will be invoked when data is received
  if (!audio_rx_busy) {
    if (tuh_audio_receive(audio_dev_addr, audio_idx, audio_rx_buffer, CFG_TUH_AUDIO_EPIN_BUFSIZE)) {
      audio_rx_busy = true;
    }
  }
}

//--------------------------------------------------------------------+
// TinyUSB Callbacks
//--------------------------------------------------------------------+

// Helper to print format info
static void print_format_info(const tuh_audio_mount_cb_t *data) {
  printf("    Format Type: %u, Channels: %u, SubFrameSize: %u, BitResolution: %u\r\n", data->format_type,
         data->num_channels, data->sub_frame_size, data->bit_resolution);

  if (data->sam_freq_type == 0) {
    printf("    Sampling Freq: Continuous range %lu Hz - %lu Hz\r\n", (unsigned long)data->sam_freq_lower,
           (unsigned long)data->sam_freq_upper);
  } else {
    printf("    Sampling Freq: Discrete, count=%u\r\n", data->sam_freq_type);
    for (uint8_t i = 0; i < data->sam_freq_type && i < CFG_TUH_AUDIO_MAX_SAM_FREQ; i++) {
      printf("      Freq[%u]: %lu Hz\r\n", i, (unsigned long)data->sam_freq[i]);
    }
  }
}

// Invoked when device with Audio interface is mounted
void tuh_audio_mount_cb(uint8_t idx, const tuh_audio_mount_cb_t *mount_cb_data) {
  printf("Audio device mounted: idx=%u, daddr=%u\r\n", idx, mount_cb_data->daddr);

  // --- Microphone (IN endpoint) ---
  if (mount_cb_data->ep_in != 0) {
    printf("  --- Microphone ---\r\n");
    printf("    IN EP: 0x%02x (max size: %u)\r\n", mount_cb_data->ep_in, mount_cb_data->ep_in_size);
    if (mount_cb_data->input_terminal_type != 0) {
      printf("    Input Terminal: ID=%u, Type=0x%04x, Channels=%u\r\n", mount_cb_data->input_terminal_id,
             mount_cb_data->input_terminal_type, mount_cb_data->input_terminal_channels);
    }
    print_format_info(mount_cb_data);
  }

  // --- Speaker (OUT endpoint) ---
  if (mount_cb_data->ep_out != 0) {
    printf("  --- Speaker ---\r\n");
    printf("    OUT EP: 0x%02x (max size: %u)\r\n", mount_cb_data->ep_out, mount_cb_data->ep_out_size);
    if (mount_cb_data->output_terminal_type != 0) {
      printf("    Output Terminal: ID=%u, Type=0x%04x\r\n", mount_cb_data->output_terminal_id,
             mount_cb_data->output_terminal_type);
    }
    print_format_info(mount_cb_data);
  }

  // Feature Unit (shared control)
  if (mount_cb_data->feature_unit_id != 0) {
    printf("  Feature Unit: ID=%u, SourceID=%u\r\n", mount_cb_data->feature_unit_id,
           mount_cb_data->feature_unit_source_id);
  }

  audio_dev_addr = mount_cb_data->daddr;
  audio_idx      = idx;
  audio_mounted  = true;

  // Set sampling frequency to 48kHz
  if (mount_cb_data->ep_in != 0) {
    tuh_audio_set_sampling_freq(mount_cb_data->daddr, mount_cb_data->ep_in, 48000, NULL, 0);
    printf("  Set sampling frequency to 48000 Hz\r\n");
  }
}

// Invoked when device with Audio interface is un-mounted
void tuh_audio_umount_cb(uint8_t idx) {
  printf("Audio device unmounted: idx=%u\r\n", idx);
  if (audio_mounted && audio_idx == idx) {
    audio_mounted  = false;
    audio_dev_addr = 0;
    audio_idx      = 0;
  }
}

// Invoked when an isochronous IN transfer is complete
void tuh_audio_rx_cb(uint8_t idx, uint8_t ep_addr, uint16_t xferred_bytes) {
  (void)idx;
  (void)ep_addr;
  (void)xferred_bytes;
  audio_rx_busy = false; // Mark IN endpoint as ready for next transfer
  tuh_audio_send(audio_dev_addr, audio_idx, audio_rx_buffer, CFG_TUH_AUDIO_EPOUT_BUFSIZE);
}

// Invoked when an isochronous OUT transfer is complete
void tuh_audio_tx_cb(uint8_t idx, uint8_t ep_addr, uint16_t xferred_bytes) {
  (void)idx;
  (void)ep_addr;
  (void)xferred_bytes;
}

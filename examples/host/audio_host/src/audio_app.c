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
#include <string.h>
#include "bsp/board_api.h"
#include "tusb.h"
#include "app.h"

//--------------------------------------------------------------------+
// MACRO TYPEDEF CONSTANT ENUM DECLARATION
//--------------------------------------------------------------------+

// Default configuration of this example, adjust to the target device:
// - AUDIO_MAX_FRAME_COUNT: buffer holds up to 48 frames (1 ms of 48 kHz)
// - AUDIO_MAX_CHANNELS: maximum channels of the capture/playback stream
// - SAMPLE_RATES: sample rates tried in order, first match wins (44.1 kHz stereo by default)
#define AUDIO_MAX_FRAME_COUNT 48
#define AUDIO_MAX_CHANNELS    2
#define SAMPLE_RATES          {48000, 44100}
// UAC1 volume values are signed 1/256 dB; 0x0600 selects +6 dB.
#define FEATURE_UNIT_VOLUME 0x0600
static uint8_t                   audio_idx      = TUSB_INDEX_INVALID_8; // index of the selected audio device
static uint8_t                   cap_stream_idx = TUSB_INDEX_INVALID_8; // capture stream index
static uint8_t                   spk_stream_idx = TUSB_INDEX_INVALID_8; // playback stream index
static bool                      mic_ready      = false;                // capture stream is running
static bool                      spk_ready      = false;                // playback stream is running
static int16_t                   mic_samples[AUDIO_MAX_FRAME_COUNT * AUDIO_MAX_CHANNELS]; // capture FIFO read buffer
static int16_t                   spk_samples[AUDIO_MAX_FRAME_COUNT * AUDIO_MAX_CHANNELS]; // playback FIFO write buffer
static tuh_audio_stream_config_t mic_config;                                // selected capture configuration
static tuh_audio_stream_config_t spk_config;                                // selected playback configuration
static uint32_t                  audio_frame_count = AUDIO_MAX_FRAME_COUNT; // frames per ms of the selected rate
static uint32_t                  spk_cb_count      = 0;                     // count of playback callbacks (for debug)
static uint32_t                  mic_cb_count      = 0;                     // count of capture callbacks (for debug)
static uint32_t                  err_cb_count      = 0;                     // count of error callbacks (for debug)


//--------------------------------------------------------------------+
// Helper Functions
//--------------------------------------------------------------------+

//--------------------------------------------------------------------+
// Async Deferred Call Queue
//--------------------------------------------------------------------+
// Schedules one-shot callbacks to be invoked after a given delay in ms.
// Processed by defer_queue_task() in the main loop, no dynamic allocation.

#define APP_DEFER_QUEUE_SZ 4

typedef void (*app_defer_func_t)(uintptr_t param);

typedef struct {
  app_defer_func_t func;
  uintptr_t        arg;
  uint32_t         at_ms;
} app_defer_t;

static app_defer_t _defer_q[APP_DEFER_QUEUE_SZ];

// Clear all pending deferred callbacks.
static void app_defer_queue_clear(void) {
  memset(_defer_q, 0, sizeof(_defer_q));
}

// Schedule func to be called after 'ms' milliseconds, returns false if queue is full
static bool app_defer_ms_async(uint32_t ms, app_defer_func_t func, uintptr_t arg) {
  for (uint8_t i = 0; i < APP_DEFER_QUEUE_SZ; i++) {
    if (_defer_q[i].func == NULL) {
      _defer_q[i].func = func;
      _defer_q[i].arg  = arg;
      // add one to ensure we wait at least 'ms' milliseconds
      _defer_q[i].at_ms = tusb_time_millis_api() + ms + 1;
      return true;
    }
  }
  return false; // queue full
}

// Invoke all callbacks whose delay has expired, must be called periodically from main loop
void defer_queue_task(void) {
  const uint32_t now_ms = tusb_time_millis_api();
  for (uint8_t i = 0; i < APP_DEFER_QUEUE_SZ; i++) {
    if (_defer_q[i].func != NULL && (int32_t)(_defer_q[i].at_ms - now_ms) <= 0) {
      const app_defer_func_t func = _defer_q[i].func;
      const uintptr_t        arg  = _defer_q[i].arg;
      _defer_q[i].func            = NULL; // free slot before invoking, callback may re-schedule
      func(arg);
    }
  }
}

// Duplicate each mono sample to both channels (mono mic -> stereo speaker)
static void mono_to_stereo(const int16_t *mono, int16_t *stereo, uint32_t frames) {
  for (uint32_t i = 0; i < frames; i++) {
    stereo[i * 2]     = mono[i];
    stereo[i * 2 + 1] = mono[i];
  }
}

// Average both channels into one sample (stereo mic -> mono speaker)
static void stereo_to_mono(const int16_t *stereo, int16_t *mono, uint32_t frames) {
  for (uint32_t i = 0; i < frames; i++) {
    mono[i] = (int16_t)(((int32_t)stereo[i * 2] + stereo[i * 2 + 1]) / 2);
  }
}

// One period of an 8 kHz sine (6 samples at 48 kHz), scaled to ~8-bit
// amplitude. The test tone plays only when no capture stream is echoing.
static const int16_t sine_period[6] = {0, 221, 221, 0, -221, -221};

// Precompute a sine wave into the playback buffer
static void spk_init_sine(void) {
  for (uint32_t i = 0; i < AUDIO_MAX_FRAME_COUNT; i++) {
    const int16_t sample = sine_period[i % 6];
    for (uint8_t ch = 0; ch < spk_config.channels; ch++) {
      spk_samples[i * AUDIO_MAX_CHANNELS + ch] = sample;
    }
  }
}

// Frames to queue this millisecond at the given sample rate: rate / 1000,
// with the fractional remainder (0.1 frame per ms at 44.1 kHz) accumulated
// and paid back as one extra frame, matching the driver's playback pacing.
static uint32_t frame_rem_acc = 0;
static uint32_t audio_frames_this_ms(uint32_t sample_rate) {
  uint32_t frames = sample_rate / 1000;
  frame_rem_acc += sample_rate % 1000;
  if (frame_rem_acc >= 1000) {
    frame_rem_acc -= 1000;
    frames++;
  }
  return frames;
}

//--------------------------------------------------------------------+
// Periodic Stream Switching
//--------------------------------------------------------------------+
// Cycles through three phases with tuh_audio_start()/stop(). The driver
// activates/deactivates the stream's interface (SET_INTERFACE alt setting)
// on each switch.
//   1. mic only  (3 s): capture runs, captured data is dropped
//   2. spk only  (5 s): playback plays the sine test tone
//   3. echo      (5 s): both streams run, captured audio is echoed back
#define APP_PHASE_MIC_ONLY_MS 5000
#define APP_PHASE_SPK_ONLY_MS 5000
#define APP_PHASE_ECHO_MS     5000

enum {
  APP_PHASE_MIC_ONLY = 0,
  APP_PHASE_SPK_ONLY,
  APP_PHASE_ECHO,
  APP_PHASE_COUNT
};

static uint8_t        app_audio_phase               = APP_PHASE_MIC_ONLY;
static const uint32_t app_phase_ms[APP_PHASE_COUNT] = {APP_PHASE_MIC_ONLY_MS, APP_PHASE_SPK_ONLY_MS, APP_PHASE_ECHO_MS};

// Start or stop the capture/playback streams according to the current phase.
// The app tasks already behave per phase: with mic_ready false the sine tone
// plays, with the playback stream stopped the echo write returns 0 (dropped).
static void app_audio_phase_apply(void) {
  switch (app_audio_phase) {
    case APP_PHASE_MIC_ONLY:
      if (!mic_ready) {
        mic_ready = tuh_audio_start(audio_idx, cap_stream_idx);
      }
      if (spk_ready) {
        spk_ready = !tuh_audio_stop(audio_idx, spk_stream_idx);
      }
      printf("  Phase %u: mic on, spk off (data dropped)\r\n", app_audio_phase);
      break;
    case APP_PHASE_SPK_ONLY:
      if (mic_ready) {
        mic_ready = !tuh_audio_stop(audio_idx, cap_stream_idx);
      }
      if (!spk_ready) {
        spk_ready = tuh_audio_start(audio_idx, spk_stream_idx);
      }
      printf("  Phase %u: mic off, spk on (sine)\r\n", app_audio_phase);
      break;
    case APP_PHASE_ECHO:
      if (!mic_ready) {
        mic_ready = tuh_audio_start(audio_idx, cap_stream_idx);
      }
      if (!spk_ready) {
        spk_ready = tuh_audio_start(audio_idx, spk_stream_idx);
      }
      printf("  Phase %u: mic + spk on (echo)\r\n", app_audio_phase);
      break;
    default:
      break;
  }
}

// Enter a phase, then schedule the next switch after this phase's duration
static void app_audio_phase_enter(uintptr_t phase) {
  app_audio_phase = (uint8_t)phase;
  // Cancel stale deferred callbacks (e.g. a stream restart scheduled on a
  // transfer error) so they cannot re-start a stream this phase stops.
  app_defer_queue_clear();
  app_audio_phase_apply();
  const uint8_t next_phase = (uint8_t)((app_audio_phase + 1) % APP_PHASE_COUNT);
  app_defer_ms_async(app_phase_ms[app_audio_phase], (app_defer_func_t)app_audio_phase_enter, next_phase);
}


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
  led_state = 1 - led_state; // toggle
#if 1
  printf(" MIC CB=%lu SPK CB=%lu ERR CB=%lu\r\n", (unsigned long)mic_cb_count, (unsigned long)spk_cb_count,
         (unsigned long)err_cb_count);
  mic_cb_count = 0;
  spk_cb_count = 0;
  err_cb_count = 0;

#endif
#if 0
  // Print the current Feature Unit volume, which is set to 0x0600 in mic_configured() and can be changed by the device.
  uint16_t volume = 0x0001;
  tuh_audio_feature_unit_get_sync(audio_idx, cap_stream_idx, AUDIO10_FU_CTRL_VOLUME, 0, &volume);
  printf("  Feature Unit volume get: 0x%04x\r\n", (unsigned int)volume);
  uint16_t mute = 0x0000;
  tuh_audio_feature_unit_get_sync(audio_idx, cap_stream_idx, AUDIO10_FU_CTRL_MUTE, 0, &mute);
  mute=!mute; // toggle mute for demonstration
  tuh_audio_feature_unit_set_sync(audio_idx, cap_stream_idx, AUDIO10_FU_CTRL_MUTE, 0, mute);
  printf("  Feature Unit mute set: 0x%04x\r\n", (unsigned int)mute);
#endif
}

//--------------------------------------------------------------------+
// Application Task
//--------------------------------------------------------------------+


// Echo the captured audio back to the playback stream: drain the capture
// FIFO into mic_samples, convert, and queue the frames into the playback
// FIFO. The driver schedules the actual isochronous transfers.

void audio_app_task_read(void) {
  if (!mic_ready) {
    return;
  }

  const uint32_t frames =
    tuh_audio_read(audio_idx, cap_stream_idx, mic_samples, audio_frames_this_ms(mic_config.sample_rate));
  if (frames == 0) {
    return;
  }

  if (spk_config.channels == mic_config.channels) {
    memcpy(spk_samples, mic_samples, frames * mic_config.channels * sizeof(int16_t));
  } else if (mic_config.channels == 1 && spk_config.channels == 2) {
    mono_to_stereo(mic_samples, spk_samples, frames);
  } else {
    stereo_to_mono(mic_samples, spk_samples, frames);
  }

  (void)tuh_audio_write(audio_idx, spk_stream_idx, spk_samples, frames);
}

void audio_app_task_write(void) {
  // Fallback: the sine test tone when no capture stream is echoing
  if (mic_ready || !spk_ready) {
    return;
  }

  const uint32_t frames = audio_frames_this_ms(spk_config.sample_rate);
  if (tuh_audio_write_available(audio_idx, spk_stream_idx) >= frames) {
    (void)tuh_audio_write(audio_idx, spk_stream_idx, spk_samples, frames);
  }
}

// Invoked when an isochronous IN transfer completes: the captured data is
// already queued into the capture FIFO and drained by audio_app_task_read().
void tuh_audio_capture_cb(uint8_t idx, uint8_t stream_idx, uint16_t xferred_bytes) {
  (void)idx;
  (void)stream_idx;
  (void)xferred_bytes;
  mic_cb_count++;
}

// Invoked when an isochronous OUT transfer completes: the next queued packet
// is submitted from the playback FIFO by the driver.
void tuh_audio_playback_cb(uint8_t idx, uint8_t stream_idx, uint16_t xferred_bytes) {
  (void)idx;
  (void)stream_idx;
  (void)xferred_bytes;
  spk_cb_count++;
}

// Re-open a stream stopped by a transfer error: the driver keeps the stream
// configured, so tuh_audio_start() resumes it. Invoked deferred so repeated
// errors cannot stall the main loop.
static void audio_app_restart_stream(uintptr_t param) {
  const uint8_t idx        = (uint8_t)(param >> 8);
  const uint8_t stream_idx = (uint8_t)param;
  if (!tuh_audio_mounted(idx)) {
    return; // device is gone
  }
  if (stream_idx == cap_stream_idx) {
    printf("  Restarting capture stream %u\r\n", stream_idx);
    mic_ready = tuh_audio_start(idx, stream_idx);
  } else if (stream_idx == spk_stream_idx) {
    printf("  Restarting playback stream %u\r\n", stream_idx);
    spk_ready = tuh_audio_start(idx, stream_idx);
  }
}

// Invoked when an isochronous transfer fails: the stream was stopped by the
// driver, re-open it after a short delay so the device can recover.
void tuh_audio_err_cb(uint8_t idx, uint8_t stream_idx, uint16_t xferred_bytes) {
  (void)xferred_bytes;
  err_cb_count++;
  printf("  AUDIO transfer error: addr=%u stream=%u xferred_bytes=%u\r\n", idx, stream_idx, (unsigned)xferred_bytes);
  app_defer_ms_async(100, (app_defer_func_t)audio_app_restart_stream, ((uintptr_t)idx << 8) | stream_idx);
}

//--------------------------------------------------------------------+
// TinyUSB Callbacks
//--------------------------------------------------------------------+

// Print all supported stream configurations
static void print_stream_configs(uint8_t idx, uint8_t stream_idx) {
  const tuh_audio_direction_t dir             = tuh_audio_stream_direction(idx, stream_idx);
  const char                 *dir_name        = (dir == TUH_AUDIO_STREAM_CAPTURE) ? "capture" : "playback";
  const uint8_t               feature_unit_id = tuh_audio_get_feature_unit_id(idx, stream_idx);
  printf("  %s stream %u Feature Unit ID: %u, configurations: %u\r\n", dir_name, stream_idx, feature_unit_id,
         tuh_audio_config_count(idx, stream_idx));
  for (uint8_t i = 0; i < tuh_audio_config_count(idx, stream_idx); i++) {
    tuh_audio_stream_config_t config;
    if (tuh_audio_config_get(idx, stream_idx, i, &config)) {
      printf("    [%u] format=%u rate=%lu channels=%u\r\n", i, (unsigned)config.format,
             (unsigned long)config.sample_rate, (unsigned)config.channels);
    }
  }
}

static void set_stream_volume(uint8_t idx, uint8_t stream_idx, const char *stream_name) {
  const uint8_t feature_unit_id = tuh_audio_get_feature_unit_id(idx, stream_idx);
  if (feature_unit_id == 0) {
    printf("  %s stream has no Feature Unit\r\n", stream_name);
    return;
  }

  uint16_t           volume = FEATURE_UNIT_VOLUME;
  tusb_xfer_result_t result = tuh_audio_feature_unit_set_sync(idx, stream_idx, AUDIO10_FU_CTRL_VOLUME, 0, volume);
  if (result == XFER_RESULT_SUCCESS) {
    printf("  %s Feature Unit %u master volume set: 0x%04x\r\n", stream_name, feature_unit_id, (unsigned int)volume);
  } else {
    printf("  Setting %s Feature Unit %u volume failed: result=%u\r\n", stream_name, feature_unit_id, result);
  }
}

// Invoked when device with Audio interface is un-mounted
void tuh_audio_umount_cb(uint8_t idx) {
  printf("Audio device unmounted: idx=%u\r\n", idx);
  if (idx == audio_idx) {
    app_defer_queue_clear();
    audio_idx      = TUSB_INDEX_INVALID_8;
    cap_stream_idx = TUSB_INDEX_INVALID_8;
    spk_stream_idx = TUSB_INDEX_INVALID_8;
    mic_ready      = false;
    spk_ready      = false;
  }
}

static void tuh_audio_mount_async(uintptr_t param) {
  uint8_t idx = (uint8_t)param;
  if (idx >= CFG_TUH_AUDIO_MAX) {
    printf("Audio device mount failed: idx=%u exceeds max=%u\r\n", idx, CFG_TUH_AUDIO_MAX);
    return;
  }

  printf("Audio device mounted: idx=%u addr=%u\r\n", idx, tuh_audio_get_dev_addr(idx));

  // Inspect every stream and print its supported configurations
  for (uint8_t stream_idx = 0; stream_idx < tuh_audio_stream_count(idx); stream_idx++) {
    if (!tuh_audio_stream_exists(idx, stream_idx)) {
      continue;
    }
    print_stream_configs(idx, stream_idx);
  }

  // Select a supported 48 kHz S16_LE capture configuration without
  // accessing USB interfaces, alternate settings, or endpoint addresses.
  // Sample rates are tried in SAMPLE_RATES order (44.1 kHz first), stereo is
  // preferred, mono is accepted.
  static const uint32_t sample_rates[] = SAMPLE_RATES;
  bool                  capture_found  = false;
  for (uint8_t r = 0; r < TU_ARRAY_SIZE(sample_rates) && !capture_found; r++) {
    const uint32_t sample_rate = sample_rates[r];
    for (uint8_t stream_idx = 0; stream_idx < tuh_audio_stream_count(idx) && !capture_found; stream_idx++) {
      // Only consider capture streams, ignore playback streams
      if (tuh_audio_stream_direction(idx, stream_idx) != TUH_AUDIO_STREAM_CAPTURE) {
        continue;
      }
      for (uint8_t ch = AUDIO_MAX_CHANNELS; ch >= 1 && !capture_found; ch--) {
        for (uint8_t i = 0; i < tuh_audio_config_count(idx, stream_idx); i++) {
          tuh_audio_stream_config_t config;
          // Check for a matching sample rate S16_LE configuration with the desired channel count
          if (tuh_audio_config_get(idx, stream_idx, i, &config) && config.format == TUH_AUDIO_FORMAT_S16_LE &&
              config.sample_rate == sample_rate && config.channels == ch) {
            printf("  Configuring %u S16_LE capture (%u channels)\r\n", (unsigned)sample_rate, config.channels);
            if (!tuh_audio_configure(idx, stream_idx, i)) {
              printf("  Microphone configuration failed\r\n");
              continue;
            }
            audio_idx      = idx;
            cap_stream_idx = stream_idx;
            mic_config     = config;
            // one ms of audio at the selected rate, rounded down to whole frames
            audio_frame_count = sample_rate / 1000;
            printf("  Microphone configured\r\n");
            set_stream_volume(idx, stream_idx, "Microphone");
            mic_ready     = tuh_audio_start(idx, stream_idx);
            capture_found = true;
            break;
          }
        }
      }
    }
  }
  if (!capture_found) {
    printf("  No supported 48/44.1 kHz S16_LE capture configuration found\r\n");
  }

  // The echo needs a playback stream at the capture sample rate (or at any
  // preferred rate when no capture stream exists, for the sine fallback).
  // Prefer the same channel count as the capture stream (direct echo), then
  // the other one (converted).
  uint8_t playback_config_idx = TUSB_INDEX_INVALID_8;
  for (uint8_t r = 0; r < TU_ARRAY_SIZE(sample_rates) && playback_config_idx == TUSB_INDEX_INVALID_8; r++) {
    const uint32_t sample_rate = capture_found ? mic_config.sample_rate : sample_rates[r];
    for (uint8_t stream_idx = 0;
         stream_idx < tuh_audio_stream_count(idx) && playback_config_idx == TUSB_INDEX_INVALID_8; stream_idx++) {
      // Only consider playback streams, ignore capture streams
      if (tuh_audio_stream_direction(idx, stream_idx) != TUH_AUDIO_STREAM_PLAYBACK) {
        continue;
      }
      const uint8_t preferred_channels = capture_found ? mic_config.channels : 2;
      for (uint8_t n = 0; n < 2 && playback_config_idx == TUSB_INDEX_INVALID_8; n++) {
        const uint8_t ch = (n == 0) ? preferred_channels : (uint8_t)(preferred_channels == 1 ? 2 : 1);
        for (uint8_t i = 0; i < tuh_audio_config_count(idx, stream_idx); i++) {
          tuh_audio_stream_config_t config;
          if (tuh_audio_config_get(idx, stream_idx, i, &config) && config.format == TUH_AUDIO_FORMAT_S16_LE &&
              config.sample_rate == sample_rate && config.channels == ch) {
            spk_stream_idx      = stream_idx;
            spk_config          = config;
            playback_config_idx = i;
            break;
          }
        }
      }
    }
  }
  if (playback_config_idx == TUSB_INDEX_INVALID_8) {
    printf("  No supported %u S16_LE playback configuration, echo disabled\r\n",
           (unsigned)(capture_found ? mic_config.sample_rate : sample_rates[0]));
    return;
  }
  printf("  Configuring %u S16_LE playback (%u channels)\r\n", (unsigned)spk_config.sample_rate, spk_config.channels);
  if (!tuh_audio_configure(idx, spk_stream_idx, playback_config_idx)) {
    printf("  Speaker configuration failed\r\n");
    return;
  }
  audio_idx = idx;
  printf("  Speaker configured\r\n");
  // playback-only device: set the frame cadence from the selected rate
  audio_frame_count = spk_config.sample_rate / 1000;
  spk_init_sine(); // fallback test tone while no capture stream is echoing
  set_stream_volume(idx, spk_stream_idx, "Speaker");
  spk_ready = tuh_audio_start(idx, spk_stream_idx);

  // both streams running: start the periodic phase switching demo
  if (mic_ready && spk_ready) {
    app_audio_phase_enter(APP_PHASE_MIC_ONLY);
  }
}

// Invoked when device with Audio interface is mounted
void tuh_audio_mount_cb(uint8_t idx) {
  app_defer_ms_async(100, (app_defer_func_t)tuh_audio_mount_async, idx);
}

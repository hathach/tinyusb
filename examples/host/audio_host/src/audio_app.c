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
// - AUDIO_MAX_CHANNELS: maximum channels of the capture/playback stream
// - SAMPLE_RATES: sample rates tried in order, first match wins (44.1 kHz stereo by default)
#define AUDIO_MAX_CHANNELS        2
#define SAMPLE_RATES              {44100, 48000}
#define FEATURE_UNIT_VOLUME_DB    (-20 * 256)
#define AUDIO_BUFFER_SAMPLE_COUNT (CFG_TUH_AUDIO_STREAM_BUFSIZE / 2 / sizeof(int16_t))
static uint8_t audio_idx      = TUSB_INDEX_INVALID_8;
static uint8_t cap_stream_idx = TUSB_INDEX_INVALID_8;
static uint8_t spk_stream_idx = TUSB_INDEX_INVALID_8;

// True after a stream start request is accepted and until it is stopped or fails.
static bool mic_enabled;
static bool spk_enabled;

static int16_t                   audio_samples[AUDIO_BUFFER_SAMPLE_COUNT];
static tuh_audio_stream_config_t mic_config;
static tuh_audio_stream_config_t spk_config;

// Diagnostic counters printed and cleared once per second.
static uint32_t spk_cb_count;
static uint32_t mic_cb_count;
static uint32_t fail_event_count;
static uint32_t stats_start_ms;

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

// Schedule func to be called after 'ms' milliseconds. Four slots cover the
// phase timer, one restart per stream, and the single supported Audio device.
static void app_defer_ms_async(uint32_t ms, app_defer_func_t func, uintptr_t arg) {
  for (uint8_t i = 0; i < APP_DEFER_QUEUE_SZ; i++) {
    if (_defer_q[i].func == NULL) {
      _defer_q[i].func = func;
      _defer_q[i].arg  = arg;
      // add one to ensure we wait at least 'ms' milliseconds
      _defer_q[i].at_ms = tusb_time_millis_api() + ms + 1;
      return;
    }
  }
  TU_ASSERT(false, ); // Queue sizing above is an application invariant.
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

// Expand backward so mono capture can be converted to stereo in place.
static void mono_to_stereo(int16_t *samples, uint32_t frames) {
  for (uint32_t i = frames; i-- > 0;) {
    const int16_t sample = samples[i];
    samples[i * 2]       = sample;
    samples[i * 2 + 1]   = sample;
  }
}

// Contract forward so stereo capture can be converted to mono in place.
static void stereo_to_mono(int16_t *samples, uint32_t frames) {
  for (uint32_t i = 0; i < frames; i++) {
    samples[i] = (int16_t)(((int32_t)samples[i * 2] + samples[i * 2 + 1]) / 2);
  }
}

#define SINE_TONE_HZ  1000u
#define SINE_LUT_BITS 6u
#define SINE_LUT_SIZE (1u << SINE_LUT_BITS)

// One sine period with a peak amplitude of 4096 (-18 dBFS).
static const int16_t sine_lut[SINE_LUT_SIZE] = {
  0,     401,   799,   1189,  1567,  1931,  2276,  2598,  2896,  3166,  3406,  3612,  3784,  3920,  4017,  4076,
  4096,  4076,  4017,  3920,  3784,  3612,  3406,  3166,  2896,  2598,  2276,  1931,  1567,  1189,  799,   401,
  0,     -401,  -799,  -1189, -1567, -1931, -2276, -2598, -2896, -3166, -3406, -3612, -3784, -3920, -4017, -4076,
  -4096, -4076, -4017, -3920, -3784, -3612, -3406, -3166, -2896, -2598, -2276, -1931, -1567, -1189, -799,  -401,
};

static uint32_t sine_phase;
static uint32_t sine_phase_step;

// Generate a continuous tone at the selected sample rate and pack each frame
// according to the actual playback channel count.
static void spk_fill_sine(uint32_t frames) {
  for (uint32_t i = 0; i < frames; i++) {
    const int16_t sample = sine_lut[sine_phase >> (32u - SINE_LUT_BITS)];
    sine_phase += sine_phase_step;
    for (uint8_t ch = 0; ch < spk_config.channels; ch++) {
      audio_samples[i * spk_config.channels + ch] = sample;
    }
  }
}

// One application buffer is half of the driver's stream FIFO. Servicing the
// FIFO at this watermark leaves the other half available to absorb scheduling
// jitter between main-loop iterations.
static uint32_t audio_half_fifo_frames(const tuh_audio_stream_config_t *config) {
  return TU_ARRAY_SIZE(audio_samples) / config->channels;
}

//--------------------------------------------------------------------+
// Periodic Stream Switching
//--------------------------------------------------------------------+
// Cycles through three phases with tuh_audio_start()/stop(). The driver
// activates/deactivates the stream's interface (SET_INTERFACE alt setting)
// on each switch.
//   1. mic only  (5 s): capture runs, captured data is dropped
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
// The app task discards capture in mic-only mode, generates sine in speaker-only
// mode, and echoes capture when both streams run.
static void app_audio_phase_apply(void) {
  switch (app_audio_phase) {
    case APP_PHASE_MIC_ONLY:
      if (!mic_enabled) {
        mic_enabled = tuh_audio_start(audio_idx, cap_stream_idx);
      }
      if (spk_enabled) {
        spk_enabled = !tuh_audio_stop(audio_idx, spk_stream_idx);
      }
      printf("  Phase %u: mic on, spk off (data dropped)\r\n", app_audio_phase);
      break;
    case APP_PHASE_SPK_ONLY:
      if (mic_enabled) {
        mic_enabled = !tuh_audio_stop(audio_idx, cap_stream_idx);
      }
      if (!spk_enabled) {
        spk_enabled = tuh_audio_start(audio_idx, spk_stream_idx);
      }
      printf("  Phase %u: mic off, spk on (sine)\r\n", app_audio_phase);
      break;
    case APP_PHASE_ECHO:
      if (!mic_enabled) {
        mic_enabled = tuh_audio_start(audio_idx, cap_stream_idx);
      }
      if (!spk_enabled) {
        spk_enabled = tuh_audio_start(audio_idx, spk_stream_idx);
      }
      printf("  Phase %u: mic + spk on (echo)\r\n", app_audio_phase);
      break;
    default:
      break;
  }
}

static void audio_stats_reset(void) {
  stats_start_ms   = tusb_time_millis_api();
  mic_cb_count     = 0;
  spk_cb_count     = 0;
  fail_event_count = 0;
}

// Enter a phase, then schedule the next switch after this phase's duration
static void app_audio_phase_enter(uintptr_t phase) {
  app_audio_phase = (uint8_t)phase;
  // Cancel stale deferred callbacks (e.g. a stream restart scheduled on a
  // transfer error) so they cannot re-start a stream this phase stops.
  app_defer_queue_clear();
  app_audio_phase_apply();
  audio_stats_reset();
  const uint8_t next_phase = (uint8_t)((app_audio_phase + 1) % APP_PHASE_COUNT);
  app_defer_ms_async(app_phase_ms[app_audio_phase], app_audio_phase_enter, next_phase);
}

//--------------------------------------------------------------------+
// Blinking Task
//--------------------------------------------------------------------+
void led_blinking_task(void) {
  const uint32_t interval_ms = 1000;

  static bool led_state = false;

  // Blink every interval ms
  if (tusb_time_millis_api() - stats_start_ms < interval_ms) {
    return; // not enough time
  }
  stats_start_ms += interval_ms;

  board_led_write(led_state);
  led_state = 1 - led_state; // toggle
  printf(" MIC CB=%lu | SPK CB=%lu | FAIL EVENT=%lu\r\n", (unsigned long)mic_cb_count, (unsigned long)spk_cb_count,
         (unsigned long)fail_event_count);
  mic_cb_count     = 0;
  spk_cb_count     = 0;
  fail_event_count = 0;
}

//--------------------------------------------------------------------+
// Application Task
//--------------------------------------------------------------------+
// Echo one chunk after capture is at least half full and playback is at least
// half drained. This runs from the main loop, independently of USB callbacks.
static void audio_echo_task(void) {
  const uint32_t mic_frames = audio_half_fifo_frames(&mic_config);
  const uint32_t spk_frames = audio_half_fifo_frames(&spk_config);
  if (tuh_audio_read_available(audio_idx, cap_stream_idx) < mic_frames ||
      tuh_audio_write_available(audio_idx, spk_stream_idx) < spk_frames) {
    return;
  }

  const uint32_t frames = TU_MIN(mic_frames, spk_frames);
  (void)tuh_audio_read(audio_idx, cap_stream_idx, audio_samples, frames);
  if (mic_config.channels == 1 && spk_config.channels == 2) {
    mono_to_stereo(audio_samples, frames);
  } else if (mic_config.channels == 2 && spk_config.channels == 1) {
    stereo_to_mono(audio_samples, frames);
  }

  (void)tuh_audio_write(audio_idx, spk_stream_idx, audio_samples, frames);
}

void audio_app_task(void) {
  if (mic_enabled && spk_enabled) {
    audio_echo_task();
  } else if (mic_enabled) {
    const uint32_t frames = audio_half_fifo_frames(&mic_config);
    if (tuh_audio_read_available(audio_idx, cap_stream_idx) >= frames) {
      (void)tuh_audio_read(audio_idx, cap_stream_idx, audio_samples, frames);
    }
  } else if (spk_enabled) {
    const uint32_t frames = audio_half_fifo_frames(&spk_config);
    if (tuh_audio_write_available(audio_idx, spk_stream_idx) >= frames) {
      spk_fill_sine(frames);
      (void)tuh_audio_write(audio_idx, spk_stream_idx, audio_samples, frames);
    }
  }
}

// Transfer callbacks are intentionally not used to service the FIFOs. The
// main-loop audio_app_task() reads and writes independently at half-FIFO
// watermarks; these callbacks only collect diagnostic counts.
void tuh_audio_capture_cb(uint8_t idx, uint8_t stream_idx, uint16_t xferred_bytes) {
  (void)idx;
  (void)stream_idx;
  (void)xferred_bytes;
  mic_cb_count++;
}

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
    mic_enabled = tuh_audio_start(idx, stream_idx);
  } else if (stream_idx == spk_stream_idx) {
    printf("  Restarting playback stream %u\r\n", stream_idx);
    spk_enabled = tuh_audio_start(idx, stream_idx);
  }
}

void tuh_audio_event_cb(uint8_t idx, uint8_t stream_idx, tuh_audio_event_t event, tusb_xfer_result_t result) {
  const char *stream_name = (stream_idx == cap_stream_idx) ? "capture" : "playback";

  if (event == TUH_AUDIO_EVENT_START_COMPLETE) {
    printf("  %s start %s: result=%u\r\n", stream_name, result == XFER_RESULT_SUCCESS ? "complete" : "failed", result);
    if (result == XFER_RESULT_SUCCESS) {
      return;
    }
  } else if (event == TUH_AUDIO_EVENT_STOP_COMPLETE) {
    printf("  %s stop %s: result=%u\r\n", stream_name, result == XFER_RESULT_SUCCESS ? "complete" : "failed", result);
    if (result != XFER_RESULT_SUCCESS) {
      fail_event_count++;
    }
    return;
  } else {
    printf("  %s transfer failed: result=%u\r\n", stream_name, result);
  }

  fail_event_count++;
  if (stream_idx == cap_stream_idx) {
    mic_enabled = false;
  } else if (stream_idx == spk_stream_idx) {
    spk_enabled = false;
  }
  app_defer_ms_async(100, audio_app_restart_stream, ((uintptr_t)idx << 8) | stream_idx);
}

//--------------------------------------------------------------------+
// TinyUSB Callbacks
//--------------------------------------------------------------------+

// Print all supported stream configurations
static void print_stream_configs(uint8_t idx, uint8_t stream_idx) {
  const tuh_audio_direction_t dir      = tuh_audio_stream_direction(idx, stream_idx);
  const char                 *dir_name = (dir == TUH_AUDIO_STREAM_CAPTURE) ? "capture" : "playback";
  printf("  %s stream %u, configurations: %u\r\n", dir_name, stream_idx, tuh_audio_config_count(idx, stream_idx));
  tuh_audio_volume_range_t range;
  if (tuh_audio_mute_supported(idx, stream_idx)) {
    printf("    master mute supported\r\n");
  }
  if (tuh_audio_volume_range_get(idx, stream_idx, &range)) {
    printf("    volume range: min=%d max=%d res=%u (1/256 dB)\r\n", (int)range.min, (int)range.max,
           (unsigned)range.res);
  }
  for (uint8_t i = 0; i < tuh_audio_config_count(idx, stream_idx); i++) {
    tuh_audio_stream_config_t config;
    if (tuh_audio_config_get(idx, stream_idx, i, &config)) {
      printf("    [%u] format=%u rate=%lu channels=%u\r\n", i, (unsigned)config.format,
             (unsigned long)config.sample_rate, (unsigned)config.channels);
    }
  }
}

static void configure_stream_controls(uint8_t idx, uint8_t stream_idx, const char *stream_name) {
  bool has_control = false;

  if (tuh_audio_mute_supported(idx, stream_idx)) {
    has_control = true;
    bool               mute;
    tusb_xfer_result_t result = tuh_audio_mute_get_sync(idx, stream_idx, &mute);
    if (result == XFER_RESULT_SUCCESS) {
      printf("  %s master mute: %s\r\n", stream_name, mute ? "on" : "off");
      result = tuh_audio_mute_set_sync(idx, stream_idx, false);
    }
    if (result != XFER_RESULT_SUCCESS) {
      printf("  Accessing %s master mute failed: result=%u\r\n", stream_name, result);
    }
  }

  tuh_audio_volume_range_t range;
  if (tuh_audio_volume_range_get(idx, stream_idx, &range)) {
    has_control = true;
    int16_t            volume;
    tusb_xfer_result_t result = tuh_audio_volume_get_sync(idx, stream_idx, TUH_AUDIO_CHANNEL_MASTER, &volume);
    if (result == XFER_RESULT_SUCCESS) {
      printf("  %s master volume: %d (1/256 dB)\r\n", stream_name, volume);
    }
    int32_t target = FEATURE_UNIT_VOLUME_DB;
    target         = TU_MAX(target, range.min);
    target         = TU_MIN(target, range.max);
    result         = tuh_audio_volume_set_sync(idx, stream_idx, TUH_AUDIO_CHANNEL_MASTER, (int16_t)target);
    if (result == XFER_RESULT_SUCCESS) {
      printf("  %s volume set: %d (1/256 dB)\r\n", stream_name, (int)target);
    }
    if (result != XFER_RESULT_SUCCESS) {
      printf("  Setting %s volume failed: result=%u\r\n", stream_name, result);
    }
  }

  if (!has_control) {
    printf("  %s stream has no mute/volume control\r\n", stream_name);
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
    mic_enabled    = false;
    spk_enabled    = false;
  }
}

static void tuh_audio_mount_async(uintptr_t param) {
  uint8_t idx = (uint8_t)param;
  printf("Audio device mounted: idx=%u addr=%u\r\n", idx, tuh_audio_get_dev_addr(idx));

  // Inspect every stream and print its supported configurations
  for (uint8_t stream_idx = 0; stream_idx < tuh_audio_stream_count(idx); stream_idx++) {
    if (!tuh_audio_stream_exists(idx, stream_idx)) {
      continue;
    }
    print_stream_configs(idx, stream_idx);
  }

  // Select a preferred S16_LE capture configuration without
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
            printf("  Microphone configured\r\n");
            configure_stream_controls(idx, stream_idx, "Microphone");
            mic_enabled   = tuh_audio_start(idx, stream_idx);
            capture_found = true;
            break;
          }
        }
      }
    }
  }
  if (!capture_found) {
    printf("  No supported 44.1/48 kHz S16_LE capture configuration found\r\n");
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
  sine_phase      = 0;
  sine_phase_step = (uint32_t)(((uint64_t)SINE_TONE_HZ << 32) / spk_config.sample_rate);
  configure_stream_controls(idx, spk_stream_idx, "Speaker");

  if (capture_found) {
    // Start in the mic-only phase without briefly activating playback first.
    app_audio_phase_enter(APP_PHASE_MIC_ONLY);
  } else {
    // Playback-only device: start the sine test tone immediately.
    spk_enabled = tuh_audio_start(idx, spk_stream_idx);
  }
}

// Invoked when device with Audio interface is mounted
void tuh_audio_mount_cb(uint8_t idx) {
  app_defer_ms_async(100, tuh_audio_mount_async, idx);
}

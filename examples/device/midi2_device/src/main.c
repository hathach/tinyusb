/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2026 Saulo Verissimo
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
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <stdio.h>
#include <string.h>
#include "bsp/board_api.h"
#include "tusb.h"
#include "class/midi/midi2_device.h"

//--------------------------------------------------------------------+
// MIDI 2.0 UMP Message Type Constants (M2-104-UM, Section 4)
//--------------------------------------------------------------------+
// Message Type (MT) occupies bits 31-28 of Word 0
#define UMP_MT_UTILITY        0x00000000  // 32-bit:  Utility (NOOP, JR Clock, JR Timestamp)
#define UMP_MT_SYSTEM          0x10000000  // 32-bit:  System Common / Real Time
#define UMP_MT_MIDI1_CV        0x20000000  // 32-bit:  MIDI 1.0 Channel Voice
#define UMP_MT_DATA64          0x30000000  // 64-bit:  Data (SysEx 7-bit)
#define UMP_MT_MIDI2_CV        0x40000000  // 64-bit:  MIDI 2.0 Channel Voice
#define UMP_MT_DATA128         0x50000000  // 128-bit: Data (SysEx 8-bit)

// MIDI 2.0 Channel Voice status (bits 23-20 of Word 0)
#define UMP_STATUS_NOTE_OFF       0x00800000
#define UMP_STATUS_NOTE_ON        0x00900000
#define UMP_STATUS_POLY_PRESSURE  0x00A00000
#define UMP_STATUS_CC             0x00B00000
#define UMP_STATUS_PROGRAM        0x00C00000
#define UMP_STATUS_CHAN_PRESSURE   0x00D00000
#define UMP_STATUS_PITCH_BEND     0x00E00000
#define UMP_STATUS_PN_MGMT        0x00F00000  // Per-Note Management

// Note Attribute Types (MIDI 2.0 spec, Section 4.2.6)
#define UMP_ATTR_NONE             0x00
#define UMP_ATTR_MANUFACTURER     0x01
#define UMP_ATTR_PROFILE          0x02
#define UMP_ATTR_PITCH_7_9        0x03  // Pitch 7.9 format

//--------------------------------------------------------------------+
// MIDI 2.0 UMP Builders - Full Spec Coverage
//--------------------------------------------------------------------+

// Helper: send a 64-bit UMP (2 words)
static inline void ump_send_64(uint32_t w0, uint32_t w1) {
  uint32_t words[2] = { w0, w1 };
  tud_midi2_ump_write(words, 2);
}

// Helper: send a 32-bit UMP (1 word)
static inline void ump_send_32(uint32_t w0) {
  tud_midi2_ump_write(&w0, 1);
}

// -- Utility Messages (MT=0x0, 32-bit) --

static inline void ump_noop(void) {
  ump_send_32(UMP_MT_UTILITY);
}

static inline void ump_jr_timestamp(uint16_t timestamp) {
  // Word 0: [MT(0x0) | Group(0) | Status(0x0020) | Timestamp(16-bit)]
  ump_send_32(UMP_MT_UTILITY | 0x00200000 | (uint32_t)timestamp);
}

// -- MIDI 2.0 Channel Voice: Note On (MT=0x4, 64-bit) --
// Word 0: [MT(4):Group(4):Status(4):Channel(4):NoteNumber(8):AttrType(8)]
// Word 1: [Velocity(16):Attribute(16)]
static inline void ump_note_on(uint8_t group, uint8_t channel,
                                uint8_t pitch, uint16_t velocity,
                                uint8_t attr_type, uint16_t attr_val) {
  uint32_t w0 = UMP_MT_MIDI2_CV | ((uint32_t)(group & 0x0F) << 24)
              | UMP_STATUS_NOTE_ON | ((uint32_t)(channel & 0x0F) << 16)
              | ((uint32_t)(pitch & 0x7F) << 8)
              | (uint32_t)(attr_type & 0xFF);
  uint32_t w1 = ((uint32_t)(velocity & 0xFFFF) << 16)
              | (uint32_t)(attr_val & 0xFFFF);
  ump_send_64(w0, w1);
}

// -- MIDI 2.0 Channel Voice: Note Off (MT=0x4, 64-bit) --
static inline void ump_note_off(uint8_t group, uint8_t channel,
                                 uint8_t pitch, uint16_t velocity,
                                 uint8_t attr_type, uint16_t attr_val) {
  uint32_t w0 = UMP_MT_MIDI2_CV | ((uint32_t)(group & 0x0F) << 24)
              | UMP_STATUS_NOTE_OFF | ((uint32_t)(channel & 0x0F) << 16)
              | ((uint32_t)(pitch & 0x7F) << 8)
              | (uint32_t)(attr_type & 0xFF);
  uint32_t w1 = ((uint32_t)(velocity & 0xFFFF) << 16)
              | (uint32_t)(attr_val & 0xFFFF);
  ump_send_64(w0, w1);
}

// -- MIDI 2.0 Channel Voice: Control Change (MT=0x4, 64-bit) --
// Word 0: [MT(4):Group(4):Status(0xB):Channel(4):Index(8):Reserved(8)]
// Word 1: [Data(32)] -- full 32-bit CC resolution (vs 7-bit MIDI 1.0)
static inline void ump_cc(uint8_t group, uint8_t channel,
                           uint8_t index, uint32_t value) {
  uint32_t w0 = UMP_MT_MIDI2_CV | ((uint32_t)(group & 0x0F) << 24)
              | UMP_STATUS_CC | ((uint32_t)(channel & 0x0F) << 16)
              | ((uint32_t)(index & 0x7F) << 8);
  ump_send_64(w0, value);
}

// -- MIDI 2.0 Channel Voice: Program Change (MT=0x4, 64-bit) --
// Word 0: [MT(4):Group(4):Status(0xC):Channel(4):Reserved(8):OptionFlags(8)]
// Word 1: [Program(8):Reserved(8):BankMSB(8):BankLSB(8)]
// OptionFlags bit 0 = Bank Valid
static inline void ump_program_change(uint8_t group, uint8_t channel,
                                       uint8_t program,
                                       bool bank_valid, uint8_t bank_msb,
                                       uint8_t bank_lsb) {
  uint8_t flags = bank_valid ? 0x01 : 0x00;
  uint32_t w0 = UMP_MT_MIDI2_CV | ((uint32_t)(group & 0x0F) << 24)
              | UMP_STATUS_PROGRAM | ((uint32_t)(channel & 0x0F) << 16)
              | (uint32_t)flags;
  uint32_t w1 = ((uint32_t)program << 24)
              | ((uint32_t)bank_msb << 8)
              | (uint32_t)bank_lsb;
  ump_send_64(w0, w1);
}

// -- MIDI 2.0 Channel Voice: Pitch Bend (MT=0x4, 64-bit) --
// Word 0: [MT(4):Group(4):Status(0xE):Channel(4):Reserved(16)]
// Word 1: [PitchBend(32)] -- full 32-bit (vs 14-bit MIDI 1.0!)
//   0x80000000 = center, 0x00000000 = min, 0xFFFFFFFF = max
static inline void ump_pitch_bend(uint8_t group, uint8_t channel,
                                   uint32_t value) {
  uint32_t w0 = UMP_MT_MIDI2_CV | ((uint32_t)(group & 0x0F) << 24)
              | UMP_STATUS_PITCH_BEND | ((uint32_t)(channel & 0x0F) << 16);
  ump_send_64(w0, value);
}

// -- MIDI 2.0 Channel Voice: Channel Pressure / Aftertouch (MT=0x4, 64-bit) --
// Word 0: [MT(4):Group(4):Status(0xD):Channel(4):Reserved(16)]
// Word 1: [Pressure(32)] -- full 32-bit (vs 7-bit MIDI 1.0)
static inline void ump_channel_pressure(uint8_t group, uint8_t channel,
                                         uint32_t pressure) {
  uint32_t w0 = UMP_MT_MIDI2_CV | ((uint32_t)(group & 0x0F) << 24)
              | UMP_STATUS_CHAN_PRESSURE | ((uint32_t)(channel & 0x0F) << 16);
  ump_send_64(w0, pressure);
}

// -- MIDI 2.0 Channel Voice: Poly Pressure / Per-Note Aftertouch --
// Word 0: [MT(4):Group(4):Status(0xA):Channel(4):NoteNumber(8):Reserved(8)]
// Word 1: [Pressure(32)]
static inline void ump_poly_pressure(uint8_t group, uint8_t channel,
                                      uint8_t pitch, uint32_t pressure) {
  uint32_t w0 = UMP_MT_MIDI2_CV | ((uint32_t)(group & 0x0F) << 24)
              | UMP_STATUS_POLY_PRESSURE | ((uint32_t)(channel & 0x0F) << 16)
              | ((uint32_t)(pitch & 0x7F) << 8);
  ump_send_64(w0, pressure);
}

// -- MIDI 2.0 Channel Voice: Per-Note Management (MT=0x4, 64-bit) --
// Exclusive to MIDI 2.0: controls per-note behavior
// Word 0: [MT(4):Group(4):Status(0xF):Channel(4):NoteNumber(8):Flags(8)]
// Word 1: Reserved
// Flags bit 1 = Reset (S), bit 0 = Detach (D)
static inline void ump_per_note_mgmt(uint8_t group, uint8_t channel,
                                      uint8_t pitch, bool detach,
                                      bool reset) {
  uint8_t flags = (reset ? 0x02 : 0x00) | (detach ? 0x01 : 0x00);
  uint32_t w0 = UMP_MT_MIDI2_CV | ((uint32_t)(group & 0x0F) << 24)
              | UMP_STATUS_PN_MGMT | ((uint32_t)(channel & 0x0F) << 16)
              | ((uint32_t)(pitch & 0x7F) << 8)
              | (uint32_t)flags;
  ump_send_64(w0, 0x00000000);
}

//--------------------------------------------------------------------+
// Song Data
//--------------------------------------------------------------------+

// Extended note event with MIDI 2.0 expression data
typedef struct {
  uint8_t pitch;            // MIDI pitch (0-127, 0=rest)
  uint16_t duration_ms;     // Duration in ms
  uint16_t velocity;        // 16-bit velocity (MIDI 2.0)
  uint32_t pressure;        // 32-bit aftertouch (0 = none)
  int16_t  bend_cents;      // Pitch bend in cents (0 = none, for vibrato/ornaments)
} midi2_note_t;

// 16-bit velocity (MIDI 2.0): values that have NO 7-bit equivalent.
// MIDI 1.0 can only express 128 levels (0x0000, 0x0200, 0x0400 ... 0xFE00).
// These use the full 16-bit range to prove genuine MIDI 2.0 resolution.
#define V_PPP   0x0A3D   //  2621 - between MIDI1 vel 5 and 6
#define V_PP    0x1C71   //  7281 - between MIDI1 vel 14 and 15
#define V_P     0x3219   // 12825 - between MIDI1 vel 24 and 25
#define V_MP    0x4F5C   // 20316 - between MIDI1 vel 39 and 40
#define V_MF    0x6E93   // 28307 - between MIDI1 vel 55 and 56
#define V_F     0x8DA5   // 36261 - between MIDI1 vel 70 and 71
#define V_FF    0xAC37   // 44087 - between MIDI1 vel 85 and 86
#define V_FFF   0xDEB8   // 57016 - between MIDI1 vel 111 and 112

// Twinkle Twinkle Little Star - Traditional
// Tempo: 120 BPM (500ms per quarter note)
// Key: C major, 4/4
// Demonstrates all MIDI 2.0 Channel Voice features:
//   16-bit velocity, 32-bit CC, 32-bit pitch bend,
//   32-bit channel pressure, per-note poly pressure,
//   per-note management, program change with bank select,
//   JR timestamps
static const midi2_note_t song_data[] = {
  // Phrase 1: "Twin-kle twin-kle lit-tle star" (C C G G A A G-)
  // Crescendo pp -> mp, gentle entry
  { .pitch = 60, .duration_ms = 500, .velocity = V_PP,  .pressure = 0,          .bend_cents = 0 },   // C4
  { .pitch = 60, .duration_ms = 500, .velocity = V_P,   .pressure = 0,          .bend_cents = 0 },   // C4
  { .pitch = 67, .duration_ms = 500, .velocity = V_MP,  .pressure = 0,          .bend_cents = 0 },   // G4
  { .pitch = 67, .duration_ms = 500, .velocity = V_MP,  .pressure = 0,          .bend_cents = 0 },   // G4
  { .pitch = 69, .duration_ms = 500, .velocity = V_MF,  .pressure = 0x1A3D7E5F, .bend_cents = 0 },   // A4 (32-bit pressure)
  { .pitch = 69, .duration_ms = 500, .velocity = V_MF,  .pressure = 0x2B851EB9, .bend_cents = 0 },   // A4 (pressure swell)
  { .pitch = 67, .duration_ms = 1000,.velocity = V_MF,  .pressure = 0x3C6EF373, .bend_cents = 7 },   // G4 (half, bend 7 cents)

  // Phrase 2: "How I won-der what you are" (F F E E D D C-)
  // mf, sustained
  { .pitch = 65, .duration_ms = 500, .velocity = V_MF,  .pressure = 0,          .bend_cents = 0 },   // F4
  { .pitch = 65, .duration_ms = 500, .velocity = V_MF,  .pressure = 0,          .bend_cents = 0 },   // F4
  { .pitch = 64, .duration_ms = 500, .velocity = V_MF,  .pressure = 0x1E4C2B7A, .bend_cents = 0 },   // E4
  { .pitch = 64, .duration_ms = 500, .velocity = V_MF,  .pressure = 0x2D5A8FC1, .bend_cents = 0 },   // E4 (aftertouch swell)
  { .pitch = 62, .duration_ms = 500, .velocity = V_MP,  .pressure = 0,          .bend_cents = 0 },   // D4
  { .pitch = 62, .duration_ms = 500, .velocity = V_MP,  .pressure = 0,          .bend_cents = 0 },   // D4
  { .pitch = 60, .duration_ms = 1000,.velocity = V_MP,  .pressure = 0,          .bend_cents = 0 },   // C4 (half, resolve)

  // Phrase 3: "Up a-bove the world so high" (G G F F E E D-)
  // f, building intensity
  { .pitch = 67, .duration_ms = 500, .velocity = V_F,   .pressure = 0,          .bend_cents = 0 },   // G4
  { .pitch = 67, .duration_ms = 500, .velocity = V_F,   .pressure = 0,          .bend_cents = 0 },   // G4
  { .pitch = 65, .duration_ms = 500, .velocity = V_F,   .pressure = 0x2F8A4E13, .bend_cents = 0 },   // F4
  { .pitch = 65, .duration_ms = 500, .velocity = V_MF,  .pressure = 0x41B2C9D7, .bend_cents = 0 },   // F4 (triggers poly pressure)
  { .pitch = 64, .duration_ms = 500, .velocity = V_MF,  .pressure = 0,          .bend_cents = 0 },   // E4
  { .pitch = 64, .duration_ms = 500, .velocity = V_MF,  .pressure = 0,          .bend_cents = 0 },   // E4
  { .pitch = 62, .duration_ms = 1000,.velocity = V_MF,  .pressure = 0x537DC2A6, .bend_cents = 13 },  // D4 (half, vibrato 13 cents)

  // Phrase 4: "Like a dia-mond in the sky" (G G F F E E D-)
  // ff, expressive peak
  { .pitch = 67, .duration_ms = 500, .velocity = V_FF,  .pressure = 0,          .bend_cents = 0 },   // G4
  { .pitch = 67, .duration_ms = 500, .velocity = V_FF,  .pressure = 0,          .bend_cents = 0 },   // G4
  { .pitch = 65, .duration_ms = 500, .velocity = V_F,   .pressure = 0x44E7B8D2, .bend_cents = 0 },   // F4 (triggers poly pressure)
  { .pitch = 65, .duration_ms = 500, .velocity = V_F,   .pressure = 0x56A3F14B, .bend_cents = 0 },   // F4 (triggers poly pressure)
  { .pitch = 64, .duration_ms = 500, .velocity = V_MF,  .pressure = 0x2C8E1F5A, .bend_cents = 0 },   // E4
  { .pitch = 64, .duration_ms = 500, .velocity = V_MF,  .pressure = 0x1D73A4E8, .bend_cents = 0 },   // E4
  { .pitch = 62, .duration_ms = 1000,.velocity = V_MF,  .pressure = 0x63F5B17D, .bend_cents = 19 },  // D4 (half, vibrato 19 cents)

  // Phrase 5: "Twin-kle twin-kle lit-tle star" (C C G G A A G-)
  // Diminuendo mf -> mp
  { .pitch = 60, .duration_ms = 500, .velocity = V_MF,  .pressure = 0,          .bend_cents = 0 },   // C4
  { .pitch = 60, .duration_ms = 500, .velocity = V_MF,  .pressure = 0,          .bend_cents = 0 },   // C4
  { .pitch = 67, .duration_ms = 500, .velocity = V_MP,  .pressure = 0,          .bend_cents = 0 },   // G4
  { .pitch = 67, .duration_ms = 500, .velocity = V_MP,  .pressure = 0,          .bend_cents = 0 },   // G4
  { .pitch = 69, .duration_ms = 500, .velocity = V_MP,  .pressure = 0x1B4F6D83, .bend_cents = 0 },   // A4
  { .pitch = 69, .duration_ms = 500, .velocity = V_P,   .pressure = 0x0E29C5A1, .bend_cents = 0 },   // A4
  { .pitch = 67, .duration_ms = 1000,.velocity = V_P,   .pressure = 0x2A6D3B9E, .bend_cents = 5 },   // G4 (half, gentle bend 5 cents)

  // Phrase 6: "How I won-der what you are" (F F E E D D C-)
  // Dying away mp -> ppp
  { .pitch = 65, .duration_ms = 500, .velocity = V_MP,  .pressure = 0,          .bend_cents = 0 },   // F4
  { .pitch = 65, .duration_ms = 500, .velocity = V_P,   .pressure = 0,          .bend_cents = 0 },   // F4
  { .pitch = 64, .duration_ms = 500, .velocity = V_P,   .pressure = 0,          .bend_cents = 0 },   // E4
  { .pitch = 64, .duration_ms = 500, .velocity = V_PP,  .pressure = 0,          .bend_cents = 0 },   // E4
  { .pitch = 62, .duration_ms = 500, .velocity = V_PP,  .pressure = 0,          .bend_cents = 0 },   // D4
  { .pitch = 62, .duration_ms = 500, .velocity = V_PPP, .pressure = 0,          .bend_cents = 0 },   // D4
  { .pitch = 60, .duration_ms = 2000,.velocity = V_PPP, .pressure = 0x07A1E3C9, .bend_cents = 0 },   // C4 (fermata)

  // Silence before loop
  { .pitch = 0,  .duration_ms = 1000,.velocity = 0,     .pressure = 0,          .bend_cents = 0 },

  // End marker
  { .pitch = 0,  .duration_ms = 0,   .velocity = 0,     .pressure = 0,          .bend_cents = 0 },
};

#define SONG_LENGTH (sizeof(song_data) / sizeof(midi2_note_t))

//--------------------------------------------------------------------+
// Song Playback State Machine
//--------------------------------------------------------------------+

typedef struct {
  uint32_t current_note_idx;
  uint32_t note_start_ms;
  uint8_t active_pitch;
  bool note_is_active;
  bool setup_sent;             // Initial setup (Program Change, CC) sent?
  uint32_t loop_count;
} song_state_t;

static song_state_t song = { 0 };

// Forward declarations
void update_song_playback(uint32_t now_ms);
void send_initial_setup(void);

//--------------------------------------------------------------------+
// MIDI 2.0 Device Callbacks (override weak stubs from middleware)
//--------------------------------------------------------------------+

void tud_midi2_rx_cb(uint8_t itf) {
  (void)itf;
}

//--------------------------------------------------------------------+
// Initial Setup - Program Change, CC, Per-Note Management
//--------------------------------------------------------------------+

void send_initial_setup(void) {
  ump_jr_timestamp(0x0001);
  ump_program_change(0, 0, 0, true, 0, 0);
  ump_cc(0, 0, 7,  0xCCCCCCCC);              // Volume 80% (32-bit)
  ump_cc(0, 0, 11, 0xFFFFFFFF);              // Expression 100%
  ump_cc(0, 0, 64, 0x00000000);              // Sustain off
  ump_cc(0, 0, 1,  0x20000000);              // Modulation
  ump_cc(0, 0, 10, 0x80000000);              // Pan center
  ump_per_note_mgmt(0, 0, 0, false, true);   // Per-Note reset
  ump_pitch_bend(0, 0, 0x80000000);          // Pitch Bend center
  ump_channel_pressure(0, 0, 0x00000000);

  printf("[SETUP] Piano | Vol 80%% | UMP\r\n");
}

//--------------------------------------------------------------------+
// Pitch Bend Conversion: cents to 32-bit value
//--------------------------------------------------------------------+

// Convert pitch bend in cents (-200 to +200) to 32-bit UMP value
// Center = 0x80000000, range = +/- 2 semitones (200 cents)
static inline uint32_t cents_to_pitch_bend(int16_t cents) {
  if (cents == 0) return 0x80000000;
  // Scale: 200 cents = full range (0x7FFFFFFF deviation from center)
  int32_t offset = (int32_t)(((int64_t)cents * 0x7FFFFFFF) / 200);
  return (uint32_t)((int32_t)0x80000000 + offset);
}

//--------------------------------------------------------------------+
// Song Playback Logic - Full MIDI 2.0 Expression
//--------------------------------------------------------------------+

void update_song_playback(uint32_t now_ms) {
  const midi2_note_t *current = &song_data[song.current_note_idx];

  if (!song.setup_sent) {
    send_initial_setup();
    song.setup_sent = true;
    song.note_start_ms = now_ms;
  }

  // Note duration elapsed: send Note Off, advance
  if (song.note_is_active && (now_ms - song.note_start_ms) >= current->duration_ms) {
    if (song.active_pitch > 0) {
      if (current->bend_cents != 0) ump_pitch_bend(0, 0, 0x80000000);
      if (current->pressure > 0)    ump_channel_pressure(0, 0, 0x00000000);
      ump_note_off(0, 0, song.active_pitch, V_P, UMP_ATTR_NONE, 0);
    }

    song.note_is_active = false;
    song.current_note_idx++;

    if (song.current_note_idx >= SONG_LENGTH) {
      song.current_note_idx = 0;
      song.setup_sent = false;
      song.loop_count++;
      printf("\r\n=== Loop %lu ===\r\n", (unsigned long)song.loop_count);
    }

    song.note_start_ms = now_ms;
  }

  // Start next note
  if (!song.note_is_active && song.current_note_idx < SONG_LENGTH) {
    const midi2_note_t *next = &song_data[song.current_note_idx];

    if (next->duration_ms == 0) {
      song.current_note_idx = 0;
      song.setup_sent = false;
      song.loop_count++;
      printf("\r\n=== Loop %lu ===\r\n", (unsigned long)song.loop_count);
      return;
    }

    if (next->pitch > 0) {
      ump_jr_timestamp((uint16_t)(now_ms & 0xFFFF));
      if (next->bend_cents != 0) {
        ump_pitch_bend(0, 0, cents_to_pitch_bend(next->bend_cents));
      }
      ump_note_on(0, 0, next->pitch, next->velocity, UMP_ATTR_NONE, 0);
      if (next->pressure > 0) {
        ump_channel_pressure(0, 0, next->pressure);
      }
      if (next->pressure > 0x40000000 && next->duration_ms > 500) {
        ump_poly_pressure(0, 0, next->pitch, next->pressure);
      }

      song.active_pitch = next->pitch;
      song.note_is_active = true;
    }

    // Rest: honor duration
    if (!song.note_is_active) {
      song.active_pitch = 0;
      song.note_is_active = true;
      song.note_start_ms = now_ms;
    }
  }
}

//--------------------------------------------------------------------+
// Main
//--------------------------------------------------------------------+

int main(void) {
  board_init();
  printf("\r\n");
  printf("===========================================\r\n");
  printf("  RP2040 MIDI 2.0 Device\r\n");
  printf("===========================================\r\n");
  printf("Tempo: 120 BPM | Format: UMP 64-bit\r\n");
  printf("Song: %u notes with full MIDI 2.0 expression\r\n",
         (unsigned)SONG_LENGTH);
  printf("Features:\r\n");
  printf("  - 16-bit Velocity (vs 7-bit MIDI 1.0)\r\n");
  printf("  - 32-bit Control Change\r\n");
  printf("  - 32-bit Pitch Bend (vs 14-bit MIDI 1.0)\r\n");
  printf("  - 32-bit Channel Pressure\r\n");
  printf("  - 32-bit Poly Pressure (per-note)\r\n");
  printf("  - Per-Note Management (MIDI 2.0 exclusive)\r\n");
  printf("  - Program Change with Bank Select\r\n");
  printf("  - JR Timestamps\r\n");
  printf("Status: Initializing...\r\n");

  tusb_rhport_init_t dev_init = {.role = TUSB_ROLE_DEVICE, .speed = TUSB_SPEED_AUTO};
  tusb_init(BOARD_TUD_RHPORT, &dev_init);

  board_init_after_tusb();
  board_led_write(true);

  uint32_t last_report_ms = 0;

  while (1) {
    tud_task();

    uint32_t now_ms = tusb_time_millis_api();

    if (tud_midi2_mounted()) {
      update_song_playback(now_ms);
      board_led_write(song.active_pitch > 0);
    } else {
      board_led_write((now_ms / 500) & 1);
    }

    // Status report every 10 seconds
    if (now_ms - last_report_ms > 10000) {
      last_report_ms = now_ms;
      if (tud_midi2_mounted()) {
        printf("[%lums] Playing idx %lu/%u loop %lu\r\n",
               (unsigned long)now_ms,
               (unsigned long)song.current_note_idx,
               (unsigned)SONG_LENGTH,
               (unsigned long)song.loop_count);
      } else {
        printf("[%lums] Waiting for host...\r\n", (unsigned long)now_ms);
      }
    }
  }

  return 0;
}

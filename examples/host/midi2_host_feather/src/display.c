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

// SSD1306 OLED display driver (128x64, I2C) for MIDI 2.0 Host example.
// Adafruit Feather RP2040 USB Host: I2C1 via STEMMA QT (SDA = GP2, SCL = GP3)
//
// Three display phases:
//   1. Splash     : title + credits (shown during init)
//   2. Connecting : spinner animation while waiting for device
//   3. Live       : header + 6 scrolling log lines + status bar

#include "display.h"
#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/gpio.h"

#define I2C_PORT     i2c1
#define I2C_SDA      2
#define I2C_SCL      3
#define I2C_FREQ     400000
#define SSD1306_ADDR 0x3C

#define SCR_W        128
#define SCR_H        64
#define PAGES        (SCR_H / 8)
#define CHARS_PER_LINE 21

//--------------------------------------------------------------------+
// Framebuffer
//--------------------------------------------------------------------+

static uint8_t fb[SCR_W * PAGES];

//--------------------------------------------------------------------+
// Log buffer (6 lines for live view)
//--------------------------------------------------------------------+

#define LOG_LINES    6
static char log_lines[LOG_LINES][CHARS_PER_LINE + 1];
static int log_count = 0;

//--------------------------------------------------------------------+
// Minimal 5x7 font (ASCII 32-126)
//--------------------------------------------------------------------+

#include "font5x7.h"

//--------------------------------------------------------------------+
// SSD1306 I2C low-level
//--------------------------------------------------------------------+

static void ssd_cmd(uint8_t cmd) {
  uint8_t buf[2] = { 0x00, cmd };
  i2c_write_blocking(I2C_PORT, SSD1306_ADDR, buf, 2, false);
}

static void ssd_data(const uint8_t* data, size_t len) {
  uint8_t buf[SCR_W + 1];
  buf[0] = 0x40;
  size_t chunk = (len > SCR_W) ? SCR_W : len;
  memcpy(buf + 1, data, chunk);
  i2c_write_blocking(I2C_PORT, SSD1306_ADDR, buf, chunk + 1, false);
}

static void ssd_flush(void) {
  ssd_cmd(0x21); ssd_cmd(0); ssd_cmd(127);
  ssd_cmd(0x22); ssd_cmd(0); ssd_cmd(7);
  for (int page = 0; page < PAGES; page++) {
    ssd_data(&fb[page * SCR_W], SCR_W);
  }
}

//--------------------------------------------------------------------+
// Framebuffer drawing
//--------------------------------------------------------------------+

static void fb_clear(void) {
  memset(fb, 0, sizeof(fb));
}

static void fb_char(int x, int y, char c) {
  if (c < 32 || c > 126) c = '?';
  const uint8_t* glyph = font5x7 + (c - 32) * 5;
  int page = y / 8;
  int bit_offset = y % 8;

  if (page >= PAGES || x + 5 > SCR_W) return;

  for (int col = 0; col < 5; col++) {
    uint8_t column_data = glyph[col];
    fb[(page * SCR_W) + x + col] |= (uint8_t)(column_data << bit_offset);
    if (bit_offset > 0 && page + 1 < PAGES) {
      fb[((page + 1) * SCR_W) + x + col] |= (uint8_t)(column_data >> (8 - bit_offset));
    }
  }
}

static void fb_string(int x, int y, const char* str) {
  while (*str) {
    fb_char(x, y, *str);
    x += 6;
    if (x + 6 > SCR_W) break;
    str++;
  }
}

// Draw a horizontal line (1px)
static void fb_hline(int x0, int x1, int y) {
  int page = y / 8;
  uint8_t mask = (uint8_t)(1 << (y % 8));
  if (page >= PAGES) return;
  for (int x = x0; x <= x1 && x < SCR_W; x++) {
    fb[page * SCR_W + x] |= mask;
  }
}

//--------------------------------------------------------------------+
// Phase 1: Splash
//--------------------------------------------------------------------+

void display_init(void) {
  i2c_init(I2C_PORT, I2C_FREQ);
  gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
  gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
  gpio_pull_up(I2C_SDA);
  gpio_pull_up(I2C_SCL);

  sleep_ms(100);

  // SSD1306 init sequence
  ssd_cmd(0xAE);
  ssd_cmd(0xD5); ssd_cmd(0x80);
  ssd_cmd(0xA8); ssd_cmd(0x3F);
  ssd_cmd(0xD3); ssd_cmd(0x00);
  ssd_cmd(0x40);
  ssd_cmd(0x8D); ssd_cmd(0x14);
  ssd_cmd(0x20); ssd_cmd(0x00);
  ssd_cmd(0xA1);
  ssd_cmd(0xC8);
  ssd_cmd(0xDA); ssd_cmd(0x12);
  ssd_cmd(0x81); ssd_cmd(0xCF);
  ssd_cmd(0xD9); ssd_cmd(0xF1);
  ssd_cmd(0xDB); ssd_cmd(0x40);
  ssd_cmd(0xA4);
  ssd_cmd(0xA6);
  ssd_cmd(0xAF);

  // Splash screen
  fb_clear();
  fb_string(4,  8, "TinyUSB MIDI 2.0");
  fb_hline(4, 123, 18);
  fb_string(16, 24, "USB Host Demo");
  fb_string(4, 40, "Feather RP2040");
  fb_string(4, 52, "PIO-USB + SSD1306");
  ssd_flush();
}

//--------------------------------------------------------------------+
// Phase 2: Connecting (spinner)
//--------------------------------------------------------------------+

static const char SPINNER[] = "|/-\\";

void display_connecting(uint32_t elapsed_ms) {
  int idx = (int)((elapsed_ms / 200) % 4);

  // Clear bottom 2 pages for spinner area
  memset(&fb[6 * SCR_W], 0, SCR_W);
  memset(&fb[7 * SCR_W], 0, SCR_W);

  char line[CHARS_PER_LINE + 1];
  snprintf(line, sizeof(line), "%c Waiting device...", SPINNER[idx]);
  fb_string(4, 52, line);
  ssd_flush();
}

//--------------------------------------------------------------------+
// Phase 3: Live view
//--------------------------------------------------------------------+

// Layout:
//   y=0  : "TinyUSB MIDI 2.0 Host" (header, fixed)
//   y=9  : separator line
//   y=10 : log line 0
//   y=18 : log line 1
//   y=26 : log line 2
//   y=34 : log line 3
//   y=42 : log line 4
//   y=50 : log line 5
//   y=57 : separator line
//   y=58 : status bar

static bool live_mode = false;

static void draw_live_frame(void) {
  fb_clear();
  fb_string(0, 0, "TinyUSB MIDI 2.0 Host");
  fb_hline(0, 127, 9);
  fb_hline(0, 127, 57);
}

void display_live_begin(void) {
  live_mode = true;
  log_count = 0;
  memset(log_lines, 0, sizeof(log_lines));

  draw_live_frame();
  fb_string(0, 58, "Connected");
  ssd_flush();
}

void display_log(const char* text, uint16_t color) {
  (void)color;

  if (!live_mode) {
    display_live_begin();
  }

  // Scroll up if full
  if (log_count >= LOG_LINES) {
    for (int i = 0; i < LOG_LINES - 1; i++) {
      strncpy(log_lines[i], log_lines[i + 1], CHARS_PER_LINE);
      log_lines[i][CHARS_PER_LINE] = '\0';
    }
    log_count = LOG_LINES - 1;
  }

  strncpy(log_lines[log_count], text, CHARS_PER_LINE);
  log_lines[log_count][CHARS_PER_LINE] = '\0';
  log_count++;

  // Redraw: header + log + status
  draw_live_frame();
  for (int i = 0; i < log_count && i < LOG_LINES; i++) {
    fb_string(0, 10 + i * 8, log_lines[i]);
  }
  ssd_flush();
}

void display_status(const char* text) {
  if (!live_mode) return;

  // Clear status area (page 7)
  memset(&fb[7 * SCR_W], 0, SCR_W);
  // Redraw separator (might have been cleared)
  fb_hline(0, 127, 57);

  char status[CHARS_PER_LINE + 1];
  strncpy(status, text, CHARS_PER_LINE);
  status[CHARS_PER_LINE] = '\0';
  fb_string(0, 58, status);
  ssd_flush();
}

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
// Minimal text-only implementation, no graphics library.
// I2C0: SDA = GP4, SCL = GP5, Address = 0x3C

#include "display.h"
#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/gpio.h"

#define I2C_PORT     i2c0
#define I2C_SDA      4
#define I2C_SCL      5
#define I2C_FREQ     400000
#define SSD1306_ADDR 0x3C

#define SCR_W        128
#define SCR_H        64
#define PAGES        (SCR_H / 8)   // 8 pages
#define CHARS_PER_LINE 21          // 128 / 6 = 21 chars

// Framebuffer
static uint8_t fb[SCR_W * PAGES];

// Log buffer
#define LOG_LINES    3
static char log_lines[LOG_LINES][CHARS_PER_LINE + 1];
static int log_count = 0;

// Status line
static char status_text[CHARS_PER_LINE + 1] = "";

//--------------------------------------------------------------------+
// Minimal 5x7 font (ASCII 32-126)
//--------------------------------------------------------------------+
#include "font5x7.h"

//--------------------------------------------------------------------+
// SSD1306 I2C commands
//--------------------------------------------------------------------+

static void ssd_cmd(uint8_t cmd) {
  uint8_t buf[2] = { 0x00, cmd };  // Co=0, D/C=0
  i2c_write_blocking(I2C_PORT, SSD1306_ADDR, buf, 2, false);
}

static void ssd_data(const uint8_t* data, size_t len) {
  uint8_t buf[SCR_W + 1];
  buf[0] = 0x40;  // Co=0, D/C=1
  size_t chunk = (len > SCR_W) ? SCR_W : len;
  memcpy(buf + 1, data, chunk);
  i2c_write_blocking(I2C_PORT, SSD1306_ADDR, buf, chunk + 1, false);
}

static void ssd_flush(void) {
  ssd_cmd(0x21); ssd_cmd(0); ssd_cmd(127);  // Column range
  ssd_cmd(0x22); ssd_cmd(0); ssd_cmd(7);    // Page range
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

//--------------------------------------------------------------------+
// Display API
//--------------------------------------------------------------------+

void display_init(void) {
  i2c_init(I2C_PORT, I2C_FREQ);
  gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
  gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
  gpio_pull_up(I2C_SDA);
  gpio_pull_up(I2C_SCL);

  sleep_ms(100);

  // SSD1306 init sequence
  ssd_cmd(0xAE);        // Display off
  ssd_cmd(0xD5); ssd_cmd(0x80);  // Clock div
  ssd_cmd(0xA8); ssd_cmd(0x3F);  // Multiplex 64
  ssd_cmd(0xD3); ssd_cmd(0x00);  // Display offset
  ssd_cmd(0x40);        // Start line 0
  ssd_cmd(0x8D); ssd_cmd(0x14);  // Charge pump on
  ssd_cmd(0x20); ssd_cmd(0x00);  // Horizontal addressing
  ssd_cmd(0xA1);        // Segment remap
  ssd_cmd(0xC8);        // COM scan direction
  ssd_cmd(0xDA); ssd_cmd(0x12);  // COM pins
  ssd_cmd(0x81); ssd_cmd(0xCF);  // Contrast
  ssd_cmd(0xD9); ssd_cmd(0xF1);  // Pre-charge
  ssd_cmd(0xDB); ssd_cmd(0x40);  // VCOMH deselect
  ssd_cmd(0xA4);        // Display from RAM
  ssd_cmd(0xA6);        // Normal display
  ssd_cmd(0xAF);        // Display on

  fb_clear();
  fb_string(0, 0, "MIDI 2.0 Host");
  ssd_flush();
}

void display_checklist_update(const checklist_t* ck) {
  struct { const char* label; bool ok; } items[] = {
    { "PWR",       ck->pwr_on },
    { "TinyUSB",   ck->tusb_init },
    { "USB bus",   ck->bus_active },
    { "Device",    ck->device_connected },
    { "Descript",  ck->descriptor_parsed },
    { "Alt1 UMP",  ck->alt_setting_ok },
    { "Mount",     ck->mounted },
    { "RX UMP",    ck->receiving },
  };

  // Clear checklist area (lines 1-4, two columns)
  for (int p = 1; p <= 4; p++) {
    memset(&fb[p * SCR_W], 0, SCR_W);
  }

  for (int i = 0; i < 8; i++) {
    int col = (i < 4) ? 0 : 64;
    int row = (i < 4) ? i : i - 4;
    int y = 9 + row * 8;
    char line[12];
    snprintf(line, sizeof(line), "%s %s", items[i].ok ? "OK" : "..", items[i].label);
    fb_string(col, y, line);
  }

  ssd_flush();
}

void display_log(const char* text, uint16_t color) {
  (void)color;  // SSD1306 is monochrome

  if (log_count >= LOG_LINES) {
    for (int i = 0; i < LOG_LINES - 1; i++) {
      strncpy(log_lines[i], log_lines[i + 1], CHARS_PER_LINE);
    }
    log_count = LOG_LINES - 1;
  }

  strncpy(log_lines[log_count], text, CHARS_PER_LINE);
  log_lines[log_count][CHARS_PER_LINE] = '\0';
  log_count++;

  // Draw log area (pages 5-6, y=40-55)
  memset(&fb[5 * SCR_W], 0, SCR_W);
  memset(&fb[6 * SCR_W], 0, SCR_W);

  for (int i = 0; i < log_count && i < LOG_LINES; i++) {
    fb_string(0, 41 + i * 8, log_lines[i]);
  }

  ssd_flush();
}

void display_status(const char* text) {
  strncpy(status_text, text, CHARS_PER_LINE);
  status_text[CHARS_PER_LINE] = '\0';

  // Status on last page (y=56)
  memset(&fb[7 * SCR_W], 0, SCR_W);
  fb_string(0, 56, status_text);
  ssd_flush();
}

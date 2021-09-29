/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
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
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "bsp/board.h"
#include "tusb.h"

#include "images.h"
//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF PROTYPES
//--------------------------------------------------------------------+

/* Blink pattern
 * - 250 ms  : device not mounted
 * - 1000 ms : device mounted
 * - 2500 ms : device is suspended
 */
enum  {
  BLINK_NOT_MOUNTED = 250,
  BLINK_MOUNTED = 1000,
  BLINK_SUSPENDED = 2500,
};

static uint32_t blink_interval_ms = BLINK_NOT_MOUNTED;

void led_blinking_task(void);
void video_task(void);

/*------------- MAIN -------------*/
int main(void)
{
  board_init();
  tusb_init();

  while (1)
  {
    tud_task(); // tinyusb device task
    led_blinking_task();

    video_task();
  }

  return 0;
}

//--------------------------------------------------------------------+
// Device callbacks
//--------------------------------------------------------------------+

// Invoked when device is mounted
void tud_mount_cb(void)
{
  blink_interval_ms = BLINK_MOUNTED;
}

// Invoked when device is unmounted
void tud_umount_cb(void)
{
  blink_interval_ms = BLINK_NOT_MOUNTED;
}

// Invoked when usb bus is suspended
// remote_wakeup_en : if host allow us  to perform remote wakeup
// Within 7ms, device must draw an average of current less than 2.5 mA from bus
void tud_suspend_cb(bool remote_wakeup_en)
{
  (void) remote_wakeup_en;
  blink_interval_ms = BLINK_SUSPENDED;
}

// Invoked when usb bus is resumed
void tud_resume_cb(void)
{
  blink_interval_ms = BLINK_MOUNTED;
}


//--------------------------------------------------------------------+
// USB Video
//--------------------------------------------------------------------+
static unsigned char const *frames[] = {
  white_128x96_yuv, white_128x96_yuv, white_128x96_yuv,
  t_128x96_yuv, ti_128x96_yuv, tin_128x96_yuv, logo_128x96_yuv,
  logo_128x96_yuv, logo_128x96_yuv, logo_128x96_yuv, logo_128x96_yuv
};
static unsigned current_frame = 0;
static unsigned tx_busy = 0;
static unsigned interval_ms = 1000 / FRAME_RATE;

void video_task(void)
{
  static unsigned start_ms = 0;
  static unsigned already_sent = 0;

  if (!tud_video_n_streaming(0, 0)) {
    already_sent  = 0;
    current_frame = 0;
    return;
  }

  if (!already_sent) {
    already_sent = 1;
    start_ms = board_millis();
    tud_video_n_frame_xfer(0, 0, (void*)frames[current_frame], FRAME_WIDTH * FRAME_HEIGHT * 12/8);
  }

  unsigned cur = board_millis();
  if (cur - start_ms < interval_ms) return; // not enough time
  if (tx_busy) return;
  start_ms += interval_ms;

  tud_video_n_frame_xfer(0, 0, (void*)frames[current_frame], FRAME_WIDTH * FRAME_HEIGHT * 12/8);
}

void tud_video_frame_xfer_complete_cb(uint_fast8_t ctl_idx, uint_fast8_t stm_idx)
{
  (void)ctl_idx; (void)stm_idx;
  tx_busy = 0;
  /* flip buffer */
  ++current_frame;
  if (current_frame == sizeof(frames)/sizeof(frames[0]))
    current_frame = 0;
}

int tud_video_commit_cb(uint_fast8_t ctl_idx, uint_fast8_t stm_idx,
			video_probe_and_commit_control_t const *parameters)
{
  /* convert unit to ms from 100 ns */
  interval_ms = parameters->dwFrameInterval / 10000;
  return VIDEO_NO_ERROR;
}

//--------------------------------------------------------------------+
// BLINKING TASK
//--------------------------------------------------------------------+
void led_blinking_task(void)
{
  static uint32_t start_ms = 0;
  static bool led_state = false;

  // Blink every interval ms
  if ( board_millis() - start_ms < blink_interval_ms) return; // not enough time
  start_ms += blink_interval_ms;

  board_led_write(led_state);
  led_state = 1 - led_state; // toggle
}

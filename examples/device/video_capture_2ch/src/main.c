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

#include "bsp/board_api.h"
#include "tusb.h"
#include "usb_descriptors.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF PROTYPES
//--------------------------------------------------------------------+

/* Blink pattern
 * - 250 ms  : device not mounted
 * - 1000 ms : device mounted
 * - 2500 ms : device is suspended
 */
enum {
  BLINK_NOT_MOUNTED = 250,
  BLINK_MOUNTED = 1000,
  BLINK_SUSPENDED = 2500,
};

static uint32_t blink_interval_ms = BLINK_NOT_MOUNTED;

void led_blinking_task(void* param);
void usb_device_task(void *param);
void video_task(void* param);

#if CFG_TUSB_OS == OPT_OS_FREERTOS
void freertos_init_task(void);
#endif


//--------------------------------------------------------------------+
// Main
//--------------------------------------------------------------------+
int main(void) {
  board_init();

  // If using FreeRTOS: create blinky, tinyusb device, video task
#if CFG_TUSB_OS == OPT_OS_FREERTOS
  freertos_init_task();
#else
  // init device stack on configured roothub port
  tud_init(BOARD_TUD_RHPORT);

  if (board_init_after_tusb) {
    board_init_after_tusb();
  }

  while (1) {
    tud_task(); // tinyusb device task
    led_blinking_task(NULL);
    video_task(NULL);
  }
#endif
}

//--------------------------------------------------------------------+
// Device callbacks
//--------------------------------------------------------------------+

// Invoked when device is mounted
void tud_mount_cb(void) {
  blink_interval_ms = BLINK_MOUNTED;
}

// Invoked when device is unmounted
void tud_umount_cb(void) {
  blink_interval_ms = BLINK_NOT_MOUNTED;
}

// Invoked when usb bus is suspended
// remote_wakeup_en : if host allow us  to perform remote wakeup
// Within 7ms, device must draw an average of current less than 2.5 mA from bus
void tud_suspend_cb(bool remote_wakeup_en) {
  (void) remote_wakeup_en;
  blink_interval_ms = BLINK_SUSPENDED;
}

// Invoked when usb bus is resumed
void tud_resume_cb(void) {
  blink_interval_ms = tud_mounted() ? BLINK_MOUNTED : BLINK_NOT_MOUNTED;
}

//--------------------------------------------------------------------+
// USB Video
//--------------------------------------------------------------------+
#define FRAMEBUF_SIZE (FRAME_WIDTH * FRAME_HEIGHT * 16 / 8)

static unsigned frame_num[CFG_TUD_VIDEO_STREAMING] = {1};
static unsigned tx_busy = 0;
static unsigned interval_ms[CFG_TUD_VIDEO_STREAMING] = {1000 / FRAME_RATE};

// For mcus that does not have enough SRAM for frame buffer, we use fixed frame data.
// To further reduce the size, we use MJPEG format instead of YUY2.
#include "images.h"

static struct {
  uint32_t       size;
  uint8_t const *buffer;
} const framebuf_mjpeg[] = {
    {sizeof(color_bar_0_jpg), color_bar_0_jpg},
    {sizeof(color_bar_1_jpg), color_bar_1_jpg},
    {sizeof(color_bar_2_jpg), color_bar_2_jpg},
    {sizeof(color_bar_3_jpg), color_bar_3_jpg},
    {sizeof(color_bar_4_jpg), color_bar_4_jpg},
    {sizeof(color_bar_5_jpg), color_bar_5_jpg},
    {sizeof(color_bar_6_jpg), color_bar_6_jpg},
    {sizeof(color_bar_7_jpg), color_bar_7_jpg},
};

#if !defined(CFG_EXAMPLE_VIDEO_READONLY)
// YUY2 frame buffer
static uint8_t framebuf_yuy2[FRAMEBUF_SIZE];

static void fill_color_bar(uint8_t* buffer, unsigned start_position) {
  /* EBU color bars: https://stackoverflow.com/questions/6939422 */
  static uint8_t const bar_color[8][4] = {
      /*  Y,   U,   Y,   V */
      { 235, 128, 235, 128}, /* 100% White */
      { 219,  16, 219, 138}, /* Yellow */
      { 188, 154, 188,  16}, /* Cyan */
      { 173,  42, 173,  26}, /* Green */
      {  78, 214,  78, 230}, /* Magenta */
      {  63, 102,  63, 240}, /* Red */
      {  32, 240,  32, 118}, /* Blue */
      {  16, 128,  16, 128}, /* Black */
  };
  uint8_t* p;

  /* Generate the 1st line */
  uint8_t* end = &buffer[FRAME_WIDTH * 2];
  unsigned idx = (FRAME_WIDTH / 2 - 1) - (start_position % (FRAME_WIDTH / 2));
  p = &buffer[idx * 4];
  for (unsigned i = 0; i < 8; ++i) {
    for (int j = 0; j < FRAME_WIDTH / (2 * 8); ++j) {
      memcpy(p, &bar_color[i], 4);
      p += 4;
      if (end <= p) {
        p = buffer;
      }
    }
  }

  /* Duplicate the 1st line to the others */
  p = &buffer[FRAME_WIDTH * 2];
  for (unsigned i = 1; i < FRAME_HEIGHT; ++i) {
    memcpy(p, buffer, FRAME_WIDTH * 2);
    p += FRAME_WIDTH * 2;
  }
}
#endif

size_t get_framebuf(uint_fast8_t ctl_idx, uint_fast8_t stm_idx, size_t fnum, void **fb) {
  uint32_t idx = ctl_idx + stm_idx;

  if (idx == 0) {
    // stream 0 use uncompressed YUY2 frame
    #if defined(CFG_EXAMPLE_VIDEO_READONLY)
    *fb = (void*)(uintptr_t ) &framebuf_yuy2_readonly[(fnum % (FRAME_WIDTH / 2)) * 4];
    #else
    fill_color_bar(framebuf_yuy2, frame_num[idx]);
    *fb = framebuf_yuy2;
    #endif

    return FRAMEBUF_SIZE;
  }else {
    // stream 1 use MJPEG frame
    size_t const bar_id = fnum & 0x7;

    *fb = (void*)(uintptr_t) framebuf_mjpeg[bar_id].buffer;
    return framebuf_mjpeg[bar_id].size;
  }
}

//--------------------------------------------------------------------+
//
//--------------------------------------------------------------------+

void video_send_frame(uint_fast8_t ctl_idx, uint_fast8_t stm_idx) {
  static unsigned start_ms[CFG_TUD_VIDEO_STREAMING] = {0, };
  static unsigned already_sent = 0;

  uint32_t idx = ctl_idx + stm_idx;
  if (!tud_video_n_streaming(ctl_idx, stm_idx)) {
    already_sent &= ~(1u << idx);
    frame_num[idx] = 0;
    return;
  }
  void* fp;
  size_t fb_size;

  if (!(already_sent & (1u << idx))) {
    already_sent |= 1u << idx;
    tx_busy |= 1u << idx;
    start_ms[idx] = board_millis();

    fb_size = get_framebuf(ctl_idx, stm_idx, frame_num[idx], &fp);
    tud_video_n_frame_xfer(ctl_idx, stm_idx, fp, fb_size);
  }

  unsigned cur = board_millis();
  if (cur - start_ms[idx] < interval_ms[idx]) return; // not enough time
  if (tx_busy & (1u << idx)) return;
  start_ms[idx] += interval_ms[idx];
  tx_busy |= 1u << idx;

  fb_size = get_framebuf(ctl_idx, stm_idx, frame_num[idx], &fp);
  tud_video_n_frame_xfer(ctl_idx, stm_idx, fp, fb_size);
}


void video_task(void* param) {
  (void) param;

  while(1) {
    video_send_frame(0, 0);
    video_send_frame(1, 0);

    #if CFG_TUSB_OS == OPT_OS_FREERTOS
    vTaskDelay(interval_ms[0] / portTICK_PERIOD_MS);
    #else
    return;
    #endif
  }
}

void tud_video_frame_xfer_complete_cb(uint_fast8_t ctl_idx, uint_fast8_t stm_idx) {
  uint32_t idx = ctl_idx + stm_idx;
  tx_busy &= ~(1u << idx);
  /* flip buffer */
  ++frame_num[idx];
}

int tud_video_commit_cb(uint_fast8_t ctl_idx, uint_fast8_t stm_idx,
                        video_probe_and_commit_control_t const* parameters) {
  uint32_t idx = ctl_idx + stm_idx;
  /* convert unit to ms from 100 ns */
  interval_ms[idx] = parameters->dwFrameInterval / 10000;
  return VIDEO_ERROR_NONE;
}

//--------------------------------------------------------------------+
// Blinking Task
//--------------------------------------------------------------------+
void led_blinking_task(void* param) {
  (void) param;
  static uint32_t start_ms = 0;
  static bool led_state = false;

  while (1) {
    #if CFG_TUSB_OS == OPT_OS_FREERTOS
    vTaskDelay(blink_interval_ms / portTICK_PERIOD_MS);
    #else
    if (board_millis() - start_ms < blink_interval_ms) return; // not enough time
    #endif

    start_ms += blink_interval_ms;
    board_led_write(led_state);
    led_state = 1 - led_state; // toggle
  }
}

//--------------------------------------------------------------------+
// FreeRTOS
//--------------------------------------------------------------------+
#if CFG_TUSB_OS == OPT_OS_FREERTOS

#define BLINKY_STACK_SIZE   configMINIMAL_STACK_SIZE
#define VIDEO_STACK_SIZE    (configMINIMAL_STACK_SIZE*4)

#if TU_CHECK_MCU(OPT_MCU_ESP32S2, OPT_MCU_ESP32S3)
  #define USBD_STACK_SIZE     4096
  int main(void);
  void app_main(void) {
    main();
  }
#else
  // Increase stack size when debug log is enabled
  #define USBD_STACK_SIZE    (3*configMINIMAL_STACK_SIZE/2) * (CFG_TUSB_DEBUG ? 2 : 1)
#endif

// static task
#if configSUPPORT_STATIC_ALLOCATION
StackType_t blinky_stack[BLINKY_STACK_SIZE];
StaticTask_t blinky_taskdef;

StackType_t  usb_device_stack[USBD_STACK_SIZE];
StaticTask_t usb_device_taskdef;

StackType_t  video_stack[VIDEO_STACK_SIZE];
StaticTask_t video_taskdef;
#endif

// USB Device Driver task
// This top level thread process all usb events and invoke callbacks
void usb_device_task(void *param) {
  (void) param;

  // init device stack on configured roothub port
  // This should be called after scheduler/kernel is started.
  // Otherwise, it could cause kernel issue since USB IRQ handler does use RTOS queue API.
  tud_init(BOARD_TUD_RHPORT);

  if (board_init_after_tusb) {
    board_init_after_tusb();
  }

  // RTOS forever loop
  while (1) {
    // put this thread to waiting state until there is new events
    tud_task();
  }
}

void freertos_init_task(void) {
  #if configSUPPORT_STATIC_ALLOCATION
  xTaskCreateStatic(led_blinking_task, "blinky", BLINKY_STACK_SIZE, NULL, 1, blinky_stack, &blinky_taskdef);
  xTaskCreateStatic(usb_device_task, "usbd", USBD_STACK_SIZE, NULL, configMAX_PRIORITIES-1, usb_device_stack, &usb_device_taskdef);
  xTaskCreateStatic(video_task, "cdc", VIDEO_STACK_SIZE, NULL, configMAX_PRIORITIES - 2, video_stack, &video_taskdef);
  #else
  xTaskCreate(led_blinking_task, "blinky", BLINKY_STACK_SIZE, NULL, 1, NULL);
  xTaskCreate(usb_device_task, "usbd", USBD_STACK_SIZE, NULL, configMAX_PRIORITIES - 1, NULL);
  xTaskCreate(video_task, "video", VIDEO_STACK_SZIE, NULL, configMAX_PRIORITIES - 2, NULL);
  #endif

  // skip starting scheduler (and return) for ESP32-S2 or ESP32-S3
  #if !TU_CHECK_MCU(OPT_MCU_ESP32S2, OPT_MCU_ESP32S3)
  vTaskStartScheduler();
  #endif
}
#endif

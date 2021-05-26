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

 /*
  * After device is enumerated in dfu mode run the following commands
  *
  * To transfer firmware from host to device:
  *
  * $ dfu-util -D [filename]
  *
  * To transfer firmware from device to host:
  *
  * $ dfu-util -U [filename]
  *
  */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "bsp/board.h"
#include "tusb.h"


//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF PROTYPES
//--------------------------------------------------------------------+
#ifndef DFU_VERBOSE
#define DFU_VERBOSE 0
#endif

/* Blink pattern
 * - 1000 ms : device should reboot
 * - 250 ms  : device not mounted
 * - 1000 ms : device mounted
 * - 2500 ms : device is suspended
 */
enum  {
  BLINK_DFU_MODE = 100,
  BLINK_NOT_MOUNTED = 250,
  BLINK_MOUNTED = 1000,
  BLINK_SUSPENDED = 2500,
};

static uint32_t blink_interval_ms = BLINK_NOT_MOUNTED;

void led_blinking_task(void);

/*------------- MAIN -------------*/
int main(void)
{
  board_init();

  tusb_init();

  while (1)
  {
    tud_task(); // tinyusb device task
    led_blinking_task();
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

// Invoked on DFU_DETACH request to reboot to the bootloader
void tud_dfu_runtime_reboot_to_dfu_cb(void)
{
  blink_interval_ms = BLINK_DFU_MODE;
}

//--------------------------------------------------------------------+
// Class callbacks
//--------------------------------------------------------------------+
bool tud_dfu_firmware_valid_check_cb(void)
{
  printf("    Firmware check\r\n");
  return true;
}

void tud_dfu_req_dnload_data_cb(uint16_t wBlockNum, uint8_t* data, uint16_t length)
{
  (void) data;
  printf("    Received BlockNum %u of length %u\r\n", wBlockNum, length);

#if DFU_VERBOSE
  for(uint16_t i=0; i<length; i++)
  {
    printf("    [%u][%u]: %x\r\n", wBlockNum, i, (uint8_t)data[i]);
  }
#endif

  tud_dfu_dnload_complete();
}

bool tud_dfu_device_data_done_check_cb(void)
{
  printf("    Host said no more data... Returning true\r\n");
  return true;
}

void tud_dfu_abort_cb(void)
{
  printf("    Host aborted transfer\r\n");
}

#define UPLOAD_SIZE (29)
const uint8_t upload_test[UPLOAD_SIZE] = "Hello world from TinyUSB DFU!";

uint16_t tud_dfu_req_upload_data_cb(uint16_t block_num, uint8_t* data, uint16_t length)
{
  (void) block_num;
  (void) length;

  memcpy(data, upload_test, UPLOAD_SIZE);

  return UPLOAD_SIZE;
}

//--------------------------------------------------------------------+
// BLINKING TASK + Indicator pulse
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

/**************************************************************************/
/*!
    @file     main.c
    @author   hathach (tinyusb.org)

    @section LICENSE

    Software License Agreement (BSD License)

    Copyright (c) 2013, hathach (tinyusb.org)
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:
    1. Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
    3. Neither the name of the copyright holders nor the
    names of its contributors may be used to endorse or promote products
    derived from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ''AS IS'' AND ANY
    EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    INCLUDING NEGLIGENCE OR OTHERWISE ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

    This file is part of the tinyusb stack.
*/
/**************************************************************************/

//--------------------------------------------------------------------+
// INCLUDE
//--------------------------------------------------------------------+
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "queue.h"
#include "semphr.h"

#include "bsp/board.h"
#include "tusb.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+

//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+
void led_blinky_cb(TimerHandle_t xTimer);
void usb_device_task(void* param);

/*------------- MAIN -------------*/
int main(void)
{
  board_init();

  // soft timer for blinky
  TimerHandle_t tm_hdl = xTimerCreate(NULL, pdMS_TO_TICKS(1000), true, NULL, led_blinky_cb);
  xTimerStart(tm_hdl, 0);

  tusb_init();

  // Create a task for tinyusb device stack
  xTaskCreate( usb_device_task, "usbd", 150, NULL, configMAX_PRIORITIES-1, NULL);

  // Create task
#if CFG_TUD_CDC
  extern void cdc_task(void* params);
  xTaskCreate( cdc_task, "cdc", 256, NULL, configMAX_PRIORITIES-2, NULL);
#endif

#if CFG_TUD_HID
  extern void usb_hid_task(void* params);
#endif

  vTaskStartScheduler();
  NVIC_SystemReset();

  return 0;
}

// USB Device Driver task
// This top level thread process all usb events and invoke callbacks
void usb_device_task(void* param)
{
  (void) param;

  // RTOS forever loop
  while (1)
  {
    // tinyusb device task
    tud_task();
  }
}

//--------------------------------------------------------------------+
// USB CDC
//--------------------------------------------------------------------+
#if CFG_TUD_CDC
void cdc_task(void* params)
{
  (void) params;

  // RTOS forever loop
  while ( 1 )
  {
    if ( tud_cdc_connected() )
    {
      // connected and there are data available
      if ( tud_cdc_available() )
      {
        uint8_t buf[64];

        // read and echo back
        uint32_t count = tud_cdc_read(buf, sizeof(buf));

        for(uint32_t i=0; i<count; i++)
        {
          tud_cdc_write_char(buf[i]);

          if ( buf[i] == '\r' ) tud_cdc_write_char('\n');
        }

        tud_cdc_write_flush();
      }
    }
  }
}

void tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts)
{
  (void) itf;

  // connected
  if ( dtr && rts )
  {
    // print initial message when connected
    tud_cdc_write_str("\r\nTinyUSB CDC MSC HID device with FreeRTOS example\r\n");
  }
}
#endif

//--------------------------------------------------------------------+
// USB HID
//--------------------------------------------------------------------+
#if CFG_TUD_HID
void usb_hid_task(void* params)
{
  (void) params;

  // Poll every 10ms
  static tu_timeout_t tm = { .start = 0, .interval = 10 };

  if ( !tu_timeout_expired(&tm) ) return; // not enough time
  tu_timeout_reset(&tm);

  uint32_t const btn = board_buttons();

  /*------------- Keyboard -------------*/
  if ( tud_hid_keyboard_ready() )
  {
    if ( btn )
    {
      uint8_t keycode[6] = { 0 };

      for(uint8_t i=0; i < 6; i++)
      {
        if ( btn & (1 << i) ) keycode[i] = HID_KEY_A + i;
      }

      tud_hid_keyboard_keycode(0, keycode);
    }else
    {
      // Null means all zeroes keycodes
      tud_hid_keyboard_keycode(0, NULL);
    }
  }


  /*------------- Mouse -------------*/
  if ( tud_hid_mouse_ready() )
  {
    enum { DELTA  = 5 };

    if ( btn & 0x01 ) tud_hid_mouse_move(-DELTA,      0); // left
    if ( btn & 0x02 ) tud_hid_mouse_move( DELTA,      0); // right
    if ( btn & 0x04 ) tud_hid_mouse_move(  0   , -DELTA); // up
    if ( btn & 0x08 ) tud_hid_mouse_move(  0   ,  DELTA); // down
  }
}

uint16_t tud_hid_generic_get_report_cb(uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen)
{
  // TODO not Implemented
  return 0;
}

void tud_hid_generic_set_report_cb(uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize)
{
  // TODO not Implemented
}
#endif

//--------------------------------------------------------------------+
// tinyusb callbacks
//--------------------------------------------------------------------+
void tud_mount_cb(void)
{

}

void tud_umount_cb(void)
{
}

void tud_cdc_rx_cb(uint8_t itf)
{
  (void) itf;
}

//--------------------------------------------------------------------+
// BLINKING TASK
//--------------------------------------------------------------------+
void led_blinky_cb(TimerHandle_t xTimer)
{
  (void) xTimer;
  static bool led_state = false;

  board_led_control(led_state);
  led_state = 1 - led_state; // toggle
}

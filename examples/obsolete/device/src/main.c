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

#include "bsp/board.h"
#include "app_os_prio.h"
#include "tusb.h"

#include "msc_device_app.h"
#include "keyboard_device_app.h"
#include "mouse_device_app.h"
#include "cdc_device_app.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+

//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+
void print_greeting(void);
void led_blinking_init(void);
void led_blinking_task(void* param);


#if CFG_TUSB_OS == OPT_OS_NONE
// like a real RTOS, this function is a main loop invoking each task in application and never return
void os_none_start_scheduler(void)
{
  while (1)
  {
    tusb_task();
    led_blinking_task(NULL);

    msc_app_task(NULL);
    keyboard_app_task(NULL);
    mouse_app_task(NULL);
    cdc_serial_app_task(NULL);
  }
}
#endif

int main(void)
{
  board_init();
  print_greeting();

  tusb_init();

  //------------- application task init -------------//
  led_blinking_init();

  msc_app_init();
  keyboard_app_init();
  mouse_app_init();
  cdc_serial_app_init();

  //------------- start OS scheduler (never return) -------------//
#if CFG_TUSB_OS == OPT_OS_FREERTOS
  vTaskStartScheduler();
#elif CFG_TUSB_OS == OPT_OS_NONE
  os_none_start_scheduler();
#else
  #error need to start RTOS schduler
#endif

  return 0;
}

//--------------------------------------------------------------------+
// tinyusb callbacks
//--------------------------------------------------------------------+
void tud_mount_cb(void)
{
  uint8_t rhport = 0; // TODO remove
  cdc_serial_app_mount(rhport);
  keyboard_app_mount(rhport);
  msc_app_mount(rhport);
}

void tud_umount_cb(void)
{
  uint8_t rhport = 0; // TODO remove
  cdc_serial_app_umount(rhport);
  keyboard_app_umount(rhport);
  msc_app_umount(rhport);
}

//--------------------------------------------------------------------+
// BLINKING TASK
//--------------------------------------------------------------------+
OSAL_TASK_DEF(blink_task_def, "blinky", led_blinking_task, LED_BLINKING_APP_TASK_PRIO, 128);

static uint32_t led_blink_interval_ms = 1000; // default is 1 second

void led_blinking_init(void)
{
  led_blink_interval_ms = 1000;
  osal_task_create(&blink_task_def);
}

void led_blinking_set_interval(uint32_t ms)
{
  led_blink_interval_ms = ms;
}

tusb_error_t led_blinking_subtask(void);
void led_blinking_task(void* param)
{
  (void) param;

  OSAL_TASK_BEGIN
  led_blinking_subtask();
  OSAL_TASK_END
}

tusb_error_t led_blinking_subtask(void)
{
  OSAL_SUBTASK_BEGIN

  static bool led_state = false;

  osal_task_delay(led_blink_interval_ms);

  board_led_control(led_state);
  led_state = 1 - led_state; // toggle

//  uint32_t btn_mask;
//  btn_mask = board_buttons();
//
//  for(uint8_t i=0; i<32; i++)
//  {
//    if ( BIT_TEST_(btn_mask, i) ) printf("button %d is pressed\n", i);
//  }

  OSAL_SUBTASK_END
}

//--------------------------------------------------------------------+
// HELPER FUNCTION
//--------------------------------------------------------------------+
void print_greeting(void)
{
  char const * const rtos_name[] =
  {
      [OPT_OS_NONE]      = "None",
      [OPT_OS_FREERTOS]  = "FreeRTOS",
  };

  printf("\n\
--------------------------------------------------------------------\n\
-                     Device Demo (a tinyusb example)\n\
- if you find any bugs or get any questions, feel free to file an\n\
- issue at https://github.com/hathach/tinyusb\n\
--------------------------------------------------------------------\n\n"
  );

  puts("This DEVICE demo is configured to support:");
  printf("  - RTOS = %s\n", rtos_name[CFG_TUSB_OS]);
  if (CFG_TUD_HID_MOUSE    ) puts("  - HID Mouse");
  if (CFG_TUD_HID_KEYBOARD ) puts("  - HID Keyboard");
  if (CFG_TUD_MSC          ) puts("  - Mass Storage");
  if (CFG_TUD_CDC          ) puts("  - Communication Device Class");
}

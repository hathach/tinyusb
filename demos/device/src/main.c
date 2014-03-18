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

#include "board.h"
#include "tusb.h"

#include "mscd_app.h"
#include "keyboardd_app.h"
#include "moused_app.h"
#include "cdcd_app.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+

//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+
OSAL_TASK_FUNCTION( led_blinking_task ) (void* p_task_para);
OSAL_TASK_DEF(led_blinking_task, 128, LED_BLINKING_APP_TASK_PRIO);

void print_greeting(void);

#if TUSB_CFG_OS == TUSB_OS_NONE
// like a real RTOS, this function is a main loop invoking each task in application and never return
void os_none_start_scheduler(void)
{
  while (1)
  {
    tusb_task_runner();
    led_blinking_task(NULL);

    keyboardd_app_task(NULL);
    moused_app_task(NULL);
    cdcd_serial_app_task(NULL);
  }
}
#endif

int main(void)
{
  board_init();
  print_greeting();

  tusb_init();

  //------------- application task init -------------//
  (void) osal_task_create( OSAL_TASK_REF(led_blinking_task) );

  msc_dev_app_init();
  keyboardd_app_init();
  moused_app_init();
  cdcd_serial_app_init();

  //------------- start OS scheduler (never return) -------------//
#if TUSB_CFG_OS == TUSB_OS_FREERTOS
  vTaskStartScheduler();
#elif TUSB_CFG_OS == TUSB_OS_NONE
  os_none_start_scheduler();
#elif TUSB_CFG_OS == TUSB_OS_CMSIS_RTX
  while(1)
  {
    osDelay(osWaitForever); // CMSIS RTX osKernelStart already started, main() is a task
  }
#else
  #error need to start RTOS schduler
#endif

  return 0;
}

//--------------------------------------------------------------------+
// BLINKING TASK
//--------------------------------------------------------------------+
static uint32_t led_blink_interval_ms = 1000; // default is 1 seconda
void led_blinking_set_interval(uint32_t ms)
{
  led_blink_interval_ms = ms;
}

OSAL_TASK_FUNCTION( led_blinking_task ) (void* p_task_para)
{
  OSAL_TASK_LOOP_BEGIN

  static uint32_t led_on_mask = 0;

  osal_task_delay(led_blink_interval_ms);

  board_leds(led_on_mask, 1 - led_on_mask);
  led_on_mask = 1 - led_on_mask; // toggle

  uint32_t btn_mask;
  btn_mask = board_buttons();
	
  for(uint8_t i=0; i<32; i++)
  {
    if ( BIT_TEST_(btn_mask, i) ) printf("button %d is pressed\n", i);
  }

  OSAL_TASK_LOOP_END
}

//--------------------------------------------------------------------+
// HELPER FUNCTION
//--------------------------------------------------------------------+
void print_greeting(void)
{
  printf("\n\
--------------------------------------------------------------------\n\
-                     Device Demo (a tinyusb example)\n\
- if you find any bugs or get any questions, feel free to file an\n\
- issue at https://github.com/hathach/tinyusb\n\
--------------------------------------------------------------------\n\n"
  );

  puts("This demo supports the following classes for DEVICE");
  if (TUSB_CFG_DEVICE_HID_MOUSE    ) puts("  - HID Mouse");
  if (TUSB_CFG_DEVICE_HID_KEYBOARD ) puts("  - HID Keyboard");
  if (TUSB_CFG_DEVICE_MSC          ) puts("  - Mass Storage");
  if (TUSB_CFG_DEVICE_CDC          ) puts("  - Communication Device Class");
}

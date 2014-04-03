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

#if TUSB_CFG_OS == TUSB_OS_NONE
// like a real RTOS, this function is a main loop invoking each task in application and never return
void os_none_start_scheduler(void)
{
  while (1)
  {
    tusb_task_runner();
    led_blinking_task(NULL);

    msc_device_app_task(NULL);
    keyboard_device_app_task(NULL);
    mouse_device_app_task(NULL);
    cdcd_serial_app_task(NULL);
  }
}
#endif

int main(void)
{
#if TUSB_CFG_OS == TUSB_OS_CMSIS_RTX
  osKernelInitialize(); // CMSIS RTX requires kernel init before any other OS functions
#endif

  board_init();
  print_greeting();

  tusb_init();

  //------------- application task init -------------//
  led_blinking_init();

  msc_device_app_init();
  keyboard_device_app_init();
  mouse_device_app_init();
  cdcd_serial_app_init();

  //------------- start OS scheduler (never return) -------------//
#if TUSB_CFG_OS == TUSB_OS_FREERTOS
  vTaskStartScheduler();
#elif TUSB_CFG_OS == TUSB_OS_NONE
  os_none_start_scheduler();
#elif TUSB_CFG_OS == TUSB_OS_CMSIS_RTX
  osKernelStart();
#else
  #error need to start RTOS schduler
#endif

  return 0;
}

//--------------------------------------------------------------------+
// HELPER FUNCTION
//--------------------------------------------------------------------+
void print_greeting(void)
{
  char const * const rtos_name[] =
  {
      [TUSB_OS_NONE]      = "None",
      [TUSB_OS_FREERTOS]  = "FreeRTOS",
      [TUSB_OS_CMSIS_RTX] = "CMSIS-RTX"
  };

  printf("\n\
--------------------------------------------------------------------\n\
-                     Device Demo (a tinyusb example)\n\
- if you find any bugs or get any questions, feel free to file an\n\
- issue at https://github.com/hathach/tinyusb\n\
--------------------------------------------------------------------\n\n"
  );

  puts("This DEVICE demo is configured to support:");
  printf("  - RTOS = %s\n", rtos_name[TUSB_CFG_OS]);
  if (TUSB_CFG_DEVICE_HID_MOUSE    ) puts("  - HID Mouse");
  if (TUSB_CFG_DEVICE_HID_KEYBOARD ) puts("  - HID Keyboard");
  if (TUSB_CFG_DEVICE_MSC          ) puts("  - Mass Storage");
  if (TUSB_CFG_DEVICE_CDC          ) puts("  - Communication Device Class");
}

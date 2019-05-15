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
 * This file is part of the TinyUSB stack.
 */

//--------------------------------------------------------------------+
// INCLUDE
//--------------------------------------------------------------------+
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "board.h"
#include "tusb.h"

#include "mouse_host_app.h"
#include "keyboard_host_app.h"
#include "msc_host_app.h"
#include "cdc_serial_host_app.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+

//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+
void print_greeting(void);

//--------------------------------------------------------------------+
// IMPLEMENTATION
//--------------------------------------------------------------------+

#if CFG_TUSB_OS == OPT_OS_NONE
// like a real RTOS, this function is a main loop invoking each task in application and never return
void os_none_start_scheduler(void)
{
  while (1)
  {
    tusb_task();
    led_blinking_task(NULL);

    keyboard_host_app_task(NULL);
    mouse_host_app_task(NULL);
    msc_host_app_task(NULL);
    cdc_serial_host_app_task(NULL);
  }
}
#endif

int main(void)
{
#if CFG_TUSB_OS == TUSB_OS_CMSIS_RTX
  osKernelInitialize(); // CMSIS RTX requires kernel init before any other OS functions
#endif

  board_init();
  print_greeting();

  tusb_init();

  //------------- application task init -------------//
  led_blinking_init();

  keyboard_host_app_init();
  mouse_host_app_init();
  msc_host_app_init();
  cdc_serial_host_app_init();

  //------------- start OS scheduler (never return) -------------//
#if CFG_TUSB_OS == OPT_OS_FREERTOS
  vTaskStartScheduler();
#elif CFG_TUSB_OS == OPT_OS_NONE
  os_none_start_scheduler();
#elif CFG_TUSB_OS == TUSB_OS_CMSIS_RTX
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
      [OPT_OS_NONE]      = "None",
      [OPT_OS_FREERTOS]  = "FreeRTOS",
  };

  puts("\n\
--------------------------------------------------------------------\n\
-                     Host Demo (a tinyusb example)\n\
- if you find any bugs or get any questions, feel free to file an\n\
- issue at https://github.com/hathach/tinyusb\n\
--------------------------------------------------------------------\n"
  );

  puts("This HOST demo is configured to support:");
  printf("  - RTOS = %s\n", rtos_name[CFG_TUSB_OS]);
  if (CFG_TUH_HUB          ) puts("  - Hub (1 level only)");
  if (CFG_TUH_HID_MOUSE    ) puts("  - HID Mouse");
  if (CFG_TUH_HID_KEYBOARD ) puts("  - HID Keyboard");
  if (CFG_TUH_MSC          ) puts("  - Mass Storage");
  if (CFG_TUH_CDC          ) puts("  - Communication Device Class");
}

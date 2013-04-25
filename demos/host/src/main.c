/*
 * main.c
 *
 *  Created on: Mar 24, 2013
 *      Author: hathach
 */

/*
 * Software License Agreement (BSD License)
 * Copyright (c) 2012, hathach (tinyusb.org)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 * This file is part of the tiny usb stack.
 */

//--------------------------------------------------------------------+
// INCLUDE
//--------------------------------------------------------------------+
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "boards/board.h"
#include "tusb.h"

#include "mouse_app.h"
#include "keyboard_app.h"

#if defined(__CODE_RED)
  #include <cr_section_macros.h>
  #include <NXP/crp.h>
  // Variable to store CRP value in. Will be placed automatically
  // by the linker when "Enable Code Read Protect" selected.
  // See crp.h header for more information
  __CRP const unsigned int CRP_WORD = CRP_NO_CRP ;
#endif

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+

//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+
void print_greeting(void);

OSAL_TASK_FUNCTION( led_blinking_task ) (void* p_task_para);
OSAL_TASK_DEF(led_blinking_task_def, led_blinking_task, 128, LED_BLINKING_APP_TASK_PRIO);

//--------------------------------------------------------------------+
// IMPLEMENTATION
//--------------------------------------------------------------------+

int main(void)
{
  board_init();
  tusb_init();

  //------------- application task init -------------//
  (void) osal_task_create(&led_blinking_task_def);

#if TUSB_CFG_HOST_HID_KEYBOARD
  keyboard_app_init();
#endif

#if TUSB_CFG_HOST_HID_MOUSE
  mouse_app_init();
#endif

  //------------- start OS scheduler (never return) -------------//
#if TUSB_CFG_OS == TUSB_OS_FREERTOS
  vTaskStartScheduler();

#elif TUSB_CFG_OS == TUSB_OS_NONE
  print_greeting();
  while (1)
  {
    tusb_task_runner();
    keyboard_app_task(NULL);
    mouse_app_task(NULL);
    led_blinking_task(NULL);
  }
#else
  #error need to start RTOS schduler
#endif

  //------------- this part of code should not be reached -------------//
  hal_debugger_breakpoint();
  while(1)
  {

  }

  return 0;
}

//--------------------------------------------------------------------+
// HELPER FUNCTION
//--------------------------------------------------------------------+
void print_greeting(void)
{
  printf("\r\n\
--------------------------------------------------------------------\
-                     Host Demo (a tinyusb example)\r\n\
- if you find any bugs or get any questions, feel free to file an\r\n\
- issue at https://github.com/hathach/tinyusb\r\n\
--------------------------------------------------------------------\r\n\r\n"
  );
}

OSAL_TASK_FUNCTION( led_blinking_task ) (void* p_task_para)
{
#if TUSB_CFG_OS != TUSB_OS_NONE // TODO abstract to approriate place
  print_greeting();
#endif

  OSAL_TASK_LOOP_BEGIN

  vTaskDelay(CFG_TICKS_PER_SECOND);

  /* Toggle LED once per second */
  if ( (xTaskGetTickCount()/CFG_TICKS_PER_SECOND) % 2)
  {
    board_leds(0x01, 0x00);
  }
  else
  {
    board_leds(0x00, 0x01);
  }

  OSAL_TASK_LOOP_END
}

//OSAL_TASK_FUNCTION( led_blinking_task ) (void* p_task_para)
//{
//  static uint32_t current_tick = 0;
//
//  OSAL_TASK_LOOP_BEGIN
//
//  if (current_tick + CFG_TICKS_PER_SECOND < system_ticks)
//  {
//    current_tick += CFG_TICKS_PER_SECOND;
//
//    /* Toggle LED once per second */
//    if ( (current_tick/CFG_TICKS_PER_SECOND) % 2)
//    {
//      board_leds(0x01, 0x00);
//    }
//    else
//    {
//      board_leds(0x00, 0x01);
//    }
//  }
//
//  OSAL_TASK_LOOP_END
//}


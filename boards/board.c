/**************************************************************************/
/*!
    @file     board.c
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

#include "board.h"
#include "app_os_prio.h"

#if TUSB_CFG_OS == TUSB_OS_NONE

volatile uint32_t system_ticks = 0;

void SysTick_Handler (void)
{
  system_ticks++;
}

uint32_t tusb_tick_get(void)
{
  return system_ticks;
}

#endif

//--------------------------------------------------------------------+
// BLINKING TASK
//--------------------------------------------------------------------+
OSAL_TASK_DEF(led_blinking_task, 128, LED_BLINKING_APP_TASK_PRIO);
static uint32_t led_blink_interval_ms = 1000; // default is 1 second

void led_blinking_init(void)
{
  led_blink_interval_ms = 1000;
  ASSERT(TUSB_ERROR_NONE == osal_task_create( OSAL_TASK_REF(led_blinking_task) ), VOID_RETURN );
}

void led_blinking_set_interval(uint32_t ms)
{
  led_blink_interval_ms = ms;
}

OSAL_TASK_FUNCTION( led_blinking_task , p_task_para)
{
  OSAL_TASK_LOOP_BEGIN

  static uint32_t led_on_mask = 0;

  osal_task_delay(led_blink_interval_ms);

  board_leds(led_on_mask, 1 - led_on_mask);
  led_on_mask = 1 - led_on_mask; // toggle

//  uint32_t btn_mask;
//  btn_mask = board_buttons();
//
//  for(uint8_t i=0; i<32; i++)
//  {
//    if ( BIT_TEST_(btn_mask, i) ) printf("button %d is pressed\n", i);
//  }

  OSAL_TASK_LOOP_END
}

// TODO remove legacy cmsis code
void check_failed(uint8_t *file, uint32_t line)
{
  (void) file;
  (void) line;
}

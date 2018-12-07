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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "bsp/board.h"
#include "tusb.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF PROTYPES
//--------------------------------------------------------------------+
void print_greeting(void);
void led_blinking_task(void);

extern void virtual_com_task(void);
extern void usb_hid_task(void);

/*------------- MAIN -------------*/
int main(void)
{
  board_init();
  print_greeting();

  tusb_init();

  while (1)
  {
    tusb_task();

    led_blinking_task();

#if CFG_TUH_CDC
    virtual_com_task();
#endif

#if CFG_TUD_HID
    usb_hid_task();
#endif
  }

  return 0;
}

//--------------------------------------------------------------------+
// USB CDC
//--------------------------------------------------------------------+
#if CFG_TUH_CDC

void tuh_cdc_mounted_cb(uint8_t dev_addr)
{
  // application set-up
  printf("\na CDC device  (address %d) is mounted\n", dev_addr);
}

void tuh_cdc_unmounted_cb(uint8_t dev_addr)
{
  // application tear-down
  printf("\na CDC device (address %d) is unmounted \n", dev_addr);
}

// invoked ISR context
void tuh_cdc_xfer_isr(uint8_t dev_addr, xfer_result_t event, cdc_pipeid_t pipe_id, uint32_t xferred_bytes)
{

}

void virtual_com_task(void)
{

}

#endif

//--------------------------------------------------------------------+
// USB HID
//--------------------------------------------------------------------+
#if CFG_TUH_HID_KEYBOARD
void usb_hid_task(void)
{

}

void tuh_hid_keyboard_mounted_cb(uint8_t dev_addr)
{
  // application set-up
  printf("\na Keyboard device (address %d) is mounted\n", dev_addr);
}

void tuh_hid_keyboard_unmounted_cb(uint8_t dev_addr)
{
  // application tear-down
  printf("\na Keyboard device (address %d) is unmounted\n", dev_addr);
}

// invoked ISR context
void tuh_hid_keyboard_isr(uint8_t dev_addr, xfer_result_t event)
{

}

#endif

#if CFG_TUH_HID_MOUSE
void tuh_hid_mouse_mounted_cb(uint8_t dev_addr)
{
  // application set-up
  printf("\na Mouse device (address %d) is mounted\n", dev_addr);
}

void tuh_hid_mouse_unmounted_cb(uint8_t dev_addr)
{
  // application tear-down
  printf("\na Mouse device (address %d) is unmounted\n", dev_addr);
}

// invoked ISR context
void tuh_hid_mouse_isr(uint8_t dev_addr, xfer_result_t event)
{
}
#endif

//--------------------------------------------------------------------+
// tinyusb callbacks
//--------------------------------------------------------------------+

//--------------------------------------------------------------------+
// BLINKING TASK
//--------------------------------------------------------------------+
void led_blinking_task(void)
{
  static tu_timeout_t tm = { .start = 0, .interval = 1000 }; // Blink every 1000 ms
  static bool led_state = false;

  if ( !tu_timeout_expired(&tm) ) return; // not enough time
  tu_timeout_reset(&tm);

  board_led_control(led_state);
  led_state = 1 - led_state; // toggle
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

  printf("\n--------------------------------------------------------------------\n");
  printf("- Host example\n");
  printf("- if you find any bugs or get any questions, feel free to file an\n");
  printf("- issue at https://github.com/hathach/tinyusb\n");
  printf("--------------------------------------------------------------------\n\n");

  printf("This Host demo is configured to support:");
  printf("  - RTOS = %s\n", rtos_name[CFG_TUSB_OS]);
//  if (CFG_TUH_CDC          ) puts("  - Communication Device Class");
//  if (CFG_TUH_MSC          ) puts("  - Mass Storage");
//  if (CFG_TUH_HID_KEYBOARD ) puts("  - HID Keyboard");
//  if (CFG_TUH_HID_MOUSE    ) puts("  - HID Mouse");
}

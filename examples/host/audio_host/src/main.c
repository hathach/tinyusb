/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2025 TinyUSB contributors
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
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "bsp/board_api.h"
#include "tusb.h"
#include "app.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF PROTYPES
//--------------------------------------------------------------------+
void led_blinking_task(void);

/*------------- MAIN -------------*/
int main(void) {
  board_init();

  printf("TinyUSB Host USB Audio Example\r\n");
  printf("Connect a USB Audio Device (UAC 1.0) to test\r\n");

  // init host stack on configured roothub port
  tusb_rhport_init_t host_init = {
    .role = TUSB_ROLE_HOST,
    .speed = TUSB_SPEED_AUTO
  };
  tusb_init(BOARD_TUH_RHPORT, &host_init);

  board_init_after_tusb();

  while (1) {
    // tinyusb host task
    tuh_task();
    led_blinking_task();
    audio_app_task();
  }
}

//--------------------------------------------------------------------+
// TinyUSB Callbacks
//--------------------------------------------------------------------+

//--------------------------------------------------------------------+
// Blinking Task
//--------------------------------------------------------------------+
void led_blinking_task(void) {
  const uint32_t interval_ms = 1000;
  static uint32_t start_ms = 0;

  static bool led_state = false;

  // Blink every interval ms
  if ( tusb_time_millis_api() - start_ms < interval_ms) return; // not enough time
  start_ms += interval_ms;

  board_led_write(led_state);
  led_state = 1 - led_state; // toggle
}

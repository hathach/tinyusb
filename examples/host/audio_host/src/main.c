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

/*------------- MAIN -------------*/
int main(void) {
  board_init();

  printf("TinyUSB Host USB Audio Example\r\n");
  printf("Connect a USB Audio Device (UAC 1.0 or 2.0) to test\r\n");

  // init host stack on configured roothub port
  tusb_rhport_init_t host_init = {.role = TUSB_ROLE_HOST, .speed = TUSB_SPEED_AUTO};
  tusb_init(BOARD_TUH_RHPORT, &host_init);

  board_init_after_tusb();
  while (1) {
    // tinyusb host task
    tuh_task();
    led_blinking_task();
    audio_app_task_read();
    audio_app_task_write();
    defer_queue_task();
  }
}

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

/** \ingroup TBD
 *  \defgroup TBD
 *  \brief TBD
 *
 *  @{
 */

#ifndef _TUSB_APP_OS_PRIO_H_
#define _TUSB_APP_OS_PRIO_H_


#ifdef __cplusplus
 extern "C" {
#endif

#include "tusb.h"

#if CFG_TUSB_OS == OPT_OS_NONE
  #define LOWER_PRIO(x)   0   // does not matter
#elif CFG_TUSB_OS == OPT_OS_FREERTOS
  #define LOWER_PRIO(x)   ((x)-1) // freeRTOS lower number --> lower priority
#elif CFG_TUSB_OS == TUSB_OS_CMSIS_RTX
  #define LOWER_PRIO(x)   ((x)-1) // CMSIS-RTOS lower number --> lower priority
#else
  #error Priority is not configured for this RTOS
#endif

enum {
  STANDARD_APP_TASK_PRIO     = LOWER_PRIO(CFG_TUD_TASK_PRIO),  // Application Task is lower than usb system task
  LED_BLINKING_APP_TASK_PRIO = LOWER_PRIO(STANDARD_APP_TASK_PRIO), // Blinking task is lower than normal task

  KEYBOARD_APP_TASK_PRIO     = STANDARD_APP_TASK_PRIO,
  MOUSE_APP_TASK_PRIO        = STANDARD_APP_TASK_PRIO,
  CDC_SERIAL_APP_TASK_PRIO   = STANDARD_APP_TASK_PRIO,
  MSC_APP_TASK_PRIO          = STANDARD_APP_TASK_PRIO
};

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_APP_OS_PRIO_H_ */

/** @} */

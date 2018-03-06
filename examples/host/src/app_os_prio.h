/**************************************************************************/
/*!
    @file     app_os_prio.h
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
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

    This file is part of the tinyusb stack.
*/
/**************************************************************************/

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

#if TUSB_CFG_OS == TUSB_OS_NONE
  #define LOWER_PRIO(x)   0   // does not matter
#elif TUSB_CFG_OS == TUSB_OS_FREERTOS
  #define LOWER_PRIO(x)   ((x)-1) // freeRTOS lower number --> lower priority
#elif TUSB_CFG_OS == TUSB_OS_CMSIS_RTX
  #define LOWER_PRIO(x)   ((x)-1) // CMSIS-RTOS lower number --> lower priority
#else
  #error Priority is not configured for this RTOS
#endif

enum {
  STANDARD_APP_TASK_PRIO     = LOWER_PRIO(TUSB_CFG_OS_TASK_PRIO),  // Application Task is lower than usb system task
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

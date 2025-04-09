/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef TUSB_DWC2_CRITICAL_H_
#define TUSB_DWC2_CRITICAL_H_

#include "common/tusb_mcu.h"

#if defined(TUP_USBIP_DWC2_ESP32)
  #include "freertos/FreeRTOS.h"
  static portMUX_TYPE dcd_lock = portMUX_INITIALIZER_UNLOCKED;
  #define DCD_ENTER_CRITICAL()    portENTER_CRITICAL(&dcd_lock)
  #define DCD_EXIT_CRITICAL()     portEXIT_CRITICAL(&dcd_lock)

#else
  // Define critical section macros for DWC2 as no-op if not defined
  // This is to avoid breaking existing code that does not use critical section
  #define DCD_ENTER_CRITICAL()    // no-op
  #define DCD_EXIT_CRITICAL()     // no-op
#endif

#endif // TUSB_DWC2_CRITICAL_H_

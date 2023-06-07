/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2023 Ha Thach (thach@tinyusb.org)
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

#include "tusb_option.h"

#if CFG_TUC_ENABLED

#include "tcd.h"
#include "utcd.h"

//--------------------------------------------------------------------+
//
//--------------------------------------------------------------------+

// Debug level of USBD
#define UTCD_DEBUG   2
#define TU_LOG_UTCD(...)   TU_LOG(UTCD_DEBUG, __VA_ARGS__)

// Event queue
// utcd_int_set() is used as mutex in OS NONE config
void utcd_int_set(bool enabled);
OSAL_QUEUE_DEF(utcd_int_set, utcd_qdef, CFG_TUC_TASK_QUEUE_SZ, tcd_event_t);
tu_static osal_queue_t utcd_q;

// if stack is initialized
static bool utcd_inited = false;

// if port is initialized
static bool port_inited[TUP_TYPEC_RHPORTS_NUM];

//--------------------------------------------------------------------+
//
//--------------------------------------------------------------------+
bool tuc_inited(uint8_t rhport) {
  return utcd_inited && port_inited[rhport];
}

bool tuc_init(uint8_t rhport, tusb_typec_port_type_t port_type) {
  // Initialize stack
  if (!utcd_inited) {
    tu_memclr(port_inited, sizeof(port_inited));

    utcd_q = osal_queue_create(&utcd_qdef);
    TU_ASSERT(utcd_q != NULL);

    utcd_inited = true;
  }

  // skip if port already initialized
  if ( port_inited[rhport] ) {
    return true;
  }

  TU_LOG_UTCD("UTCD init on port %u\r\n", rhport);

  TU_ASSERT(tcd_init(rhport, port_type));
  tcd_int_enable(rhport);

  port_inited[rhport] = true;
  return true;
}


//--------------------------------------------------------------------+
//
//--------------------------------------------------------------------+
void utcd_int_set(bool enabled) {
  // Disable all controllers since they shared the same event queue
  for (uint8_t p = 0; p < TUP_TYPEC_RHPORTS_NUM; p++) {
    if ( port_inited[p] ) {
      if (enabled) {
        tcd_int_enable(p);
      }else {
        tcd_int_disable(p);
      }
    }
  }
}

#endif

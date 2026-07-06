/*
 * SPDX-FileCopyrightText: Copyright (c) 2023 Ha Thach (tinyusb.org)
 * SPDX-License-Identifier: MIT
 *
 * This file is part of the TinyUSB stack.
 */

#ifndef TUSB_UTCD_H_
#define TUSB_UTCD_H_

#include "common/tusb_common.h"
#include "pd_types.h"

#ifdef __cplusplus
extern "C" {
#endif

//--------------------------------------------------------------------+
// TypeC Configuration
//--------------------------------------------------------------------+

#ifndef CFG_TUC_TASK_QUEUE_SZ
#define CFG_TUC_TASK_QUEUE_SZ   8
#endif

//--------------------------------------------------------------------+
// Application API
//--------------------------------------------------------------------+

// Init typec stack on a port
bool tuc_init(uint8_t rhport, uint32_t port_type);

// Check if typec port is initialized
bool tuc_inited(uint8_t rhport);

// Enable Type-C port terminations
// Return false if port is not initialized
bool tuc_connect(uint8_t rhport);

// Disable Type-C port terminations
// Return false if port is not initialized
bool tuc_disconnect(uint8_t rhport);

// Task function should be called in main/rtos loop, extended version of tud_task()
// - timeout_ms: millisecond to wait, zero = no wait, 0xFFFFFFFF = wait forever
// - in_isr: if function is called in ISR
void tuc_task_ext(uint32_t timeout_ms, bool in_isr);

// Task function should be called in main/rtos loop
TU_ATTR_ALWAYS_INLINE static inline
void tuc_task (void) {
  tuc_task_ext(UINT32_MAX, false);
}

#ifndef TUSB_TCD_H_
extern void tcd_int_handler(uint8_t rhport);
#endif

// Interrupt handler, name alias to TCD
#define tuc_int_handler tcd_int_handler

//--------------------------------------------------------------------+
// Callbacks
//--------------------------------------------------------------------+

bool tuc_pd_data_received_cb(uint8_t rhport, pd_header_t const* header, uint8_t const* dobj, uint8_t const* p_end);
bool tuc_pd_control_received_cb(uint8_t rhport, pd_header_t const* header);

//--------------------------------------------------------------------+
//
//--------------------------------------------------------------------+

bool tuc_msg_request(uint8_t rhport, void const* rdo);


#ifdef __cplusplus
}
#endif

#endif

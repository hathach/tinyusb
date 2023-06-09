/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2023 Ha Thach (tinyusb.org)
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

#ifndef _TUSB_UTCD_H_
#define _TUSB_UTCD_H_

#include "common/tusb_common.h"

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
//
//--------------------------------------------------------------------+

// All table references are from USBPD Specification rev3.1 version 1.8
enum {
  PD_PDO_TYPE_FIXED = 0, // Vmin = Vmax
  PD_PDO_TYPE_BATTERY,
  PD_PDO_TYPE_VARIABLE, // non-battery
  PD_PDO_TYPE_APDO, // Augmented Power Data Object
};

// Fixed Power Data Object (PDO) table 6-9
typedef struct TU_ATTR_PACKED {
  uint32_t current_max_10ma          : 10; // [9..0] Max current in 10mA unit
  uint32_t voltage_50mv              : 10; // [19..10] Voltage in 50mV unit
  uint32_t current_peak              :  2; // [21..20] Peak current
  uint32_t reserved                  :  1; // [22] Reserved
  uint32_t epr_mode_capable          :  1; // [23] epr_mode_capable
  uint32_t unchunked_ext_msg_support :  1; // [24] UnChunked Extended Message Supported
  uint32_t dual_role_data            :  1; // [25] Dual Role Data
  uint32_t usb_comm_capable          :  1; // [26] USB Communications Capable
  uint32_t unconstrained_power       :  1; // [27] Unconstrained Power
  uint32_t usb_suspend_supported     :  1; // [28] USB Suspend Supported
  uint32_t dual_role_power           :  1; // [29] Dual Role Power
  uint32_t type                      :  2; // [30] Fixed Supply type = PD_PDO_TYPE_FIXED
} pd_pdo_fixed_t;
TU_VERIFY_STATIC(sizeof(pd_pdo_fixed_t) == 4, "Invalid size");

// Battery Power Data Object (PDO) table 6-12
typedef struct TU_ATTR_PACKED {
  uint32_t power_max_250mw   : 10; // [9..0] Max allowable power in 250mW unit
  uint32_t voltage_min_50mv  : 10; // [19..10] Minimum voltage in 50mV unit
  uint32_t voltage_max_50mv  : 10; // [29..20] Maximum voltage in 50mV unit
  uint32_t type              :  2; // [31..30] Battery type = PD_PDO_TYPE_BATTERY
} pd_pdo_battery_t;
TU_VERIFY_STATIC(sizeof(pd_pdo_battery_t) == 4, "Invalid size");

// Variable Power Data Object (PDO) table 6-11
typedef struct TU_ATTR_PACKED {
  uint32_t current_max_10ma  : 10; // [9..0] Max current in 10mA unit
  uint32_t voltage_min_50mv  : 10; // [19..10] Minimum voltage in 50mV unit
  uint32_t voltage_max_50mv  : 10; // [29..20] Maximum voltage in 50mV unit
  uint32_t type              :  2; // [31..30] Variable Supply type = PD_PDO_TYPE_VARIABLE
} pd_pdo_variable_t;
TU_VERIFY_STATIC(sizeof(pd_pdo_variable_t) == 4, "Invalid size");

// Augmented Power Data Object (PDO) table 6-13
typedef struct TU_ATTR_PACKED {
  uint32_t current_max_50ma  :  7; // [6..0] Max current in 50mA unit
  uint32_t reserved1         :  1; // [7] Reserved
  uint32_t voltage_min_100mv :  8; // [15..8] Minimum Voltage in 100mV unit
  uint32_t reserved2         :  1; // [16] Reserved
  uint32_t voltage_max_100mv :  8; // [24..17] Maximum Voltage in 100mV unit
  uint32_t reserved3         :  2; // [26..25] Reserved
  uint32_t pps_power_limited :  1; // [27] PPS Power Limited
  uint32_t spr_programmable  :  2; // [29..28] SPR Programmable Power Supply
  uint32_t type              :  2; // [31..30] Augmented Power Data Object = PD_PDO_TYPE_APDO
} pd_pdo_apdo_t;
TU_VERIFY_STATIC(sizeof(pd_pdo_apdo_t) == 4, "Invalid size");

//--------------------------------------------------------------------+
// Application API
//--------------------------------------------------------------------+

// Init typec stack on a port
bool tuc_init(uint8_t rhport, tusb_typec_port_type_t port_type);

// Check if typec port is initialized
bool tuc_inited(uint8_t rhport);

#ifndef _TUSB_TCD_H_
extern void tcd_int_handler(uint8_t rhport);
#endif

// Interrupt handler, name alias to TCD
#define tuc_int_handler tcd_int_handler

//--------------------------------------------------------------------+
//
//--------------------------------------------------------------------+

#endif

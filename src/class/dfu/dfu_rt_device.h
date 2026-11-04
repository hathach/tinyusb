/*
 * SPDX-FileCopyrightText: Copyright (c) 2019 Sylvain Munaut <tnt@246tNt.com>
 * SPDX-FileCopyrightText: Copyright (c) 2019 Ha Thach (tinyusb.org)
 * SPDX-License-Identifier: MIT
 *
 * This file is part of the TinyUSB stack.
 */

#ifndef TUSB_DFU_RT_DEVICE_H_
#define TUSB_DFU_RT_DEVICE_H_

#include "dfu.h"

#ifdef __cplusplus
 extern "C" {
#endif

//--------------------------------------------------------------------+
// Application Callback API (weak is optional)
//--------------------------------------------------------------------+
// Invoked after DFU_DETACH: once the device completes the request's status stage when bitWillDetach is set (not proof
// the host received it), otherwise on the next USB bus reset. An unplug, a SET_CONFIGURATION that changes the
// configuration (a repeat of the active one does not) or an unsupported or wrong-direction DFU request cancels a
// pending detach. Deviation from DFU 1.1 5.1/A.2.2: there is no detach timer, so
// wDetachTimeOut and wTimeout are ignored and a pending detach never expires; a repeated DFU_DETACH re-arms it instead
// of stalling.
void tud_dfu_runtime_reboot_to_dfu_cb(void);

//--------------------------------------------------------------------+
// Internal Class Driver API
//--------------------------------------------------------------------+
void     dfu_rtd_init(void);
bool     dfu_rtd_deinit(void);
void     dfu_rtd_reset(uint8_t rhport);
void     dfu_rtd_bus_reset(void);
uint16_t dfu_rtd_open(uint8_t rhport, tusb_desc_interface_t const * itf_desc, uint16_t max_len);
bool     dfu_rtd_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const * request);

#ifdef __cplusplus
 }
#endif

#endif /* TUSB_DFU_RT_DEVICE_H_ */

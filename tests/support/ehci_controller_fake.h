/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2018, hathach (tinyusb.org)
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

/** \file
 *  \brief TBD
 *
 *  \note TBD
 */

/** \ingroup TBD
 *  \defgroup TBD
 *  \brief TBD
 *
 *  @{
 */

#ifndef _TUSB_EHCI_CONTROLLER_H_
#define _TUSB_EHCI_CONTROLLER_H_

#include "host/hcd.h"

#ifdef __cplusplus
 extern "C" {
#endif

extern ehci_data_t ehci_data;

void ehci_controller_init(void);
void ehci_controller_run(uint8_t hostid);
void ehci_controller_run_error(uint8_t hostid);
void ehci_controller_control_xfer_proceed(uint8_t dev_addr, uint8_t p_data[]);
void ehci_controller_device_plug(uint8_t hostid, tusb_speed_t speed);
void ehci_controller_device_unplug(uint8_t hostid);

ehci_registers_t* get_operational_register(uint8_t hostid);
ehci_link_t* get_period_frame_list(uint8_t hostid);
ehci_qhd_t* get_async_head(uint8_t hostid);
ehci_link_t* get_period_head(uint8_t hostid, uint8_t interval_ms);
ehci_qhd_t* get_control_qhd(uint8_t dev_addr);
ehci_qtd_t* get_control_qtds(uint8_t dev_addr);
ehci_qhd_t* qhd_get_from_pipe_handle(pipe_handle_t pipe_hdl);

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_EHCI_CONTROLLER_H_ */

/** @} */

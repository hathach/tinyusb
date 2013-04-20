/*
 * ehci_controller.h
 *
 *  Created on: Mar 10, 2013
 *      Author: hathach
 */

/*
 * Software License Agreement (BSD License)
 * Copyright (c) 2012, hathach (tinyusb.org)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 * This file is part of the tiny usb stack.
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

#ifdef __cplusplus
 extern "C" {
#endif

extern ehci_data_t ehci_data;

void ehci_controller_init(void);
void ehci_controller_run(uint8_t hostid);
void ehci_controller_control_xfer_proceed(uint8_t dev_addr, uint8_t p_data[]);
void ehci_controller_device_plug(uint8_t hostid, tusb_speed_t speed);
void ehci_controller_device_unplug(uint8_t hostid);

ehci_registers_t* get_operational_register(uint8_t hostid);
ehci_link_t* get_period_frame_list(uint8_t hostid);
ehci_qhd_t* get_async_head(uint8_t hostid);
ehci_qhd_t* get_period_head(uint8_t hostid, uint8_t interval_ms);
ehci_qhd_t* get_control_qhd(uint8_t dev_addr);
ehci_qtd_t* get_control_qtds(uint8_t dev_addr);
ehci_qhd_t* qhd_get_from_pipe_handle(pipe_handle_t pipe_hdl);

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_EHCI_CONTROLLER_H_ */

/** @} */

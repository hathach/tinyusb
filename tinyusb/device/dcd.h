/*
 * dcd.h
 *
 *  Created on: Nov 26, 2012
 *      Author: hathach
 */

/*
 * Software License Agreement (BSD License)
 * Copyright (c) 2013, hathach (tinyusb.org)
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
 * This file is part of the tinyUSB stack.
 */

/** \file
 *  \brief Device Controller Driver
 *
 *  \note TBD
 */

/** 
 *  \defgroup Group_DCD Device Controller Driver
 *  \brief Device Controller Driver
 *
 *  @{
 */

#ifndef _TUSB_DCD_H_
#define _TUSB_DCD_H_

#ifdef __cplusplus
 extern "C" {
#endif

#include "common/common.h"

tusb_error_t dcd_init(void) ATTR_WARN_UNUSED_RESULT;
tusb_error_t dcd_controller_reset(uint8_t coreid) ATTR_WARN_UNUSED_RESULT;
void dcd_controller_connect(uint8_t coreid);
void dcd_isr(uint8_t coreid);

tusb_error_t dcd_pipe_control_write(uint8_t coreid, void const * buffer, uint16_t length);
tusb_error_t dcd_pipe_control_read(uint8_t coreid, void * buffer, uint16_t length);

void dcd_pipe_control_write_zero_length(uint8_t coreid);
tusb_error_t dcd_pipe_open(uint8_t coreid, tusb_descriptor_endpoint_t const * p_endpoint_desc) ATTR_WARN_UNUSED_RESULT;
void dcd_device_set_address(uint8_t coreid, uint8_t dev_addr);
void dcd_device_set_configuration(uint8_t coreid, uint8_t config_num);

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_DCD_H_ */

/// @}

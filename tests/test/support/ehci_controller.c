/*
 * ehci_controller.c
 *
 *  Created on: Mar 9, 2013
 *      Author: hathach
 */

/*
 * Software License Agreement (BSD License)
 * Copyright (c) 2012, hathach (tinyusb.net)
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

//--------------------------------------------------------------------+
// INCLUDE
//--------------------------------------------------------------------+
#include "unity.h"
#include "tusb_option.h"
#include "errors.h"
#include "binary.h"
#include "hal.h"
#include "ehci.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+
extern ehci_data_t ehci_data;

//--------------------------------------------------------------------+
// IMPLEMENTATION
//--------------------------------------------------------------------+
void ehci_controller_run(uint8_t hostid)
{
  //------------- Async List -------------//
  ehci_registers_t* const regs = get_operational_register(hostid);

  ehci_qhd_t *p_qhd = (ehci_qhd_t*) regs->async_list_base;
  do
  {
    if ( !p_qhd->qtd_overlay.halted )
    {
      while(!p_qhd->qtd_overlay.next.terminate)
      {
        ehci_qtd_t* p_qtd = (ehci_qtd_t*) align32(p_qhd->qtd_overlay.next.address);
        p_qtd->active = 0;
        p_qhd->qtd_overlay = *p_qtd;
      }
    }
    p_qhd = (ehci_qhd_t*) align32(p_qhd->next.address);
  }while(p_qhd != get_async_head(hostid)); // stop if loop around
  //------------- Period List -------------//

  regs->usb_sts = EHCI_INT_MASK_NXP_ASYNC;
}

void ehci_controller_device_plug(uint8_t hostid, tusb_speed_t speed)
{
  ehci_registers_t* const regs = get_operational_register(hostid);

  regs->usb_sts_bit.port_change_detect = 1;
  regs->portsc_bit.connect_status_change = 1;
  regs->portsc_bit.current_connect_status = 1;
  regs->portsc_bit.nxp_port_speed = speed;
}

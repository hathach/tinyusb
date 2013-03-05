/*
 * ehci.c
 *
 *  Created on: Dec 2, 2012
 *      Author: hathach
 */

/*
 * Software License Agreement (BSD License)
 * Copyright (c) 2013, hathach (tinyusb.net)
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

#include "common/common.h"

#if MODE_HOST_SUPPORTED && (MCU == MCU_LPC43XX || MCU == MCU_LPC18XX)
//--------------------------------------------------------------------+
// INCLUDE
//--------------------------------------------------------------------+
#include "hal/hal.h"
#include "osal/osal.h"
#include "common/timeout_timer.h"

#include "../hcd.h"
#include "../usbh_hcd.h"
#include "ehci.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+

//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+
STATIC_ ehci_data_t ehci_data TUSB_CFG_ATTR_USBRAM;
STATIC_ ehci_link_t period_frame_list0[EHCI_FRAMELIST_SIZE] ATTR_ALIGNED(4096) TUSB_CFG_ATTR_USBRAM;
#if CONTROLLER_HOST_NUMBER > 1
STATIC_ ehci_link_t period_frame_list1[EHCI_FRAMELIST_SIZE] ATTR_ALIGNED(4096) TUSB_CFG_ATTR_USBRAM;
#endif

//------------- Validation -------------//
STATIC_ASSERT( ALIGN_OF(period_frame_list0) == 4096, "Period Framelist must be 4k alginment");
#if CONTROLLER_HOST_NUMBER > 1
STATIC_ASSERT( ALIGN_OF(period_frame_list1) == 4096, "Period Framelist must be 4k alginment");
#endif

//--------------------------------------------------------------------+
// IMPLEMENTATION
//--------------------------------------------------------------------+
STATIC_ INLINE_ ehci_registers_t* const get_operational_register(uint8_t hostid) ATTR_PURE ATTR_ALWAYS_INLINE ATTR_WARN_UNUSED_RESULT;
STATIC_ INLINE_ ehci_registers_t* const get_operational_register(uint8_t hostid)
{
  return (ehci_registers_t* const) (hostid ? (&LPC_USB1->USBCMD_H) : (&LPC_USB0->USBCMD_H) );
}

STATIC_ INLINE_ ehci_link_t* const get_period_frame_list(uint8_t list_idx) ATTR_PURE ATTR_ALWAYS_INLINE ATTR_WARN_UNUSED_RESULT;
STATIC_ INLINE_ ehci_link_t* const get_period_frame_list(uint8_t list_idx)
{
#if CONTROLLER_HOST_NUMBER > 1
  return list_idx ? period_frame_list1 : period_frame_list0; // TODO more than 2 controller
#else
  return period_frame_list0;
#endif
}

STATIC_ INLINE_ uint8_t hostid_to_data_idx(uint8_t hostid) ATTR_ALWAYS_INLINE ATTR_CONST ATTR_WARN_UNUSED_RESULT;
STATIC_ INLINE_ uint8_t hostid_to_data_idx(uint8_t hostid)
{
  #if (CONTROLLER_HOST_NUMBER == 1) && (TUSB_CFG_CONTROLLER1_MODE & TUSB_MODE_HOST)
    (void) hostid;
    return 0;
  #else
    return hostid;
  #endif
}

STATIC_ INLINE_ ehci_qhd_t* const get_async_head(uint8_t hostid) ATTR_ALWAYS_INLINE ATTR_PURE ATTR_WARN_UNUSED_RESULT;
STATIC_ INLINE_ ehci_qhd_t* const get_async_head(uint8_t hostid)
{
  return &ehci_data.controller.async_head[hostid_to_data_idx(hostid)];
}

STATIC_ INLINE_ ehci_qhd_t* const get_period_head(uint8_t hostid) ATTR_ALWAYS_INLINE ATTR_PURE ATTR_WARN_UNUSED_RESULT;
STATIC_ INLINE_ ehci_qhd_t* const get_period_head(uint8_t hostid)
{
  return &ehci_data.controller.period_head[hostid_to_data_idx(hostid)];
}

tusb_error_t hcd_controller_init(uint8_t hostid) ATTR_WARN_UNUSED_RESULT;

//--------------------------------------------------------------------+
// USBH-HCD API
//--------------------------------------------------------------------+
tusb_error_t hcd_init(void)
{
  //------------- Data Structure init -------------//
  memclr_(&ehci_data, sizeof(ehci_data_t));

  #if (TUSB_CFG_CONTROLLER0_MODE & TUSB_MODE_HOST)
    ASSERT_STATUS (hcd_controller_init(0));
  #endif

  #if (TUSB_CFG_CONTROLLER1_MODE & TUSB_MODE_HOST)
    ASSERT_STATUS (hcd_controller_init(1));
  #endif

  return TUSB_ERROR_NONE;
}

void hcd_isr(uint8_t hostid)
{

}

//--------------------------------------------------------------------+
// Controller API
//--------------------------------------------------------------------+
tusb_error_t hcd_controller_init(uint8_t hostid)
{
  ehci_registers_t* const regs = get_operational_register(hostid);

  //------------- CTRLDSSEGMENT Register (skip) -------------//
  //------------- USB INT Register -------------//
  regs->usb_int_enable = 0;                 // 1. disable all the interrupt
  regs->usb_sts        = EHCI_INT_MASK_ALL; // 2. clear all status
  regs->usb_int_enable =
      /*EHCI_INT_MASK_USB |*/ EHCI_INT_MASK_ERROR | EHCI_INT_MASK_PORT_CHANGE | EHCI_INT_MASK_ASYNC_ADVANCE
#if 1 // TODO enable usbint olny
      | EHCI_INT_MASK_NXP_ASYNC | EHCI_INT_MASK_NXP_PERIODIC
#endif
      ;

  //------------- Asynchronous List -------------//
  ehci_qhd_t * const async_head = get_async_head(hostid);
  memclr_(async_head, sizeof(ehci_qhd_t));

  async_head->next.address                    = align32( (uint32_t) async_head); // circular list, next is itself
  async_head->next.type                       = EHCI_QUEUE_ELEMENT_QHD;
  async_head->head_list_flag                  = 1;

  async_head->qtd_overlay.next.terminate      = 1;
  async_head->qtd_overlay.alternate.terminate = 1;
  async_head->qtd_overlay.halted              = 1;

  regs->async_list_base = (uint32_t) async_head;

  //------------- Periodic List -------------//
#if EHCI_PERIODIC_LIST
  ehci_link_t * const framelist  = get_period_frame_list(hostid);
  ehci_qhd_t * const period_head = get_period_head(hostid);

  uint32_t i;
  for(i=0; i<EHCI_FRAMELIST_SIZE; i++)
  {
    framelist[i].address = (uint32_t) period_head;
    framelist[i].type    = EHCI_QUEUE_ELEMENT_QHD;
  }

  period_head->smask                           = 1; // queue head in period list must have smask non-zero
  period_head->next.terminate                  = 1;
  period_head->qtd_overlay.next.terminate      = 1;
  period_head->qtd_overlay.alternate.terminate = 1;
  period_head->qtd_overlay.halted              = 1;

  regs->periodic_list_base = (uint32_t) framelist;
#else
  regs->periodic_list_base = 0;
#endif

  //------------- TT Control (NXP only) -------------//
  regs->tt_control = 0;

  //------------- USB CMD Register -------------//

  regs->usb_cmd = BIT_(EHCI_USBCMD_POS_ASYNC_ENABLE)
#if EHCI_PERIODIC_LIST
                  | BIT_(EHCI_USBCMD_POS_PERIOD_ENABLE)
#endif
                  | ((EHCI_CFG_FRAMELIST_SIZE_BITS & BIN8(011)) << EHCI_USBCMD_POS_FRAMELIST_SZIE)
                  | ((EHCI_CFG_FRAMELIST_SIZE_BITS >> 2) << EHCI_USBCMD_POS_NXP_FRAMELIST_SIZE_MSB);

  //------------- ConfigFlag Register (skip) -------------//

  return TUSB_ERROR_NONE;
}

tusb_error_t hcd_controller_stop(uint8_t hostid) ATTR_WARN_UNUSED_RESULT;
tusb_error_t hcd_controller_stop(uint8_t hostid)
{
  ehci_registers_t* const regs = get_operational_register(hostid);
  timeout_timer_t timeout;

  regs->usb_cmd_bit.run_stop = 0;

  timeout_set(&timeout, 16); // USB Spec: controller has to stop within 16 uframe
  while( regs->usb_sts_bit.hc_halted == 0 && !timeout_expired(&timeout)) {}

  return timeout_expired(&timeout) ? TUSB_ERROR_OSAL_TIMEOUT : TUSB_ERROR_NONE;
}

//TODO host/device mode must be set immediately after a reset
tusb_error_t hcd_controller_reset(uint8_t hostid) ATTR_WARN_UNUSED_RESULT;
tusb_error_t hcd_controller_reset(uint8_t hostid)
{
  ehci_registers_t* const regs = get_operational_register(hostid);
  timeout_timer_t timeout;

  if (regs->usb_sts_bit.hc_halted == 0) // need to stop before reset
  {
    ASSERT_STATUS( hcd_controller_stop(hostid) );
  }

  regs->usb_cmd_bit.reset = 1;

  timeout_set(&timeout, 16); // should not take longer the time to stop controller
  while( regs->usb_cmd_bit.reset && !timeout_expired(&timeout)) {}

  return timeout_expired(&timeout) ? TUSB_ERROR_OSAL_TIMEOUT : TUSB_ERROR_NONE;
}

//--------------------------------------------------------------------+
// PORT API
//--------------------------------------------------------------------+


//--------------------------------------------------------------------+
// PIPE API
//--------------------------------------------------------------------+
tusb_error_t  hcd_pipe_control_open(uint8_t dev_addr, uint8_t max_packet_size)
{
  ehci_qhd_t *p_qhd;

  if (dev_addr == 0)
  {
    // async head of selected controller is used as Addr0 Endpoint --> horizontal link have to be preserved
    p_qhd = get_async_head( usbh_device_info_pool[dev_addr].core_id );
    p_qhd->head_list_flag      = 1; // make sure it is still head of list
  }else
  {
    p_qhd = &ehci_data.device[dev_addr].control.qhd;
    p_qhd->head_list_flag      = 0; // make sure it is still head of list
  }

  p_qhd->device_address      = dev_addr;
  p_qhd->inactive_next_xact  = 0;
  p_qhd->endpoint_number     = 0;
  p_qhd->endpoint_speed      = usbh_device_info_pool[dev_addr].speed;
  p_qhd->data_toggle_control = 1;
  p_qhd->max_package_size    = max_packet_size;
  p_qhd->non_hs_control_endpoint = (usbh_device_info_pool[dev_addr].speed != TUSB_SPEED_HIGH) ? 1 : 0;
  p_qhd->nak_count_reload    = 0;

  p_qhd->smask               = 0;
  p_qhd->cmask               = 0;
  p_qhd->hub_address         = usbh_device_info_pool[dev_addr].hub_addr;
  p_qhd->hub_port            = usbh_device_info_pool[dev_addr].hub_port;
  p_qhd->mult                = 1; // TODO not use high bandwidth/park mode yet

  //------------- inactive when just opened -------------//
  p_qhd->qtd_overlay.next.terminate      = 1;
  p_qhd->qtd_overlay.alternate.terminate = 1;
  p_qhd->qtd_overlay.halted              = 1;

  //------------- HCD Management Data -------------//
  p_qhd->used       = 1;
  p_qhd->p_qtd_list = NULL;

  //------------- insert to async list -------------//
  // TODO disable async list first if got error
  if (dev_addr != 0)
  {
    ehci_qhd_t * const async_head = get_async_head(usbh_device_info_pool[dev_addr].core_id);
    p_qhd->next = async_head->next;
    async_head->next.address = (uint32_t) p_qhd;
    async_head->next.type = EHCI_QUEUE_ELEMENT_QHD;
  }

  return TUSB_ERROR_NONE;
}
//
//tusb_error_t  hcd_pipe_control_xfer(uint8_t dev_addr, tusb_std_request_t const * p_request, uint8_t data[])
//{
//  return TUSB_ERROR_NONE;
//}
//
//pipe_handle_t hcd_pipe_open(uint8_t dev_addr, tusb_descriptor_endpoint_t const * endpoint_desc)
//{
//  return TUSB_ERROR_NONE;
//}

#endif

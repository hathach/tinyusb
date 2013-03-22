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
// TODO static assert for memory placement on some known MCU such as lpc43xx

//--------------------------------------------------------------------+
// IMPLEMENTATION
//--------------------------------------------------------------------+
STATIC_ INLINE_ ehci_registers_t* get_operational_register(uint8_t hostid) ATTR_PURE ATTR_ALWAYS_INLINE ATTR_WARN_UNUSED_RESULT;
STATIC_ INLINE_ ehci_registers_t* get_operational_register(uint8_t hostid)
{
  return (ehci_registers_t*) (hostid ? (&LPC_USB1->USBCMD_H) : (&LPC_USB0->USBCMD_H) );
}

STATIC_ INLINE_ ehci_link_t* get_period_frame_list(uint8_t list_idx) ATTR_PURE ATTR_ALWAYS_INLINE ATTR_WARN_UNUSED_RESULT;
STATIC_ INLINE_ ehci_link_t* get_period_frame_list(uint8_t list_idx)
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

STATIC_ INLINE_ ehci_qhd_t* get_async_head(uint8_t hostid) ATTR_ALWAYS_INLINE ATTR_PURE ATTR_WARN_UNUSED_RESULT;
STATIC_ INLINE_ ehci_qhd_t* get_async_head(uint8_t hostid)
{
  return &ehci_data.async_head[ hostid_to_data_idx(hostid) ];
}

STATIC_ INLINE_ ehci_qhd_t* get_period_head(uint8_t hostid) ATTR_ALWAYS_INLINE ATTR_PURE ATTR_WARN_UNUSED_RESULT;
STATIC_ INLINE_ ehci_qhd_t* get_period_head(uint8_t hostid)
{
  return &ehci_data.period_head[ hostid_to_data_idx(hostid) ];
}

tusb_error_t hcd_controller_init(uint8_t hostid) ATTR_WARN_UNUSED_RESULT;

//--------------------------------------------------------------------+
// USBH-HCD API
//--------------------------------------------------------------------+
tusb_error_t hcd_init(void)
{
  // oops, ehci_qtd_t:used must be at Reserved places in EHCI specs
  ASSERT(offsetof(ehci_qtd_t, used) == 16, TUSB_ERROR_HCD_FAILED); // TODO can be removed after an THOROUGH checked

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

//--------------------------------------------------------------------+
// PORT API
//--------------------------------------------------------------------+
void hcd_port_reset(uint8_t hostid)
{
  ehci_registers_t* const regs = get_operational_register(hostid);

  regs->portsc_bit.port_enable = 0; // disable port before reset
  regs->portsc_bit.port_reset = 1;

#ifndef _TEST_
  // NXP specific, port reset will automatically be 0 when reset sequence complete
  while( regs->portsc_bit.port_reset || !regs->portsc_bit.port_enable){}
#endif
}

bool hcd_port_connect_status(uint8_t hostid)
{
  return get_operational_register(hostid)->portsc_bit.current_connect_status;
}

//--------------------------------------------------------------------+
// EHCI Interrupt Handler
//--------------------------------------------------------------------+
static inline uint8_t get_qhd_index(ehci_qhd_t * p_qhd) ATTR_ALWAYS_INLINE ATTR_PURE;
static inline uint8_t get_qhd_index(ehci_qhd_t * p_qhd)
{
  return p_qhd - ehci_data.device[p_qhd->device_address].qhd;
}

void async_advance_isr(ehci_qhd_t * const async_head)
{
  if(async_head->is_removing) // closing control pipe of addr0
  {
    async_head->is_removing = 0;
    async_head->p_qtd_list_head = async_head->p_qtd_list_tail = NULL;
    async_head->qtd_overlay.halted = 1;
  }

  for(uint8_t relative_dev_addr=0; relative_dev_addr < TUSB_CFG_HOST_DEVICE_MAX; relative_dev_addr++)
  {
    // check if control endpoint is removing
    ehci_qhd_t *p_control_qhd = &ehci_data.device[relative_dev_addr].control.qhd;
    if( p_control_qhd->is_removing )
    {
      p_control_qhd->is_removing     = 0;
      p_control_qhd->used            = 0;
      p_control_qhd->p_qtd_list_head = p_control_qhd->p_qtd_list_tail = NULL;
    }

    // check if any other endpoints in pool is removing
    for (uint8_t i=0; i<EHCI_MAX_QHD; i++)
    {
      ehci_qhd_t *p_qhd = &ehci_data.device[relative_dev_addr].qhd[i];
      if (p_qhd->is_removing)
      {
        p_qhd->used        = 0;
        p_qhd->is_removing = 0;

        while(p_qhd->p_qtd_list_head != NULL) // remove all TDs
        {
          p_qhd->p_qtd_list_head->used = 0; // free QTD

          if (p_qhd->p_qtd_list_head == p_qhd->p_qtd_list_tail) // last TD --> make it NULL
          {
            p_qhd->p_qtd_list_head = p_qhd->p_qtd_list_tail = NULL;
          }else
          {
            p_qhd->p_qtd_list_head = (ehci_qtd_t*) align32(p_qhd->p_qtd_list_head->next.address);
          }
        }
      }
    }
  }
}

void port_connect_status_change_isr(uint8_t hostid)
{
  ehci_registers_t* const regs = get_operational_register(hostid);

  if (regs->portsc_bit.current_connect_status) // device plugged
  {
    hcd_port_reset(hostid);
    usbh_device_plugged_isr(hostid, regs->portsc_bit.nxp_port_speed); // NXP specific port speed
  }else // device unplugged
  {
    usbh_device_unplugged_isr(hostid);
    regs->usb_cmd_bit.advacne_async = 1; // Async doorbell check EHCI 4.8.2 for operational details
  }

}

void async_list_process_isr(ehci_qhd_t * const async_head, ehci_registers_t * const regs)
{
  ehci_qhd_t *p_qhd = async_head;
  do
  {
    if ( !p_qhd->qtd_overlay.halted )
    {
      // free all TDs from the head td to the first active TD
      while(p_qhd->p_qtd_list_head != NULL && !p_qhd->p_qtd_list_head->active)
      {
        // TODO check halted TD
        if (p_qhd->p_qtd_list_head->int_on_complete) // end of request
        {
          pipe_handle_t pipe_hdl = { .dev_addr = p_qhd->device_address };
          if (p_qhd->endpoint_number) // if not Control, can only be Bulk
          {
            pipe_hdl.xfer_type = TUSB_XFER_BULK;
            pipe_hdl.index = get_qhd_index(p_qhd);
          }
          usbh_isr( pipe_hdl, p_qhd->class_code); // call USBH call back
        }
        p_qhd->p_qtd_list_head->used = 0; // free QTD

        if (p_qhd->p_qtd_list_head == p_qhd->p_qtd_list_tail) // last TD --> make it NULL
        {
          p_qhd->p_qtd_list_head = p_qhd->p_qtd_list_tail = NULL;
        }else
        {
          p_qhd->p_qtd_list_head = (ehci_qtd_t*) align32(p_qhd->p_qtd_list_head->next.address);
        }
      }
    }
    p_qhd = (ehci_qhd_t*) align32(p_qhd->next.address);
  }while(p_qhd != async_head); // async list traversal, stop if loop around
}

//------------- Host Controller Driver's Interrupt Handler -------------//
void hcd_isr(uint8_t hostid)
{
  ehci_registers_t* const regs = get_operational_register(hostid);

  uint32_t int_status = regs->usb_sts & regs->usb_int_enable;

  if (int_status == 0)
    return;

  if (int_status & EHCI_INT_MASK_ERROR)
  {
    // TODO handle Queue Head halted
    // TODO invoke some error callback if not async head
    hal_debugger_breakpoint();
  }

  //------------- some QTD/SITD/ITD with IOC set is completed -------------//
  if (int_status & EHCI_INT_MASK_NXP_ASYNC)
  {
    async_list_process_isr(get_async_head(hostid), regs);
  }

  if (int_status & EHCI_INT_MASK_NXP_PERIODIC)
  {

  }

  if (int_status & EHCI_INT_MASK_PORT_CHANGE)
  {
    if (regs->portsc_bit.connect_status_change)
    {
      port_connect_status_change_isr(hostid);
    }

    regs->portsc |= EHCI_PORTSC_MASK_ALL; // Acknowledge all the change bit in portsc
  }

  if (int_status & EHCI_INT_MASK_ASYNC_ADVANCE) // need to place after EHCI_INT_MASK_NXP_ASYNC
  {
    async_advance_isr( get_async_head(hostid) );
  }

  regs->usb_sts |= regs->usb_sts; // Acknowledge interrupt & clear it
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
  regs->usb_int_enable = EHCI_INT_MASK_ERROR | EHCI_INT_MASK_PORT_CHANGE |
#if EHCI_PERIODIC_LIST
                         EHCI_INT_MASK_NXP_PERIODIC |
#endif
                         EHCI_INT_MASK_ASYNC_ADVANCE | EHCI_INT_MASK_NXP_ASYNC;

  //------------- Asynchronous List -------------//
  ehci_qhd_t * const async_head = get_async_head(hostid);
  memclr_(async_head, sizeof(ehci_qhd_t));

  async_head->next.address                    = (uint32_t) async_head; // circular list, next is itself
  async_head->next.type                       = EHCI_QUEUE_ELEMENT_QHD;
  async_head->head_list_flag                  = 1;
  async_head->qtd_overlay.halted              = 1; // inactive most of time
  async_head->qtd_overlay.next.terminate      = 1; // TODO removed if verified

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

  period_head->interrupt_smask    = 1; // queue head in period list must have smask non-zero
  period_head->next.terminate     = 1;
  period_head->qtd_overlay.halted = 1; // dummy node, always inactive

  regs->periodic_list_base        = (uint32_t) framelist;
#else
  regs->periodic_list_base = 0;
#endif

  //------------- TT Control (NXP only) -------------//
  regs->tt_control = 0;

  //------------- USB CMD Register -------------//

  regs->usb_cmd |= BIT_(EHCI_USBCMD_POS_RUN_STOP) | BIT_(EHCI_USBCMD_POS_ASYNC_ENABLE)
#if EHCI_PERIODIC_LIST
                  | BIT_(EHCI_USBCMD_POS_PERIOD_ENABLE)
#endif
                  | ((EHCI_CFG_FRAMELIST_SIZE_BITS & BIN8(011)) << EHCI_USBCMD_POS_FRAMELIST_SZIE)
                  | ((EHCI_CFG_FRAMELIST_SIZE_BITS >> 2) << EHCI_USBCMD_POS_NXP_FRAMELIST_SIZE_MSB);

  //------------- ConfigFlag Register (skip) -------------//

  regs->portsc_bit.port_power = 1; // enable port power

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

tusb_error_t hcd_controller_reset(uint8_t hostid)
{
  ehci_registers_t* const regs = get_operational_register(hostid);
  timeout_timer_t timeout;

// NXP chip powered with non-host mode --> sts bit is not correctly reflected
//  if (regs->usb_sts_bit.hc_halted == 0) // need to stop before reset
//  {
//    ASSERT_STATUS( hcd_controller_stop(hostid) );
//  }

  regs->usb_cmd_bit.reset = 1;

  timeout_set(&timeout, 16); // should not take longer the time to stop controller
  while( regs->usb_cmd_bit.reset && !timeout_expired(&timeout)) {}

  return timeout_expired(&timeout) ? TUSB_ERROR_OSAL_TIMEOUT : TUSB_ERROR_NONE;
}

//--------------------------------------------------------------------+
// PIPE API
//--------------------------------------------------------------------+
static void init_qhd(ehci_qhd_t *p_qhd, uint8_t dev_addr, uint16_t max_packet_size, uint8_t endpoint_addr, uint8_t xfer_type);

static inline ehci_qhd_t* get_control_qhd(uint8_t dev_addr) ATTR_ALWAYS_INLINE ATTR_PURE ATTR_WARN_UNUSED_RESULT;
static inline ehci_qtd_t* get_control_qtds(uint8_t dev_addr) ATTR_ALWAYS_INLINE ATTR_PURE ATTR_WARN_UNUSED_RESULT;

//--------------------------------------------------------------------+
// CONTROL PIPE API
//--------------------------------------------------------------------+
static void init_qtd(ehci_qtd_t* p_qtd, uint32_t data_ptr, uint16_t total_bytes)
{
  memclr_(p_qtd, sizeof(ehci_qtd_t));

  p_qtd->used                = 1;

  p_qtd->next.terminate      = 1; // init to null
  p_qtd->alternate.terminate = 1; // not used, always set to terminated
  p_qtd->active              = 1;
  p_qtd->cerr                = 3; // TODO 3 consecutive errors tolerance
  p_qtd->data_toggle         = 0;
  p_qtd->total_bytes         = total_bytes;

  p_qtd->buffer[0]           = data_ptr;

  for(uint8_t i=1; i<5; i++)
  {
    p_qtd->buffer[i] |= align4k( p_qtd->buffer[i-1] ) + 4096;
  }
}

static inline void list_insert(ehci_link_t *current, ehci_link_t *new, uint8_t new_type) ATTR_ALWAYS_INLINE;
static inline void list_insert(ehci_link_t *current, ehci_link_t *new, uint8_t new_type)
{
  new->address = current->address;
  current->address = (uint32_t) new;
  current->type = new_type;
}

static inline void insert_qtd_to_qhd(ehci_qhd_t *p_qhd, ehci_qtd_t *p_qtd_new) ATTR_ALWAYS_INLINE;
static inline void insert_qtd_to_qhd(ehci_qhd_t *p_qhd, ehci_qtd_t *p_qtd_new)
{
  if (p_qhd->p_qtd_list_head == NULL) // empty list
  {
    p_qhd->p_qtd_list_head               = p_qhd->p_qtd_list_tail = p_qtd_new;
    p_qhd->qtd_overlay.next.address      = (uint32_t) p_qtd_new;
  }else
  {
    p_qhd->p_qtd_list_tail->next.address = (uint32_t) p_qtd_new;
    p_qhd->p_qtd_list_tail               = p_qtd_new;
  }
}

tusb_error_t  hcd_pipe_control_open(uint8_t dev_addr, uint8_t max_packet_size)
{
  ehci_qhd_t * const p_qhd = get_control_qhd(dev_addr);

  init_qhd(p_qhd, dev_addr, max_packet_size, 0, TUSB_XFER_CONTROL);

  if (dev_addr != 0)
  {
    //------------- insert to async list -------------//
    // TODO might need to to disable async list first
    list_insert( (ehci_link_t*) get_async_head(usbh_device_info_pool[dev_addr].core_id),
                 (ehci_link_t*) p_qhd, EHCI_QUEUE_ELEMENT_QHD);
  }

  return TUSB_ERROR_NONE;
}

tusb_error_t  hcd_pipe_control_xfer(uint8_t dev_addr, tusb_std_request_t const * p_request, uint8_t data[])
{
  ehci_qhd_t * const p_qhd = get_control_qhd(dev_addr);

  ehci_qtd_t *p_setup      = get_control_qtds(dev_addr);
  ehci_qtd_t *p_data       = p_setup + 1;
  ehci_qtd_t *p_status     = p_setup + 2;

  //------------- SETUP Phase -------------//
  init_qtd(p_setup, (uint32_t) p_request, 8);
  p_setup->pid          = EHCI_PID_SETUP;
  p_setup->next.address = (uint32_t) p_data;

  //------------- DATA Phase -------------//
  if (p_request->wLength > 0)
  {
    init_qtd(p_data, (uint32_t) data, p_request->wLength);
    p_data->data_toggle = 1;
    p_data->pid         = p_request->bmRequestType.direction ? EHCI_PID_IN : EHCI_PID_OUT;
  }else
  {
    p_data = p_setup;
  }
  p_data->next.address = (uint32_t) p_status;

  //------------- STATUS Phase -------------//
  init_qtd(p_status, 0, 0); // zero-length data
  p_status->int_on_complete = 1;
  p_status->data_toggle     = 1;
  p_status->pid             = p_request->bmRequestType.direction ? EHCI_PID_OUT : EHCI_PID_IN; // reverse direction of data phase
  p_status->next.terminate  = 1;

  //------------- Attach TDs list to Control Endpoint -------------//
  p_qhd->p_qtd_list_head = p_setup;
  p_qhd->p_qtd_list_tail = p_status;

  p_qhd->qtd_overlay.next.address = (uint32_t) p_setup;

  return TUSB_ERROR_NONE;
}

ehci_qhd_t* find_previous_qhd(ehci_qhd_t* p_head, ehci_qhd_t* p_qhd)
{
  ehci_qhd_t *p_prev_qhd = p_head;
  while( (align32(p_prev_qhd->next.address) != (uint32_t) p_head) && (align32(p_prev_qhd->next.address) != (uint32_t) p_qhd) )
  {
    p_prev_qhd = (ehci_qhd_t*) align32(p_prev_qhd->next.address);
  }

  return  align32(p_prev_qhd->next.address) != (uint32_t) p_head ? p_prev_qhd : NULL;
}

tusb_error_t remove_qhd_from_async_list(ehci_qhd_t* p_head, ehci_qhd_t* p_qhd_remove)
{
  ehci_qhd_t *p_prev_qhd = find_previous_qhd(p_head, p_qhd_remove);

  ASSERT_PTR(p_prev_qhd, TUSB_ERROR_INVALID_PARA);

  p_prev_qhd->next.address   = p_qhd_remove->next.address;
  // EHCI 4.8.2 link the removing queue head to async_head (which always on the async list)
  p_qhd_remove->next.address = (uint32_t) p_head;
  p_qhd_remove->next.type    = EHCI_QUEUE_ELEMENT_QHD;

  return TUSB_ERROR_NONE;
}

tusb_error_t  hcd_pipe_control_close(uint8_t dev_addr)
{
  //------------- TODO pipe handle validate -------------//
  ehci_qhd_t * const p_qhd = get_control_qhd(dev_addr);

  p_qhd->is_removing = 1;

  if (dev_addr != 0)
  {
    ASSERT_STATUS( remove_qhd_from_async_list(get_async_head( usbh_device_info_pool[dev_addr].core_id ), p_qhd) );
  }

  return TUSB_ERROR_NONE;
}

//--------------------------------------------------------------------+
// BULK/INT/ISO PIPE API
//--------------------------------------------------------------------+
pipe_handle_t hcd_pipe_open(uint8_t dev_addr, tusb_descriptor_endpoint_t const * p_endpoint_desc, uint8_t class_code)
{
  pipe_handle_t const null_handle = { .dev_addr = 0, .xfer_type = 0, .index = 0 };

  if (p_endpoint_desc->bmAttributes.xfer == TUSB_XFER_ISOCHRONOUS)
    return null_handle; // TODO not support ISO yet

  //------------- find a free queue head -------------//
  uint8_t index=0;
  while( index<EHCI_MAX_QHD && ehci_data.device[dev_addr].qhd[index].used )
  {
    index++;
  }
  ASSERT( index < EHCI_MAX_QHD, null_handle);

  ehci_qhd_t * const p_qhd = &ehci_data.device[dev_addr].qhd[index];
  init_qhd(p_qhd, dev_addr, p_endpoint_desc->wMaxPacketSize, p_endpoint_desc->bEndpointAddress, p_endpoint_desc->bmAttributes.xfer);
  p_qhd->class_code = class_code;

  ehci_qhd_t * list_head;

  if (p_endpoint_desc->bmAttributes.xfer == TUSB_XFER_BULK)
  {
    // TODO might need to to disable async list first
    list_head = get_async_head(usbh_device_info_pool[dev_addr].core_id);
  }else if (p_endpoint_desc->bmAttributes.xfer == TUSB_XFER_INTERRUPT)
  {
    // TODO might need to to disable period list first
    list_head = get_period_head(usbh_device_info_pool[dev_addr].core_id);
  }

  //------------- insert to async/period list -------------//
  list_insert( (ehci_link_t*) list_head,
               (ehci_link_t*) p_qhd, EHCI_QUEUE_ELEMENT_QHD);

  return (pipe_handle_t) { .dev_addr = dev_addr, .xfer_type = p_endpoint_desc->bmAttributes.xfer, .index = index};
}

tusb_error_t  hcd_pipe_xfer(pipe_handle_t pipe_hdl, uint8_t buffer[], uint16_t total_bytes, bool int_on_complete)
{
  //------------- TODO pipe handle validate -------------//

  //------------- find a free qtd -------------//
  uint8_t index=0;
  while( index<EHCI_MAX_QTD && ehci_data.device[pipe_hdl.dev_addr].qtd[index].used )
  {
    index++;
  }
  ASSERT( index < EHCI_MAX_QTD, TUSB_ERROR_EHCI_NOT_ENOUGH_QTD);

  //------------- set up QTD -------------//
  ehci_qhd_t *p_qhd = &ehci_data.device[pipe_hdl.dev_addr].qhd[pipe_hdl.index];
  ehci_qtd_t *p_qtd = &ehci_data.device[pipe_hdl.dev_addr].qtd[index];

  init_qtd(p_qtd, (uint32_t) buffer, total_bytes);
  p_qtd->pid = p_qhd->pid_non_control;
  p_qtd->int_on_complete = int_on_complete ? 1 : 0;

  //------------- insert TD to TD list -------------//
  insert_qtd_to_qhd(p_qhd, p_qtd);

  return TUSB_ERROR_NONE;
}

static inline ehci_qhd_t* get_qhd_from_pipe_handle(pipe_handle_t pipe_hdl) ATTR_PURE ATTR_ALWAYS_INLINE;
static inline ehci_qhd_t* get_qhd_from_pipe_handle(pipe_handle_t pipe_hdl)
{
  return &ehci_data.device[ pipe_hdl.dev_addr ].qhd[ pipe_hdl.index ];
}

tusb_error_t  hcd_pipe_close(pipe_handle_t pipe_hdl)
{
  ASSERT(pipe_hdl.xfer_type != TUSB_XFER_ISOCHRONOUS, TUSB_ERROR_INVALID_PARA);

  ehci_qhd_t *p_qhd = get_qhd_from_pipe_handle( pipe_hdl );
  p_qhd->is_removing = 1;

  if ( pipe_hdl.xfer_type == TUSB_XFER_BULK )
  {
    ASSERT_STATUS( remove_qhd_from_async_list(
        get_async_head( usbh_device_info_pool[pipe_hdl.dev_addr].core_id ),
        p_qhd) );
  }else
  {
    ASSERT(false, TUSB_ERROR_INVALID_PARA);
  }

  return TUSB_ERROR_NONE;
}

//--------------------------------------------------------------------+
// HELPER
//--------------------------------------------------------------------+
static inline ehci_qhd_t* get_control_qhd(uint8_t dev_addr)
{
  return (dev_addr == 0) ?
      get_async_head( usbh_device_info_pool[dev_addr].core_id ) :
      &ehci_data.device[dev_addr-1].control.qhd;
}
static inline ehci_qtd_t* get_control_qtds(uint8_t dev_addr)
{
  return (dev_addr == 0) ?
      ehci_data.addr0_qtd :
      ehci_data.device[ dev_addr ].control.qtd;

}

static void init_qhd(ehci_qhd_t *p_qhd, uint8_t dev_addr, uint16_t max_packet_size, uint8_t endpoint_addr, uint8_t xfer_type)
{
  // address 0 uses async head, which always on the list --> cannot be cleared (ehci halted otherwise)
  if (dev_addr != 0)
  {
    memclr_(p_qhd, sizeof(ehci_qhd_t));
  }

  p_qhd->device_address                   = dev_addr;
  p_qhd->non_hs_period_inactive_next_xact = 0;
  p_qhd->endpoint_number                  = endpoint_addr & 0x0F;
  p_qhd->endpoint_speed                   = usbh_device_info_pool[dev_addr].speed;
  p_qhd->data_toggle_control              = (xfer_type == TUSB_XFER_CONTROL) ? 1 : 0;
  p_qhd->head_list_flag                   = (dev_addr == 0) ? 1 : 0; // addr0's endpoint is the static asyn list head
  p_qhd->max_package_size                 = max_packet_size;
  p_qhd->non_hs_control_endpoint          = ((TUSB_XFER_CONTROL == xfer_type) && (usbh_device_info_pool[dev_addr].speed != TUSB_SPEED_HIGH) )  ? 1 : 0;
  p_qhd->nak_count_reload                 = 0;

  // Bulk/Control -> smask = cmask = 0
  if (TUSB_XFER_INTERRUPT == xfer_type)
  {
    // Highspeed: schedule every uframe (1 us interval); Full/Low: schedule only 1st frame
    p_qhd->interrupt_smask         = (TUSB_SPEED_HIGH == usbh_device_info_pool[dev_addr].speed) ? 0xFF : 0x01;
    // Highspeed: ignored by Host Controller, Full/Low: 4.12.2.1 (EHCI) case 1 schedule complete split at 2,3,4 uframe
    p_qhd->non_hs_interrupt_cmask  = BIN8(11100);
  }else
  {
    p_qhd->interrupt_smask = p_qhd->non_hs_interrupt_cmask = 0;
  }

  p_qhd->hub_address             = usbh_device_info_pool[dev_addr].hub_addr;
  p_qhd->hub_port                = usbh_device_info_pool[dev_addr].hub_port;
  p_qhd->mult                    = 1; // TODO not use high bandwidth/park mode yet

  //------------- active, but no TD list -------------//
  p_qhd->qtd_overlay.halted              = 0;
  p_qhd->qtd_overlay.next.terminate      = 1;
  p_qhd->qtd_overlay.alternate.terminate = 1;

  //------------- HCD Management Data -------------//
  p_qhd->used            = 1;
  p_qhd->is_removing     = 0;
  p_qhd->p_qtd_list_head = NULL;
  p_qhd->p_qtd_list_tail = NULL;
  p_qhd->pid_non_control = (endpoint_addr & 0x80) ? EHCI_PID_IN : EHCI_PID_OUT; // PID for TD under this endpoint

}


#endif

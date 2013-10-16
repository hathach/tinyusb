/**************************************************************************/
/*!
    @file     ehci_controller_fake.c
    @author   hathach (tinyusb.org)

    @section LICENSE

    Software License Agreement (BSD License)

    Copyright (c) 2013, hathach (tinyusb.org)
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:
    1. Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
    3. Neither the name of the copyright holders nor the
    names of its contributors may be used to endorse or promote products
    derived from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ''AS IS'' AND ANY
    EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    INCLUDING NEGLIGENCE OR OTHERWISE ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

    This file is part of the tinyusb stack.
*/
/**************************************************************************/

//--------------------------------------------------------------------+
// INCLUDE
//--------------------------------------------------------------------+
#include "unity.h"
#include "common/common.h"
#include "hal.h"
#include "usbh_hcd.h"
#include "ehci.h"

#include "ehci_controller_fake.h"


//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+
LPC_USB0_Type lpc_usb0;
LPC_USB1_Type lpc_usb1;

extern usbh_device_info_t usbh_devices[TUSB_CFG_HOST_DEVICE_MAX+1];

//--------------------------------------------------------------------+
// IMPLEMENTATION
//--------------------------------------------------------------------+
void ehci_controller_init(void)
{
  memclr_(&lpc_usb0, sizeof(LPC_USB0_Type));
  memclr_(&lpc_usb1, sizeof(LPC_USB1_Type));
}

void ehci_controller_control_xfer_proceed(uint8_t dev_addr, uint8_t p_data[])
{
  ehci_registers_t* const regs = get_operational_register( usbh_devices[dev_addr].core_id );
  ehci_qhd_t * p_qhd = get_control_qhd(dev_addr);
  ehci_qtd_t * p_qtd_setup = get_control_qtds(dev_addr);
  ehci_qtd_t * p_qtd_data  = p_qtd_setup + 1;
  ehci_qtd_t * p_qtd_status = p_qtd_setup + 2;

  tusb_control_request_t const *p_request = (tusb_control_request_t *) p_qtd_setup->buffer[0];

  if (p_request->wLength > 0 && p_request->bmRequestType_bit.direction == TUSB_DIR_DEV_TO_HOST)
  {
    memcpy(p_qtd_data, p_data, p_request->wLength);
  }

  //------------- retire all QTDs -------------//
  p_qtd_setup->active = p_qtd_data->active = p_qtd_status->active = 0;
  p_qhd->qtd_overlay = *p_qtd_status;

  regs->usb_sts = EHCI_INT_MASK_NXP_ASYNC | EHCI_INT_MASK_NXP_PERIODIC;

  hcd_isr( usbh_devices[dev_addr].core_id );
}

void complete_qtd_in_qhd(ehci_qhd_t *p_qhd)
{
  if ( !p_qhd->qtd_overlay.halted )
  {
    while(!p_qhd->qtd_overlay.next.terminate)
    {
      ehci_qtd_t* p_qtd = (ehci_qtd_t*) align32(p_qhd->qtd_overlay.next.address);
      p_qtd->active = 0;
      p_qtd->total_bytes = 0;
      p_qhd->qtd_overlay = *p_qtd;
    }
  }
}

bool complete_all_qtd_in_async(ehci_qhd_t *head)
{
  ehci_qhd_t *p_qhd = head;

  do
  {
    complete_qtd_in_qhd(p_qhd);
    p_qhd = (ehci_qhd_t*) align32(p_qhd->next.address);
  }while(p_qhd != head); // stop if loop around

  return true;
}

bool complete_all_qtd_in_period(ehci_link_t *head)
{
  while(!head->terminate)
  {
    uint32_t queue_type = head->type;
    head = (ehci_link_t*) align32(head->address);

    if ( queue_type == EHCI_QUEUE_ELEMENT_QHD)
    {
      complete_qtd_in_qhd( (ehci_qhd_t*) head );
    }
  }
  return true;
}

void ehci_controller_run(uint8_t hostid)
{
  //------------- Async List -------------//
  ehci_registers_t* const regs = get_operational_register(hostid);
  complete_all_qtd_in_async((ehci_qhd_t*) regs->async_list_base);

  //------------- Period List -------------//
  for(uint8_t i=1; i <= EHCI_FRAMELIST_SIZE; i *= 2)
  {
    complete_all_qtd_in_period( get_period_head(hostid, i) );
  }
  regs->usb_sts = EHCI_INT_MASK_NXP_ASYNC | EHCI_INT_MASK_NXP_PERIODIC;

  hcd_isr(hostid);
}

void complete_1st_qtd_with_error(ehci_qhd_t* p_qhd, bool halted, bool xact_err)
{
  if ( !p_qhd->qtd_overlay.halted )
  {
    if(!p_qhd->qtd_overlay.next.terminate) // TODO add active check
    {
      ehci_qtd_t* p_qtd = (ehci_qtd_t*) align32(p_qhd->qtd_overlay.next.address);
      p_qtd->active   = 0;
      p_qtd->halted   = halted ? 1 : 0;
      p_qtd->xact_err = xact_err ? 1 : 0;

      p_qhd->qtd_overlay = *p_qtd;
    }
  }
}

void complete_list_with_error(uint8_t hostid, bool halted, bool xact_err)
{
  //------------- Async List -------------//
  ehci_registers_t* const regs = get_operational_register(hostid);

  ehci_qhd_t *p_qhd = (ehci_qhd_t*) regs->async_list_base;
  do
  {
    complete_1st_qtd_with_error(p_qhd, halted, xact_err);
    p_qhd = (ehci_qhd_t*) align32(p_qhd->next.address);
  }while(p_qhd != get_async_head(hostid)); // stop if loop around

  //------------- Period List -------------//
  for(uint8_t i=1; i <= EHCI_FRAMELIST_SIZE; i *= 2)
  {
    ehci_link_t *head = get_period_head(hostid, i);

    while(!head->terminate)
    {
      uint32_t queue_type = head->type;
      head = (ehci_link_t*) align32(head->address);

      if ( queue_type == EHCI_QUEUE_ELEMENT_QHD)
      {
        complete_1st_qtd_with_error((ehci_qhd_t*) head, halted, xact_err);
      }
    }
  }

  regs->usb_sts = EHCI_INT_MASK_ERROR;

  hcd_isr(hostid);
}

void ehci_controller_run_stall(uint8_t hostid)
{
  complete_list_with_error(hostid, true, false);
}

void ehci_controller_run_error(uint8_t hostid)
{
  complete_list_with_error(hostid, true, true);
}

void ehci_controller_device_plug(uint8_t hostid, tusb_speed_t speed)
{
  ehci_registers_t* const regs = get_operational_register(hostid);

  regs->usb_sts_bit.port_change_detect    = 1;
  regs->portsc_bit.connect_status_change  = 1;
  regs->portsc_bit.current_connect_status = 1;
  regs->portsc_bit.nxp_port_speed         = speed;

  hcd_isr(hostid);
}

void ehci_controller_device_unplug(uint8_t hostid)
{
  ehci_registers_t* const regs = get_operational_register(hostid);

  regs->usb_sts_bit.port_change_detect    = 1;
  regs->portsc_bit.connect_status_change  = 1;
  regs->portsc_bit.current_connect_status = 0;

  hcd_isr(hostid);
}

/**************************************************************************/
/*!
    @file     ehci.c
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

#include "common/common.h"

#if MODE_HOST_SUPPORTED && (TUSB_CFG_MCU == MCU_LPC43XX || TUSB_CFG_MCU == MCU_LPC18XX)
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
TUSB_CFG_ATTR_USBRAM STATIC_VAR ehci_data_t ehci_data;

#if EHCI_PERIODIC_LIST

  #if (TUSB_CFG_CONTROLLER_0_MODE & TUSB_MODE_HOST)
  TUSB_CFG_ATTR_USBRAM ATTR_ALIGNED(4096) STATIC_VAR ehci_link_t period_frame_list0[EHCI_FRAMELIST_SIZE];

    #ifndef __ICCARM__ // IAR cannot able to determine the alignment with datalignment pragma
    STATIC_ASSERT( ALIGN_OF(period_frame_list0) == 4096, "Period Framelist must be 4k alginment"); // validation
    #endif
  #endif

  #if (TUSB_CFG_CONTROLLER_1_MODE & TUSB_MODE_HOST)
  TUSB_CFG_ATTR_USBRAM ATTR_ALIGNED(4096) STATIC_VAR ehci_link_t period_frame_list1[EHCI_FRAMELIST_SIZE];

    #ifndef __ICCARM__ // IAR cannot able to determine the alignment with datalignment pragma
    STATIC_ASSERT( ALIGN_OF(period_frame_list1) == 4096, "Period Framelist must be 4k alginment"); // validation
    #endif
  #endif
#endif

//------------- Validation -------------//
// TODO static assert for memory placement on some known MCU such as lpc43xx

//--------------------------------------------------------------------+
// PROTOTYPE
//--------------------------------------------------------------------+
STATIC_ INLINE_ ehci_registers_t*  get_operational_register(uint8_t hostid) ATTR_PURE ATTR_ALWAYS_INLINE ATTR_WARN_UNUSED_RESULT;
STATIC_ INLINE_ ehci_link_t*       get_period_frame_list(uint8_t hostid) ATTR_PURE ATTR_ALWAYS_INLINE ATTR_WARN_UNUSED_RESULT;
STATIC_ INLINE_ uint8_t            hostid_to_data_idx(uint8_t hostid) ATTR_ALWAYS_INLINE ATTR_CONST ATTR_WARN_UNUSED_RESULT;

STATIC_ INLINE_ ehci_qhd_t*  get_async_head(uint8_t hostid) ATTR_ALWAYS_INLINE ATTR_PURE ATTR_WARN_UNUSED_RESULT;
STATIC_ INLINE_ ehci_link_t* get_period_head(uint8_t hostid, uint8_t interval_ms) ATTR_ALWAYS_INLINE ATTR_PURE ATTR_WARN_UNUSED_RESULT;

STATIC_ INLINE_ ehci_qhd_t* get_control_qhd(uint8_t dev_addr) ATTR_ALWAYS_INLINE ATTR_PURE ATTR_WARN_UNUSED_RESULT;
STATIC_ INLINE_ ehci_qtd_t* get_control_qtds(uint8_t dev_addr) ATTR_ALWAYS_INLINE ATTR_PURE ATTR_WARN_UNUSED_RESULT;

static inline uint8_t        qhd_get_index(ehci_qhd_t const * p_qhd) ATTR_ALWAYS_INLINE ATTR_PURE;
static inline ehci_qhd_t*    qhd_next(ehci_qhd_t const * p_qhd) ATTR_ALWAYS_INLINE ATTR_PURE;
static inline ehci_qhd_t*    qhd_find_free (uint8_t dev_addr) ATTR_PURE ATTR_ALWAYS_INLINE;
static inline tusb_xfer_type_t qhd_get_xfer_type(ehci_qhd_t const * p_qhd) ATTR_ALWAYS_INLINE ATTR_PURE;
STATIC_ INLINE_ ehci_qhd_t*  qhd_get_from_pipe_handle(pipe_handle_t pipe_hdl) ATTR_PURE ATTR_ALWAYS_INLINE;
static inline pipe_handle_t  qhd_create_pipe_handle(ehci_qhd_t const * p_qhd, tusb_xfer_type_t xfer_type) ATTR_PURE ATTR_ALWAYS_INLINE;
// determine if a queue head has bus-related error
static inline bool qhd_has_xact_error(ehci_qhd_t * p_qhd) ATTR_ALWAYS_INLINE ATTR_PURE;
static inline bool qhd_has_xact_error(ehci_qhd_t * p_qhd)
{
  return ( p_qhd->qtd_overlay.buffer_err ||p_qhd->qtd_overlay.babble_err || p_qhd->qtd_overlay.xact_err );
  //p_qhd->qtd_overlay.non_hs_period_missed_uframe || p_qhd->qtd_overlay.pingstate_err TODO split transaction error
}

static void qhd_init(ehci_qhd_t *p_qhd, uint8_t dev_addr, uint16_t max_packet_size, uint8_t endpoint_addr, uint8_t xfer_type, uint8_t interval);


STATIC_ INLINE_ ehci_qtd_t*  qtd_find_free(uint8_t dev_addr) ATTR_PURE ATTR_ALWAYS_INLINE;
static inline ehci_qtd_t*    qtd_next(ehci_qtd_t const * p_qtd ) ATTR_PURE ATTR_ALWAYS_INLINE;
static inline void           qtd_insert_to_qhd(ehci_qhd_t *p_qhd, ehci_qtd_t *p_qtd_new) ATTR_ALWAYS_INLINE;
static inline void           qtd_remove_1st_from_qhd(ehci_qhd_t *p_qhd) ATTR_ALWAYS_INLINE;
static void qtd_init(ehci_qtd_t* p_qtd, uint32_t data_ptr, uint16_t total_bytes);

static inline void  list_insert(ehci_link_t *current, ehci_link_t *new, uint8_t new_type) ATTR_ALWAYS_INLINE;
static inline ehci_link_t* list_next(ehci_link_t *p_link_pointer) ATTR_PURE ATTR_ALWAYS_INLINE;
static ehci_link_t*  list_find_previous_item(ehci_link_t* p_head, ehci_link_t* p_current);
static tusb_error_t list_remove_qhd(ehci_link_t* p_head, ehci_link_t* p_remove);

static tusb_error_t hcd_controller_init(uint8_t hostid) ATTR_WARN_UNUSED_RESULT;
static tusb_error_t hcd_controller_stop(uint8_t hostid) ATTR_WARN_UNUSED_RESULT ATTR_UNUSED;

//--------------------------------------------------------------------+
// USBH-HCD API
//--------------------------------------------------------------------+
tusb_error_t hcd_init(void)
{
  //------------- Data Structure init -------------//
  memclr_(&ehci_data, sizeof(ehci_data_t));

  #if (TUSB_CFG_CONTROLLER_0_MODE & TUSB_MODE_HOST)
    ASSERT_STATUS (hcd_controller_init(0));
  #endif

  #if (TUSB_CFG_CONTROLLER_1_MODE & TUSB_MODE_HOST)
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
}

bool hcd_port_connect_status(uint8_t hostid)
{
  return get_operational_register(hostid)->portsc_bit.current_connect_status;
}

tusb_speed_t hcd_port_speed_get(uint8_t hostid)
{
  return (tusb_speed_t) get_operational_register(hostid)->portsc_bit.nxp_port_speed; // NXP specific port speed
}

// TODO refractor abtract later
void hcd_port_unplug(uint8_t hostid)
{
	ehci_registers_t* const regs = get_operational_register(hostid);
  regs->usb_cmd_bit.advacne_async = 1; // Async doorbell check EHCI 4.8.2 for operational details
}

//--------------------------------------------------------------------+
// Controller API
//--------------------------------------------------------------------+
static tusb_error_t hcd_controller_init(uint8_t hostid)
{
  ehci_registers_t* const regs = get_operational_register(hostid);

  //------------- CTRLDSSEGMENT Register (skip) -------------//
  //------------- USB INT Register -------------//
  regs->usb_int_enable = 0;                 // 1. disable all the interrupt
#ifndef _TEST_ // the fake controller does not have write-to-clear behavior
  regs->usb_sts        = EHCI_INT_MASK_ALL; // 2. clear all status
#endif
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

#if EHCI_PERIODIC_LIST
  //------------- Periodic List -------------//
  // Build the polling interval tree with 1 ms, 2 ms, 4 ms and 8 ms (framesize) only

  for(uint32_t i=0; i<4; i++)
  {
    ehci_data.period_head_arr[ hostid_to_data_idx(hostid) ][i].interrupt_smask    = 1; // queue head in period list must have smask non-zero
    ehci_data.period_head_arr[ hostid_to_data_idx(hostid) ][i].qtd_overlay.halted = 1; // dummy node, always inactive
  }

  ehci_link_t * const framelist  = get_period_frame_list(hostid);
  ehci_link_t * const period_1ms = get_period_head(hostid, 1);
  // all links --> period_head_arr[0] (1ms)
  // 0, 2, 4, 6 etc --> period_head_arr[1] (2ms)
  // 1, 5 --> period_head_arr[2] (4ms)
  // 3 --> period_head_arr[3] (8ms)

  // TODO EHCI_FRAMELIST_SIZE with other size than 8
  for(uint32_t i=0; i<EHCI_FRAMELIST_SIZE; i++)
  {
    framelist[i].address = (uint32_t) period_1ms;
    framelist[i].type    = EHCI_QUEUE_ELEMENT_QHD;
  }

  for(uint32_t i=0; i<EHCI_FRAMELIST_SIZE; i+=2)
  {
    list_insert(framelist + i, get_period_head(hostid, 2), EHCI_QUEUE_ELEMENT_QHD);
  }

  for(uint32_t i=1; i<EHCI_FRAMELIST_SIZE; i+=4)
  {
    list_insert(framelist + i, get_period_head(hostid, 4), EHCI_QUEUE_ELEMENT_QHD);
  }

  list_insert(framelist+3, get_period_head(hostid, 8), EHCI_QUEUE_ELEMENT_QHD);

  period_1ms->terminate    = 1;

  regs->periodic_list_base = (uint32_t) framelist;
#else
  regs->periodic_list_base = 0;
#endif

  //------------- TT Control (NXP only) -------------//
  regs->tt_control = 0;

  //------------- USB CMD Register -------------//

  regs->usb_cmd |= BIT_(EHCI_USBCMD_POS_RUN_STOP) | BIT_(EHCI_USBCMD_POS_ASYNC_ENABLE)
#if EHCI_PERIODIC_LIST
                  | BIT_(EHCI_USBCMD_POS_PERIOD_ENABLE) // TODO enable period list only there is int/iso endpoint
#endif
                  | ((EHCI_CFG_FRAMELIST_SIZE_BITS & BIN8(011)) << EHCI_USBCMD_POS_FRAMELIST_SZIE)
                  | ((EHCI_CFG_FRAMELIST_SIZE_BITS >> 2) << EHCI_USBCMD_POS_NXP_FRAMELIST_SIZE_MSB);

  //------------- ConfigFlag Register (skip) -------------//

  regs->portsc_bit.port_power = 1; // enable port power

  return TUSB_ERROR_NONE;
}

static tusb_error_t hcd_controller_stop(uint8_t hostid)
{
  ehci_registers_t* const regs = get_operational_register(hostid);
  timeout_timer_t timeout;

  regs->usb_cmd_bit.run_stop = 0;

  timeout_set(&timeout, 2); // USB Spec: controller has to stop within 16 uframe = 2 frames
  while( regs->usb_sts_bit.hc_halted == 0 && !timeout_expired(&timeout)) {}

  return timeout_expired(&timeout) ? TUSB_ERROR_OSAL_TIMEOUT : TUSB_ERROR_NONE;
}

//--------------------------------------------------------------------+
// CONTROL PIPE API
//--------------------------------------------------------------------+
tusb_error_t  hcd_pipe_control_open(uint8_t dev_addr, uint8_t max_packet_size)
{
  ehci_qhd_t * const p_qhd = get_control_qhd(dev_addr);

  qhd_init(p_qhd, dev_addr, max_packet_size, 0, TUSB_XFER_CONTROL, 1); // TODO binterval of control is ignored

  if (dev_addr != 0)
  {
    //------------- insert to async list -------------//
    list_insert( (ehci_link_t*) get_async_head(usbh_devices[dev_addr].core_id),
                 (ehci_link_t*) p_qhd, EHCI_QUEUE_ELEMENT_QHD);
  }

  return TUSB_ERROR_NONE;
}

tusb_error_t  hcd_pipe_control_xfer(uint8_t dev_addr, tusb_control_request_t const * p_request, uint8_t data[])
{
  ehci_qhd_t * const p_qhd = get_control_qhd(dev_addr);

  ehci_qtd_t *p_setup      = get_control_qtds(dev_addr);
  ehci_qtd_t *p_data       = p_setup + 1;
  ehci_qtd_t *p_status     = p_setup + 2;

  //------------- SETUP Phase -------------//
  qtd_init(p_setup, (uint32_t) p_request, 8);
  p_setup->pid          = EHCI_PID_SETUP;
  p_setup->next.address = (uint32_t) p_data;

  //------------- DATA Phase -------------//
  if (p_request->wLength > 0)
  {
    qtd_init(p_data, (uint32_t) data, p_request->wLength);
    p_data->data_toggle = 1;
    p_data->pid         = p_request->bmRequestType_bit.direction ? EHCI_PID_IN : EHCI_PID_OUT;
  }else
  {
    p_data = p_setup;
  }
  p_data->next.address = (uint32_t) p_status;

  //------------- STATUS Phase -------------//
  qtd_init(p_status, 0, 0); // zero-length data
  p_status->int_on_complete = 1;
  p_status->data_toggle     = 1;
  p_status->pid             = p_request->bmRequestType_bit.direction ? EHCI_PID_OUT : EHCI_PID_IN; // reverse direction of data phase
  p_status->next.terminate  = 1;

  //------------- Attach TDs list to Control Endpoint -------------//
  p_qhd->p_qtd_list_head = p_setup;
  p_qhd->p_qtd_list_tail = p_status;

  p_qhd->qtd_overlay.next.address = (uint32_t) p_setup;

  return TUSB_ERROR_NONE;
}

tusb_error_t  hcd_pipe_control_close(uint8_t dev_addr)
{
  //------------- TODO pipe handle validate -------------//
  ehci_qhd_t * const p_qhd = get_control_qhd(dev_addr);

  p_qhd->is_removing = 1;

  if (dev_addr != 0)
  {
    ASSERT_STATUS( list_remove_qhd( (ehci_link_t*) get_async_head( usbh_devices[dev_addr].core_id ),
                                    (ehci_link_t*) p_qhd) );
  }

  return TUSB_ERROR_NONE;
}

//--------------------------------------------------------------------+
// BULK/INT/ISO PIPE API
//--------------------------------------------------------------------+
pipe_handle_t hcd_pipe_open(uint8_t dev_addr, tusb_descriptor_endpoint_t const * p_endpoint_desc, uint8_t class_code)
{
  pipe_handle_t const null_handle = { .dev_addr = 0, .xfer_type = 0, .index = 0 };

  ASSERT(dev_addr > 0, null_handle);

  if (p_endpoint_desc->bmAttributes.xfer == TUSB_XFER_ISOCHRONOUS)
    return null_handle; // TODO not support ISO yet

  //------------- Prepare Queue Head -------------//
  ehci_qhd_t * const p_qhd = qhd_find_free(dev_addr);
  ASSERT_PTR(p_qhd, null_handle);

  qhd_init( p_qhd, dev_addr, p_endpoint_desc->wMaxPacketSize.size, p_endpoint_desc->bEndpointAddress,
            p_endpoint_desc->bmAttributes.xfer, p_endpoint_desc->bInterval );
  p_qhd->class_code = class_code;

  //------------- Insert to Async List -------------//
  ehci_link_t * list_head;

  if (p_endpoint_desc->bmAttributes.xfer == TUSB_XFER_BULK)
  {
    list_head = (ehci_link_t*) get_async_head(usbh_devices[dev_addr].core_id);
  }
  #if EHCI_PERIODIC_LIST // TODO refractor/group this together
  else if (p_endpoint_desc->bmAttributes.xfer == TUSB_XFER_INTERRUPT)
  {
    list_head = get_period_head(usbh_devices[dev_addr].core_id, p_qhd->interval_ms);
  }
  #endif

  //------------- insert to async/period list TODO might need to disable async/period list -------------//
  list_insert( list_head,
               (ehci_link_t*) p_qhd, EHCI_QUEUE_ELEMENT_QHD);

  return (pipe_handle_t) { .dev_addr = dev_addr, .xfer_type = p_endpoint_desc->bmAttributes.xfer, .index = qhd_get_index(p_qhd) };
}

tusb_error_t  hcd_pipe_queue_xfer(pipe_handle_t pipe_hdl, uint8_t buffer[], uint16_t total_bytes)
{
  //------------- TODO pipe handle validate -------------//

  //------------- set up QTD -------------//
  ehci_qhd_t *p_qhd = qhd_get_from_pipe_handle(pipe_hdl);
  ehci_qtd_t *p_qtd = qtd_find_free(pipe_hdl.dev_addr);

  ASSERT_PTR(p_qtd, TUSB_ERROR_EHCI_NOT_ENOUGH_QTD);

  qtd_init(p_qtd, (uint32_t) buffer, total_bytes);
  p_qtd->pid = p_qhd->pid_non_control;

  //------------- insert TD to TD list -------------//
  qtd_insert_to_qhd(p_qhd, p_qtd);

  return TUSB_ERROR_NONE;
}

tusb_error_t  hcd_pipe_xfer(pipe_handle_t pipe_hdl, uint8_t buffer[], uint16_t total_bytes, bool int_on_complete)
{
  ASSERT_STATUS ( hcd_pipe_queue_xfer(pipe_hdl, buffer, total_bytes) );

  ehci_qhd_t *p_qhd = qhd_get_from_pipe_handle(pipe_hdl);

  if ( int_on_complete )
  { // the just added qtd is pointed by list_tail
    p_qhd->p_qtd_list_tail->int_on_complete = 1;
  }
  p_qhd->qtd_overlay.next.address = (uint32_t) p_qhd->p_qtd_list_head; // attach head QTD to QHD start transferring

  return TUSB_ERROR_NONE;
}

/// pipe_close should only be called as a part of unmount/safe-remove process
tusb_error_t  hcd_pipe_close(pipe_handle_t pipe_hdl)
{
  ASSERT(pipe_hdl.dev_addr > 0, TUSB_ERROR_INVALID_PARA);

  ASSERT(pipe_hdl.xfer_type != TUSB_XFER_ISOCHRONOUS, TUSB_ERROR_INVALID_PARA);

  ehci_qhd_t *p_qhd = qhd_get_from_pipe_handle( pipe_hdl );

  // async list needs async advance handshake to make sure host controller has released cached data
  // non-control does not use async advance, it will eventually free by control pipe close
  // period list queue element is guarantee to be free in the next frame (1 ms)
  p_qhd->is_removing = 1; // TODO redundant, only apply to control queue head

  if ( pipe_hdl.xfer_type == TUSB_XFER_BULK )
  {
    ASSERT_STATUS( list_remove_qhd(
        (ehci_link_t*) get_async_head( usbh_devices[pipe_hdl.dev_addr].core_id ),
        (ehci_link_t*) p_qhd) );
  }
  #if EHCI_PERIODIC_LIST // TODO refractor/group this together
  else
  {
    ASSERT_STATUS( list_remove_qhd(
        get_period_head( usbh_devices[pipe_hdl.dev_addr].core_id, p_qhd->interval_ms ),
        (ehci_link_t*) p_qhd) );
  }
  #endif

  return TUSB_ERROR_NONE;
}

bool hcd_pipe_is_busy(pipe_handle_t pipe_hdl)
{
  ehci_qhd_t *p_qhd = qhd_get_from_pipe_handle( pipe_hdl );
  return !p_qhd->qtd_overlay.halted && (p_qhd->p_qtd_list_head != NULL);
}

bool hcd_pipe_is_error(pipe_handle_t pipe_hdl)
{
  ehci_qhd_t *p_qhd = qhd_get_from_pipe_handle( pipe_hdl );
  return p_qhd->qtd_overlay.halted;
}

bool hcd_pipe_is_stalled(pipe_handle_t pipe_hdl)
{
  ehci_qhd_t *p_qhd = qhd_get_from_pipe_handle( pipe_hdl );
  return p_qhd->qtd_overlay.halted && !qhd_has_xact_error(p_qhd);
}

uint8_t hcd_pipe_get_endpoint_addr(pipe_handle_t pipe_hdl)
{
  ehci_qhd_t *p_qhd = qhd_get_from_pipe_handle( pipe_hdl );
  return p_qhd->endpoint_number + ( (p_qhd->pid_non_control == EHCI_PID_IN) ? TUSB_DIR_DEV_TO_HOST_MASK : 0);
}

tusb_error_t hcd_pipe_clear_stall(pipe_handle_t pipe_hdl)
{
  ehci_qhd_t *p_qhd = qhd_get_from_pipe_handle( pipe_hdl );
  p_qhd->qtd_overlay.halted = 0;
  // TODO reset data toggle ?
  return TUSB_ERROR_NONE;
}

//--------------------------------------------------------------------+
// EHCI Interrupt Handler
//--------------------------------------------------------------------+

// async_advance is handshake between sw stack & ehci controller where ehci free all memory from an deleted queue head.
// In tinyusb, queue head is only removed when device is unplugged. So only control queue head is checked if removing
static void async_advance_isr(ehci_qhd_t * const async_head)
{
  // TODO do we need to close addr0
  if (async_head->is_removing) // closing control pipe of addr0
  {
    async_head->is_removing        = 0;
    async_head->p_qtd_list_head    = async_head->p_qtd_list_tail = NULL;
    async_head->qtd_overlay.halted = 1;

    usbh_devices[0].state          = TUSB_DEVICE_STATE_UNPLUG;
  }

  for(uint8_t relative_dev_addr=0; relative_dev_addr < TUSB_CFG_HOST_DEVICE_MAX; relative_dev_addr++)
  {
    // check if control endpoint is removing
    ehci_qhd_t *p_control_qhd = &ehci_data.device[relative_dev_addr].control.qhd;
    if ( p_control_qhd->is_removing )
    {
      p_control_qhd->is_removing = 0;
      p_control_qhd->used        = 0;

      // Host Controller has cleaned up its cached data for this device, set state to unplug
      usbh_devices[relative_dev_addr+1].state = TUSB_DEVICE_STATE_UNPLUG;

      for (uint8_t i=0; i<HCD_MAX_ENDPOINT; i++) // free all qhd
      {
        ehci_data.device[relative_dev_addr].qhd[i].used        = 0;
        ehci_data.device[relative_dev_addr].qhd[i].is_removing = 0;
      }
      for (uint8_t i=0; i<HCD_MAX_XFER; i++) // free all qtd
      {
        ehci_data.device[relative_dev_addr].qtd[i].used = 0;
      }
      // TODO free all itd & sitd
    }
  } // end for device[] loop
}

static void port_connect_status_change_isr(uint8_t hostid)
{
  ehci_registers_t* const regs = get_operational_register(hostid);

  // NOTE There is an sequence plug->unplug->…..-> plug if device is powering with pre-plugged device
  if (regs->portsc_bit.current_connect_status)
  {
    hcd_port_reset(hostid);
    usbh_hcd_rhport_plugged_isr(hostid);
  }else // device unplugged
  {
    usbh_hcd_rhport_unplugged_isr(hostid);
  }
}

static void qhd_xfer_complete_isr(ehci_qhd_t * p_qhd)
{
  uint8_t max_loop = 0;
  tusb_xfer_type_t const xfer_type = qhd_get_xfer_type(p_qhd);

  // free all TDs from the head td to the first active TD
  while(p_qhd->p_qtd_list_head != NULL && !p_qhd->p_qtd_list_head->active
      && max_loop < HCD_MAX_XFER)
  {
    // TD need to be freed and removed from qhd, before invoking callback
    bool is_ioc = (p_qhd->p_qtd_list_head->int_on_complete != 0);
    p_qhd->total_xferred_bytes += p_qhd->p_qtd_list_head->expected_bytes - p_qhd->p_qtd_list_head->total_bytes;

    p_qhd->p_qtd_list_head->used = 0; // free QTD
    qtd_remove_1st_from_qhd(p_qhd);

    if (is_ioc) // end of request
    { // call USBH callback
      usbh_xfer_isr( qhd_create_pipe_handle(p_qhd, xfer_type),
                     p_qhd->class_code, TUSB_EVENT_XFER_COMPLETE,
                     p_qhd->total_xferred_bytes - (xfer_type == TUSB_XFER_CONTROL ? 8 : 0) ); // subtract setup packet size if control,
      p_qhd->total_xferred_bytes = 0;
    }

    max_loop++;
  }
}

static void async_list_xfer_complete_isr(ehci_qhd_t * const async_head)
{
  uint8_t max_loop = 0;
  ehci_qhd_t *p_qhd = async_head;
  do
  {
    if ( !p_qhd->qtd_overlay.halted ) // halted or error is processed in error isr
    {
      qhd_xfer_complete_isr(p_qhd);
    }
    p_qhd = qhd_next(p_qhd);
    max_loop++;
  }while(p_qhd != async_head && max_loop < HCD_MAX_ENDPOINT*TUSB_CFG_HOST_DEVICE_MAX); // async list traversal, stop if loop around
  // TODO abstract max loop guard for async
}

#if EHCI_PERIODIC_LIST // TODO refractor/group this together
static void period_list_xfer_complete_isr(uint8_t hostid, uint8_t interval_ms)
{
  uint8_t max_loop = 0;
  uint32_t const period_1ms_addr = (uint32_t) get_period_head(hostid, 1);
  ehci_link_t next_item = * get_period_head(hostid, interval_ms);

  // TODO abstract max loop guard for period
  while( !next_item.terminate &&
      !(interval_ms > 1 && period_1ms_addr == align32(next_item.address)) &&
      max_loop < (HCD_MAX_ENDPOINT + EHCI_MAX_ITD + EHCI_MAX_SITD)*TUSB_CFG_HOST_DEVICE_MAX)
  {
    switch ( next_item.type )
    {
      case EHCI_QUEUE_ELEMENT_QHD:
      {
        ehci_qhd_t *p_qhd_int = (ehci_qhd_t *) align32(next_item.address);
        if ( !p_qhd_int->qtd_overlay.halted )
        {
          qhd_xfer_complete_isr(p_qhd_int);
        }
      }
      break;

      case EHCI_QUEUE_ELEMENT_ITD: // TODO support hs/fs ISO
      case EHCI_QUEUE_ELEMENT_SITD:
      case EHCI_QUEUE_ELEMENT_FSTN:
				
      default: break;
    }

    next_item = *list_next(&next_item);
    max_loop++;
  }
}
#endif

static void qhd_xfer_error_isr(ehci_qhd_t * p_qhd)
{
  if ( (p_qhd->device_address != 0 && p_qhd->qtd_overlay.halted) || // addr0 cannot be protocol STALL
        qhd_has_xact_error(p_qhd) )
  { // current qhd has error in transaction
    tusb_xfer_type_t const xfer_type = qhd_get_xfer_type(p_qhd);
    tusb_event_t error_event;

    // no error bits are set, endpoint is halted due to STALL
    error_event = qhd_has_xact_error(p_qhd) ? TUSB_EVENT_XFER_ERROR : TUSB_EVENT_XFER_STALLED;

    p_qhd->total_xferred_bytes += p_qhd->p_qtd_list_head->expected_bytes - p_qhd->p_qtd_list_head->total_bytes;


//    if ( TUSB_EVENT_XFER_ERROR == error_event )    hal_debugger_breakpoint(); // TODO skip unplugged device

    p_qhd->p_qtd_list_head->used = 0; // free QTD
    qtd_remove_1st_from_qhd(p_qhd);

    if ( TUSB_XFER_CONTROL == xfer_type )
    {
      p_qhd->total_xferred_bytes -= min8_of(8, p_qhd->total_xferred_bytes); // subtract setup size

      // control cannot be halted --> clear all qtd list
      p_qhd->p_qtd_list_head = NULL;
      p_qhd->p_qtd_list_tail = NULL;

      p_qhd->qtd_overlay.next.terminate      = 1;
      p_qhd->qtd_overlay.alternate.terminate = 1;
      p_qhd->qtd_overlay.halted              = 0;

      ehci_qtd_t *p_setup  = get_control_qtds(p_qhd->device_address);
      ehci_qtd_t *p_data   = p_setup + 1;
      ehci_qtd_t *p_status = p_setup + 2;

      p_setup->used = p_data->used = p_status->used = 0;
    }

    // call USBH callback
    usbh_xfer_isr( qhd_create_pipe_handle(p_qhd, xfer_type),
                   p_qhd->class_code, error_event,
                   p_qhd->total_xferred_bytes);

    p_qhd->total_xferred_bytes = 0;
  }
}

static void xfer_error_isr(uint8_t hostid)
{
  //------------- async list -------------//
  uint8_t max_loop = 0;
  ehci_qhd_t * const async_head = get_async_head(hostid);
  ehci_qhd_t *p_qhd = async_head;
  do
  {
    qhd_xfer_error_isr( p_qhd );
    p_qhd = qhd_next(p_qhd);
    max_loop++;
  }while(p_qhd != async_head && max_loop < HCD_MAX_ENDPOINT*TUSB_CFG_HOST_DEVICE_MAX); // async list traversal, stop if loop around

  #if EHCI_PERIODIC_LIST
  //------------- TODO refractor period list -------------//
  uint32_t const period_1ms_addr = (uint32_t) get_period_head(hostid, 1);
  for (uint8_t interval_ms=1; interval_ms <= EHCI_FRAMELIST_SIZE; interval_ms *= 2)
  {
    uint8_t period_max_loop = 0;
    ehci_link_t next_item = * get_period_head(hostid, interval_ms);

    // TODO abstract max loop guard for period
    while( !next_item.terminate &&
        !(interval_ms > 1 && period_1ms_addr == align32(next_item.address)) &&
        period_max_loop < (HCD_MAX_ENDPOINT + EHCI_MAX_ITD + EHCI_MAX_SITD)*TUSB_CFG_HOST_DEVICE_MAX)
    {
      switch ( next_item.type )
      {
        case EHCI_QUEUE_ELEMENT_QHD:
        {
          ehci_qhd_t *p_qhd_int = (ehci_qhd_t *) align32(next_item.address);
          qhd_xfer_error_isr(p_qhd_int);
        }
        break;

				// TODO support hs/fs ISO
        case EHCI_QUEUE_ELEMENT_ITD:
        case EHCI_QUEUE_ELEMENT_SITD:
        case EHCI_QUEUE_ELEMENT_FSTN:
        default: break;
      }

      next_item = *list_next(&next_item);
      period_max_loop++;
    }
  }
  #endif
}

//------------- Host Controller Driver's Interrupt Handler -------------//
void hcd_isr(uint8_t hostid)
{
  ehci_registers_t* const regs = get_operational_register(hostid);

  uint32_t int_status = regs->usb_sts;
  int_status &= regs->usb_int_enable;
  
  regs->usb_sts |= int_status; // Acknowledge handled interrupt

  if (int_status == 0) return;

  if (int_status & EHCI_INT_MASK_PORT_CHANGE)
  {
    uint32_t port_status = regs->portsc & EHCI_PORTSC_MASK_ALL;

    if (regs->portsc_bit.connect_status_change)
    {
      port_connect_status_change_isr(hostid);
    }

    regs->portsc |= port_status; // Acknowledge change bits in portsc
  }

  if (int_status & EHCI_INT_MASK_ERROR)
  {
    xfer_error_isr(hostid);
  }

  //------------- some QTD/SITD/ITD with IOC set is completed -------------//
  if (int_status & EHCI_INT_MASK_NXP_ASYNC)
  {
    async_list_xfer_complete_isr( get_async_head(hostid) );
  }

#if EHCI_PERIODIC_LIST // TODO refractor/group this together
  if (int_status & EHCI_INT_MASK_NXP_PERIODIC)
  {
    for (uint8_t i=1; i <= EHCI_FRAMELIST_SIZE; i *= 2)
    {
      period_list_xfer_complete_isr( hostid, i );
    }
  }
#endif

  //------------- There is some removed async previously -------------//
  if (int_status & EHCI_INT_MASK_ASYNC_ADVANCE) // need to place after EHCI_INT_MASK_NXP_ASYNC
  {
    async_advance_isr( get_async_head(hostid) );
  }
}

//--------------------------------------------------------------------+
// HELPER
//--------------------------------------------------------------------+
STATIC_ INLINE_ ehci_registers_t* get_operational_register(uint8_t hostid)
{
  return (ehci_registers_t*) (hostid ? (&LPC_USB1->USBCMD_H) : (&LPC_USB0->USBCMD_H) );
}

#if EHCI_PERIODIC_LIST // TODO refractor/group this together
STATIC_ INLINE_ ehci_link_t* get_period_frame_list(uint8_t hostid)
{
  switch(hostid)
  {
#if (TUSB_CFG_CONTROLLER_0_MODE & TUSB_MODE_HOST)
    case 0:
      return period_frame_list0;
#endif

#if (TUSB_CFG_CONTROLLER_1_MODE & TUSB_MODE_HOST)
    case 1:
      return period_frame_list1;
#endif
	 
    default: return NULL;
  }
}
#endif

STATIC_ INLINE_ uint8_t hostid_to_data_idx(uint8_t hostid)
{
  #if (CONTROLLER_HOST_NUMBER == 1) && (TUSB_CFG_CONTROLLER_1_MODE & TUSB_MODE_HOST)
    (void) hostid;
    return 0;
  #else
    return hostid;
  #endif
}

//------------- queue head helper -------------//
STATIC_ INLINE_ ehci_qhd_t* get_async_head(uint8_t hostid)
{
  return &ehci_data.async_head[ hostid_to_data_idx(hostid) ];
}

#if EHCI_PERIODIC_LIST // TODO refractor/group this together
STATIC_ INLINE_ ehci_link_t* get_period_head(uint8_t hostid, uint8_t interval_ms)
{
  return (ehci_link_t*) (&ehci_data.period_head_arr[ hostid_to_data_idx(hostid) ]
                                                    [ log2_of( min8_of(EHCI_FRAMELIST_SIZE, interval_ms) ) ] );
}
#endif

STATIC_ INLINE_ ehci_qhd_t* get_control_qhd(uint8_t dev_addr)
{
  return (dev_addr == 0) ?
      get_async_head( usbh_devices[dev_addr].core_id ) :
      &ehci_data.device[dev_addr-1].control.qhd;
}
STATIC_ INLINE_ ehci_qtd_t* get_control_qtds(uint8_t dev_addr)
{
  return (dev_addr == 0) ?
      ehci_data.addr0_qtd :
      ehci_data.device[ dev_addr-1 ].control.qtd;

}

static inline ehci_qhd_t* qhd_find_free (uint8_t dev_addr)
{
  uint8_t relative_address = dev_addr-1;
  uint8_t index=0;
  while( index<HCD_MAX_ENDPOINT && ehci_data.device[relative_address].qhd[index].used )
  {
    index++;
  }
  return (index < HCD_MAX_ENDPOINT) ? &ehci_data.device[relative_address].qhd[index] : NULL;
}

static inline uint8_t qhd_get_index(ehci_qhd_t const * p_qhd)
{
  return p_qhd - ehci_data.device[p_qhd->device_address-1].qhd;
}
static inline tusb_xfer_type_t qhd_get_xfer_type(ehci_qhd_t const * p_qhd)
{
  return  ( p_qhd->endpoint_number == 0 ) ? TUSB_XFER_CONTROL :
          ( p_qhd->interrupt_smask != 0 ) ? TUSB_XFER_INTERRUPT : TUSB_XFER_BULK;
}

static inline ehci_qhd_t* qhd_next(ehci_qhd_t const * p_qhd)
{
  return (ehci_qhd_t*) align32(p_qhd->next.address);
}

STATIC_ INLINE_ ehci_qhd_t* qhd_get_from_pipe_handle(pipe_handle_t pipe_hdl)
{
  return &ehci_data.device[ pipe_hdl.dev_addr-1 ].qhd[ pipe_hdl.index ];
}

static inline pipe_handle_t qhd_create_pipe_handle(ehci_qhd_t const * p_qhd, tusb_xfer_type_t xfer_type)
{
  pipe_handle_t pipe_hdl = {
      .dev_addr  = p_qhd->device_address,
      .xfer_type = xfer_type
  };

  // TODO Isochronous transfer support
  if (TUSB_XFER_CONTROL != xfer_type) // qhd index for control is meaningless
  {
    pipe_hdl.index = qhd_get_index(p_qhd);
  }

  return pipe_hdl;
}

//------------- TD helper -------------//
STATIC_ INLINE_ ehci_qtd_t* qtd_find_free(uint8_t dev_addr)
{
  uint8_t index=0;
  while( index<HCD_MAX_XFER && ehci_data.device[dev_addr-1].qtd[index].used )
  {
    index++;
  }

  return (index < HCD_MAX_XFER) ? &ehci_data.device[dev_addr-1].qtd[index] : NULL;
}

static inline ehci_qtd_t* qtd_next(ehci_qtd_t const * p_qtd )
{
  return (ehci_qtd_t*) align32(p_qtd->next.address);
}

static inline void qtd_remove_1st_from_qhd(ehci_qhd_t *p_qhd)
{
  if (p_qhd->p_qtd_list_head == p_qhd->p_qtd_list_tail) // last TD --> make it NULL
  {
    p_qhd->p_qtd_list_head = p_qhd->p_qtd_list_tail = NULL;
  }else
  {
    p_qhd->p_qtd_list_head = qtd_next( p_qhd->p_qtd_list_head );
  }
}

static inline void qtd_insert_to_qhd(ehci_qhd_t *p_qhd, ehci_qtd_t *p_qtd_new)
{
  if (p_qhd->p_qtd_list_head == NULL) // empty list
  {
    p_qhd->p_qtd_list_head               = p_qhd->p_qtd_list_tail = p_qtd_new;
  }else
  {
    p_qhd->p_qtd_list_tail->next.address = (uint32_t) p_qtd_new;
    p_qhd->p_qtd_list_tail               = p_qtd_new;
  }
}

static void qhd_init(ehci_qhd_t *p_qhd, uint8_t dev_addr, uint16_t max_packet_size, uint8_t endpoint_addr, uint8_t xfer_type, uint8_t interval)
{
  // address 0 is used as async head, which always on the list --> cannot be cleared (ehci halted otherwise)
  if (dev_addr != 0)
  {
    memclr_(p_qhd, sizeof(ehci_qhd_t));
  }

  p_qhd->device_address                   = dev_addr;
  p_qhd->non_hs_period_inactive_next_xact = 0;
  p_qhd->endpoint_number                  = endpoint_addr & 0x0F;
  p_qhd->endpoint_speed                   = usbh_devices[dev_addr].speed;
  p_qhd->data_toggle_control              = (xfer_type == TUSB_XFER_CONTROL) ? 1 : 0;
  p_qhd->head_list_flag                   = (dev_addr == 0) ? 1 : 0; // addr0's endpoint is the static asyn list head
  p_qhd->max_package_size                 = max_packet_size;
  p_qhd->non_hs_control_endpoint          = ((TUSB_XFER_CONTROL == xfer_type) && (p_qhd->endpoint_speed != TUSB_SPEED_HIGH))  ? 1 : 0;
  p_qhd->nak_count_reload                 = 0;

  // Bulk/Control -> smask = cmask = 0
  // TODO Isochronous
  if (TUSB_XFER_INTERRUPT == xfer_type)
  {
    if (TUSB_SPEED_HIGH == p_qhd->endpoint_speed)
    {
      ASSERT_INT_WITHIN(1, 16, interval, VOID_RETURN);
      if ( interval < 4) // sub milisecond interval
      {
        p_qhd->interval_ms     = 0;
        p_qhd->interrupt_smask = (interval == 1) ? BIN8(11111111) :
                                 (interval == 2) ? BIN8(10101010) : BIN8(01000100);
      }else
      {
        p_qhd->interval_ms     = (uint8_t) min16_of( 1 << (interval-4), 255 );
        p_qhd->interrupt_smask = BIT_(interval % 8);
      }
    }else
    {
      ASSERT( 0 != interval, VOID_RETURN);
      // Full/Low: 4.12.2.1 (EHCI) case 1 schedule start split at 1 us & complete split at 2,3,4 uframes
      p_qhd->interrupt_smask        = 0x01;
      p_qhd->non_hs_interrupt_cmask = BIN8(11100);
      p_qhd->interval_ms            = interval;
    }
  }else
  {
    p_qhd->interrupt_smask = p_qhd->non_hs_interrupt_cmask = 0;
  }

  p_qhd->hub_address             = usbh_devices[dev_addr].hub_addr;
  p_qhd->hub_port                = usbh_devices[dev_addr].hub_port;
  p_qhd->mult                    = 1; // TODO not use high bandwidth/park mode yet

  //------------- HCD Management Data -------------//
  p_qhd->used            = 1;
  p_qhd->is_removing     = 0;
  p_qhd->p_qtd_list_head = NULL;
  p_qhd->p_qtd_list_tail = NULL;
  p_qhd->pid_non_control = (endpoint_addr & 0x80) ? EHCI_PID_IN : EHCI_PID_OUT; // PID for TD under this endpoint

  //------------- active, but no TD list -------------//
  p_qhd->qtd_overlay.halted              = 0;
  p_qhd->qtd_overlay.next.terminate      = 1;
  p_qhd->qtd_overlay.alternate.terminate = 1;
  if (TUSB_XFER_BULK == xfer_type && p_qhd->endpoint_speed == TUSB_SPEED_HIGH && p_qhd->pid_non_control == EHCI_PID_OUT)
  {
    p_qhd->qtd_overlay.pingstate_err = 1; // do PING for Highspeed Bulk OUT, EHCI section 4.11
  }
}

static void qtd_init(ehci_qtd_t* p_qtd, uint32_t data_ptr, uint16_t total_bytes)
{
  memclr_(p_qtd, sizeof(ehci_qtd_t));

  p_qtd->used                = 1;

  p_qtd->next.terminate      = 1; // init to null
  p_qtd->alternate.terminate = 1; // not used, always set to terminated
  p_qtd->active              = 1;
  p_qtd->cerr                = 3; // TODO 3 consecutive errors tolerance
  p_qtd->data_toggle         = 0;
  p_qtd->total_bytes         = total_bytes;
  p_qtd->expected_bytes      = total_bytes;

  p_qtd->buffer[0]           = data_ptr;
  for(uint8_t i=1; i<5; i++)
  {
    p_qtd->buffer[i] |= align4k( p_qtd->buffer[i-1] ) + 4096;
  }
}

//------------- List Managing Helper -------------//
static inline void list_insert(ehci_link_t *current, ehci_link_t *new, uint8_t new_type)
{
  new->address = current->address;
  current->address = ((uint32_t) new) | (new_type << 1);
}

static inline ehci_link_t* list_next(ehci_link_t *p_link_pointer)
{
  return (ehci_link_t*) align32(p_link_pointer->address);
}

static ehci_link_t* list_find_previous_item(ehci_link_t* p_head, ehci_link_t* p_current)
{
  ehci_link_t *p_prev = p_head;
  uint32_t max_loop = 0;
  while( (align32(p_prev->address) != (uint32_t) p_head)    && // not loop around
         (align32(p_prev->address) != (uint32_t) p_current) && // not found yet
         !p_prev->terminate                                 && // not advanceable
         max_loop < HCD_MAX_ENDPOINT)
  {
    p_prev = list_next(p_prev);
    max_loop++;
  }

  return  (align32(p_prev->address) != (uint32_t) p_head) ? p_prev : NULL;
}

static tusb_error_t list_remove_qhd(ehci_link_t* p_head, ehci_link_t* p_remove)
{
  ehci_link_t *p_prev = list_find_previous_item(p_head, p_remove);

  ASSERT_PTR(p_prev, TUSB_ERROR_INVALID_PARA);

  p_prev->address   = p_remove->address;
  // EHCI 4.8.2 link the removing queue head to async/period head (which always reachable by Host Controller)
  p_remove->address = ((uint32_t) p_head) | (EHCI_QUEUE_ELEMENT_QHD << 1);

  return TUSB_ERROR_NONE;
}

#endif

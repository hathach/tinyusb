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

#include "common/tusb_common.h"

#if TUSB_OPT_HOST_ENABLED && (CFG_TUSB_MCU == OPT_MCU_LPC43XX || CFG_TUSB_MCU == OPT_MCU_LPC18XX)
//--------------------------------------------------------------------+
// INCLUDE
//--------------------------------------------------------------------+
#include "osal/osal.h"

#include "../hcd.h"
#include "../usbh_hcd.h"
#include "ehci.h"

// TODO remove
#include "chip.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+

//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+
// Periodic frame list must be 4K alignment
CFG_TUSB_MEM_SECTION ATTR_ALIGNED(4096) static ehci_data_t ehci_data;

//------------- Validation -------------//
// TODO static assert for memory placement on some known MCU such as lpc43xx

//--------------------------------------------------------------------+
// PROTOTYPE
//--------------------------------------------------------------------+
static inline ehci_registers_t*  get_operational_register(uint8_t hostid) ATTR_PURE ATTR_ALWAYS_INLINE ATTR_WARN_UNUSED_RESULT;

static inline ehci_qhd_t*  get_async_head(uint8_t hostid) ATTR_ALWAYS_INLINE ATTR_PURE ATTR_WARN_UNUSED_RESULT;
static inline ehci_link_t* get_period_head(uint8_t hostid, uint8_t interval_ms) ATTR_ALWAYS_INLINE ATTR_PURE ATTR_WARN_UNUSED_RESULT;

static inline ehci_qhd_t* get_control_qhd(uint8_t dev_addr) ATTR_ALWAYS_INLINE ATTR_PURE ATTR_WARN_UNUSED_RESULT;
static inline ehci_qtd_t* get_control_qtds(uint8_t dev_addr) ATTR_ALWAYS_INLINE ATTR_PURE ATTR_WARN_UNUSED_RESULT;

static inline ehci_qhd_t*    qhd_next(ehci_qhd_t const * p_qhd) ATTR_ALWAYS_INLINE ATTR_PURE;
static inline ehci_qhd_t*    qhd_find_free (void);
static inline tusb_xfer_type_t qhd_get_xfer_type(ehci_qhd_t const * p_qhd) ATTR_ALWAYS_INLINE ATTR_PURE;
static inline ehci_qhd_t* qhd_get_from_addr(uint8_t dev_addr, uint8_t ep_addr);

// determine if a queue head has bus-related error
static inline bool qhd_has_xact_error(ehci_qhd_t * p_qhd)
{
  return ( p_qhd->qtd_overlay.buffer_err ||p_qhd->qtd_overlay.babble_err || p_qhd->qtd_overlay.xact_err );
  //p_qhd->qtd_overlay.non_hs_period_missed_uframe || p_qhd->qtd_overlay.pingstate_err TODO split transaction error
}

static void qhd_init(ehci_qhd_t *p_qhd, uint8_t dev_addr, tusb_desc_endpoint_t const * ep_desc);

static inline ehci_qtd_t*  qtd_find_free(uint8_t dev_addr) ATTR_PURE ATTR_ALWAYS_INLINE;
static inline ehci_qtd_t*  qtd_next(ehci_qtd_t const * p_qtd ) ATTR_PURE ATTR_ALWAYS_INLINE;
static inline void         qtd_insert_to_qhd(ehci_qhd_t *p_qhd, ehci_qtd_t *p_qtd_new) ATTR_ALWAYS_INLINE;
static inline void         qtd_remove_1st_from_qhd(ehci_qhd_t *p_qhd) ATTR_ALWAYS_INLINE;
static void qtd_init(ehci_qtd_t* p_qtd, uint32_t data_ptr, uint16_t total_bytes);

static inline void  list_insert(ehci_link_t *current, ehci_link_t *new, uint8_t new_type) ATTR_ALWAYS_INLINE;
static inline ehci_link_t* list_next(ehci_link_t *p_link_pointer) ATTR_PURE ATTR_ALWAYS_INLINE;
static ehci_link_t*  list_find_previous_item(ehci_link_t* p_head, ehci_link_t* p_current);
static tusb_error_t list_remove_qhd(ehci_link_t* p_head, ehci_link_t* p_remove);

static bool ehci_init(uint8_t hostid);

//--------------------------------------------------------------------+
// USBH-HCD API
//--------------------------------------------------------------------+
bool hcd_init(void)
{
  tu_memclr(&ehci_data, sizeof(ehci_data_t));
  return ehci_init(TUH_OPT_RHPORT);
}

//--------------------------------------------------------------------+
// PORT API
//--------------------------------------------------------------------+
void hcd_port_reset(uint8_t hostid)
{
  ehci_registers_t* regs = ehci_data.regs;

  regs->portsc_bm.port_enabled = 0; // disable port before reset
  regs->portsc_bm.port_reset = 1;
}

bool hcd_port_connect_status(uint8_t hostid)
{
  return ehci_data.regs->portsc_bm.current_connect_status;
}

tusb_speed_t hcd_port_speed_get(uint8_t hostid)
{
  return (tusb_speed_t) ehci_data.regs->portsc_bm.nxp_port_speed; // NXP specific port speed
}

// TODO refractor abtract later
void hcd_device_remove(uint8_t rhport, uint8_t dev_addr)
{
  ehci_data.regs->command_bm.async_adv_doorbell = 1; // Async doorbell check EHCI 4.8.2 for operational details
}

//--------------------------------------------------------------------+
// Controller API
//--------------------------------------------------------------------+
static bool ehci_init(uint8_t hostid)
{
  ehci_data.regs = get_operational_register(hostid);

  ehci_registers_t* regs = ehci_data.regs;

  //------------- CTRLDSSEGMENT Register (skip) -------------//
  //------------- USB INT Register -------------//
  regs->inten = 0;                 // 1. disable all the interrupt
  regs->status        = EHCI_INT_MASK_ALL; // 2. clear all status

  regs->inten = EHCI_INT_MASK_ERROR | EHCI_INT_MASK_PORT_CHANGE |
                         EHCI_INT_MASK_NXP_PERIODIC |
                         EHCI_INT_MASK_ASYNC_ADVANCE | EHCI_INT_MASK_NXP_ASYNC;

  //------------- Asynchronous List -------------//
  ehci_qhd_t * const async_head = get_async_head(hostid);
  tu_memclr(async_head, sizeof(ehci_qhd_t));

  async_head->next.address                    = (uint32_t) async_head; // circular list, next is itself
  async_head->next.type                       = EHCI_QUEUE_ELEMENT_QHD;
  async_head->head_list_flag                  = 1;
  async_head->qtd_overlay.halted              = 1; // inactive most of time
  async_head->qtd_overlay.next.terminate      = 1; // TODO removed if verified

  regs->async_list_addr = (uint32_t) async_head;

  //------------- Periodic List -------------//
  // Build the polling interval tree with 1 ms, 2 ms, 4 ms and 8 ms (framesize) only

  for(uint32_t i=0; i<4; i++)
  {
    ehci_data.period_head_arr[i].int_smask    = 1; // queue head in period list must have smask non-zero
    ehci_data.period_head_arr[i].qtd_overlay.halted = 1; // dummy node, always inactive
  }

  ehci_link_t * const framelist  = ehci_data.period_framelist;
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

  //------------- TT Control (NXP only) -------------//
  regs->nxp_tt_control = 0;

  //------------- USB CMD Register -------------//
  regs->command |= BIT_(EHCI_USBCMD_POS_RUN_STOP) | BIT_(EHCI_USBCMD_POS_ASYNC_ENABLE)
                  | BIT_(EHCI_USBCMD_POS_PERIOD_ENABLE) // TODO enable period list only there is int/iso endpoint
                  | ((EHCI_CFG_FRAMELIST_SIZE_BITS & BIN8(011)) << EHCI_USBCMD_POS_FRAMELIST_SZIE)
                  | ((EHCI_CFG_FRAMELIST_SIZE_BITS >> 2) << EHCI_USBCMD_POS_NXP_FRAMELIST_SIZE_MSB);

  //------------- ConfigFlag Register (skip) -------------//
  regs->portsc_bm.port_power = 1; // enable port power

  return true;
}

static tusb_error_t hcd_controller_stop(uint8_t hostid)
{
  ehci_registers_t* regs = ehci_data.regs;

  regs->command_bm.run_stop = 0;

  tu_timeout_t timeout;
  tu_timeout_set(&timeout, 2); // USB Spec: controller has to stop within 16 uframe = 2 frames
  while( regs->status_bm.hc_halted == 0 && !tu_timeout_expired(&timeout)) {}

  return tu_timeout_expired(&timeout) ? TUSB_ERROR_OSAL_TIMEOUT : TUSB_ERROR_NONE;
}

//--------------------------------------------------------------------+
// CONTROL PIPE API
//--------------------------------------------------------------------+
bool hcd_pipe_control_close(uint8_t dev_addr)
{
  //------------- TODO pipe handle validate -------------//
  ehci_qhd_t * const p_qhd = get_control_qhd(dev_addr);

  p_qhd->is_removing = 1;

  if (dev_addr != 0)
  {
    TU_ASSERT_ERR( list_remove_qhd( (ehci_link_t*) get_async_head( _usbh_devices[dev_addr].rhport ),
                                    (ehci_link_t*) p_qhd) );
  }

  return true;
}

bool hcd_edpt_close(uint8_t rhport, uint8_t dev_addr, uint8_t ep_addr)
{
  // FIXME control only for now
  return hcd_pipe_control_close(dev_addr);
}

bool hcd_edpt_xfer(uint8_t rhport, uint8_t dev_addr, uint8_t ep_addr, uint8_t * buffer, uint16_t buflen)
{
  uint8_t const epnum = edpt_number(ep_addr);
  uint8_t const dir   = edpt_dir(ep_addr);

  // FIXME control only for now
  if ( epnum == 0 )
  {
    ehci_qhd_t* qhd = get_control_qhd(dev_addr);
    ehci_qtd_t* qtd = get_control_qtds(dev_addr);

    qtd_init(qtd, (uint32_t) buffer, buflen);

    // first first data toggle is always 1 (data & setup stage)
    qtd->data_toggle = 1;
    qtd->pid         = dir ? EHCI_PID_IN : EHCI_PID_OUT;
    qtd->int_on_complete = 1;
    qtd->next.terminate  = 1;

    // sw region
    qhd->p_qtd_list_head = qtd;
    qhd->p_qtd_list_tail = qtd;

    // attach TD
    qhd->qtd_overlay.next.address = (uint32_t) qtd;
  }

  return true;
}

bool hcd_setup_send(uint8_t rhport, uint8_t dev_addr, uint8_t const setup_packet[8])
{
  ehci_qhd_t * const p_qhd = get_control_qhd(dev_addr);
  ehci_qtd_t *p_setup      = get_control_qtds(dev_addr);

  qtd_init(p_setup, (uint32_t) setup_packet, 8);
  p_setup->pid          = EHCI_PID_SETUP;
  p_setup->int_on_complete = 1;
  p_setup->next.terminate  = 1;

  // sw region
  p_qhd->p_qtd_list_head = p_setup;
  p_qhd->p_qtd_list_tail = p_setup;

  // attach TD
  p_qhd->qtd_overlay.next.address = (uint32_t) p_setup;

  return true;
}

//--------------------------------------------------------------------+
// BULK/INT/ISO PIPE API
//--------------------------------------------------------------------+
bool hcd_pipe_open(uint8_t rhport, uint8_t dev_addr, tusb_desc_endpoint_t const * ep_desc)
{
  // TODO not support ISO yet
  TU_ASSERT (ep_desc->bmAttributes.xfer != TUSB_XFER_ISOCHRONOUS);

  //------------- Prepare Queue Head -------------//
  ehci_qhd_t * p_qhd;

  if ( ep_desc->bEndpointAddress == 0 )
  {
    p_qhd = get_control_qhd(dev_addr);
  }else
  {
    p_qhd = qhd_find_free();
  }
  TU_ASSERT(p_qhd);

  qhd_init(p_qhd, dev_addr, ep_desc);

  // control of dev0 is always present as async head
  if ( dev_addr == 0 ) return true;

  // Insert to list
  ehci_link_t * list_head;

  switch (ep_desc->bmAttributes.xfer)
  {
    case TUSB_XFER_CONTROL:
    case TUSB_XFER_BULK:
      list_head = (ehci_link_t*) get_async_head(_usbh_devices[dev_addr].rhport);
    break;

    case TUSB_XFER_INTERRUPT:
      list_head = get_period_head(_usbh_devices[dev_addr].rhport, p_qhd->interval_ms);
    break;

    case TUSB_XFER_ISOCHRONOUS:
      // TODO iso is not supported
    break;

    default: break;
  }

  // TODO might need to disable async/period list
  list_insert( list_head, (ehci_link_t*) p_qhd, EHCI_QUEUE_ELEMENT_QHD);

  return true;
}

bool hcd_pipe_queue_xfer(uint8_t dev_addr, uint8_t ep_addr, uint8_t buffer[], uint16_t total_bytes)
{
  //------------- set up QTD -------------//
  ehci_qhd_t *p_qhd = qhd_get_from_addr(dev_addr, ep_addr);
  ehci_qtd_t *p_qtd = qtd_find_free(dev_addr);

  TU_ASSERT(p_qtd);

  qtd_init(p_qtd, (uint32_t) buffer, total_bytes);
  p_qtd->pid = p_qhd->pid_non_control;

  //------------- insert TD to TD list -------------//
  qtd_insert_to_qhd(p_qhd, p_qtd);

  return true;
}

bool hcd_pipe_xfer(uint8_t dev_addr, uint8_t ep_addr, uint8_t buffer[], uint16_t total_bytes, bool int_on_complete)
{
  TU_ASSERT ( hcd_pipe_queue_xfer(dev_addr, ep_addr, buffer, total_bytes) );

  ehci_qhd_t *p_qhd = qhd_get_from_addr(dev_addr, ep_addr);

  if ( int_on_complete )
  { // the just added qtd is pointed by list_tail
    p_qhd->p_qtd_list_tail->int_on_complete = 1;
  }
  p_qhd->qtd_overlay.next.address = (uint32_t) p_qhd->p_qtd_list_head; // attach head QTD to QHD start transferring

  return true;
}

/// pipe_close should only be called as a part of unmount/safe-remove process
bool hcd_pipe_close(uint8_t rhport, uint8_t dev_addr, uint8_t ep_addr)
{
  ehci_qhd_t *p_qhd = qhd_get_from_addr(dev_addr, ep_addr);

  // async list needs async advance handshake to make sure host controller has released cached data
  // non-control does not use async advance, it will eventually free by control pipe close
  // period list queue element is guarantee to be free in the next frame (1 ms)
  p_qhd->is_removing = 1; // TODO redundant, only apply to control queue head

  if ( p_qhd->xfer_type == TUSB_XFER_BULK )
  {
    TU_ASSERT_ERR( list_remove_qhd(
        (ehci_link_t*) get_async_head( _usbh_devices[dev_addr].rhport ),
        (ehci_link_t*) p_qhd), false );
  }
  else
  {
    TU_ASSERT_ERR( list_remove_qhd(
        get_period_head( _usbh_devices[dev_addr].rhport, p_qhd->interval_ms ),
        (ehci_link_t*) p_qhd), false );
  }

  return true;
}

bool hcd_pipe_is_busy(uint8_t dev_addr, uint8_t ep_addr)
{
  ehci_qhd_t *p_qhd = qhd_get_from_addr(dev_addr, ep_addr);
  return !p_qhd->qtd_overlay.halted && (p_qhd->p_qtd_list_head != NULL);
}

bool hcd_pipe_is_stalled(uint8_t dev_addr, uint8_t ep_addr)
{
  ehci_qhd_t *p_qhd = qhd_get_from_addr(dev_addr, ep_addr);
  return p_qhd->qtd_overlay.halted && !qhd_has_xact_error(p_qhd);
}

bool hcd_pipe_clear_stall(uint8_t dev_addr, uint8_t ep_addr)
{
  ehci_qhd_t *p_qhd = qhd_get_from_addr(dev_addr, ep_addr);
  p_qhd->qtd_overlay.halted = 0;
  // TODO reset data toggle ?
  return true;
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

    _usbh_devices[0].state          = TUSB_DEVICE_STATE_UNPLUG;
  }

  for(uint8_t dev_addr=1; dev_addr < CFG_TUSB_HOST_DEVICE_MAX; dev_addr++)
  {
    // check if control endpoint is removing
    ehci_qhd_t *p_control_qhd = get_control_qhd(dev_addr);
    if ( p_control_qhd->is_removing )
    {
      p_control_qhd->is_removing = 0;
      p_control_qhd->used        = 0;

      // Host Controller has cleaned up its cached data for this device, set state to unplug
      _usbh_devices[dev_addr].state = TUSB_DEVICE_STATE_UNPLUG;

      for (uint8_t i=0; i<HCD_MAX_ENDPOINT; i++) // free all qhd
      {
        if (ehci_data.qhd_pool[i].dev_addr == dev_addr)
        {
          ehci_data.qhd_pool[i].used        = 0;
          ehci_data.qhd_pool[i].is_removing = 0;
        }
      }

//      for (uint8_t i=0; i<HCD_MAX_XFER; i++) // free all qtd
//      {
//        ehci_data.device[dev_addr].qtd[i].used = 0;
//      }
//      // TODO free all itd & sitd
    }
  }
}

static void port_connect_status_change_isr(uint8_t hostid)
{
  // NOTE There is an sequence plug->unplug->…..-> plug if device is powering with pre-plugged device
  if (ehci_data.regs->portsc_bm.current_connect_status)
  {
    hcd_port_reset(hostid);
    hcd_event_device_attach(hostid);
  }else // device unplugged
  {
    hcd_event_device_remove(hostid);
  }
}

static void qhd_xfer_complete_isr(ehci_qhd_t * p_qhd)
{
  // free all TDs from the head td to the first active TD
  while(p_qhd->p_qtd_list_head != NULL && !p_qhd->p_qtd_list_head->active)
  {
    // TD need to be freed and removed from qhd, before invoking callback
    bool is_ioc = (p_qhd->p_qtd_list_head->int_on_complete != 0);
    p_qhd->total_xferred_bytes += p_qhd->p_qtd_list_head->expected_bytes - p_qhd->p_qtd_list_head->total_bytes;

    p_qhd->p_qtd_list_head->used = 0; // free QTD
    qtd_remove_1st_from_qhd(p_qhd);

    if (is_ioc)
    {
      // end of request
      // call USBH callback
      hcd_event_xfer_complete(p_qhd->dev_addr, edpt_addr(p_qhd->ep_number, p_qhd->pid_non_control == EHCI_PID_IN ? 1 : 0), XFER_RESULT_SUCCESS, p_qhd->total_xferred_bytes);
      p_qhd->total_xferred_bytes = 0;
    }
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
  }while(p_qhd != async_head && max_loop < HCD_MAX_ENDPOINT*CFG_TUSB_HOST_DEVICE_MAX); // async list traversal, stop if loop around
  // TODO abstract max loop guard for async
}

static void period_list_xfer_complete_isr(uint8_t hostid, uint8_t interval_ms)
{
  uint8_t max_loop = 0;
  uint32_t const period_1ms_addr = (uint32_t) get_period_head(hostid, 1);
  ehci_link_t next_item = * get_period_head(hostid, interval_ms);

  // TODO abstract max loop guard for period
  while( !next_item.terminate &&
      !(interval_ms > 1 && period_1ms_addr == tu_align32(next_item.address)) &&
      max_loop < (HCD_MAX_ENDPOINT + EHCI_MAX_ITD + EHCI_MAX_SITD)*CFG_TUSB_HOST_DEVICE_MAX)
  {
    switch ( next_item.type )
    {
      case EHCI_QUEUE_ELEMENT_QHD:
      {
        ehci_qhd_t *p_qhd_int = (ehci_qhd_t *) tu_align32(next_item.address);
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

static void qhd_xfer_error_isr(ehci_qhd_t * p_qhd)
{
  if ( (p_qhd->dev_addr != 0 && p_qhd->qtd_overlay.halted) || // addr0 cannot be protocol STALL
        qhd_has_xact_error(p_qhd) )
  { // current qhd has error in transaction
    tusb_xfer_type_t const xfer_type = qhd_get_xfer_type(p_qhd);
    xfer_result_t error_event;

    // no error bits are set, endpoint is halted due to STALL
    error_event = qhd_has_xact_error(p_qhd) ? XFER_RESULT_FAILED : XFER_RESULT_STALLED;

    p_qhd->total_xferred_bytes += p_qhd->p_qtd_list_head->expected_bytes - p_qhd->p_qtd_list_head->total_bytes;

//    if ( XFER_RESULT_FAILED == error_event )    TU_BREAKPOINT(); // TODO skip unplugged device

    p_qhd->p_qtd_list_head->used = 0; // free QTD
    qtd_remove_1st_from_qhd(p_qhd);

    if ( 0 == p_qhd->ep_number )
    {
      // control cannot be halted --> clear all qtd list
      p_qhd->p_qtd_list_head = NULL;
      p_qhd->p_qtd_list_tail = NULL;

      p_qhd->qtd_overlay.next.terminate      = 1;
      p_qhd->qtd_overlay.alternate.terminate = 1;
      p_qhd->qtd_overlay.halted              = 0;

      ehci_qtd_t *p_setup = get_control_qtds(p_qhd->dev_addr);
      p_setup->used = 0;
    }

    // call USBH callback
    hcd_event_xfer_complete(p_qhd->dev_addr, edpt_addr(p_qhd->ep_number, p_qhd->pid_non_control == EHCI_PID_IN ? 1 : 0), error_event, p_qhd->total_xferred_bytes);

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
  }while(p_qhd != async_head && max_loop < HCD_MAX_ENDPOINT*CFG_TUSB_HOST_DEVICE_MAX); // async list traversal, stop if loop around

  //------------- TODO refractor period list -------------//
  uint32_t const period_1ms_addr = (uint32_t) get_period_head(hostid, 1);
  for (uint8_t interval_ms=1; interval_ms <= EHCI_FRAMELIST_SIZE; interval_ms *= 2)
  {
    uint8_t period_max_loop = 0;
    ehci_link_t next_item = * get_period_head(hostid, interval_ms);

    // TODO abstract max loop guard for period
    while( !next_item.terminate &&
        !(interval_ms > 1 && period_1ms_addr == tu_align32(next_item.address)) &&
        period_max_loop < (HCD_MAX_ENDPOINT + EHCI_MAX_ITD + EHCI_MAX_SITD)*CFG_TUSB_HOST_DEVICE_MAX)
    {
      switch ( next_item.type )
      {
        case EHCI_QUEUE_ELEMENT_QHD:
        {
          ehci_qhd_t *p_qhd_int = (ehci_qhd_t *) tu_align32(next_item.address);
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
}

//------------- Host Controller Driver's Interrupt Handler -------------//
void hal_hcd_isr(uint8_t hostid)
{
  ehci_registers_t* regs = ehci_data.regs;

  uint32_t int_status = regs->status;
  int_status &= regs->inten;
  
  regs->status |= int_status; // Acknowledge handled interrupt

  if (int_status == 0) return;

  if (int_status & EHCI_INT_MASK_PORT_CHANGE)
  {
    uint32_t port_status = regs->portsc & EHCI_PORTSC_MASK_ALL;

    if (regs->portsc_bm.connect_status_change)
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

  if (int_status & EHCI_INT_MASK_NXP_PERIODIC)
  {
    for (uint8_t i=1; i <= EHCI_FRAMELIST_SIZE; i *= 2)
    {
      period_list_xfer_complete_isr( hostid, i );
    }
  }

  //------------- There is some removed async previously -------------//
  if (int_status & EHCI_INT_MASK_ASYNC_ADVANCE) // need to place after EHCI_INT_MASK_NXP_ASYNC
  {
    async_advance_isr( get_async_head(hostid) );
  }
}

//--------------------------------------------------------------------+
// HELPER
//--------------------------------------------------------------------+
static inline ehci_registers_t* get_operational_register(uint8_t hostid)
{
  return (ehci_registers_t*) (hostid ? (&LPC_USB1->USBCMD_H) : (&LPC_USB0->USBCMD_H) );
}

//------------- queue head helper -------------//
static inline ehci_qhd_t* get_async_head(uint8_t hostid)
{
  return get_control_qhd(0);
}

static inline ehci_link_t* get_period_head(uint8_t hostid, uint8_t interval_ms)
{
  return (ehci_link_t*) &ehci_data.period_head_arr[ tu_log2( tu_min8(EHCI_FRAMELIST_SIZE, interval_ms) ) ];
}

static inline ehci_qhd_t* get_control_qhd(uint8_t dev_addr)
{
  return &ehci_data.control[dev_addr].qhd;
}
static inline ehci_qtd_t* get_control_qtds(uint8_t dev_addr)
{
  return &ehci_data.control[dev_addr].qtd;
}

static inline ehci_qhd_t* qhd_find_free (void)
{
  for (uint32_t i=0; i<HCD_MAX_ENDPOINT; i++)
  {
    if ( !ehci_data.qhd_pool[i].used ) return &ehci_data.qhd_pool[i];
  }

  return NULL;
}

static inline tusb_xfer_type_t qhd_get_xfer_type(ehci_qhd_t const * p_qhd)
{
  return  ( p_qhd->ep_number == 0 ) ? TUSB_XFER_CONTROL :
          ( p_qhd->int_smask != 0 ) ? TUSB_XFER_INTERRUPT : TUSB_XFER_BULK;
}

static inline ehci_qhd_t* qhd_next(ehci_qhd_t const * p_qhd)
{
  return (ehci_qhd_t*) tu_align32(p_qhd->next.address);
}

static inline ehci_qhd_t* qhd_get_from_addr(uint8_t dev_addr, uint8_t ep_addr)
{
  ehci_qhd_t* qhd_pool = ehci_data.qhd_pool;

  for(uint32_t i=0; i<HCD_MAX_ENDPOINT; i++)
  {
    if ( (qhd_pool[i].dev_addr == dev_addr) &&
          ep_addr == edpt_addr(qhd_pool[i].ep_number, qhd_pool[i].pid_non_control) )
    {
      return &qhd_pool[i];
    }
  }

  return NULL;
}

//------------- TD helper -------------//
static inline ehci_qtd_t* qtd_find_free(uint8_t dev_addr)
{
  for (uint32_t i=0; i<HCD_MAX_XFER; i++)
  {
    if ( !ehci_data.qtd_pool[i].used ) return &ehci_data.qtd_pool[i];
  }

  return NULL;
}

static inline ehci_qtd_t* qtd_next(ehci_qtd_t const * p_qtd )
{
  return (ehci_qtd_t*) tu_align32(p_qtd->next.address);
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

static void qhd_init(ehci_qhd_t *p_qhd, uint8_t dev_addr, tusb_desc_endpoint_t const * ep_desc)
{
  // address 0 is used as async head, which always on the list --> cannot be cleared (ehci halted otherwise)
  if (dev_addr != 0)
  {
    tu_memclr(p_qhd, sizeof(ehci_qhd_t));
  }

  uint8_t const xfer_type = ep_desc->bmAttributes.xfer;
  uint8_t const interval = ep_desc->bInterval;

  p_qhd->dev_addr           = dev_addr;
  p_qhd->inactive_next_xact = 0;
  p_qhd->ep_number          = edpt_number(ep_desc->bEndpointAddress);
  p_qhd->ep_speed           = _usbh_devices[dev_addr].speed;
  p_qhd->data_toggle        = (xfer_type == TUSB_XFER_CONTROL) ? 1 : 0;
  p_qhd->head_list_flag     = (dev_addr == 0) ? 1 : 0; // addr0's endpoint is the static asyn list head
  p_qhd->max_packet_size    = ep_desc->wMaxPacketSize.size;
  p_qhd->fl_ctrl_ep_flag    = ((xfer_type == TUSB_XFER_CONTROL) && (p_qhd->ep_speed != TUSB_SPEED_HIGH))  ? 1 : 0;
  p_qhd->nak_reload         = 0;

  // Bulk/Control -> smask = cmask = 0
  // TODO Isochronous
  if (TUSB_XFER_INTERRUPT == xfer_type)
  {
    if (TUSB_SPEED_HIGH == p_qhd->ep_speed)
    {
      TU_ASSERT( interval <= 16, );
      if ( interval < 4) // sub milisecond interval
      {
        p_qhd->interval_ms = 0;
        p_qhd->int_smask   = (interval == 1) ? BIN8(11111111) :
                             (interval == 2) ? BIN8(10101010) : BIN8(01000100);
      }else
      {
        p_qhd->interval_ms = (uint8_t) tu_min16( 1 << (interval-4), 255 );
        p_qhd->int_smask = BIT_(interval % 8);
      }
    }else
    {
      TU_ASSERT( 0 != interval, );
      // Full/Low: 4.12.2.1 (EHCI) case 1 schedule start split at 1 us & complete split at 2,3,4 uframes
      p_qhd->int_smask    = 0x01;
      p_qhd->fl_int_cmask = BIN8(11100);
      p_qhd->interval_ms  = interval;
    }
  }else
  {
    p_qhd->int_smask = p_qhd->fl_int_cmask = 0;
  }

  p_qhd->fl_hub_addr     = _usbh_devices[dev_addr].hub_addr;
  p_qhd->fl_hub_port     = _usbh_devices[dev_addr].hub_port;
  p_qhd->mult            = 1; // TODO not use high bandwidth/park mode yet

  //------------- HCD Management Data -------------//
  p_qhd->used            = 1;
  p_qhd->is_removing     = 0;
  p_qhd->p_qtd_list_head = NULL;
  p_qhd->p_qtd_list_tail = NULL;
  p_qhd->pid_non_control = edpt_dir(ep_desc->bEndpointAddress) ? EHCI_PID_IN : EHCI_PID_OUT; // PID for TD under this endpoint
  p_qhd->xfer_type       = xfer_type;

  //------------- active, but no TD list -------------//
  p_qhd->qtd_overlay.halted              = 0;
  p_qhd->qtd_overlay.next.terminate      = 1;
  p_qhd->qtd_overlay.alternate.terminate = 1;
  if (TUSB_XFER_BULK == xfer_type && p_qhd->ep_speed == TUSB_SPEED_HIGH && p_qhd->pid_non_control == EHCI_PID_OUT)
  {
    p_qhd->qtd_overlay.ping_err = 1; // do PING for Highspeed Bulk OUT, EHCI section 4.11
  }
}

static void qtd_init(ehci_qtd_t* p_qtd, uint32_t data_ptr, uint16_t total_bytes)
{
  tu_memclr(p_qtd, sizeof(ehci_qtd_t));

  p_qtd->used                = 1;

  p_qtd->next.terminate      = 1; // init to null
  p_qtd->alternate.terminate = 1; // not used, always set to terminated
  p_qtd->active              = 1;
  p_qtd->err_count           = 3; // TODO 3 consecutive errors tolerance
  p_qtd->data_toggle         = 0;
  p_qtd->total_bytes         = total_bytes;
  p_qtd->expected_bytes      = total_bytes;

  p_qtd->buffer[0] = data_ptr;
  for(uint8_t i=1; i<5; i++)
  {
    p_qtd->buffer[i] |= tu_align4k( p_qtd->buffer[i-1] ) + 4096;
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
  return (ehci_link_t*) tu_align32(p_link_pointer->address);
}

static ehci_link_t* list_find_previous_item(ehci_link_t* p_head, ehci_link_t* p_current)
{
  ehci_link_t *p_prev = p_head;
  uint32_t max_loop = 0;
  while( (tu_align32(p_prev->address) != (uint32_t) p_head)    && // not loop around
         (tu_align32(p_prev->address) != (uint32_t) p_current) && // not found yet
         !p_prev->terminate                                 && // not advanceable
         max_loop < HCD_MAX_ENDPOINT)
  {
    p_prev = list_next(p_prev);
    max_loop++;
  }

  return  (tu_align32(p_prev->address) != (uint32_t) p_head) ? p_prev : NULL;
}

static tusb_error_t list_remove_qhd(ehci_link_t* p_head, ehci_link_t* p_remove)
{
  ehci_link_t *p_prev = list_find_previous_item(p_head, p_remove);

  TU_ASSERT(p_prev, TUSB_ERROR_INVALID_PARA);

  p_prev->address   = p_remove->address;
  // EHCI 4.8.2 link the removing queue head to async/period head (which always reachable by Host Controller)
  p_remove->address = ((uint32_t) p_head) | (EHCI_QUEUE_ELEMENT_QHD << 1);

  return TUSB_ERROR_NONE;
}

#endif

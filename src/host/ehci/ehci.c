/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
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

#include "common/tusb_common.h"

#if TUSB_OPT_HOST_ENABLED && (CFG_TUSB_MCU == OPT_MCU_LPC43XX || CFG_TUSB_MCU == OPT_MCU_LPC18XX)
//--------------------------------------------------------------------+
// INCLUDE
//--------------------------------------------------------------------+
#include "osal/osal.h"

#include "../hcd.h"
#include "../usbh_hcd.h"
#include "ehci.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+

//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+
// Periodic frame list must be 4K alignment
CFG_TUSB_MEM_SECTION TU_ATTR_ALIGNED(4096) static ehci_data_t ehci_data;

// EHCI portable
uint32_t hcd_ehci_register_addr(uint8_t rhport);

//--------------------------------------------------------------------+
// PROTOTYPE
//--------------------------------------------------------------------+
static inline ehci_link_t* get_period_head(uint8_t rhport, uint8_t interval_ms)
{
  (void) rhport;
  return (ehci_link_t*) &ehci_data.period_head_arr[ tu_log2( tu_min8(EHCI_FRAMELIST_SIZE, interval_ms) ) ];
}

static inline ehci_qhd_t* qhd_control(uint8_t dev_addr)
{
  return &ehci_data.control[dev_addr].qhd;
}

static inline ehci_qhd_t* qhd_async_head(uint8_t rhport)
{
  (void) rhport;
  return qhd_control(0); // control qhd of dev0 is used as async head
}

static inline ehci_qtd_t* qtd_control(uint8_t dev_addr)
{
  return &ehci_data.control[dev_addr].qtd;
}


static inline ehci_qhd_t* qhd_next (ehci_qhd_t const * p_qhd);
static inline ehci_qhd_t* qhd_find_free (void);
static inline ehci_qhd_t* qhd_get_from_addr (uint8_t dev_addr, uint8_t ep_addr);

// determine if a queue head has bus-related error
static inline bool qhd_has_xact_error (ehci_qhd_t * p_qhd)
{
  return (p_qhd->qtd_overlay.buffer_err || p_qhd->qtd_overlay.babble_err || p_qhd->qtd_overlay.xact_err);
  //p_qhd->qtd_overlay.non_hs_period_missed_uframe || p_qhd->qtd_overlay.pingstate_err TODO split transaction error
}

static void qhd_init (ehci_qhd_t *p_qhd, uint8_t dev_addr, tusb_desc_endpoint_t const * ep_desc);

static inline ehci_qtd_t* qtd_find_free (void);
static inline ehci_qtd_t* qtd_next (ehci_qtd_t const * p_qtd);
static inline void qtd_insert_to_qhd (ehci_qhd_t *p_qhd, ehci_qtd_t *p_qtd_new);
static inline void qtd_remove_1st_from_qhd (ehci_qhd_t *p_qhd);
static void qtd_init (ehci_qtd_t* p_qtd, void* buffer, uint16_t total_bytes);

static inline void list_insert (ehci_link_t *current, ehci_link_t *new, uint8_t new_type);
static inline ehci_link_t* list_next (ehci_link_t *p_link_pointer);

static bool ehci_init (uint8_t rhport);

//--------------------------------------------------------------------+
// HCD API
//--------------------------------------------------------------------+
bool hcd_init(void)
{
  tu_memclr(&ehci_data, sizeof(ehci_data_t));
  return ehci_init(TUH_OPT_RHPORT);
}

void hcd_port_reset(uint8_t rhport)
{
  (void) rhport;

  ehci_registers_t* regs = ehci_data.regs;

  regs->portsc_bm.port_enabled = 0; // disable port before reset
  regs->portsc_bm.port_reset = 1;
}

bool hcd_port_connect_status(uint8_t rhport)
{
  (void) rhport;
  return ehci_data.regs->portsc_bm.current_connect_status;
}

tusb_speed_t hcd_port_speed_get(uint8_t rhport)
{
  (void) rhport;
  return (tusb_speed_t) ehci_data.regs->portsc_bm.nxp_port_speed; // NXP specific port speed
}

static void list_remove_qhd_by_addr(ehci_link_t* list_head, uint8_t dev_addr)
{
  for(ehci_link_t* prev = list_head;
      !prev->terminate && (tu_align32(prev->address) != (uint32_t) list_head);
      prev = list_next(prev) )
  {
    // TODO check type for ISO iTD and siTD
    ehci_qhd_t* qhd = (ehci_qhd_t*) list_next(prev);
    if ( qhd->dev_addr == dev_addr )
    {
      // TODO deactive all TD, wait for QHD to inactive before removal
      prev->address = qhd->next.address;

      // EHCI 4.8.2 link the removed qhd to async head (which always reachable by Host Controller)
      qhd->next.address = ((uint32_t) list_head) | (EHCI_QTYPE_QHD << 1);

      if ( qhd->int_smask )
      {
        // period list queue element is guarantee to be free in the next frame (1 ms)
        qhd->used = 0;
      }else
      {
        // async list use async advance handshake
        // mark as removing, will completely re-usable when async advance isr occurs
        qhd->removing = 1;
      }
    }
  }
}

// Close all opened endpoint belong to this device
void hcd_device_close(uint8_t rhport, uint8_t dev_addr)
{
  // skip dev0
  if (dev_addr == 0) return;

  // Remove from async list
  list_remove_qhd_by_addr( (ehci_link_t*) qhd_async_head(rhport), dev_addr );

  // Remove from all interval period list
  for(uint8_t i = 0; i < TU_ARRAY_SIZE(ehci_data.period_head_arr); i++)
  {
    list_remove_qhd_by_addr( (ehci_link_t*) &ehci_data.period_head_arr[i], dev_addr);
  }

  // Async doorbell (EHCI 4.8.2 for operational details)
  ehci_data.regs->command_bm.async_adv_doorbell = 1;
}

// EHCI controller init
static bool ehci_init(uint8_t rhport)
{
  ehci_data.regs = (ehci_registers_t* ) hcd_ehci_register_addr(rhport);

  ehci_registers_t* regs = ehci_data.regs;

  //------------- CTRLDSSEGMENT Register (skip) -------------//
  //------------- USB INT Register -------------//
  regs->inten  = 0;                 // 1. disable all the interrupt
  regs->status = EHCI_INT_MASK_ALL; // 2. clear all status

  regs->inten  = EHCI_INT_MASK_ERROR | EHCI_INT_MASK_PORT_CHANGE | EHCI_INT_MASK_ASYNC_ADVANCE |
                 EHCI_INT_MASK_NXP_PERIODIC | EHCI_INT_MASK_NXP_ASYNC ;

  //------------- Asynchronous List -------------//
  ehci_qhd_t * const async_head = qhd_async_head(rhport);
  tu_memclr(async_head, sizeof(ehci_qhd_t));

  async_head->next.address                    = (uint32_t) async_head; // circular list, next is itself
  async_head->next.type                       = EHCI_QTYPE_QHD;
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
  ehci_link_t * const period_1ms = get_period_head(rhport, 1);
  // all links --> period_head_arr[0] (1ms)
  // 0, 2, 4, 6 etc --> period_head_arr[1] (2ms)
  // 1, 5 --> period_head_arr[2] (4ms)
  // 3 --> period_head_arr[3] (8ms)

  // TODO EHCI_FRAMELIST_SIZE with other size than 8
  for(uint32_t i=0; i<EHCI_FRAMELIST_SIZE; i++)
  {
    framelist[i].address = (uint32_t) period_1ms;
    framelist[i].type    = EHCI_QTYPE_QHD;
  }

  for(uint32_t i=0; i<EHCI_FRAMELIST_SIZE; i+=2)
  {
    list_insert(framelist + i, get_period_head(rhport, 2), EHCI_QTYPE_QHD);
  }

  for(uint32_t i=1; i<EHCI_FRAMELIST_SIZE; i+=4)
  {
    list_insert(framelist + i, get_period_head(rhport, 4), EHCI_QTYPE_QHD);
  }

  list_insert(framelist+3, get_period_head(rhport, 8), EHCI_QTYPE_QHD);

  period_1ms->terminate    = 1;

  regs->periodic_list_base = (uint32_t) framelist;

  //------------- TT Control (NXP only) -------------//
  regs->nxp_tt_control = 0;

  //------------- USB CMD Register -------------//
  regs->command |= TU_BIT(EHCI_USBCMD_POS_RUN_STOP) | TU_BIT(EHCI_USBCMD_POS_ASYNC_ENABLE)
                | TU_BIT(EHCI_USBCMD_POS_PERIOD_ENABLE) // TODO enable period list only there is int/iso endpoint
                | ((EHCI_CFG_FRAMELIST_SIZE_BITS & TU_BIN8(011)) << EHCI_USBCMD_POS_FRAMELIST_SZIE)
                | ((EHCI_CFG_FRAMELIST_SIZE_BITS >> 2) << EHCI_USBCMD_POS_NXP_FRAMELIST_SIZE_MSB);

  //------------- ConfigFlag Register (skip) -------------//
  regs->portsc_bm.port_power = 1; // enable port power

  return true;
}

#if 0
static void ehci_stop(uint8_t rhport)
{
  (void) rhport;

  ehci_registers_t* regs = ehci_data.regs;

  regs->command_bm.run_stop = 0;

  // USB Spec: controller has to stop within 16 uframe = 2 frames
  while( regs->status_bm.hc_halted == 0 ) {}
}
#endif

//--------------------------------------------------------------------+
// CONTROL PIPE API
//--------------------------------------------------------------------+
bool hcd_edpt_xfer(uint8_t rhport, uint8_t dev_addr, uint8_t ep_addr, uint8_t * buffer, uint16_t buflen)
{
  (void) rhport;

  uint8_t const epnum = tu_edpt_number(ep_addr);
  uint8_t const dir   = tu_edpt_dir(ep_addr);

  // FIXME control only for now
  if ( epnum == 0 )
  {
    ehci_qhd_t* qhd = qhd_control(dev_addr);
    ehci_qtd_t* qtd = qtd_control(dev_addr);

    qtd_init(qtd, buffer, buflen);

    // first first data toggle is always 1 (data & setup stage)
    qtd->data_toggle = 1;
    qtd->pid = dir ? EHCI_PID_IN : EHCI_PID_OUT;
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
  (void) rhport;

  ehci_qhd_t* qhd = &ehci_data.control[dev_addr].qhd;
  ehci_qtd_t* td  = &ehci_data.control[dev_addr].qtd;

  qtd_init(td, (void*) setup_packet, 8);
  td->pid          = EHCI_PID_SETUP;
  td->int_on_complete = 1;
  td->next.terminate  = 1;

  // sw region
  qhd->p_qtd_list_head = td;
  qhd->p_qtd_list_tail = td;

  // attach TD
  qhd->qtd_overlay.next.address = (uint32_t) td;

  return true;
}

//--------------------------------------------------------------------+
// BULK/INT/ISO PIPE API
//--------------------------------------------------------------------+
bool hcd_edpt_open(uint8_t rhport, uint8_t dev_addr, tusb_desc_endpoint_t const * ep_desc)
{
  (void) rhport;

  // TODO not support ISO yet
  TU_ASSERT (ep_desc->bmAttributes.xfer != TUSB_XFER_ISOCHRONOUS);

  //------------- Prepare Queue Head -------------//
  ehci_qhd_t * p_qhd;

  if ( ep_desc->bEndpointAddress == 0 )
  {
    p_qhd = qhd_control(dev_addr);
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
      list_head = (ehci_link_t*) qhd_async_head(rhport);
    break;

    case TUSB_XFER_INTERRUPT:
      list_head = get_period_head(rhport, p_qhd->interval_ms);
    break;

    case TUSB_XFER_ISOCHRONOUS:
      // TODO iso is not supported
    break;

    default: break;
  }

  // TODO might need to disable async/period list
  list_insert( list_head, (ehci_link_t*) p_qhd, EHCI_QTYPE_QHD);

  return true;
}

bool hcd_pipe_queue_xfer(uint8_t dev_addr, uint8_t ep_addr, uint8_t buffer[], uint16_t total_bytes)
{
  //------------- set up QTD -------------//
  ehci_qhd_t *p_qhd = qhd_get_from_addr(dev_addr, ep_addr);
  ehci_qtd_t *p_qtd = qtd_find_free();

  TU_ASSERT(p_qtd);

  qtd_init(p_qtd, buffer, total_bytes);
  p_qtd->pid = p_qhd->pid;

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

bool hcd_edpt_busy(uint8_t dev_addr, uint8_t ep_addr)
{
  ehci_qhd_t *p_qhd = qhd_get_from_addr(dev_addr, ep_addr);
  return !p_qhd->qtd_overlay.halted && (p_qhd->p_qtd_list_head != NULL);
}

bool hcd_edpt_stalled(uint8_t dev_addr, uint8_t ep_addr)
{
  ehci_qhd_t *p_qhd = qhd_get_from_addr(dev_addr, ep_addr);
  return p_qhd->qtd_overlay.halted && !qhd_has_xact_error(p_qhd);
}

bool hcd_edpt_clear_stall(uint8_t dev_addr, uint8_t ep_addr)
{
  ehci_qhd_t *p_qhd = qhd_get_from_addr(dev_addr, ep_addr);
  p_qhd->qtd_overlay.halted = 0;
  // TODO reset data toggle ?
  return true;
}

//--------------------------------------------------------------------+
// EHCI Interrupt Handler
//--------------------------------------------------------------------+

// async_advance is handshake between usb stack & ehci controller.
// This isr mean it is safe to modify previously removed queue head from async list.
// In tinyusb, queue head is only removed when device is unplugged.
static void async_advance_isr(uint8_t rhport)
{
  (void) rhport;

  ehci_qhd_t* qhd_pool = ehci_data.qhd_pool;
  for(uint32_t i = 0; i < HCD_MAX_ENDPOINT; i++)
  {
    if ( qhd_pool[i].removing )
    {
      qhd_pool[i].removing = 0;
      qhd_pool[i].used     = 0;
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
      hcd_event_xfer_complete(p_qhd->dev_addr, tu_edpt_addr(p_qhd->ep_number, p_qhd->pid == EHCI_PID_IN ? 1 : 0), XFER_RESULT_SUCCESS, p_qhd->total_xferred_bytes);
      p_qhd->total_xferred_bytes = 0;
    }
  }
}

static void async_list_xfer_complete_isr(ehci_qhd_t * const async_head)
{
  ehci_qhd_t *p_qhd = async_head;
  do
  {
    if ( !p_qhd->qtd_overlay.halted ) // halted or error is processed in error isr
    {
      qhd_xfer_complete_isr(p_qhd);
    }
    p_qhd = qhd_next(p_qhd);
  }while(p_qhd != async_head); // async list traversal, stop if loop around
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
      case EHCI_QTYPE_QHD:
      {
        ehci_qhd_t *p_qhd_int = (ehci_qhd_t *) tu_align32(next_item.address);
        if ( !p_qhd_int->qtd_overlay.halted )
        {
          qhd_xfer_complete_isr(p_qhd_int);
        }
      }
      break;

      case EHCI_QTYPE_ITD: // TODO support hs/fs ISO
      case EHCI_QTYPE_SITD:
      case EHCI_QTYPE_FSTN:
				
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
  {
    // current qhd has error in transaction
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

      ehci_qtd_t *p_setup = qtd_control(p_qhd->dev_addr);
      p_setup->used = 0;
    }

    // call USBH callback
    hcd_event_xfer_complete(p_qhd->dev_addr, tu_edpt_addr(p_qhd->ep_number, p_qhd->pid == EHCI_PID_IN ? 1 : 0), error_event, p_qhd->total_xferred_bytes);

    p_qhd->total_xferred_bytes = 0;
  }
}

static void xfer_error_isr(uint8_t hostid)
{
  //------------- async list -------------//
  ehci_qhd_t * const async_head = qhd_async_head(hostid);
  ehci_qhd_t *p_qhd = async_head;
  do
  {
    qhd_xfer_error_isr( p_qhd );
    p_qhd = qhd_next(p_qhd);
  }while(p_qhd != async_head); // async list traversal, stop if loop around

  //------------- TODO refractor period list -------------//
  uint32_t const period_1ms_addr = (uint32_t) get_period_head(hostid, 1);
  for (uint8_t interval_ms=1; interval_ms <= EHCI_FRAMELIST_SIZE; interval_ms *= 2)
  {
    ehci_link_t next_item = * get_period_head(hostid, interval_ms);

    // TODO abstract max loop guard for period
    while( !next_item.terminate &&
           !(interval_ms > 1 && period_1ms_addr == tu_align32(next_item.address)) )
    {
      switch ( next_item.type )
      {
        case EHCI_QTYPE_QHD:
        {
          ehci_qhd_t *p_qhd_int = (ehci_qhd_t *) tu_align32(next_item.address);
          qhd_xfer_error_isr(p_qhd_int);
        }
        break;

				// TODO support hs/fs ISO
        case EHCI_QTYPE_ITD:
        case EHCI_QTYPE_SITD:
        case EHCI_QTYPE_FSTN:
        default: break;
      }

      next_item = *list_next(&next_item);
    }
  }
}

//------------- Host Controller Driver's Interrupt Handler -------------//
void hcd_isr(uint8_t rhport)
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
      port_connect_status_change_isr(rhport);
    }

    regs->portsc |= port_status; // Acknowledge change bits in portsc
  }

  if (int_status & EHCI_INT_MASK_ERROR)
  {
    xfer_error_isr(rhport);
  }

  //------------- some QTD/SITD/ITD with IOC set is completed -------------//
  if (int_status & EHCI_INT_MASK_NXP_ASYNC)
  {
    async_list_xfer_complete_isr( qhd_async_head(rhport) );
  }

  if (int_status & EHCI_INT_MASK_NXP_PERIODIC)
  {
    for (uint8_t i=1; i <= EHCI_FRAMELIST_SIZE; i *= 2)
    {
      period_list_xfer_complete_isr( rhport, i );
    }
  }

  //------------- There is some removed async previously -------------//
  if (int_status & EHCI_INT_MASK_ASYNC_ADVANCE) // need to place after EHCI_INT_MASK_NXP_ASYNC
  {
    async_advance_isr(rhport);
  }
}

//--------------------------------------------------------------------+
// HELPER
//--------------------------------------------------------------------+


//------------- queue head helper -------------//
static inline ehci_qhd_t* qhd_find_free (void)
{
  for (uint32_t i=0; i<HCD_MAX_ENDPOINT; i++)
  {
    if ( !ehci_data.qhd_pool[i].used ) return &ehci_data.qhd_pool[i];
  }

  return NULL;
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
          ep_addr == tu_edpt_addr(qhd_pool[i].ep_number, qhd_pool[i].pid) )
    {
      return &qhd_pool[i];
    }
  }

  return NULL;
}

//------------- TD helper -------------//
static inline ehci_qtd_t* qtd_find_free(void)
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
  p_qhd->fl_inactive_next_xact = 0;
  p_qhd->ep_number          = tu_edpt_number(ep_desc->bEndpointAddress);
  p_qhd->ep_speed           = _usbh_devices[dev_addr].speed;
  p_qhd->data_toggle_control= (xfer_type == TUSB_XFER_CONTROL) ? 1 : 0;
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
        p_qhd->int_smask   = (interval == 1) ? TU_BIN8(11111111) :
                             (interval == 2) ? TU_BIN8(10101010) : TU_BIN8(01000100);
      }else
      {
        p_qhd->interval_ms = (uint8_t) tu_min16( 1 << (interval-4), 255 );
        p_qhd->int_smask = TU_BIT(interval % 8);
      }
    }else
    {
      TU_ASSERT( 0 != interval, );
      // Full/Low: 4.12.2.1 (EHCI) case 1 schedule start split at 1 us & complete split at 2,3,4 uframes
      p_qhd->int_smask    = 0x01;
      p_qhd->fl_int_cmask = TU_BIN8(11100);
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
  p_qhd->removing        = 0;
  p_qhd->p_qtd_list_head = NULL;
  p_qhd->p_qtd_list_tail = NULL;
  p_qhd->pid = tu_edpt_dir(ep_desc->bEndpointAddress) ? EHCI_PID_IN : EHCI_PID_OUT; // PID for TD under this endpoint

  //------------- active, but no TD list -------------//
  p_qhd->qtd_overlay.halted              = 0;
  p_qhd->qtd_overlay.next.terminate      = 1;
  p_qhd->qtd_overlay.alternate.terminate = 1;
  if (TUSB_XFER_BULK == xfer_type && p_qhd->ep_speed == TUSB_SPEED_HIGH && p_qhd->pid == EHCI_PID_OUT)
  {
    p_qhd->qtd_overlay.ping_err = 1; // do PING for Highspeed Bulk OUT, EHCI section 4.11
  }
}

static void qtd_init(ehci_qtd_t* p_qtd, void* buffer, uint16_t total_bytes)
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

  p_qtd->buffer[0] = (uint32_t) buffer;
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

#endif

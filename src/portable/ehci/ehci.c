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

#include "tusb_option.h"

#if CFG_TUH_ENABLED && defined(TUP_USBIP_EHCI)

//--------------------------------------------------------------------+
// INCLUDE
//--------------------------------------------------------------------+
#include "osal/osal.h"

#include "host/hcd.h"
#include "ehci_api.h"
#include "ehci.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+

// Debug level of EHCI
#define EHCI_DBG     2

// Framelist size as small as possible to save SRAM
#ifdef TUP_USBIP_CHIPIDEA_HS
  // NXP Transdimension: 8 elements
  #define FRAMELIST_SIZE_BIT_VALUE      7u
  #define FRAMELIST_SIZE_USBCMD_VALUE   (((FRAMELIST_SIZE_BIT_VALUE &  3) << EHCI_USBCMD_POS_FRAMELIST_SIZE) | \
                                         ((FRAMELIST_SIZE_BIT_VALUE >> 2) << EHCI_USBCMD_POS_NXP_FRAMELIST_SIZE_MSB))
#else
  // STD EHCI: 256 elements
  #define FRAMELIST_SIZE_BIT_VALUE      2u
  #define FRAMELIST_SIZE_USBCMD_VALUE   ((FRAMELIST_SIZE_BIT_VALUE &  3) << EHCI_USBCMD_POS_FRAMELIST_SIZE)
#endif

#define FRAMELIST_SIZE                  (1024 >> FRAMELIST_SIZE_BIT_VALUE)

// Total queue head pool. TODO should be user configurable and more optimize memory usage in the future
#define QHD_MAX      (CFG_TUH_DEVICE_MAX*CFG_TUH_ENDPOINT_MAX + CFG_TUH_HUB)
#define QTD_MAX      QHD_MAX

typedef struct
{
  ehci_link_t period_framelist[FRAMELIST_SIZE];

  // TODO only implement 1 ms & 2 ms & 4 ms, 8 ms (framelist)
  // [0] : 1ms, [1] : 2ms, [2] : 4ms, [3] : 8 ms
  // TODO better implementation without dummy head to save SRAM
  ehci_qhd_t period_head_arr[4];

  // Note control qhd of dev0 is used as head of async list
  struct {
    ehci_qhd_t qhd;
    ehci_qtd_t qtd;
  }control[CFG_TUH_DEVICE_MAX+CFG_TUH_HUB+1];

  ehci_qhd_t qhd_pool[QHD_MAX];
  ehci_qtd_t qtd_pool[QTD_MAX] TU_ATTR_ALIGNED(32);

  ehci_registers_t* regs;         // operational register
  ehci_cap_registers_t* cap_regs; // capability register

  volatile uint32_t uframe_number;
}ehci_data_t;

// Periodic frame list must be 4K alignment
CFG_TUH_MEM_SECTION TU_ATTR_ALIGNED(4096) static ehci_data_t ehci_data;

//--------------------------------------------------------------------+
// Debug
//--------------------------------------------------------------------+
#if CFG_TUSB_DEBUG >= (EHCI_DBG + 1)
static inline void print_portsc(ehci_registers_t* regs) {
  TU_LOG_HEX(EHCI_DBG, regs->portsc);
  TU_LOG(EHCI_DBG, "  Connect Status : %u\r\n", regs->portsc_bm.current_connect_status);
  TU_LOG(EHCI_DBG, "  Connect Change : %u\r\n", regs->portsc_bm.connect_status_change);
  TU_LOG(EHCI_DBG, "  Enabled        : %u\r\n", regs->portsc_bm.port_enabled);
  TU_LOG(EHCI_DBG, "  Enabled Change : %u\r\n", regs->portsc_bm.port_enable_change);

  TU_LOG(EHCI_DBG, "  OverCurr Change: %u\r\n", regs->portsc_bm.over_current_change);
  TU_LOG(EHCI_DBG, "  Force Resume   : %u\r\n", regs->portsc_bm.force_port_resume);
  TU_LOG(EHCI_DBG, "  Suspend        : %u\r\n", regs->portsc_bm.suspend);
  TU_LOG(EHCI_DBG, "  Reset          : %u\r\n", regs->portsc_bm.port_reset);
  TU_LOG(EHCI_DBG, "  Power          : %u\r\n", regs->portsc_bm.port_power);
}

static inline void print_intr(uint32_t intr) {
  TU_LOG_HEX(EHCI_DBG, intr);
  TU_LOG(EHCI_DBG, "  USB Interrupt      : %u\r\n", (intr & EHCI_INT_MASK_USB) ? 1 : 0);
  TU_LOG(EHCI_DBG, "  USB Error          : %u\r\n", (intr & EHCI_INT_MASK_ERROR) ? 1 : 0);
  TU_LOG(EHCI_DBG, "  Port Change Detect : %u\r\n", (intr & EHCI_INT_MASK_PORT_CHANGE) ? 1 : 0);
  TU_LOG(EHCI_DBG, "  Frame List Rollover: %u\r\n", (intr & EHCI_INT_MASK_FRAMELIST_ROLLOVER) ? 1 : 0);
  TU_LOG(EHCI_DBG, "  Host System Error  : %u\r\n", (intr & EHCI_INT_MASK_PCI_HOST_SYSTEM_ERROR) ? 1 : 0);
  TU_LOG(EHCI_DBG, "  Async Advance      : %u\r\n", (intr & EHCI_INT_MASK_ASYNC_ADVANCE) ? 1 : 0);
//  TU_LOG(EHCI_DBG, "  Interrupt on Async: %u\r\n", (intr & EHCI_INT_MASK_NXP_ASYNC));
//  TU_LOG(EHCI_DBG, "  Periodic Schedule : %u\r\n", (intr & EHCI_INT_MASK_NXP_PERIODIC));
}

#else
#define print_portsc(_reg)
#endif

//--------------------------------------------------------------------+
// PROTOTYPE
//--------------------------------------------------------------------+
static inline ehci_link_t* get_period_head(uint8_t rhport, uint32_t interval_ms)
{
  (void) rhport;
  return (ehci_link_t*) &ehci_data.period_head_arr[ tu_log2( tu_min32(FRAMELIST_SIZE, interval_ms) ) ];
}

static inline ehci_qhd_t* qhd_control(uint8_t dev_addr)
{
  return &ehci_data.control[dev_addr].qhd;
}

static inline ehci_qhd_t* qhd_async_head(uint8_t rhport)
{
  (void) rhport;
  // control qhd of dev0 is used as async head
  return qhd_control(0);
}

static inline ehci_qtd_t* qtd_control(uint8_t dev_addr)
{
  return &ehci_data.control[dev_addr].qtd;
}


static inline ehci_qhd_t* qhd_next (ehci_qhd_t const * p_qhd);
static inline ehci_qhd_t* qhd_find_free (void);
static inline ehci_qhd_t* qhd_get_from_addr (uint8_t dev_addr, uint8_t ep_addr);
static void qhd_init(ehci_qhd_t *p_qhd, uint8_t dev_addr, tusb_desc_endpoint_t const * ep_desc);
static void qhd_attach_qtd(ehci_qhd_t *qhd, ehci_qtd_t *qtd);

static inline ehci_qtd_t* qtd_find_free (void);
static void qtd_init (ehci_qtd_t* qtd, void const* buffer, uint16_t total_bytes);

static inline void list_insert (ehci_link_t *current, ehci_link_t *new, uint8_t new_type);
static inline ehci_link_t* list_next (ehci_link_t const *p_link);

TU_ATTR_WEAK void hcd_dcache_clean(void const* addr, uint32_t data_size) {
  (void) addr; (void) data_size;
}

TU_ATTR_WEAK void hcd_dcache_invalidate(void const* addr, uint32_t data_size) {
  (void) addr; (void) data_size;
}

TU_ATTR_WEAK void hcd_dcache_clean_invalidate(void const* addr, uint32_t data_size) {
  (void) addr; (void) data_size;
}

//--------------------------------------------------------------------+
// HCD API
//--------------------------------------------------------------------+

uint32_t hcd_frame_number(uint8_t rhport)
{
  (void) rhport;
  return (ehci_data.uframe_number + ehci_data.regs->frame_index) >> 3;
}

void hcd_port_reset(uint8_t rhport)
{
  (void) rhport;

  ehci_registers_t* regs = ehci_data.regs;

  // mask out Write-1-to-Clear bits
  uint32_t portsc = regs->portsc & ~EHCI_PORTSC_MASK_W1C;

  // EHCI Table 2-16 PortSC
  // when software writes Port Reset bit to a one, it must also write a zero to the Port Enable bit.
  portsc &= ~(EHCI_PORTSC_MASK_PORT_EANBLED);
  portsc |= EHCI_PORTSC_MASK_PORT_RESET;

  regs->portsc = portsc;
}

void hcd_port_reset_end(uint8_t rhport)
{
  (void) rhport;

#if 0 // TODO check if this is necessary
  ehci_registers_t* regs = ehci_data.regs;

  // mask out all change bits since they are Write 1 to clear
  uint32_t portsc = regs->portsc & ~EHCI_PORTSC_MASK_CHANGE_ALL;
  portsc &= ~(EHCI_PORTSC_MASK_PORT_RESET);

  regs->portsc = portsc;
#endif
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

static void list_remove_qhd_by_daddr(ehci_link_t* list_head, uint8_t dev_addr) {
  ehci_link_t* prev = list_head;

  while (prev && !prev->terminate) {
    ehci_qhd_t* qhd = (ehci_qhd_t*) (uintptr_t) list_next(prev);

    // done if loop back to head
    if ( (uintptr_t) qhd == (uintptr_t) list_head) {
      break;
    }

    if ( qhd->dev_addr == dev_addr ) {
      // TODO deactivate all TD, wait for QHD to inactive before removal
      prev->address = qhd->next.address;

      // EHCI 4.8.2 link the removed qhd's next to async head (which always reachable by Host Controller)
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

      hcd_dcache_clean(qhd, sizeof(ehci_qhd_t));
      hcd_dcache_clean(prev, sizeof(ehci_qhd_t));
    }else {
      prev = list_next(prev);
    }
  }
}

// Close all opened endpoint belong to this device
void hcd_device_close(uint8_t rhport, uint8_t daddr)
{
  // skip dev0
  if (daddr == 0) {
    return;
  }

  // Remove from async list
  list_remove_qhd_by_daddr((ehci_link_t *) qhd_async_head(rhport), daddr);

  // Remove from all interval period list
  for(uint8_t i = 0; i < TU_ARRAY_SIZE(ehci_data.period_head_arr); i++) {
    list_remove_qhd_by_daddr((ehci_link_t *) &ehci_data.period_head_arr[i], daddr);
  }

  // Async doorbell (EHCI 4.8.2 for operational details)
  ehci_data.regs->command_bm.async_adv_doorbell = 1;
}

static void init_periodic_list(uint8_t rhport) {
  // Build the polling interval tree with 1 ms, 2 ms, 4 ms and 8 ms (framesize) only
  for ( uint32_t i = 0; i < TU_ARRAY_SIZE(ehci_data.period_head_arr); i++ ) {
    ehci_data.period_head_arr[i].int_smask          = 1; // queue head in period list must have smask non-zero
    ehci_data.period_head_arr[i].qtd_overlay.halted = 1; // dummy node, always inactive
  }

  ehci_link_t * const framelist  = ehci_data.period_framelist;
  ehci_link_t * const period_1ms = get_period_head(rhport, 1u);

  // all links --> period_head_arr[0] (1ms)
  // 0, 2, 4, 6 etc --> period_head_arr[1] (2ms)
  // 1, 5 --> period_head_arr[2] (4ms)
  // 3 --> period_head_arr[3] (8ms)

  // TODO EHCI_FRAMELIST_SIZE with other size than 8
  for (uint32_t i = 0; i < FRAMELIST_SIZE; i++) {
    framelist[i].address = (uint32_t) period_1ms;
    framelist[i].type = EHCI_QTYPE_QHD;
  }

  for (uint32_t i = 0; i < FRAMELIST_SIZE; i += 2) {
    list_insert(framelist + i, get_period_head(rhport, 2u), EHCI_QTYPE_QHD);
  }

  for (uint32_t i = 1; i < FRAMELIST_SIZE; i += 4) {
    list_insert(framelist + i, get_period_head(rhport, 4u), EHCI_QTYPE_QHD);
  }

  list_insert(framelist + 3, get_period_head(rhport, 8u), EHCI_QTYPE_QHD);

  period_1ms->terminate    = 1;
}

bool ehci_init(uint8_t rhport, uint32_t capability_reg, uint32_t operatial_reg)
{
  tu_memclr(&ehci_data, sizeof(ehci_data_t));

  ehci_data.regs = (ehci_registers_t*) operatial_reg;
  ehci_data.cap_regs = (ehci_cap_registers_t*) capability_reg;

  ehci_registers_t* regs = ehci_data.regs;

  // EHCI 4.1 Host Controller Initialization

  //------------- CTRLDSSEGMENT Register (skip) -------------//

  //------------- USB INT Register -------------//

  // disable all the interrupt
  regs->inten  = 0;

  // clear all status except port change since device maybe connected before this driver is initialized
  regs->status = (EHCI_INT_MASK_ALL & ~EHCI_INT_MASK_PORT_CHANGE);

  // Enable interrupts
  regs->inten  = EHCI_INT_MASK_ERROR | EHCI_INT_MASK_PORT_CHANGE | EHCI_INT_MASK_ASYNC_ADVANCE |
                 EHCI_INT_MASK_NXP_PERIODIC | EHCI_INT_MASK_NXP_ASYNC | EHCI_INT_MASK_FRAMELIST_ROLLOVER;

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
  init_periodic_list(rhport);
  regs->periodic_list_base = (uint32_t) ehci_data.period_framelist;

  hcd_dcache_clean(&ehci_data, sizeof(ehci_data_t));

  //------------- TT Control (NXP only) -------------//
  regs->nxp_tt_control = 0;

  //------------- USB CMD Register -------------//
  regs->command |= TU_BIT(EHCI_USBCMD_POS_RUN_STOP) | TU_BIT(EHCI_USBCMD_POS_ASYNC_ENABLE) |
                   TU_BIT(EHCI_USBCMD_POS_PERIOD_ENABLE) |  // TODO enable period list only there is int/iso endpoint
                   FRAMELIST_SIZE_USBCMD_VALUE;

  //------------- ConfigFlag Register (skip) -------------//

  // enable port power bit in portsc. The function of this bit depends on the value of the Port
  // Power Control (PPC) field in the HCSPARAMS register.
  if (ehci_data.cap_regs->hcsparams_bm.port_power_control) {
    // mask out all change bits since they are Write 1 to clear
    uint32_t portsc = (regs->portsc & ~EHCI_PORTSC_MASK_W1C);
    portsc |= ECHI_PORTSC_MASK_PORT_POWER;

    regs->portsc = portsc;
  }

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
// Endpoint API
//--------------------------------------------------------------------+

bool hcd_edpt_open(uint8_t rhport, uint8_t dev_addr, tusb_desc_endpoint_t const * ep_desc)
{
  (void) rhport;

  // TODO not support ISO yet
  TU_ASSERT (ep_desc->bmAttributes.xfer != TUSB_XFER_ISOCHRONOUS);

  //------------- Prepare Queue Head -------------//
  ehci_qhd_t *p_qhd = (ep_desc->bEndpointAddress == 0) ? qhd_control(dev_addr) : qhd_find_free();
  TU_ASSERT(p_qhd);

  qhd_init(p_qhd, dev_addr, ep_desc);

  // control of dev0 is always present as async head
  if ( dev_addr == 0 ) return true;

  // Insert to list
  ehci_link_t * list_head = NULL;

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

  TU_ASSERT(list_head);

  // TODO might need to disable async/period list
  list_insert(list_head, (ehci_link_t*) p_qhd, EHCI_QTYPE_QHD);

  hcd_dcache_clean(p_qhd, sizeof(ehci_qhd_t));
  hcd_dcache_clean(list_head, sizeof(ehci_qhd_t));

  return true;
}

bool hcd_setup_send(uint8_t rhport, uint8_t dev_addr, uint8_t const setup_packet[8])
{
  (void) rhport;

  ehci_qhd_t* qhd = &ehci_data.control[dev_addr].qhd;
  ehci_qtd_t* td  = &ehci_data.control[dev_addr].qtd;

  qtd_init(td, setup_packet, 8);
  td->pid = EHCI_PID_SETUP;

  hcd_dcache_clean(setup_packet, 8);

  // attach TD to QHD -> start transferring
  qhd_attach_qtd(qhd, td);

  return true;
}

bool hcd_edpt_xfer(uint8_t rhport, uint8_t dev_addr, uint8_t ep_addr, uint8_t * buffer, uint16_t buflen)
{
  (void) rhport;

  uint8_t const epnum = tu_edpt_number(ep_addr);
  uint8_t const dir   = tu_edpt_dir(ep_addr);

  ehci_qhd_t* qhd;
  ehci_qtd_t* qtd;

  if (epnum == 0) {
    qhd = qhd_control(dev_addr);
    qtd = qtd_control(dev_addr);

    qtd_init(qtd, buffer, buflen);

    // first data toggle is always 1 (data & setup stage)
    qtd->data_toggle = 1;
    qtd->pid = dir ? EHCI_PID_IN : EHCI_PID_OUT;
  } else {
    qhd = qhd_get_from_addr(dev_addr, ep_addr);
    qtd = qtd_find_free();
    TU_ASSERT(qtd);

    qtd_init(qtd, buffer, buflen);
    qtd->pid = qhd->pid;
  }

  // IN transfer: invalidate buffer, OUT transfer: clean buffer
  if (dir) {
    hcd_dcache_invalidate(buffer, buflen);
  }else {
    hcd_dcache_clean(buffer, buflen);
  }

  // attach TD to QHD -> start transferring
  qhd_attach_qtd(qhd, qtd);

  return true;
}

bool hcd_edpt_clear_stall(uint8_t daddr, uint8_t ep_addr)
{
  ehci_qhd_t *qhd = qhd_get_from_addr(daddr, ep_addr);
  qhd->qtd_overlay.halted = 0;
  hcd_dcache_clean_invalidate(qhd, sizeof(ehci_qhd_t));
  // TODO reset data toggle ?
  return true;
}

//--------------------------------------------------------------------+
// EHCI Interrupt Handler
//--------------------------------------------------------------------+

// async_advance is handshake between usb stack & ehci controller.
// This isr mean it is safe to modify previously removed queue head from async list.
// In tinyusb, queue head is only removed when device is unplugged.
TU_ATTR_ALWAYS_INLINE static inline
void async_advance_isr(uint8_t rhport)
{
  (void) rhport;

  ehci_qhd_t *qhd_pool = ehci_data.qhd_pool;
  for (uint32_t i = 0; i < QHD_MAX; i++) {
    if (qhd_pool[i].removing) {
      qhd_pool[i].removing = 0;
      qhd_pool[i].used = 0;
    }
  }
}

TU_ATTR_ALWAYS_INLINE static inline
void port_connect_status_change_isr(uint8_t rhport)
{
  // NOTE There is an sequence plug->unplug->…..-> plug if device is powering with pre-plugged device
  if (ehci_data.regs->portsc_bm.current_connect_status)
  {
    hcd_port_reset(rhport);
    hcd_event_device_attach(rhport, true);
  }else // device unplugged
  {
    hcd_event_device_remove(rhport, true);
  }
}

TU_ATTR_ALWAYS_INLINE static inline
void qhd_xfer_complete_isr(ehci_qhd_t * qhd) {
  // examine TD attached to queue head
  ehci_qtd_t * volatile qtd = (ehci_qtd_t * volatile) qhd->attached_qtd;
  if (qtd == NULL) return; // no TD attached
  hcd_dcache_invalidate(qtd, sizeof(ehci_qtd_t));

  // TD is still active, no need to process
  if (qtd->active) {
    return;
  }

  uint8_t dir = (qtd->pid == EHCI_PID_IN) ? 1 : 0;
  uint32_t const xferred_bytes = qtd->expected_bytes - qtd->total_bytes;

  // invalidate dcache if IN transfer
  if (dir == 1 && qhd->attached_buffer != 0 && xferred_bytes > 0) {
    hcd_dcache_invalidate((void*) qhd->attached_buffer, xferred_bytes);
  }

  // remove and free TD before invoking callback
  qhd->attached_qtd = NULL;
  qhd->attached_buffer = 0;
  qtd->used = 0; // free QTD

  // notify usbh
  uint8_t const ep_addr = tu_edpt_addr(qhd->ep_number, dir);
  hcd_event_xfer_complete(qhd->dev_addr, ep_addr, xferred_bytes, XFER_RESULT_SUCCESS, true);
}

TU_ATTR_ALWAYS_INLINE static inline
void async_list_xfer_complete_isr(ehci_qhd_t * const async_head)
{
  ehci_qhd_t *p_qhd = async_head;
  do
  {
    hcd_dcache_invalidate(p_qhd, sizeof(ehci_qhd_t));

    // halted or error is processed in error isr
    if ( !p_qhd->qtd_overlay.halted ) {
      qhd_xfer_complete_isr(p_qhd);
    }

    p_qhd = qhd_next(p_qhd);
  }while(p_qhd != async_head); // async list traversal, stop if loop around
}

TU_ATTR_ALWAYS_INLINE static inline
void period_list_xfer_complete_isr(uint8_t rhport, uint32_t interval_ms)
{
  uint32_t const period_1ms_addr = (uint32_t) get_period_head(rhport, 1u);
  ehci_link_t next_link = * get_period_head(rhport, interval_ms);

  while (!next_link.terminate) {
    if (interval_ms > 1 && period_1ms_addr == tu_align32(next_link.address)) {
      // 1ms period list is end of list for all larger interval
      break;
    }

    uintptr_t const entry_addr = tu_align32(next_link.address);

    switch (next_link.type) {
      case EHCI_QTYPE_QHD: {
        ehci_qhd_t *qhd = (ehci_qhd_t *) entry_addr;
        hcd_dcache_invalidate(qhd, sizeof(ehci_qhd_t));

        if (!qhd->qtd_overlay.halted) {
          qhd_xfer_complete_isr(qhd);
        }
      }
        break;

      case EHCI_QTYPE_ITD:
        // TODO support hs ISO
        break;

      case EHCI_QTYPE_SITD:
        // TODO support split ISO
        break;

      case EHCI_QTYPE_FSTN:
      default:
        break;
    }

    next_link = *list_next(&next_link);
  }
}

// TODO merge with qhd_xfer_complete_isr()
TU_ATTR_ALWAYS_INLINE static inline
void qhd_xfer_error_isr(ehci_qhd_t * qhd)
{
  volatile ehci_qtd_t *qtd_overlay = &qhd->qtd_overlay;

  // TD has error
  if (qtd_overlay->halted) {
    xfer_result_t xfer_result;

    if (qtd_overlay->xact_err || qtd_overlay->err_count == 0 || qtd_overlay->buffer_err || qtd_overlay->babble_err) {
      // Error count = 0 often occurs when device disconnected, or other bus-related error
      xfer_result = XFER_RESULT_FAILED;
    }else {
      // no error bits are set, endpoint is halted due to STALL
      xfer_result = XFER_RESULT_STALLED;
    }

//    if (XFER_RESULT_FAILED == xfer_result ) {
//      TU_LOG1("  QHD xfer err count: %d\n", qtd_overlay->err_count);
//      TU_BREAKPOINT(); // TODO skip unplugged device
//    }

    ehci_qtd_t * volatile qtd = (ehci_qtd_t * volatile) qhd->attached_qtd;
    TU_ASSERT(qtd, ); // No TD yet, probably a race condition or cache issue !?

    hcd_dcache_invalidate(qtd, sizeof(ehci_qtd_t));

    uint8_t dir = (qtd->pid == EHCI_PID_IN) ? 1 : 0;
    uint32_t const xferred_bytes = qtd->expected_bytes - qtd->total_bytes;

    // invalidate dcache if IN transfer
    if (dir == 1 && qhd->attached_buffer != 0 && xferred_bytes > 0) {
      hcd_dcache_invalidate((void*) qhd->attached_buffer, xferred_bytes);
    }

    // remove and free TD before invoking callback
    qhd->attached_qtd = NULL;
    qhd->attached_buffer = 0;
    qtd->used = 0; // free QTD

    if (0 == qhd->ep_number ) {
      // control cannot be halted
      qhd->qtd_overlay.next.terminate      = 1;
      qhd->qtd_overlay.alternate.terminate = 1;
      qhd->qtd_overlay.halted              = 0;

      hcd_dcache_clean(qhd, sizeof(ehci_qhd_t));
    }

    // notify usbh
    uint8_t const ep_addr = tu_edpt_addr(qhd->ep_number, dir);
    hcd_event_xfer_complete(qhd->dev_addr, ep_addr, xferred_bytes, xfer_result, true);
  }
}

TU_ATTR_ALWAYS_INLINE static inline
void xfer_error_isr(uint8_t rhport)
{
  //------------- async list -------------//
  ehci_qhd_t * const async_head = qhd_async_head(rhport);
  ehci_qhd_t *p_qhd = async_head;
  do
  {
    hcd_dcache_invalidate(p_qhd, sizeof(ehci_qhd_t));
    qhd_xfer_error_isr( p_qhd );
    p_qhd = qhd_next(p_qhd);
  }while(p_qhd != async_head); // async list traversal, stop if loop around

  //------------- TODO refractor period list -------------//
  uint32_t const period_1ms_addr = (uint32_t) get_period_head(rhport, 1u);
  for (uint32_t interval_ms=1; interval_ms <= FRAMELIST_SIZE; interval_ms *= 2)
  {
    ehci_link_t next_item = * get_period_head(rhport, interval_ms);

    // TODO abstract max loop guard for period
    while( !next_item.terminate &&
           !(interval_ms > 1 && period_1ms_addr == tu_align32(next_item.address)) )
    {
      switch ( next_item.type )
      {
        case EHCI_QTYPE_QHD:
        {
          ehci_qhd_t *p_qhd_int = (ehci_qhd_t *) tu_align32(next_item.address);
          hcd_dcache_invalidate(p_qhd_int, sizeof(ehci_qhd_t));

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
void hcd_int_handler(uint8_t rhport)
{
  ehci_registers_t* regs = ehci_data.regs;
  uint32_t const int_status = regs->status;

  if (int_status & EHCI_INT_MASK_HC_HALTED) {
    // something seriously wrong, maybe forget to flush/invalidate cache
    TU_BREAKPOINT();
    TU_LOG1("  HC halted\n");
    return;
  }

  if (int_status & EHCI_INT_MASK_FRAMELIST_ROLLOVER) {
    ehci_data.uframe_number += (FRAMELIST_SIZE << 3);
    regs->status = EHCI_INT_MASK_FRAMELIST_ROLLOVER; // Acknowledge
  }

  if (int_status & EHCI_INT_MASK_PORT_CHANGE) {
    // Including: Force port resume, over-current change, enable/disable change and connect status change.
    uint32_t const port_status = regs->portsc & EHCI_PORTSC_MASK_W1C;
    print_portsc(regs);

    if (regs->portsc_bm.connect_status_change) {
      port_connect_status_change_isr(rhport);
    }

    regs->portsc |= port_status; // Acknowledge change bits in portsc
    regs->status = EHCI_INT_MASK_PORT_CHANGE; // Acknowledge
  }

  if (int_status & EHCI_INT_MASK_ERROR) {
    xfer_error_isr(rhport);
    regs->status = EHCI_INT_MASK_ERROR; // Acknowledge
  }

  //------------- some QTD/SITD/ITD with IOC set is completed -------------//
  if (int_status & EHCI_INT_MASK_NXP_ASYNC) {
    async_list_xfer_complete_isr(qhd_async_head(rhport));
    regs->status = EHCI_INT_MASK_NXP_ASYNC; // Acknowledge
  }

  if (int_status & EHCI_INT_MASK_NXP_PERIODIC)
  {
    for (uint32_t i=1; i <= FRAMELIST_SIZE; i *= 2)
    {
      period_list_xfer_complete_isr(rhport, i);
    }
    regs->status = EHCI_INT_MASK_NXP_PERIODIC; // Acknowledge
  }

  if (int_status & EHCI_INT_MASK_USB) {
    // TODO standard EHCI xfer complete
    regs->status = EHCI_INT_MASK_USB; // Acknowledge
  }

  //------------- There is some removed async previously -------------//
  // need to place after EHCI_INT_MASK_NXP_ASYNC
  if (int_status & EHCI_INT_MASK_ASYNC_ADVANCE) {
    async_advance_isr(rhport);
    regs->status = EHCI_INT_MASK_ASYNC_ADVANCE; // Acknowledge
  }
}

//--------------------------------------------------------------------+
// HELPER
//--------------------------------------------------------------------+


//------------- queue head helper -------------//
static inline ehci_qhd_t* qhd_find_free (void)
{
  for (uint32_t i=0; i<QHD_MAX; i++)
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

  for(uint32_t i=0; i<QHD_MAX; i++)
  {
    if ( (qhd_pool[i].dev_addr == dev_addr) &&
          ep_addr == tu_edpt_addr(qhd_pool[i].ep_number, qhd_pool[i].pid) )
    {
      return &qhd_pool[i];
    }
  }

  return NULL;
}

static void qhd_init(ehci_qhd_t *p_qhd, uint8_t dev_addr, tusb_desc_endpoint_t const * ep_desc)
{
  // address 0 is used as async head, which always on the list --> cannot be cleared (ehci halted otherwise)
  if (dev_addr != 0) {
    tu_memclr(p_qhd, sizeof(ehci_qhd_t));
  }

  hcd_devtree_info_t devtree_info;
  hcd_devtree_get_info(dev_addr, &devtree_info);

  uint8_t const xfer_type = ep_desc->bmAttributes.xfer;
  uint8_t const interval = ep_desc->bInterval;

  p_qhd->dev_addr           = dev_addr;
  p_qhd->fl_inactive_next_xact = 0;
  p_qhd->ep_number          = tu_edpt_number(ep_desc->bEndpointAddress);
  p_qhd->ep_speed           = devtree_info.speed;
  p_qhd->data_toggle_control= (xfer_type == TUSB_XFER_CONTROL) ? 1 : 0;
  p_qhd->head_list_flag     = (dev_addr == 0) ? 1 : 0; // addr0's endpoint is the static asyn list head
  p_qhd->max_packet_size    = tu_edpt_packet_size(ep_desc);
  p_qhd->fl_ctrl_ep_flag    = ((xfer_type == TUSB_XFER_CONTROL) && (p_qhd->ep_speed != TUSB_SPEED_HIGH))  ? 1 : 0;
  p_qhd->nak_reload         = 0;

  // Bulk/Control -> smask = cmask = 0
  // TODO Isochronous
  if (TUSB_XFER_INTERRUPT == xfer_type)
  {
    if (TUSB_SPEED_HIGH == p_qhd->ep_speed)
    {
      TU_ASSERT( interval <= 16, );
      if ( interval < 4) // sub millisecond interval
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

  p_qhd->fl_hub_addr  = devtree_info.hub_addr;
  p_qhd->fl_hub_port  = devtree_info.hub_port;
  p_qhd->mult         = 1; // TODO not use high bandwidth/park mode yet

  //------------- HCD Management Data -------------//
  p_qhd->used         = 1;
  p_qhd->removing     = 0;
  p_qhd->attached_qtd = NULL;
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

static void qhd_attach_qtd(ehci_qhd_t *qhd, ehci_qtd_t *qtd) {
  qhd->attached_qtd = qtd;
  qhd->attached_buffer = qtd->buffer[0];

  // clean and invalidate cache before physically write
  hcd_dcache_clean_invalidate(qtd, sizeof(ehci_qtd_t));

  qhd->qtd_overlay.next.address = (uint32_t) qtd;
  hcd_dcache_clean_invalidate(qhd, sizeof(ehci_qhd_t));
}


//------------- TD helper -------------//
static inline ehci_qtd_t *qtd_find_free(void) {
  for (uint32_t i = 0; i < QTD_MAX; i++) {
    if (!ehci_data.qtd_pool[i].used) return &ehci_data.qtd_pool[i];
  }
  return NULL;
}

static void qtd_init(ehci_qtd_t* qtd, void const* buffer, uint16_t total_bytes)
{
  tu_memclr(qtd, sizeof(ehci_qtd_t));
  qtd->used                = 1;

  qtd->next.terminate      = 1; // init to null
  qtd->alternate.terminate = 1; // not used, always set to terminated
  qtd->active              = 1;
  qtd->err_count           = 3; // TODO 3 consecutive errors tolerance
  qtd->data_toggle         = 0;
  qtd->int_on_complete     = 1;
  qtd->total_bytes         = total_bytes;
  qtd->expected_bytes      = total_bytes;

  qtd->buffer[0] = (uint32_t) buffer;
  for(uint8_t i=1; i<5; i++)
  {
    qtd->buffer[i] |= tu_align4k(qtd->buffer[i - 1] ) + 4096;
  }
}

//------------- List Managing Helper -------------//

// insert at head
static inline void list_insert(ehci_link_t *current, ehci_link_t *new, uint8_t new_type)
{
  new->address = current->address;
  current->address = ((uint32_t) new) | (new_type << 1);
}

static inline ehci_link_t* list_next(ehci_link_t const *p_link)
{
  return (ehci_link_t*) tu_align32(p_link->address);
}

#endif

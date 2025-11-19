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

#if CFG_TUH_ENABLED && defined(TUP_USBIP_OHCI)

#ifndef TUP_OHCI_RHPORTS
#error  OHCI is enabled, but TUP_OHCI_RHPORTS is not defined.
#endif

//--------------------------------------------------------------------+
// INCLUDE
//--------------------------------------------------------------------+
#include "osal/osal.h"

#include "host/hcd.h"
#include "host/usbh.h"
#include "ohci.h"

#if defined(TUP_USBIP_OHCI_NXP)
  #include "ohci_nxp.h"
#else
  #error Unsupported OHCI IP
#endif

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+


enum {
  OHCI_CONTROL_FUNCSTATE_RESET = 0,
  OHCI_CONTROL_FUNCSTATE_RESUME,
  OHCI_CONTROL_FUNCSTATE_OPERATIONAL,
  OHCI_CONTROL_FUNCSTATE_SUSPEND
};

enum {
  OHCI_CONTROL_CONTROL_BULK_RATIO           = 3, ///< This specifies the service ratio between Control and Bulk EDs. 0 = 1:1, 3 = 4:1
  OHCI_CONTROL_LIST_PERIODIC_ENABLE_MASK    = TU_BIT(2),
  OHCI_CONTROL_LIST_ISOCHRONOUS_ENABLE_MASK = TU_BIT(3),
  OHCI_CONTROL_LIST_CONTROL_ENABLE_MASK     = TU_BIT(4),
  OHCI_CONTROL_LIST_BULK_ENABLE_MASK        = TU_BIT(5),
};

enum {
  OHCI_FMINTERVAL_FI    = 0x2EDF, // 7.3.1 nominal (reset) value
  OHCI_FMINTERVAL_FSMPS = (6*(OHCI_FMINTERVAL_FI-210)) / 7, // 5.4 calculated based on maximum overhead + bit stuffing
};

enum {
  OHCI_PERIODIC_START = 0x3E67
};

enum {
  OHCI_INT_SCHEDULING_OVERUN_MASK    = TU_BIT(0),
  OHCI_INT_WRITEBACK_DONEHEAD_MASK   = TU_BIT(1),
  OHCI_INT_SOF_MASK                  = TU_BIT(2),
  OHCI_INT_RESUME_DETECTED_MASK      = TU_BIT(3),
  OHCI_INT_UNRECOVERABLE_ERROR_MASK  = TU_BIT(4),
  OHCI_INT_FRAME_OVERFLOW_MASK       = TU_BIT(5),
  OHCI_INT_RHPORT_STATUS_CHANGE_MASK = TU_BIT(6),

  OHCI_INT_OWNERSHIP_CHANGE_MASK     = TU_BIT(30),
  OHCI_INT_MASTER_ENABLE_MASK        = TU_BIT(31),
};

enum {
  RHPORT_CURRENT_CONNECT_STATUS_MASK      = TU_BIT(0),
  RHPORT_PORT_ENABLE_STATUS_MASK          = TU_BIT(1),
  RHPORT_PORT_SUSPEND_STATUS_MASK         = TU_BIT(2),
  RHPORT_PORT_OVER_CURRENT_INDICATOR_MASK = TU_BIT(3),
  RHPORT_PORT_RESET_STATUS_MASK           = TU_BIT(4), ///< write '1' to reset port

  RHPORT_PORT_POWER_STATUS_MASK           = TU_BIT(8),
  RHPORT_LOW_SPEED_DEVICE_ATTACHED_MASK   = TU_BIT(9),

  RHPORT_CONNECT_STATUS_CHANGE_MASK       = TU_BIT(16),
  RHPORT_PORT_ENABLE_CHANGE_MASK          = TU_BIT(17),
  RHPORT_PORT_SUSPEND_CHANGE_MASK         = TU_BIT(18),
  RHPORT_OVER_CURRENT_CHANGE_MASK         = TU_BIT(19),
  RHPORT_PORT_RESET_CHANGE_MASK           = TU_BIT(20),

  RHPORT_ALL_CHANGE_MASK = RHPORT_CONNECT_STATUS_CHANGE_MASK | RHPORT_PORT_ENABLE_CHANGE_MASK |
    RHPORT_PORT_SUSPEND_CHANGE_MASK | RHPORT_OVER_CURRENT_CHANGE_MASK | RHPORT_PORT_RESET_CHANGE_MASK
};

enum {
  OHCI_CCODE_NO_ERROR              = 0,
  OHCI_CCODE_CRC                   = 1,
  OHCI_CCODE_BIT_STUFFING          = 2,
  OHCI_CCODE_DATA_TOGGLE_MISMATCH  = 3,
  OHCI_CCODE_STALL                 = 4,
  OHCI_CCODE_DEVICE_NOT_RESPONDING = 5,
  OHCI_CCODE_PID_CHECK_FAILURE     = 6,
  OHCI_CCODE_UNEXPECTED_PID        = 7,
  OHCI_CCODE_DATA_OVERRUN          = 8,
  OHCI_CCODE_DATA_UNDERRUN         = 9,
  OHCI_CCODE_BUFFER_OVERRUN        = 12,
  OHCI_CCODE_BUFFER_UNDERRUN       = 13,
  OHCI_CCODE_NOT_ACCESSED          = 14,
};

enum {
  OHCI_INT_ON_COMPLETE_YES = 0,
  OHCI_INT_ON_COMPLETE_NO  = 0x7 // 0b111
};

enum {
  GTD_DT_TOGGLE_CARRY = 0,
  GTD_DT_DATA0 = TU_BIT(1) | 0,
  GTD_DT_DATA1 = TU_BIT(1) | 1,
};

enum {
  PID_SETUP = 0,
  PID_OUT,
  PID_IN,
};

enum {
  PID_FROM_TD = 0,
};

//--------------------------------------------------------------------+
// Support for explicit D-cache operations
//--------------------------------------------------------------------+
TU_ATTR_WEAK bool hcd_dcache_clean(void const* addr, uint32_t data_size) { (void) addr; (void) data_size; return true; }
TU_ATTR_WEAK bool hcd_dcache_invalidate(void const* addr, uint32_t data_size) { (void) addr; (void) data_size; return true; }

// Optional macro to access ED in uncached way
#ifndef hcd_dcache_uncached
#define hcd_dcache_uncached(x) (x)
#endif

//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+
CFG_TUH_MEM_SECTION TU_ATTR_ALIGNED(256) static ohci_data_t ohci_data;

static ohci_ed_t * const p_ed_head[] = {
    [TUSB_XFER_CONTROL]     = hcd_dcache_uncached(&ohci_data.control[0].ed),
    [TUSB_XFER_BULK   ]     = hcd_dcache_uncached(&ohci_data.bulk_head_ed),
    [TUSB_XFER_INTERRUPT]   = hcd_dcache_uncached(&ohci_data.period_head_ed),
    [TUSB_XFER_ISOCHRONOUS] = NULL // TODO Isochronous
};

static void ed_list_insert(ohci_ed_t * p_pre, ohci_ed_t * p_ed);
static void ed_list_remove_by_addr(ohci_ed_t * p_head, uint8_t dev_addr);
static gtd_extra_data_t *gtd_get_extra_data(ohci_gtd_t const * const gtd);
static ohci_ed_t* ed_from_addr(uint8_t dev_addr, uint8_t ep_addr);

TU_ATTR_ALWAYS_INLINE static inline ohci_ed_t* ed_control(uint8_t daddr) {
  return hcd_dcache_uncached(&ohci_data.control[daddr].ed);
}

//--------------------------------------------------------------------+
// USBH-HCD API
//--------------------------------------------------------------------+

// If your system requires separation of virtual and physical memory, implement
// tusb_app_virt_to_phys and tusb_app_virt_to_phys in your application.
TU_ATTR_ALWAYS_INLINE static inline void *_phys_addr(void *virtual_address) {
  return tusb_app_virt_to_phys(virtual_address);
}

TU_ATTR_ALWAYS_INLINE static inline void *_virt_addr(void *physical_address) {
  return tusb_app_phys_to_virt(physical_address);
}

// Initialization according to 5.1.1.4
bool hcd_init(uint8_t rhport, const tusb_rhport_init_t* rh_init) {
  (void) rhport;
  (void) rh_init;

  ohci_phy_init(rhport);

  //------------- Data Structure init -------------//
  tu_memclr(&ohci_data, sizeof(ohci_data_t));
  // assign all interrupt pointers to period head ed
  for(uint8_t i=0; i<32; i++) {
    ohci_data.hcca.interrupt_table[i] = (uint32_t) _phys_addr(&ohci_data.period_head_ed);
  }

  ohci_data.control[0].ed.w0.skip  = 1;
  ohci_data.bulk_head_ed.w0.skip   = 1;
  ohci_data.period_head_ed.w0.skip = 1;

  //If OHCI hardware is in SMM mode, gain ownership (Ref OHCI spec 5.1.1.3.3)
  if (OHCI_REG->control_bit.interrupt_routing == 1) {
    OHCI_REG->command_status_bit.ownership_change_request = 1;
    while (OHCI_REG->control_bit.interrupt_routing == 1) {}
  } else if (OHCI_REG->control_bit.hc_functional_state != OHCI_CONTROL_FUNCSTATE_RESET &&
             OHCI_REG->control_bit.hc_functional_state != OHCI_CONTROL_FUNCSTATE_OPERATIONAL) {
    //If OHCI hardware has come from warm-boot, signal resume (Ref OHCI spec 5.1.1.3.4)
    //Wait 20 ms. (Ref Usb spec 7.1.7.7)
    OHCI_REG->control_bit.hc_functional_state = OHCI_CONTROL_FUNCSTATE_RESUME;
    tusb_time_delay_ms_api(20);
  }

  hcd_dcache_clean(&ohci_data, sizeof(ohci_data));

  // reset controller
  OHCI_REG->command_status_bit.controller_reset = 1;
  while( OHCI_REG->command_status_bit.controller_reset ) {} // should not take longer than 10 us

  //------------- init ohci registers -------------//
  OHCI_REG->control_head_ed = (uint32_t) _phys_addr(&ohci_data.control[0].ed);
  OHCI_REG->bulk_head_ed    = (uint32_t) _phys_addr(&ohci_data.bulk_head_ed);
  OHCI_REG->hcca            = (uint32_t) _phys_addr(&ohci_data.hcca);

  OHCI_REG->interrupt_disable = OHCI_REG->interrupt_enable; // disable all interrupts
  OHCI_REG->interrupt_status  = OHCI_REG->interrupt_status; // clear current set bits
  OHCI_REG->interrupt_enable  = OHCI_INT_WRITEBACK_DONEHEAD_MASK | OHCI_INT_RESUME_DETECTED_MASK |
      OHCI_INT_UNRECOVERABLE_ERROR_MASK | OHCI_INT_FRAME_OVERFLOW_MASK | OHCI_INT_RHPORT_STATUS_CHANGE_MASK |
      OHCI_INT_MASTER_ENABLE_MASK;

  OHCI_REG->control = OHCI_CONTROL_CONTROL_BULK_RATIO | OHCI_CONTROL_LIST_CONTROL_ENABLE_MASK |
       OHCI_CONTROL_LIST_BULK_ENABLE_MASK | OHCI_CONTROL_LIST_PERIODIC_ENABLE_MASK; // TODO Isochronous

  OHCI_REG->frame_interval = (OHCI_FMINTERVAL_FSMPS << 16) | OHCI_FMINTERVAL_FI;
  OHCI_REG->frame_interval ^= (1 << 31); //Must toggle when frame_interval is updated.
  OHCI_REG->periodic_start = (OHCI_FMINTERVAL_FI * 9) / 10; // Periodic start is 90% of frame interval

  OHCI_REG->control_bit.hc_functional_state = OHCI_CONTROL_FUNCSTATE_OPERATIONAL; // make HC's state to operational state TODO use this to suspend (save power)
  OHCI_REG->rh_status_bit.local_power_status_change = 1; // set global power for ports

  tusb_time_delay_ms_api(OHCI_REG->rh_descriptorA_bit.power_on_to_good_time * 2); // Wait POTG after power up

  return true;
}

uint32_t hcd_frame_number(uint8_t rhport)
{
  (void) rhport;
  return (ohci_data.frame_number_hi << 16) | OHCI_REG->frame_number;
}

//--------------------------------------------------------------------+
// PORT API
//--------------------------------------------------------------------+
void hcd_port_reset(uint8_t hostid)
{
  OHCI_REG->rhport_status[hostid] = RHPORT_PORT_RESET_STATUS_MASK;
}

void hcd_port_reset_end(uint8_t rhport)
{
  (void) rhport;
}

bool hcd_port_connect_status(uint8_t hostid)
{
  return OHCI_REG->rhport_status_bit[hostid].current_connect_status;
}

tusb_speed_t hcd_port_speed_get(uint8_t hostid)
{
  return OHCI_REG->rhport_status_bit[hostid].low_speed_device_attached ? TUSB_SPEED_LOW : TUSB_SPEED_FULL;
}

// endpoints are tied to an address, which only reclaim after a long delay when enumerating
// thus there is no need to make sure ED is not in HC's cahed as it will not for sure
void hcd_device_close(uint8_t rhport, uint8_t dev_addr) {
  // TODO OHCI
  (void) rhport;

  // addr0 serves as static head --> only set skip bit
  if (dev_addr == 0) {
    ohci_ed_t* ed = ed_control(0);
    ed->w0.skip = 1;
  } else {
    ed_list_remove_by_addr(p_ed_head[TUSB_XFER_CONTROL], dev_addr); // remove control
    ed_list_remove_by_addr(p_ed_head[TUSB_XFER_BULK], dev_addr); // remove bulk
    ed_list_remove_by_addr(p_ed_head[TUSB_XFER_INTERRUPT], dev_addr); // remove interrupt
    // TODO remove ISO
  }
}

//--------------------------------------------------------------------+
// Controller API
//--------------------------------------------------------------------+

//--------------------------------------------------------------------+
// List Helper
//--------------------------------------------------------------------+
static inline tusb_xfer_type_t ed_get_xfer_type(ohci_ed_word0_t w0) {
  return (w0.ep_number == 0   ) ? TUSB_XFER_CONTROL     :
         (w0.is_iso           ) ? TUSB_XFER_ISOCHRONOUS :
         (w0.is_interrupt_xfer) ? TUSB_XFER_INTERRUPT   : TUSB_XFER_BULK;
}

static void ed_init(ohci_ed_t *p_ed, uint8_t dev_addr, uint16_t ep_size, uint8_t ep_addr, uint8_t xfer_type, uint8_t interval) {
  (void) interval;

  // address 0 is used as async head, which always on the list --> cannot be cleared
  if (dev_addr != 0) {
    p_ed->td_tail = 0;
    p_ed->td_head.address = 0;
    p_ed->next = 0;
  }

  tuh_bus_info_t bus_info;
  tuh_bus_info_get(dev_addr, &bus_info);

  ohci_ed_word0_t w0 = {.value = 0};
  w0.dev_addr          = dev_addr;
  w0.ep_number         = ep_addr & 0x0F;
  w0.pid               = (xfer_type == TUSB_XFER_CONTROL) ? PID_FROM_TD : (tu_edpt_dir(ep_addr) ? PID_IN : PID_OUT);
  w0.speed             = bus_info.speed;
  w0.is_iso            = (xfer_type == TUSB_XFER_ISOCHRONOUS) ? 1 : 0;
  w0.max_packet_size   = ep_size;

  w0.used              = 1;
  w0.is_interrupt_xfer = (xfer_type == TUSB_XFER_INTERRUPT ? 1 : 0);
  p_ed->w0 = w0;
}

static void gtd_init(ohci_gtd_t *p_td, uint8_t *data_ptr, uint16_t total_bytes) {
  tu_memclr(p_td, sizeof(ohci_gtd_t));

  p_td->used = 1;
  gtd_get_extra_data(p_td)->expected_bytes = total_bytes;

  p_td->buffer_rounding = 1; // less than queued length is not a error
  p_td->delay_interrupt = OHCI_INT_ON_COMPLETE_NO;
  p_td->condition_code = OHCI_CCODE_NOT_ACCESSED;

  uint8_t *cbp = (uint8_t *) _phys_addr(data_ptr);

  p_td->current_buffer_pointer = cbp;
  if ( total_bytes ) {
    p_td->buffer_end = _phys_addr(data_ptr + total_bytes - 1);
  } else {
    p_td->buffer_end = cbp;
  }
}

static ohci_ed_t* ed_from_addr(uint8_t dev_addr, uint8_t ep_addr) {
  if (tu_edpt_number(ep_addr) == 0) {
    return ed_control(dev_addr);
  }

  ohci_ed_t* ed_pool = ohci_data.ed_pool;
  for (size_t i = 0; i < ED_MAX; i++) {
    ohci_ed_t* qhd = hcd_dcache_uncached(&ed_pool[i]);
    if ((qhd->w0.dev_addr == dev_addr) &&
        ep_addr == tu_edpt_addr(qhd->w0.ep_number, qhd->w0.pid == PID_IN)) {
      return qhd;
    }
  }

  return NULL;
}

static ohci_ed_t* ed_find_free(void) {
  ohci_ed_t* ed_pool = ohci_data.ed_pool;
  for (size_t i = 0; i < ED_MAX; i++) {
    ohci_ed_t* qhd = hcd_dcache_uncached(&ed_pool[i]);
    if (!qhd->w0.used) {
      return qhd;
    }
  }
  return NULL;
}

static void ed_list_insert(ohci_ed_t * p_pre, ohci_ed_t * p_ed) {
  p_ed->next = p_pre->next;
  p_pre->next = (uint32_t) _phys_addr(p_ed);
}

static void ed_list_remove_by_addr(ohci_ed_t * p_head, uint8_t dev_addr) {
  ohci_ed_t* p_prev = p_head;

  while (p_prev->next) {
    ohci_ed_t* ed = (ohci_ed_t*)_virt_addr((void*)p_prev->next);

    if (ed->w0.dev_addr == dev_addr) {
      // Prevent Host Controller from processing this ED while we remove it
      ed->w0.skip = 1;

      // unlink ed, will also move up p_prev
      p_prev->next = ed->next;

      // point the removed ED's next pointer to list head to make sure HC can always safely move away from this ED
      ed->next = (uint32_t)_phys_addr(p_head);
      ed->w0.used = 0;
      ed->w0.skip = 0;
    } else {
      p_prev = (ohci_ed_t*)_virt_addr((void*)p_prev->next);
    }
  }
}

static ohci_gtd_t* gtd_find_free(void) {
  for (uint8_t i = 0; i < GTD_MAX; i++) {
    if (!ohci_data.gtd_pool[i].used) {
      return &ohci_data.gtd_pool[i];
    }
  }
  return NULL;
}

//--------------------------------------------------------------------+
// Endpoint API
//--------------------------------------------------------------------+
bool hcd_edpt_open(uint8_t rhport, uint8_t dev_addr, tusb_desc_endpoint_t const* ep_desc) {
  (void)rhport;

  // TODO iso support
  TU_ASSERT(ep_desc->bmAttributes.xfer != TUSB_XFER_ISOCHRONOUS);

  //------------- Prepare Queue Head -------------//
  ohci_ed_t* p_ed;
  if (ep_desc->bEndpointAddress == 0) {
    p_ed = ed_control(dev_addr);
  } else {
    p_ed = ed_find_free();
  }
  TU_ASSERT(p_ed);

  ed_init(p_ed, dev_addr, tu_edpt_packet_size(ep_desc), ep_desc->bEndpointAddress,
          ep_desc->bmAttributes.xfer, ep_desc->bInterval);

  // control of dev0 is used as static async head
  if (dev_addr == 0) {
    p_ed->w0.skip = 0; // only need to clear skip bit
    return true;
  }

  if (tu_edpt_number(ep_desc->bEndpointAddress) != 0) {
    // Get an empty TD and use it as the end-of-list marker.
    // This marker TD will be used when a transfer is made on this EP
    // (and a new, empty TD will be allocated for the next-next transfer).
    ohci_gtd_t* gtd = gtd_find_free();
    TU_ASSERT(gtd);
    p_ed->td_head.address = (uint32_t)_phys_addr(gtd);
    p_ed->td_tail = (uint32_t)_phys_addr(gtd);
  }

  ed_list_insert(p_ed_head[ep_desc->bmAttributes.xfer], p_ed);
  return true;
}

bool hcd_edpt_close(uint8_t rhport, uint8_t daddr, uint8_t ep_addr) {
  (void) rhport; (void) daddr; (void) ep_addr;
  return false; // TODO not implemented yet
}

bool hcd_setup_send(uint8_t rhport, uint8_t dev_addr, uint8_t const setup_packet[8]) {
  (void) rhport;

  ohci_ed_t* ed   = ed_control(dev_addr);
  ohci_gtd_t *qtd = &ohci_data.control[dev_addr].gtd;

  hcd_dcache_clean(setup_packet, 8);

  gtd_init(qtd, (uint8_t*)(uintptr_t) setup_packet, 8);
  qtd->index           = dev_addr;
  qtd->pid             = PID_SETUP;
  qtd->data_toggle     = GTD_DT_DATA0;
  qtd->delay_interrupt = OHCI_INT_ON_COMPLETE_YES;
  hcd_dcache_clean(qtd, sizeof(ohci_gtd_t));

  //------------- Attach TDs list to Control Endpoint -------------//
  ed->td_head.address = (uint32_t) _phys_addr(qtd);

  OHCI_REG->command_status_bit.control_list_filled = 1;

  return true;
}

bool hcd_edpt_xfer(uint8_t rhport, uint8_t dev_addr, uint8_t ep_addr, uint8_t * buffer, uint16_t buflen) {
  (void) rhport;

  uint8_t const epnum = tu_edpt_number(ep_addr);
  uint8_t const dir   = tu_edpt_dir(ep_addr);

  // IN transfer: invalidate buffer, OUT transfer: clean buffer
  if (dir) {
    hcd_dcache_invalidate(buffer, buflen);
  } else {
    hcd_dcache_clean(buffer, buflen);
  }

  ohci_ed_t * ed = ed_from_addr(dev_addr, ep_addr);
  TU_ASSERT(ed);
  if (epnum == 0) {
    ohci_gtd_t* gtd = &ohci_data.control[dev_addr].gtd;
    gtd_init(gtd, buffer, buflen);
    gtd->index           = dev_addr;
    gtd->pid = dir ? PID_IN : PID_OUT;
    gtd->data_toggle = GTD_DT_DATA1; // Both Data and Ack stage start with DATA1
    gtd->delay_interrupt = OHCI_INT_ON_COMPLETE_YES;
    hcd_dcache_clean(gtd, sizeof(ohci_gtd_t));

    ed->td_head.address = (uint32_t)_phys_addr(gtd);

    OHCI_REG->command_status_bit.control_list_filled = 1;
  } else {
    tusb_xfer_type_t xfer_type = ed_get_xfer_type(ed->w0);
    ohci_gtd_t* gtd = (ohci_gtd_t*)_virt_addr((void*)ed->td_tail);

    gtd_init(gtd, buffer, buflen);
    gtd->index = ed-ohci_data.ed_pool;
    gtd->delay_interrupt = OHCI_INT_ON_COMPLETE_YES;

    // Insert a new, empty TD at the tail, to be used by the next transfer
    ohci_gtd_t* new_gtd = gtd_find_free();
    TU_ASSERT(new_gtd);

    gtd->next = (uint32_t)_phys_addr(new_gtd);
    hcd_dcache_clean(gtd, sizeof(ohci_gtd_t));

    ed->td_tail = (uint32_t)_phys_addr(new_gtd);

    if (TUSB_XFER_BULK == xfer_type) {
      OHCI_REG->command_status_bit.bulk_list_filled = 1;
    }
  }

  return true;
}

bool hcd_edpt_abort_xfer(uint8_t rhport, uint8_t dev_addr, uint8_t ep_addr) {
  (void) rhport;
  (void) dev_addr;
  (void) ep_addr;
  // TODO not implemented yet
  return false;
}

bool hcd_edpt_clear_stall(uint8_t rhport, uint8_t dev_addr, uint8_t ep_addr) {
  (void) rhport;
  ohci_ed_t * const p_ed = ed_from_addr(dev_addr, ep_addr);

  ohci_ed_word2_t td_head = p_ed->td_head;
  td_head.toggle = 0; // reset data toggle
  td_head.halted = 0;
  p_ed->td_head = td_head;

  if (TUSB_XFER_BULK == ed_get_xfer_type(p_ed->w0)) {
    OHCI_REG->command_status_bit.bulk_list_filled = 1;
  }

  return true;
}


//--------------------------------------------------------------------+
// OHCI Interrupt Handler
//--------------------------------------------------------------------+
TU_ATTR_ALWAYS_INLINE static inline bool is_itd(ohci_td_item_t* item) {
  (void) item;
  return false; // ISO not supported yet
}

static ohci_td_item_t* list_reverse(ohci_td_item_t* td_head) {
  ohci_td_item_t* td_reverse_head = NULL;
  while(td_head != NULL) {
    td_head = _virt_addr(td_head);
    const uint32_t item_size = is_itd(td_head) ? sizeof(ohci_itd_t) : sizeof(ohci_gtd_t);
    hcd_dcache_invalidate(td_head, item_size);
    const uint32_t next = td_head->next;

    // make current's item become reverse's first item
    td_head->next = (uint32_t) td_reverse_head;
    td_reverse_head  = _phys_addr(td_head);

    td_head = (ohci_td_item_t*) next; // advance to next item
  }

  return _virt_addr(td_reverse_head);
}

TU_ATTR_ALWAYS_INLINE static inline bool gtd_is_control(ohci_gtd_t const * const p_qtd) {
  return ((uint32_t) p_qtd) < ((uint32_t) ohci_data.gtd_pool); // check ohci_data_t for memory layout
}

TU_ATTR_ALWAYS_INLINE static inline ohci_ed_t* gtd_get_ed(ohci_gtd_t const* const p_qtd) {
  ohci_ed_t* ed;
  if (gtd_is_control(p_qtd)) {
    ed = &ohci_data.control[p_qtd->index].ed;
  } else {
    ed = &ohci_data.ed_pool[p_qtd->index];
  }
  return hcd_dcache_uncached(ed);
}

static gtd_extra_data_t *gtd_get_extra_data(ohci_gtd_t const * const gtd) {
  if (gtd_is_control(gtd)) {
    uint8_t idx = ((uintptr_t)gtd - (uintptr_t)&ohci_data.control->gtd) / sizeof(ohci_data.control[0]);
    return &ohci_data.gtd_extra_control[idx];
  }else {
    return &ohci_data.gtd_extra[gtd - ohci_data.gtd_pool];
  }
}

TU_ATTR_ALWAYS_INLINE static inline uint32_t gtd_xfer_byte_left(uint32_t buffer_end, uint32_t current_buffer) {
  // 5.2.9 OHCI sample code
  // CBP is 0 mean all data is transferred
  if (current_buffer == 0) {
    return 0;
  }

  return (tu_align4k(buffer_end ^ current_buffer) ? 0x1000 : 0) +
         tu_offset4k(buffer_end) - tu_offset4k(current_buffer) + 1;
}

static void done_queue_isr(uint8_t hostid) {
  (void)hostid;

  // done head is written in reversed order of completion --> need to reverse the done queue first
  ohci_td_item_t* td_head = list_reverse((ohci_td_item_t*)tu_align16(ohci_data.hcca.done_head));
  ohci_data.hcca.done_head = 0;

  while (td_head != NULL) {
    // TODO check if td_head is iso td
    //------------- Non ISO transfer -------------//
    ohci_gtd_t* const qtd = (ohci_gtd_t*) td_head;
    xfer_result_t const event = (qtd->condition_code == OHCI_CCODE_NO_ERROR) ? XFER_RESULT_SUCCESS :
                                (qtd->condition_code == OHCI_CCODE_STALL) ? XFER_RESULT_STALLED : XFER_RESULT_FAILED;
    qtd->used = 0; // free TD
    if ((qtd->delay_interrupt == OHCI_INT_ON_COMPLETE_YES) || (event != XFER_RESULT_SUCCESS)) {
      const ohci_ed_t* ed = gtd_get_ed(qtd);
      const ohci_ed_word0_t ed_w0 = ed->w0;
      const uint32_t xferred_bytes = gtd_get_extra_data(qtd)->expected_bytes - gtd_xfer_byte_left((uint32_t)qtd->buffer_end, (uint32_t)qtd->current_buffer_pointer);
      uint8_t dir = (ed_w0.ep_number == 0) ? (qtd->pid == PID_IN) : (ed_w0.pid == PID_IN);
      const uint8_t ep_addr = tu_edpt_addr(ed_w0.ep_number, dir);
      hcd_event_xfer_complete(ed_w0.dev_addr, ep_addr, xferred_bytes, event, true);
    }

    td_head = (ohci_td_item_t*)_virt_addr((void*)td_head->next);
  }
}

void hcd_int_handler(uint8_t hostid, bool in_isr) {
  (void)in_isr;
  uint32_t const int_en = OHCI_REG->interrupt_enable;
  uint32_t const int_status = OHCI_REG->interrupt_status & int_en;

  if (int_status == 0) {
    return;
  }

  // Disable MIE as per OHCI spec 5.3
  OHCI_REG->interrupt_disable = OHCI_INT_MASTER_ENABLE_MASK;

  // Frame number overflow
  if (int_status & OHCI_INT_FRAME_OVERFLOW_MASK) {
    ohci_data.frame_number_hi++;
  }

  //------------- RootHub status -------------//
  if (int_status & OHCI_INT_RHPORT_STATUS_CHANGE_MASK) {
    for (int i = 0; i < TUP_OHCI_RHPORTS; i++) {
      uint32_t const rhport_status = OHCI_REG->rhport_status[i] & RHPORT_ALL_CHANGE_MASK;
      if (rhport_status & RHPORT_CONNECT_STATUS_CHANGE_MASK) {
        // TODO check if remote wake-up
        if (OHCI_REG->rhport_status_bit[i].current_connect_status) {
          // TODO reset port immediately, without this controller will got 2-3 (debouncing connection status change)
          OHCI_REG->rhport_status[i] = RHPORT_PORT_RESET_STATUS_MASK;
          hcd_event_device_attach(i, true);
        } else {
          hcd_event_device_remove(i, true);
        }
      }

      if (rhport_status & RHPORT_PORT_SUSPEND_CHANGE_MASK) {
      }

      OHCI_REG->rhport_status[i] = rhport_status; // acknowledge all interrupt
    }
  }

  //------------- Transfer Complete -------------//
  if (int_status & OHCI_INT_WRITEBACK_DONEHEAD_MASK) {
    done_queue_isr(hostid);
  }

  OHCI_REG->interrupt_status = int_status; // Acknowledge handled interrupt
  OHCI_REG->interrupt_enable = OHCI_INT_MASTER_ENABLE_MASK; // Enable MIE
}
//--------------------------------------------------------------------+
// HELPER
//--------------------------------------------------------------------+


#endif

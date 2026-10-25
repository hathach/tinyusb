/*
 * SPDX-FileCopyrightText: Copyright (c) 2019 Ha Thach (tinyusb.org)
 * SPDX-License-Identifier: MIT
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
#include "host/usbh.h"
#include "host/usbh_pvt.h"
#if CFG_TUH_HUB
#include "host/hub.h"
#endif
#include "ehci_api.h"
#include "ehci.h"

#ifndef TU_MEMORY_BARRIER
  #error "TU_MEMORY_BARRIER() is not defined for this compiler (see tusb_compiler.h)"
#endif

// Descriptors and the frame list get no cache maintenance. With D-cache in use,
// their section must be confirmed non-cacheable (or coherent) explicitly.
#if CFG_TUH_MEM_DCACHE_ENABLE && defined(TUH_UNCACHED_MEM_SECTION_IMPLICIT)
  #warning "EHCI descriptors use CFG_TUH_MEM_SECTION, which must be non-cacheable; define CFG_TUH_UNCACHED_MEM_SECTION to confirm"
#endif

// NXP specific fixes
#if TU_CHECK_MCU(OPT_MCU_MIMXRT1XXX, OPT_MCU_LPC55, OPT_MCU_MCXN9, OPT_MCU_RW61X)
#include "fsl_device_registers.h"
#endif

#if TU_CHECK_MCU(OPT_MCU_HPM)
#include "ci_hs_hpm.h"
#endif

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+

// Debug level of EHCI
#define EHCI_DBG     2

// Framelist size as small as possible to save SRAM
#ifdef TUP_USBIP_CHIPIDEA_HS
  // NXP Transdimension: 8 elements
  #define FRAMELIST_SIZE_BIT_VALUE      7u
  #define FRAMELIST_SIZE_USBCMD_VALUE   (((FRAMELIST_SIZE_BIT_VALUE &  3) << EHCI_USBCMD_FRAMELIST_SIZE_SHIFT) | \
                                         ((FRAMELIST_SIZE_BIT_VALUE >> 2) << EHCI_USBCMD_CHIPIDEA_FRAMELIST_SIZE_MSB_SHIFT))
#else
  // STD EHCI: 256 elements
  #define FRAMELIST_SIZE_BIT_VALUE      2u
  #define FRAMELIST_SIZE_USBCMD_VALUE   ((FRAMELIST_SIZE_BIT_VALUE &  3) << EHCI_USBCMD_FRAMELIST_SIZE_SHIFT)
#endif

#define FRAMELIST_SIZE                  (1024 >> FRAMELIST_SIZE_BIT_VALUE)

// Total queue head pool. TODO should be user configurable and more optimize memory usage in the future
#define QHD_MAX      (CFG_TUH_DEVICE_MAX*CFG_TUH_ENDPOINT_MAX + CFG_TUH_HUB)
#define QTD_MAX      QHD_MAX

/* Isochronous transfers (ChipIdea HS, CFG_TUH_CHIPIDEA_ISO_ENABLE)
 *
 * Each endpoint has one outstanding transfer covering one service interval
 * and owns one TD: an iTD at HS (up to 3 transactions) or an siTD at FS.
 * Larger requests are rejected. The TD occupies one QH pool slot and the
 * endpoint state one qTD pool slot until close; bulk and interrupt transfers
 * use the rest. Completion unlinks the TD before USBH gets the buffer back,
 * so its callback can submit the next interval.
 *
 * Service intervals are limited to 4 ms, so every submission's slot lies
 * within the 8-frame list and its TD is linked at submission.
 *
 * Scheduling uses no SOF interrupt (ChipIdea raises one every 125 us, even on
 * a FS port). Completions (IOC) drive it; frame-list rollover, once per
 * 8-frame ring, retires slots that passed unvisited and keeps the extended
 * microframe counter current.
 * - Endpoint phase is preserved and late slots are skipped. FRINDEX is
 *   rechecked just before linking, so a delayed submission moves to a later
 *   slot of its phase instead of publishing one the controller may have passed.
 * - ChipIdea caches no descriptors (its isochronous scheduling threshold is
 *   always 0), so a TD is published for the next microframe, one less than
 *   the lead EHCI 4.7.2.1 asks for; this keeps 125 us HS intervals back to
 *   back and is verified on RT1064, not specified.
 * - On a native FS port the counter lags FRINDEX by one frame
 *   (iso_frame_offset). The ChipIdea manual documents only a one-microframe
 *   B/H-frame lag; the frame offset is empirical.
 * - A TD that is still not visited may be retired by rollover only after the
 *   ring repeats; it is then reported failed while its buffer stays owned.
 *   Missed slots, cancellation and close retire a TD only once the controller
 *   cannot be on it (iso_td_idle), without stopping the periodic schedule;
 *   close falls back to a bounded schedule stop. Missed packets are not retried.
 *
 * FS behind a HS hub uses best-effort windows per transaction translator (each
 * port of a multi-TT hub, or all ports of a single-TT hub), reserved by
 * maximum packet size: OUT from H0 upwards, IN from the end with two
 * complete-split retries before H7 ends. No bus-time admission, interrupt
 * reservations or frame-spanning splits; IN is limited
 * to 564 bytes and OUT to 1023. Direct FS uses the embedded TT with the
 * S-mask/C-mask left zero. A single endpoint may use 1023 bytes in either
 * direction, but several share a small per-frame budget (iso_ep_open).
 *
 * CFG_TUH_UNCACHED_MEM_SECTION must be non-cacheable or coherent because TDs
 * are unlinked while the schedule runs; payload buffers keep cache maintenance.
 *
 * References: EHCI 1.0 ch. 3/4; USB 2.0 ch. 11; RT1064 RM Rev. 2 ch. 42;
 * ChipIdea (TDI) HS USB controller core manual.
 */
#if defined(TUP_USBIP_CHIPIDEA_HS) && CFG_TUH_CHIPIDEA_ISO_ENABLE
  #define EHCI_ISO_ENABLED 1
#else
  #define EHCI_ISO_ENABLED 0
#endif

#if EHCI_ISO_ENABLED
// An iTD must not cross a 4 KiB boundary (EHCI chapter 3).
typedef union TU_ATTR_ALIGNED(64) {
  ehci_itd_t itd;
  ehci_sitd_t sitd;
  volatile uint32_t words[16]; // snapshot a hardware status word without repeated bitfield reads
} iso_td_t;

#define ITD_XACT_WORD(slot) (1u + (slot)) // iTD transaction status/control
#define SITD_EP_WORD        1u            // siTD endpoint characteristics
#define SITD_STATE_WORD     3u            // siTD transfer status/control

typedef struct {
  iso_td_t* td;
  uint8_t* buffer;
  uint32_t interval;
  uint32_t uframe; // Current service slot while busy, next slot while idle.
  uint16_t max_xfer_bytes;
  uint16_t buflen;
  uint8_t daddr;
  uint8_t ep_addr;
  uint8_t speed;
  uint8_t split_slot;
  bool busy; // a transfer is in progress and its TD is linked
} iso_ep_t;

TU_VERIFY_STATIC(sizeof(iso_ep_t) <= sizeof(ehci_qtd_t), "ISO state must fit in one qTD slot");
#endif

typedef union {
  ehci_qhd_t qhd;
#if EHCI_ISO_ENABLED
  iso_td_t iso;
#endif
} ehci_ep_t;

typedef union {
  ehci_qtd_t qtd;
#if EHCI_ISO_ENABLED
  iso_ep_t iso;
#endif
} ehci_td_t;

// Exactly one member is non-NULL for an open endpoint.
typedef struct {
  ehci_qhd_t* qhd;
#if EHCI_ISO_ENABLED
  iso_ep_t* iso;
#endif
} ehci_edpt_t;

typedef struct {
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

  ehci_ep_t qhd_pool[QHD_MAX];
  ehci_td_t qtd_pool[QTD_MAX] TU_ATTR_ALIGNED(32);
#if EHCI_ISO_ENABLED
  uint32_t qhd_is_iso[(QHD_MAX + 31) / 32];
  uint32_t qtd_is_iso[(QTD_MAX + 31) / 32];
#endif

#if EHCI_ISO_ENABLED
  uint32_t iso_uframe;
  uint16_t iso_last_frindex;
  uint8_t iso_frame_offset;
#endif

  ehci_registers_t* regs;         // operational register
  ehci_cap_registers_t* cap_regs; // capability register

  volatile uint32_t uframe_number;
}ehci_data_t;

// Live descriptors/links must be uncached or DMA-coherent. The frame list must be 4K aligned.
CFG_TUH_UNCACHED_MEM_SECTION TU_ATTR_ALIGNED(4096) static ehci_data_t ehci_data;

#if EHCI_ISO_ENABLED
TU_ATTR_ALWAYS_INLINE static inline bool qhd_is_iso(size_t index) {
  return (ehci_data.qhd_is_iso[index / 32] & TU_BIT(index % 32)) != 0;
}

TU_ATTR_ALWAYS_INLINE static inline bool qtd_is_iso(size_t index) {
  return (ehci_data.qtd_is_iso[index / 32] & TU_BIT(index % 32)) != 0;
}

// Skip unused pool slots without imposing a separate ISO endpoint limit.
static size_t iso_ep_next(size_t index) {
  while (index < QTD_MAX) {
    uint32_t const mask = ehci_data.qtd_is_iso[index / 32] >> (index % 32);
    if (mask != 0) {
      return index + tu_log2(mask & (0u - mask));
    }
    index = (index | 31u) + 1;
  }
  return QTD_MAX;
}
#endif

//--------------------------------------------------------------------+
// Debug
//--------------------------------------------------------------------+
#if 0 && CFG_TUSB_DEBUG >= (EHCI_DBG + 1)
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

// weak dcache for non-cacheable MCU
TU_ATTR_WEAK bool hcd_dcache_clean(void const* addr, uint32_t data_size) { (void) addr; (void) data_size; return true; }
TU_ATTR_WEAK bool hcd_dcache_invalidate(void const* addr, uint32_t data_size) { (void) addr; (void) data_size; return true; }
TU_ATTR_WEAK bool hcd_dcache_clean_invalidate(void const* addr, uint32_t data_size) { (void) addr; (void) data_size; return true; }

TU_ATTR_ALWAYS_INLINE static inline ehci_qhd_t* qhd_control(uint8_t dev_addr);
TU_ATTR_ALWAYS_INLINE static inline ehci_qhd_t* qhd_next (ehci_qhd_t const * p_qhd);
static ehci_qhd_t* qhd_find_free (void);
static ehci_edpt_t edpt_find(uint8_t dev_addr, uint8_t ep_addr);
static void qhd_init(ehci_qhd_t *p_qhd, uint8_t dev_addr, tusb_desc_endpoint_t const * ep_desc);
static void qhd_attach_qtd(ehci_qhd_t *qhd, ehci_qtd_t *qtd);
static void qhd_remove_qtd(ehci_qhd_t *qhd);
TU_ATTR_ALWAYS_INLINE static inline bool qhd_is_periodic(ehci_qhd_t const *qhd) {
  return qhd->int_smask != 0;
}
TU_ATTR_ALWAYS_INLINE static inline uint8_t qhd_ep_addr(ehci_qhd_t const *qhd) {
  return tu_edpt_addr(qhd->ep_number, qhd->pid);
}

TU_ATTR_ALWAYS_INLINE static inline ehci_qtd_t* qtd_control(uint8_t dev_addr);
static ehci_qtd_t* qtd_find_free (void);
static void qtd_init (ehci_qtd_t* qtd, void const* buffer, uint16_t total_bytes);

TU_ATTR_ALWAYS_INLINE static inline ehci_link_t* list_get_period_head(uint8_t rhport, uint32_t interval_ms);
TU_ATTR_ALWAYS_INLINE static inline ehci_qhd_t* list_get_async_head(uint8_t rhport);
TU_ATTR_ALWAYS_INLINE static inline ehci_link_t* list_next (ehci_link_t const *p_link);
TU_ATTR_ALWAYS_INLINE static inline void list_insert (ehci_link_t *current, ehci_link_t *entry, uint8_t type);
TU_ATTR_ALWAYS_INLINE static inline void list_remove(ehci_link_t* head, ehci_link_t* prev, ehci_qhd_t* qhd);
static void list_remove_qhd_by_addr(ehci_link_t *list_head, uint8_t dev_addr, uint8_t ep_addr);

#if EHCI_ISO_ENABLED
static bool iso_ep_open(uint8_t rhport, uint8_t daddr, tusb_desc_endpoint_t const* desc);
static bool iso_stop(uint8_t rhport, iso_ep_t* ep, bool close);
static bool iso_xfer(uint8_t rhport, iso_ep_t* ep, uint8_t* buffer, uint16_t buflen);
static void iso_process(bool in_isr);
#endif

// Wait until a schedule's status bit equals on. The controller samples the
// enable bit only at (micro)frame boundaries (EHCI 4.6, 4.8), so the wait is
// bounded by FRINDEX (2 frames), by a halted controller, and by an iteration
// guard in case FRINDEX is not counting.
static bool ehci_schedule_wait(ehci_registers_t* regs, bool is_period, bool on) {
  uint32_t const start = regs->frame_index;
  for (uint32_t guard = 0; guard < 0x100000u; guard++) {
    bool const status = is_period ? regs->status_bm.periodic_status : regs->status_bm.async_status;
    if (status == on) {
      return true;
    }
    if (regs->status_bm.hc_halted || ((regs->frame_index - start) & 0x3fffu) > 16u) {
      return false;
    }
  }
  return false;
}

// Stop a schedule and wait until the controller no longer runs it. The enable
// bit changes only once its status matches it (EHCI 4.6). Returns false if the
// schedule did not stop in time; a halted controller runs nothing.
static bool ehci_disable_schedule(ehci_registers_t* regs, bool is_period) {
  if (regs->status_bm.hc_halted) {
    return true;
  }
  bool const enabled = is_period ? regs->command_bm.periodic_enable : regs->command_bm.async_enable;
  if (enabled && !ehci_schedule_wait(regs, is_period, true)) {
    return false;
  }
  if (is_period) {
    regs->command_bm.periodic_enable = 0;
  } else {
    regs->command_bm.async_enable = 0;
  }
  return ehci_schedule_wait(regs, is_period, false);
}

// Restart a schedule without waiting for it to run; the next disable waits for
// the status to catch up before changing the enable bit again.
static void ehci_enable_schedule(ehci_registers_t* regs, bool is_period) {
  if (is_period) {
    regs->command_bm.periodic_enable = 1;
  } else {
    regs->command_bm.async_enable = 1;
  }
}

#if ((defined FSL_FEATURE_SOC_USBPHY_COUNT) && (FSL_FEATURE_SOC_USBPHY_COUNT > 0U))
static void nxp_usbphy_disconn_detector_set(uint8_t port, bool enable) {
  // unify naming convention
#if !defined(USBPHY1) && defined(USBPHY)
  #define USBPHY1 USBPHY
#endif

  if (port == 0) {
    if (enable) {
      USBPHY1->CTRL_SET = USBPHY_CTRL_ENHOSTDISCONDETECT_MASK;
    } else {
      USBPHY1->CTRL_CLR = USBPHY_CTRL_ENHOSTDISCONDETECT_MASK;
    }
  }
#if FSL_FEATURE_SOC_USBPHY_COUNT > 1U
  else if (port == 1) {
    if (enable) {
      USBPHY2->CTRL_SET = USBPHY_CTRL_ENHOSTDISCONDETECT_MASK;
    } else {
      USBPHY2->CTRL_CLR = USBPHY_CTRL_ENHOSTDISCONDETECT_MASK;
    }
  }
#endif

#if !defined(USBPHY1) && defined(USBPHY)
  #undef USBPHY1
#endif
}
#endif

//--------------------------------------------------------------------+
// HCD API
//--------------------------------------------------------------------+
uint32_t hcd_frame_number(uint8_t rhport) {
  (void) rhport;
  uint32_t uframe = ehci_data.regs->frame_index;
  return (ehci_data.uframe_number + uframe) >> 3;
}

void hcd_port_reset(uint8_t rhport) {
  (void) rhport;

  ehci_registers_t* regs = ehci_data.regs;

  // skip if already in reset
  if (regs->portsc_bm.port_reset) {
    return;
  }

  // mask out Write-1-to-Clear bits
  uint32_t portsc = regs->portsc & ~EHCI_PORTSC_MASK_W1C;

#if TU_CHECK_MCU(OPT_MCU_HPM)
  if (usb_phy_get_line_state((USB_Type *)CI_HS_REG(rhport)) == usb_line_state2) {
      portsc |= USB_PORTSC1_STS_MASK;
  } else {
      portsc &= ~USB_PORTSC1_STS_MASK;
  }
#endif

  // EHCI Table 2-16 PortSC
  // when software writes Port Reset bit to a one, it must also write a zero to the Port Enable bit.
  portsc &= ~(EHCI_PORTSC_MASK_PORT_EANBLED);
  portsc |= EHCI_PORTSC_MASK_PORT_RESET;

  regs->portsc = portsc;
}

void hcd_port_reset_end(uint8_t rhport) {
  (void) rhport;
  ehci_registers_t* regs = ehci_data.regs;

  // stop reset only if is not complete yet
  if (regs->portsc_bm.port_reset) {
    // mask out all change bits since they are Write 1 to clear
    uint32_t portsc = regs->portsc & ~EHCI_PORTSC_MASK_W1C;
    portsc &= ~EHCI_PORTSC_MASK_PORT_RESET;

    regs->portsc = portsc;
  }

#if ((defined FSL_FEATURE_SOC_USBPHY_COUNT) && (FSL_FEATURE_SOC_USBPHY_COUNT > 0U))
    // Enable disconnect detector for highspeed device only
    if (hcd_port_speed_get(rhport) == TUSB_SPEED_HIGH) {
      nxp_usbphy_disconn_detector_set(rhport, true);
    }
#endif
}

bool hcd_port_connect_status(uint8_t rhport) {
  (void) rhport;
  return ehci_data.regs->portsc_bm.current_connect_status;
}

tusb_speed_t hcd_port_speed_get(uint8_t rhport) {
  (void) rhport;
  return (tusb_speed_t) ehci_data.regs->portsc_bm.nxp_port_speed; // NXP specific port speed
}

// Close all opened endpoint belong to this device
void hcd_device_close(uint8_t rhport, uint8_t daddr) {
  // skip dev0
  if (daddr == 0) {
    return;
  }

#if EHCI_ISO_ENABLED
  for (size_t i = iso_ep_next(0); i < QTD_MAX; i = iso_ep_next(i + 1)) {
    if (ehci_data.qtd_pool[i].iso.daddr == daddr) {
      (void) iso_stop(rhport, &ehci_data.qtd_pool[i].iso, true);
    }
  }
#endif

  // Remove from async list all endpoints of this device
  list_remove_qhd_by_addr((ehci_link_t *) list_get_async_head(rhport), daddr, TUSB_INDEX_INVALID_8);

  // Remove from all interval period list of this device
  for (uint8_t i = 0; i < TU_ARRAY_SIZE(ehci_data.period_head_arr); i++) {
    list_remove_qhd_by_addr((ehci_link_t *) &ehci_data.period_head_arr[i], daddr, TUSB_INDEX_INVALID_8);
  }

  // Async doorbell (EHCI 4.8.2 for operational details)
  ehci_data.regs->command_bm.async_adv_doorbell = 1;
}

static void init_periodic_list(uint8_t rhport) {
  (void) rhport;

  // Build the polling interval tree with 1 ms, 2 ms, 4 ms and 8 ms (framesize) only
  for ( uint32_t i = 0; i < TU_ARRAY_SIZE(ehci_data.period_head_arr); i++ ) {
    ehci_data.period_head_arr[i].int_smask          = 1; // queue head in period list must have smask non-zero
    ehci_data.period_head_arr[i].qtd_overlay.halted = 1; // dummy node, always inactive
  }

  // TODO EHCI_FRAMELIST_SIZE with other size than 8
  // all links --> period_head_arr[0] (1ms)
  // 0, 2, 4, 6 etc --> period_head_arr[1] (2ms)
  // 1, 5 --> period_head_arr[2] (4ms)
  // 3 --> period_head_arr[3] (8ms)

  ehci_link_t * const framelist  = ehci_data.period_framelist;
  ehci_link_t * const head_1ms = (ehci_link_t *) &ehci_data.period_head_arr[0];
  ehci_link_t * const head_2ms = (ehci_link_t *) &ehci_data.period_head_arr[1];
  ehci_link_t * const head_4ms = (ehci_link_t *) &ehci_data.period_head_arr[2];
  ehci_link_t * const head_8ms = (ehci_link_t *) &ehci_data.period_head_arr[3];

  for (uint32_t i = 0; i < FRAMELIST_SIZE; i++) {
    framelist[i].address = (uint32_t) head_1ms;
    framelist[i].type = EHCI_QTYPE_QHD;
  }

  for (uint32_t i = 0; i < FRAMELIST_SIZE; i += 2) {
    list_insert(framelist + i, head_2ms, EHCI_QTYPE_QHD);
  }

  for (uint32_t i = 1; i < FRAMELIST_SIZE; i += 4) {
    list_insert(framelist + i, head_4ms, EHCI_QTYPE_QHD);
  }

  list_insert(framelist + 3, head_8ms, EHCI_QTYPE_QHD);

  head_1ms->terminate = 1;
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
  regs->inten  = EHCI_INT_MASK_USB | EHCI_INT_MASK_ERROR | EHCI_INT_MASK_PORT_CHANGE |
                 EHCI_INT_MASK_ASYNC_ADVANCE | EHCI_INT_MASK_FRAMELIST_ROLLOVER;

  //------------- Asynchronous List -------------//
  ehci_qhd_t * const async_head = list_get_async_head(rhport);
  tu_memclr(async_head, sizeof(ehci_qhd_t));

  async_head->next.address               = (uint32_t) async_head; // circular list, next is itself
  async_head->next.type                  = EHCI_QTYPE_QHD;
  async_head->head_list_flag             = 1;
  async_head->qtd_overlay.halted         = 1; // inactive most of time
  async_head->qtd_overlay.next.terminate = 1; // TODO removed if verified

  regs->async_list_addr = (uint32_t) async_head;

  //------------- Periodic List -------------//
  init_periodic_list(rhport);
  regs->periodic_list_base = (uint32_t) ehci_data.period_framelist;

  TU_MEMORY_BARRIER();

  //------------- TT Control (NXP only) -------------//
  regs->nxp_tt_control = 0;

  //------------- USB CMD Register -------------//
  // Interrupt threshold 0: retire completions without waiting for the next
  // 8-microframe boundary, so task context can resubmit in time (ISO).
  regs->command_bm.int_threshold = 0;
  regs->command |= EHCI_USBCMD_RUN_STOP | EHCI_USBCMD_PERIOD_SCHEDULE_ENABLE | EHCI_USBCMD_ASYNC_SCHEDULE_ENABLE |
                   FRAMELIST_SIZE_USBCMD_VALUE;

  //------------- ConfigFlag Register (skip) -------------//

  // enable port power bit in portsc. The function of this bit depends on the value of the Port
  // Power Control (PPC) field in the HCSPARAMS register.
  if (ehci_data.cap_regs->hcsparams_bm.port_power_control) {
    // mask out all change bits since they are Write 1 to clear
    uint32_t portsc = (regs->portsc & ~EHCI_PORTSC_MASK_W1C);
    portsc |= EHCI_PORTSC_MASK_PORT_POWER;

    regs->portsc = portsc;
  }

  return true;
}

bool ehci_deinit(uint8_t rhport) {
  (void) rhport;

  ehci_registers_t* regs = ehci_data.regs;

  // Disable all the interrupt
  regs->inten  = 0;

  // Disable schedules
  regs->command_bm.run_stop = 0;

  // USB Spec: controller has to stop within 16 uframe = 2 frames
  while( regs->status_bm.hc_halted == 0 ) {}

  return true;
}

//--------------------------------------------------------------------+
// Endpoint API
//--------------------------------------------------------------------+

bool hcd_edpt_open(uint8_t rhport, uint8_t dev_addr, tusb_desc_endpoint_t const * ep_desc) {
  if (ep_desc->bmAttributes.xfer == TUSB_XFER_ISOCHRONOUS) {
#if EHCI_ISO_ENABLED
    return iso_ep_open(rhport, dev_addr, ep_desc);
#else
    return false;
#endif
  }

  //------------- Prepare Queue Head -------------//
  ehci_qhd_t *p_qhd;
  if (ep_desc->bEndpointAddress == 0) {
    p_qhd = qhd_control(dev_addr);
  } else {
    if (NULL != edpt_find(dev_addr, ep_desc->bEndpointAddress).qhd) {
      return true; // already opened
    }
    p_qhd = qhd_find_free();
  }
  TU_ASSERT(p_qhd);
  qhd_init(p_qhd, dev_addr, ep_desc);

  // control of dev0 always exists as async head
  if (dev_addr == 0) {
    return true;
  }

  // Insert to list
  ehci_link_t * list_head = NULL;
  switch (ep_desc->bmAttributes.xfer) {
    case TUSB_XFER_CONTROL:
    case TUSB_XFER_BULK:
      list_head = (ehci_link_t *) list_get_async_head(rhport);
      break;

    case TUSB_XFER_INTERRUPT:
      list_head = list_get_period_head(rhport, p_qhd->interval_ms);
      break;

    default:
      break;
  }
  TU_ASSERT(list_head);

  list_insert(list_head, (ehci_link_t*) p_qhd, EHCI_QTYPE_QHD);

  return true;
}

bool hcd_edpt_close(uint8_t rhport, uint8_t daddr, uint8_t ep_addr) {
  ehci_edpt_t const ep = edpt_find(daddr, ep_addr);
#if EHCI_ISO_ENABLED
  iso_ep_t* iso = ep.iso;
  if (iso != NULL) {
    return iso_stop(rhport, iso, true);
  }
#endif
  ehci_qhd_t* qhd = ep.qhd;
  TU_VERIFY(qhd != NULL);

  ehci_link_t * list_head;
  if (qhd_is_periodic(qhd)) {
    // interrupt endpoint
    list_head = list_get_period_head(rhport, qhd->interval_ms);;
  } else {
    list_head = (ehci_link_t *) list_get_async_head(rhport);
  }

  list_remove_qhd_by_addr(list_head, daddr, ep_addr);
  return true;
}

bool hcd_setup_send(uint8_t rhport, uint8_t dev_addr, uint8_t const setup_packet[8]) {
  (void) rhport;

  ehci_qhd_t* qhd = &ehci_data.control[dev_addr].qhd;
  ehci_qtd_t* td  = &ehci_data.control[dev_addr].qtd;

  qtd_init(td, setup_packet, 8);
  td->pid = EHCI_PID_SETUP;

  hcd_dcache_clean(setup_packet, 8);

  // Control endpoint never be stalled. Skip reset Data Toggle since it is fixed per stage
  if (qhd->qtd_overlay.halted) {
    qhd->qtd_overlay.halted = false;
  }

  // attach TD to QHD -> start transferring
  qhd_attach_qtd(qhd, td);

  return true;
}

bool hcd_edpt_xfer(uint8_t rhport, uint8_t dev_addr, uint8_t ep_addr, uint8_t * buffer, uint16_t buflen) {
  (void) rhport;

  ehci_edpt_t const ep = edpt_find(dev_addr, ep_addr);
#if EHCI_ISO_ENABLED
  iso_ep_t* iso = ep.iso;
  if (iso != NULL) {
    return iso_xfer(rhport, iso, buffer, buflen);
  }
#endif

  uint8_t const epnum = tu_edpt_number(ep_addr);
  uint8_t const dir   = tu_edpt_dir(ep_addr);

  ehci_qhd_t* qhd = ep.qhd;
  TU_VERIFY(qhd != NULL);
  ehci_qtd_t* qtd;

  if (epnum == 0) {
    // Control endpoint never be stalled. Skip reset Data Toggle since it is fixed per stage
    if (qhd->qtd_overlay.halted) {
      qhd->qtd_overlay.halted = false;
    }

    qtd = qtd_control(dev_addr);
    qtd_init(qtd, buffer, buflen);

    // first data toggle is always 1 (data & setup stage)
    qtd->data_toggle = 1;
    qtd->pid = dir ? EHCI_PID_IN : EHCI_PID_OUT;
  } else {
    // skip if endpoint is halted
    TU_VERIFY(!qhd->qtd_overlay.halted);

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

bool hcd_edpt_abort_xfer(uint8_t rhport, uint8_t dev_addr, uint8_t ep_addr) {
  (void) rhport;

  ehci_edpt_t const ep = edpt_find(dev_addr, ep_addr);
#if EHCI_ISO_ENABLED
  iso_ep_t* iso = ep.iso;
  if (iso != NULL) {
    return iso_stop(rhport, iso, false);
  }
#endif
  ehci_qhd_t* qhd = ep.qhd;
  TU_VERIFY(qhd != NULL);
  ehci_qtd_t * volatile qtd = qhd->attached_qtd;
  TU_VERIFY(qtd != NULL); // no queued transfer

  TU_VERIFY(qtd->active); // transfer is already complete

  // HC is still processing, disable HC list schedule before making changes
  bool const is_period = (qhd->interval_ms > 0);

  if (!ehci_disable_schedule(ehci_data.regs, is_period)) {
    // The controller may still own the TD: leave the transfer in place.
    ehci_enable_schedule(ehci_data.regs, is_period);
    return false;
  }

  // check active bit again just in case HC has just processed the TD
  bool const still_active = qtd->active;
  if (still_active) {
    // remove TD from QH overlay
    qhd->qtd_overlay.next.terminate = 1;

    // remove TD from QH software list
    qhd_remove_qtd(qhd);
  }

  TU_MEMORY_BARRIER();
  ehci_enable_schedule(ehci_data.regs, is_period);

  return still_active; // true if removed an active transfer
}

bool hcd_edpt_clear_stall(uint8_t rhport, uint8_t daddr, uint8_t ep_addr) {
  (void) rhport;
  ehci_qhd_t *qhd = edpt_find(daddr, ep_addr).qhd;
  TU_VERIFY(qhd != NULL);
  qhd->qtd_overlay.halted = 0;
  qhd->qtd_overlay.data_toggle = 0;
  TU_MEMORY_BARRIER();

  return true;
}

#if EHCI_ISO_ENABLED
//--------------------------------------------------------------------+
// Isochronous transfers: one service interval per HCD submission
//--------------------------------------------------------------------+

// FRINDEX is a 14-bit microframe counter, independent of the frame-list size
// (RT1064 RM 42.7.21). Sample at each SOF, including while an endpoint is idle.
static uint32_t iso_now(void) {
  uint16_t const index = (uint16_t) (ehci_data.regs->frame_index & 0x3fff);
  ehci_data.iso_uframe += (index - ehci_data.iso_last_frindex) & 0x3fff;
  ehci_data.iso_last_frindex = index;
  // Native FS runs the embedded translator on frame boundaries. FRINDEX has
  // already advanced to the next frame while the current bus frame executes.
  // On a HS root link the normal microframe scheduling clock applies.
  return ehci_data.iso_uframe - ehci_data.iso_frame_offset;
}

// First slot that can still be published: the next microframe (see above).
static uint32_t iso_earliest(uint32_t now) {
  return now + 1;
}

static void iso_ep_free(iso_ep_t* ep) {
  size_t const qhd_index = (ehci_ep_t*) ep->td - ehci_data.qhd_pool;
  size_t const qtd_index = (ehci_td_t*) ep - ehci_data.qtd_pool;
  tu_memclr(ep->td, sizeof(iso_td_t));
  tu_memclr(ep, sizeof(*ep));
  ehci_data.qhd_is_iso[qhd_index / 32] &= ~TU_BIT(qhd_index % 32);
  ehci_data.qtd_is_iso[qtd_index / 32] &= ~TU_BIT(qtd_index % 32);
}

// Reserve on the upstream HS hub's transaction translator: one per port of a
// multi-TT hub, one shared by all ports of a single-TT hub.
#if CFG_TUH_HUB
static bool iso_split_slot(uint8_t hub_addr, uint8_t hub_port, uint16_t packet_size, uint8_t dir, uint8_t* slot) {
  bool const multi = hub_tt_multi(hub_addr);
  uint8_t used = 0;
  for (size_t i = iso_ep_next(0); i < QTD_MAX; i = iso_ep_next(i + 1)) {
    iso_ep_t const* ep = &ehci_data.qtd_pool[i].iso;
    ehci_sitd_t const* td = &ep->td->sitd;
    if (ep->speed == TUSB_SPEED_FULL && td->hub_addr == hub_addr && (!multi || td->port_number == hub_port)) {
      uint8_t const count = (uint8_t) ((ep->max_xfer_bytes + 187u) / 188u);
      used |= ((1u << count) - 1u) << ep->split_slot;
    }
  }
  // FS/LS interrupt QHs start their splits in microframe 0 (S-mask 0x01), after
  // the siTDs, so one would land inside an OUT packet starting there. A Genesys
  // 05e3:0610 TT takes that as a break: it loses the packet and fails the
  // interrupt transaction. Keep OUT clear of slot 0 while it fits.
  if (dir == TUSB_DIR_OUT && hub_iso_split_slot(used | 1u, packet_size, dir, slot)) {
    return true;
  }
  return hub_iso_split_slot(used, packet_size, dir, slot);
}
#endif

static bool iso_ep_open(uint8_t rhport, uint8_t daddr, tusb_desc_endpoint_t const* desc) {
  TU_VERIFY(daddr != 0 && tu_edpt_number(desc->bEndpointAddress) != 0);
  tuh_bus_info_t bus;
  TU_VERIFY(tuh_bus_info_get(daddr, &bus));
  TU_VERIFY(bus.speed == TUSB_SPEED_FULL || bus.speed == TUSB_SPEED_HIGH);
  // At most 4 ms, so a submission always fits in the 8-frame list.
  TU_VERIFY(desc->bInterval >= 1 && desc->bInterval <= (bus.speed == TUSB_SPEED_HIGH ? 6 : 3));
  uint16_t const mps = tu_edpt_packet_size(desc);
  uint8_t const mult = (uint8_t) (((tu_le16toh(desc->wMaxPacketSize) >> 11) & 3) + 1);
  TU_VERIFY(mps != 0 && mult <= 3);
  if (bus.speed == TUSB_SPEED_FULL) {
    TU_VERIFY(mps <= 1023 && mult == 1);
  } else {
    TU_VERIFY(mps <= 1024);
  }
  uint32_t const interval = (1u << (desc->bInterval - 1)) * (bus.speed == TUSB_SPEED_FULL ? 8u : 1u);
  iso_ep_t const* opened = edpt_find(daddr, desc->bEndpointAddress).iso;
  if (opened != NULL) {
    // Reopening with other parameters needs a close first, which releases the
    // TD and any split window sized for the old maximum packet.
    return opened->max_xfer_bytes == mps * mult && opened->interval == interval;
  }
  if (bus.speed == TUSB_SPEED_FULL && ehci_data.regs->portsc_bm.nxp_port_speed == TUSB_SPEED_FULL) {
    // On a native FS port the embedded TT serves one ISO endpoint of any size,
    // but several only while each packet is at most 220 bytes and they total at
    // most 440 per frame, in either direction. Beyond that the earliest-opened
    // endpoints get no tokens at all. Measured on RT1064, the S-masks have no
    // effect on it; the manual documents no limit.
    uint16_t total = mps;
    uint16_t largest = mps;
    bool shared = false;
    for (size_t i = iso_ep_next(0); i < QTD_MAX; i = iso_ep_next(i + 1)) {
      uint16_t const size = ehci_data.qtd_pool[i].iso.max_xfer_bytes;
      total += size;
      largest = tu_max16(largest, size);
      shared = true;
    }
    TU_ASSERT(!shared || (largest <= 220 && total <= 440));
  }

  uint8_t const dir = tu_edpt_dir(desc->bEndpointAddress);
  uint8_t hub_addr = 0;
  uint8_t hub_port = 0;
  uint8_t split_slot = 0;
#if CFG_TUH_HUB
  // Find the upstream HS transaction translator, including FS hubs between
  // the endpoint and TT. Direct FS uses the embedded TT (hub address zero).
  hub_addr = bus.hub_addr;
  hub_port = bus.hub_port;
  while (bus.speed != TUSB_SPEED_HIGH && hub_addr != 0) {
    tuh_bus_info_t hub;
    TU_VERIFY(tuh_bus_info_get(hub_addr, &hub));
    if (hub.speed == TUSB_SPEED_HIGH) {
      break;
    }
    hub_addr = hub.hub_addr;
    hub_port = hub.hub_port;
  }
  bool const split = bus.speed == TUSB_SPEED_FULL &&
                     ehci_data.regs->portsc_bm.nxp_port_speed == TUSB_SPEED_HIGH;
  if (split) {
    // Best effort, single H-frame only. Large FS IN packets require a
    // frame-spanning complete-split schedule (RT1064 RM 42.5.3.12.3.1).
    TU_VERIFY(dir == TUSB_DIR_OUT || mps <= 564);
    TU_VERIFY(iso_split_slot(hub_addr, hub_port, mps, dir, &split_slot));
  }
#else
  // Without hubs a FS endpoint is on a native FS port: no split transactions.
  bool const split = false;
#endif

  hcd_int_disable(rhport);
  ehci_qhd_t* qhd = qhd_find_free();
  ehci_qtd_t* qtd = qtd_find_free();
  if (qhd == NULL || qtd == NULL) {
    hcd_int_enable(rhport);
    return false;
  }
  size_t const qhd_index = ((uintptr_t) qhd - (uintptr_t) ehci_data.qhd_pool) / sizeof(ehci_ep_t);
  size_t const qtd_index = (ehci_td_t*) qtd - ehci_data.qtd_pool;
  bool const first_iso = iso_ep_next(0) == QTD_MAX;
  iso_ep_t* ep = &ehci_data.qtd_pool[qtd_index].iso;
  tu_memclr(ep, sizeof(*ep));
  ep->td = &ehci_data.qhd_pool[qhd_index].iso;
  tu_memclr(ep->td, sizeof(iso_td_t));
  ehci_data.qhd_is_iso[qhd_index / 32] |= TU_BIT(qhd_index % 32);
  ehci_data.qtd_is_iso[qtd_index / 32] |= TU_BIT(qtd_index % 32);
  ep->ep_addr = desc->bEndpointAddress;
  ep->speed = bus.speed;
  ep->split_slot = split_slot;
  ep->max_xfer_bytes = (uint16_t) (mps * mult);
  ep->interval = interval;
  // Keep the extended counter congruent to FRINDEX even when ISO was disabled.
  if (first_iso) {
    ehci_data.iso_last_frindex = (uint16_t) (ehci_data.regs->frame_index & 0x3fff);
    ehci_data.iso_uframe = ehci_data.iso_last_frindex;
    // Fixed for this root connection; refreshed when ISO is reopened.
    ehci_data.iso_frame_offset = ehci_data.regs->portsc_bm.nxp_port_speed == TUSB_SPEED_FULL ? 8 : 0;
  }
  ep->uframe = iso_now();
  ep->daddr = daddr;

  iso_td_t* td = ep->td;
  td->itd.next.terminate = 1;
  if (ep->speed == TUSB_SPEED_HIGH) {
    // Endpoint fields share the low bits of the buffer page pointers.
    td->itd.BufferPointer[0] = ep->daddr | (tu_edpt_number(ep->ep_addr) << 8);
    td->itd.BufferPointer[1] = mps | (dir << 11);
    td->itd.BufferPointer[2] = mult;
  } else {
    // siTD endpoint characteristics (EHCI 3.4.2); the TD is not linked yet.
    td->words[SITD_EP_WORD] = (ep->daddr & 0x7fu) |
                              ((uint32_t) tu_edpt_number(ep->ep_addr) << EHCI_SITD_EP_ENDPT_SHIFT) |
                              ((uint32_t) (hub_addr & 0x7fu) << EHCI_SITD_EP_HUB_SHIFT) |
                              ((uint32_t) (hub_port & 0x7fu) << EHCI_SITD_EP_PORT_SHIFT) |
                              ((uint32_t) dir << EHCI_SITD_EP_DIR_SHIFT);
    td->sitd.back.terminate = 1;
    if (dir == TUSB_DIR_IN && split) {
      uint8_t const count = (uint8_t) ((mps + 187u) / 188u);
      td->sitd.int_smask = 1u << split_slot;
      td->sitd.fl_int_cmask = ((1u << (count + 2u)) - 1u) << (split_slot + 2u);
    }
  }
  hcd_int_enable(rhport);
  return true;
}

// The caller has observed completion or stopped periodic DMA. A predecessor
// may still be active, so its link must be in coherent/non-cacheable memory.
static void iso_td_unlink(iso_ep_t const* ep) {
  iso_td_t* td = ep->td;
  ehci_link_t* link = &ehci_data.period_framelist[(ep->uframe >> 3) % FRAMELIST_SIZE];
  while (!link->terminate && tu_align32(link->address) != (uint32_t) td) {
    link = list_next(link);
  }
  if (!link->terminate) {
    *link = td->itd.next;
  }
  // Retire the single transaction, including cancelled or timed-out work.
  TU_MEMORY_BARRIER();
  td->words[ep->speed == TUSB_SPEED_HIGH ? ITD_XACT_WORD(ep->uframe & 7) : SITD_STATE_WORD] = 0;
  TU_MEMORY_BARRIER();
}

// True when the controller can neither be accessing ep's TD nor start before a
// single store retires it. The TD sits in frame-list entry (uframe >> 3) and is
// visited when FRINDEX reaches that frame. An iTD transaction runs only in its
// own microframe. An siTD runs during its H-frame, and on a native FS port the
// embedded TT works on it while FRINDEX already shows the next frame (see
// iso_frame_offset). One microframe or frame on each side is kept as margin.
static bool iso_td_idle(iso_ep_t const* ep) {
  ehci_registers_t* regs = ehci_data.regs;
  if (regs->status_bm.hc_halted) {
    return true;
  }
  uint32_t const raw = regs->frame_index;
  if (ep->speed == TUSB_SPEED_HIGH) {
    uint32_t const d = (raw - ep->uframe) % (FRAMELIST_SIZE * 8u);
    return d != 0 && d != FRAMELIST_SIZE * 8u - 1;
  }
  uint32_t const df = ((raw >> 3) - (ep->uframe >> 3)) % FRAMELIST_SIZE;
  return df >= 3 && df <= FRAMELIST_SIZE - 2;
}

// Advance to the endpoint's first slot that can still be published, keeping
// its phase: ISO intervals are powers of two.
static void iso_schedule(iso_ep_t* ep, uint32_t now) {
  uint32_t earliest = iso_earliest(now);
  if (ep->speed == TUSB_SPEED_FULL) {
    earliest = (earliest + 7) & ~7u;
  }
  if ((int32_t) (ep->uframe - earliest) < 0) {
    uint32_t const behind = earliest - ep->uframe;
    ep->uframe += (behind + ep->interval - 1) & ~(ep->interval - 1);
  }
  if (ep->speed == TUSB_SPEED_FULL) {
    ep->uframe = (ep->uframe + 7) & ~7u;
  }
}

// Called with the controller interrupt excluded. The 4 ms interval limit keeps
// the service slot within the ring starting at the current frame, after its
// frame-list entry's previous visit.
static void iso_arm(iso_ep_t* ep) {
  iso_td_t* td = ep->td;
  uint32_t const buffer = (uint32_t) ep->buffer;
  uint32_t const page = buffer & ~0xfffu;
  uint8_t const dir = tu_edpt_dir(ep->ep_addr);
  for (;;) {
    iso_schedule(ep, iso_now());
    if (ep->speed == TUSB_SPEED_HIGH) {
      ehci_itd_t* itd = &td->itd;
      // One service interval is at most 3072 bytes, spanning at most two pages.
      // BufferPointer[2] keeps only the endpoint's Mult field set at open.
      itd->BufferPointer[0] = page | (itd->BufferPointer[0] & 0xfff);
      itd->BufferPointer[1] = (page + 4096) | (itd->BufferPointer[1] & 0xfff);
      uint8_t const slot = ep->uframe & 7;
      td->words[ITD_XACT_WORD(slot)] = (buffer & 0xfff) | EHCI_ITD_XACT_IOC | EHCI_ITD_XACT_ACTIVE |
                                       ((uint32_t) ep->buflen << EHCI_ITD_XACT_LENGTH_SHIFT);
    } else {
      ehci_sitd_t* sitd = &td->sitd;
      sitd->buffer[0] = buffer;
      sitd->buffer[1] = page + 4096;
#if CFG_TUH_HUB
      if (dir == TUSB_DIR_OUT && !ehci_data.iso_frame_offset) {
        uint8_t const count = (uint8_t) tu_max32(1, (ep->buflen + 187u) / 188u);
        sitd->int_smask = (uint8_t) (((1u << count) - 1u) << ep->split_slot);
        sitd->buffer[1] |= count | (count > 1 ? EHCI_SITD_TP_BEGIN : 0); // TP = Begin or All
      }
#else
      (void) dir;
#endif
      // Reset status, split progress and page selection; retain endpoint/masks.
      td->words[SITD_STATE_WORD] = ((uint32_t) ep->buflen << EHCI_SITD_STATE_BYTES_SHIFT) |
                                   EHCI_SITD_STATE_IOC | EHCI_SITD_STATE_ACTIVE;
    }
    // An unvisited slot raises no interrupt. Recheck just before linking: a
    // delay since scheduling (preemption) moves the request to a later slot of
    // its phase rather than publishing one the controller may have passed.
    if ((int32_t) (ep->uframe - iso_earliest(iso_now())) >= 0) {
      break;
    }
    if (ep->speed == TUSB_SPEED_HIGH) {
      td->words[ITD_XACT_WORD(ep->uframe & 7)] = 0; // the next slot may be another microframe
    }
  }
  // The TD is unlinked, so Active was prepared with the rest of its state.
  // list_insert() publishes the TD and its next link before the frame-list head.
  ehci_link_t* head = &ehci_data.period_framelist[(ep->uframe >> 3) % FRAMELIST_SIZE];
  list_insert(head, &td->itd.next, ep->speed == TUSB_SPEED_HIGH ? EHCI_QTYPE_ITD : EHCI_QTYPE_SITD);
}

static bool iso_xfer(uint8_t rhport, iso_ep_t* ep, uint8_t* buffer, uint16_t buflen) {
  TU_VERIFY(!ep->busy && buflen <= ep->max_xfer_bytes);
  TU_VERIFY(buffer != NULL || buflen == 0);
  if (CFG_TUH_MEM_DCACHE_ENABLE && buflen != 0) {
    if (tu_edpt_dir(ep->ep_addr) == TUSB_DIR_IN) {
      TU_VERIFY(hcd_dcache_clean_invalidate(buffer, buflen));
    } else {
      TU_VERIFY(hcd_dcache_clean(buffer, buflen));
    }
  }
  hcd_int_disable(rhport);
  ep->buffer = buffer;
  ep->buflen = buflen;
  ep->busy = true;
  iso_arm(ep);
  hcd_int_enable(rhport);
  return true;
}

static void iso_process(bool in_isr) {
  (void) iso_now();
  for (size_t i = iso_ep_next(0); i < QTD_MAX; i = iso_ep_next(i + 1)) {
    iso_ep_t* ep = &ehci_data.qtd_pool[i].iso;
    if (!ep->busy) {
      continue;
    }
    uint32_t const now = iso_now();
    // Future requests cannot complete yet.
    if ((int32_t) (now - ep->uframe) < 0) {
      continue;
    }
    bool active;
    bool error = false;
    uint32_t actual = 0;
    iso_td_t const* td = ep->td;
    if (ep->speed == TUSB_SPEED_HIGH) {
      uint32_t const status = td->words[ITD_XACT_WORD(ep->uframe & 7)];
      active = (status & EHCI_ITD_XACT_ACTIVE) != 0;
      if (!active) {
        error = (status & EHCI_ITD_XACT_ERRORS) != 0;
        actual = tu_edpt_dir(ep->ep_addr) ?
          (status >> EHCI_ITD_XACT_LENGTH_SHIFT) & EHCI_ITD_XACT_LENGTH_MASK : ep->buflen;
      }
    } else {
      uint32_t const status = td->words[SITD_STATE_WORD];
      active = (status & EHCI_SITD_STATE_ACTIVE) != 0;
      if (!active) {
        error = (status & EHCI_SITD_STATE_ERRORS) != 0;
        uint16_t const remaining = (status >> EHCI_SITD_STATE_BYTES_SHIFT) & EHCI_SITD_STATE_BYTES_MASK;
        actual = ep->buflen - tu_min16(ep->buflen, remaining);
      }
    }
    uint32_t const end = ep->speed == TUSB_SPEED_HIGH ? ep->uframe + 1 :
                        (ep->uframe & ~7u) + 8;
    if (active && (int32_t) (now - end) < 0) {
      continue;
    }
    if (active) {
      // The service slot passed unvisited. Retire the TD once the controller
      // cannot be on it (it may be revisiting the entry a ring later); until
      // then a later interrupt retries. Next to the rollover tick that later
      // interrupt can be the revisit itself: an OUT then sends its stale
      // packet a ring late, reported as failed below. Misses are rare (#4301).
      if (!iso_td_idle(ep)) {
        continue;
      }
      error = true;
    }
    // Once a frame-list entry has recurred, a cleared Active bit cannot prove
    // which visit delivered the packet. Do not claim an on-time success.
    if ((int32_t) (now - ((ep->uframe & ~7u) + FRAMELIST_SIZE * 8u)) >= 0) {
      error = true;
    }
    if (error) {
      actual = 0; // descriptor byte counts need not be valid on an error
    }
    TU_MEMORY_BARRIER();
    if (CFG_TUH_MEM_DCACHE_ENABLE && tu_edpt_dir(ep->ep_addr) && actual != 0) {
      if (!hcd_dcache_invalidate(ep->buffer, actual)) {
        error = true;
        actual = 0;
      }
    }
    iso_td_unlink(ep);
    ep->uframe += ep->interval;
    ep->busy = false;
    hcd_event_xfer_complete(ep->daddr, ep->ep_addr, actual,
                           error ? XFER_RESULT_FAILED : XFER_RESULT_SUCCESS, in_isr);
  }
}

// Wait until the controller cannot be on ep's TD, bounded to a few frames.
static bool iso_td_wait_idle(iso_ep_t const* ep) {
  uint32_t const start = ehci_data.regs->frame_index;
  for (uint32_t guard = 0; guard < 0x100000u; guard++) {
    if (iso_td_idle(ep)) {
      return true;
    }
    if (((ehci_data.regs->frame_index - start) & 0x3fffu) > 32u) {
      return false;
    }
  }
  return false;
}

// Close unconditionally, or cancel only work whose service slot is still ahead
// and that the controller cannot be accessing.
static bool iso_stop(uint8_t rhport, iso_ep_t* ep, bool close) {
  TU_VERIFY(close || ep->busy);
  hcd_int_disable(rhport);
  bool done = true;
  if (ep->busy) {
    if (close) {
      // The buffer goes back unconditionally; stop periodic DMA only if the
      // controller stays on the TD. Restart the schedule even when the stop
      // timed out, so other periodic endpoints keep running.
      bool const on_td = !iso_td_wait_idle(ep);
      if (on_td) {
        (void) ehci_disable_schedule(ehci_data.regs, true);
      }
      iso_td_unlink(ep);
      if (on_td) {
        ehci_enable_schedule(ehci_data.regs, true);
      }
    } else {
      iso_td_t* td = ep->td;
      bool const active = ep->speed == TUSB_SPEED_HIGH ?
        td->itd.xact[ep->uframe & 7].active : td->sitd.active;
      done = active && (int32_t) (ep->uframe - iso_earliest(iso_now())) >= 0 && iso_td_idle(ep);
      if (done) {
        iso_td_unlink(ep);
      }
    }
  }
  if (done) {
    if (close) {
      iso_ep_free(ep);
    } else {
      // Keep the cancelled slot: the next submission advances from it in phase.
      ep->busy = false;
    }
  }
  hcd_int_enable(rhport);
  return done;
}
#endif

//--------------------------------------------------------------------+
// EHCI Interrupt Handler
//--------------------------------------------------------------------+

// async_advance is handshake between usb stack & ehci controller.
// This isr mean it is safe to modify previously removed queue head from async list.
// In tinyusb, queue head is only removed when device is unplugged.
TU_ATTR_ALWAYS_INLINE static inline
void async_advance_isr(uint8_t rhport) {
  (void) rhport;

  for (uint32_t i = 0; i < QHD_MAX; i++) {
#if EHCI_ISO_ENABLED
    if (qhd_is_iso(i)) {
      continue;
    }
#endif
    ehci_qhd_t* qhd = &ehci_data.qhd_pool[i].qhd;
    if (qhd->removing) {
      qhd->removing = 0;
      qhd->used = 0;
    }
  }
}

TU_ATTR_ALWAYS_INLINE static inline
void port_connect_status_change_isr(uint8_t rhport) {
  // NOTE There is an sequence plug->unplug->…..-> plug if device is powering with pre-plugged device
  if ( ehci_data.regs->portsc_bm.current_connect_status ) {
    // USBH resets the port after connection debounce.
    hcd_event_device_attach(rhport, true);
  } else // device unplugged
  {
#if ((defined FSL_FEATURE_SOC_USBPHY_COUNT) && (FSL_FEATURE_SOC_USBPHY_COUNT > 0U))
    // Disable disconnect detector
    nxp_usbphy_disconn_detector_set(rhport, false);
#endif
    hcd_event_device_remove(rhport, true);
  }
}

// Check queue head for potential transfer complete (successful or error)
static void qhd_xfer_complete_isr(ehci_qhd_t * qhd) {
  // This pointer is software-owned; idle QHs need no hardware status reads.
  ehci_qtd_t* const qtd = qhd->attached_qtd;
  if (qtd == NULL) {
    return;
  }
  volatile ehci_qtd_t *qtd_overlay = &qhd->qtd_overlay;

  // process non-active (completed) QHD with attached (scheduled) TD
  if ( !qtd_overlay->active ) {
    // An unrelated periodic interrupt can arrive before the controller has
    // fetched a newly attached qTD. The inactive overlay then still belongs
    // to the previous transfer; only a retired qTD establishes completion.
    if (qtd->active) {
      return;
    }
    TU_MEMORY_BARRIER();
    xfer_result_t xfer_result;

    if ( qtd_overlay->halted ) {
      if (qtd_overlay->xact_err || qtd_overlay->err_count == 0 || qtd_overlay->buffer_err || qtd_overlay->babble_err) {
        // Error count = 0 often occurs when device disconnected, or other bus-related error
        // clear halted bit if not caused by STALL to allow more transfer
        xfer_result = XFER_RESULT_FAILED;
        qtd_overlay->halted = false;
        TU_LOG3("  QHD xfer err count: %d\r\n", qtd_overlay->err_count);
        // TU_BREAKPOINT(); // TODO skip unplugged device
      }else {
        // no error bits are set, endpoint is halted due to STALL
        xfer_result = XFER_RESULT_STALLED;
      }
    } else {
      xfer_result = XFER_RESULT_SUCCESS;
    }

    uint8_t const dir = (qtd->pid == EHCI_PID_IN) ? 1 : 0;
    uint32_t const xferred_bytes = qtd->expected_bytes - qtd->total_bytes;

    // invalidate dcache if IN transfer with data
    if (dir == 1 && qhd->attached_buffer != 0 && xferred_bytes > 0) {
      hcd_dcache_invalidate((void*) qhd->attached_buffer, xferred_bytes);
    }

    // remove and free TD before invoking callback
    qhd_remove_qtd(qhd);

    // notify usbh
    uint8_t const ep_addr = tu_edpt_addr(qhd->ep_number, dir);
    hcd_event_xfer_complete(qhd->dev_addr, ep_addr, xferred_bytes, xfer_result, true);
  }
}

TU_ATTR_ALWAYS_INLINE static inline
void proccess_async_xfer_isr(ehci_qhd_t * const list_head) {
  ehci_qhd_t *qhd = list_head;

  do {
    qhd_xfer_complete_isr(qhd);
    qhd = qhd_next(qhd);
  } while ( qhd != list_head ); // async list traversal, stop if loop around
}

TU_ATTR_ALWAYS_INLINE static inline
void process_period_xfer_isr(uint8_t rhport, uint32_t interval_ms) {
  uint32_t const period_1ms_addr = (uint32_t) list_get_period_head(rhport, 1u);
  ehci_link_t next_link = *list_get_period_head(rhport, interval_ms);

  while (!next_link.terminate) {
    if (interval_ms > 1 && period_1ms_addr == tu_align32(next_link.address)) {
      // 1ms period list is end of list for all larger interval
      break;
    }

    uintptr_t const entry_addr = tu_align32(next_link.address);

    switch (next_link.type) {
      case EHCI_QTYPE_QHD: {
        ehci_qhd_t *qhd = (ehci_qhd_t *) entry_addr;
        qhd_xfer_complete_isr(qhd);
      }
        break;

      // ISO descriptors are retired by iso_process().
      case EHCI_QTYPE_ITD:
      case EHCI_QTYPE_SITD:
      case EHCI_QTYPE_FSTN:
      default:
        break;
    }

    next_link = *list_next(&next_link);
  }
}

//------------- Host Controller Driver's Interrupt Handler -------------//
void hcd_int_handler(uint8_t rhport, bool in_isr) {
  (void) in_isr;
  ehci_registers_t* regs = ehci_data.regs;
  uint32_t const int_status = regs->status;

  if (int_status & EHCI_INT_MASK_HC_HALTED) {
    // something seriously wrong, e.g. a descriptor placed in cacheable memory
    TU_BREAKPOINT();
    TU_LOG1("  HC halted\r\n");
    return;
  }

  if (int_status & EHCI_INT_MASK_FRAMELIST_ROLLOVER) {
    ehci_data.uframe_number += (FRAMELIST_SIZE << 3);
    regs->status = EHCI_INT_MASK_FRAMELIST_ROLLOVER; // Acknowledge
  }

  if (int_status & EHCI_INT_MASK_PORT_CHANGE) {
    // Including: Force port resume, over-current change, enable/disable change and connect status change.
    uint32_t const port_status = regs->portsc & EHCI_PORTSC_MASK_W1C;
    // print_portsc(regs);

    if (regs->portsc_bm.connect_status_change) {
      port_connect_status_change_isr(rhport);
    }

    regs->portsc |= port_status; // Acknowledge change bits in portsc
    regs->status = EHCI_INT_MASK_PORT_CHANGE; // Acknowledge
  }

  // A USB transfer is completed (OK or error)
  uint32_t const usb_int = int_status & (EHCI_INT_MASK_USB | EHCI_INT_MASK_ERROR);
  if (usb_int) {
    // Acknowledge before scanning: a completion that arrives after its QH was
    // visited must remain pending for the next interrupt.
    regs->status = usb_int;
  }
#if EHCI_ISO_ENABLED
  // Completions drive ISO. Frame-list rollover retires slots that passed
  // unvisited and samples FRINDEX well within its 14-bit wrap.
  uint32_t const iso_tick = EHCI_INT_MASK_FRAMELIST_ROLLOVER;
  if ((usb_int || (int_status & regs->inten & iso_tick)) && iso_ep_next(0) != QTD_MAX) {
    iso_process(in_isr);
  }
#endif
  if (usb_int) {
    proccess_async_xfer_isr(list_get_async_head(rhport));

    for ( uint32_t i = 1; i <= FRAMELIST_SIZE; i *= 2 ) {
      process_period_xfer_isr(rhport, i);
    }

  }

  //------------- There is some removed async previously -------------//
  // need to place after EHCI_INT_MASK_NXP_ASYNC
  if (int_status & EHCI_INT_MASK_ASYNC_ADVANCE) {
    async_advance_isr(rhport);
    regs->status = EHCI_INT_MASK_ASYNC_ADVANCE; // Acknowledge
  }
}

//--------------------------------------------------------------------+
// List Managing Helper
//--------------------------------------------------------------------+

// Get head of periodic list
TU_ATTR_ALWAYS_INLINE static inline ehci_link_t* list_get_period_head(uint8_t rhport, uint32_t interval_ms) {
  (void) rhport;
  return (ehci_link_t*) &ehci_data.period_head_arr[ tu_log2( tu_min32(FRAMELIST_SIZE, interval_ms) ) ];
}

// Get head of async list
TU_ATTR_ALWAYS_INLINE static inline ehci_qhd_t* list_get_async_head(uint8_t rhport) {
  (void) rhport;
  return qhd_control(0); // control qhd of dev0 is used as async head
}

TU_ATTR_ALWAYS_INLINE static inline ehci_link_t* list_next(ehci_link_t const *p_link) {
  return (ehci_link_t*) tu_align32(p_link->address);
}

TU_ATTR_ALWAYS_INLINE static inline void list_insert(ehci_link_t *current, ehci_link_t *entry, uint8_t type) {
  entry->address = current->address;
  TU_MEMORY_BARRIER();
  current->address = ((uint32_t) entry) | (type << 1);
  TU_MEMORY_BARRIER();
}

// Remove a queue head from the list.
// Per EHCI 4.8.2 the removed qhd's next is linked to list head (which always reachable by Host Controller)
// TODO support iTD/siTD
TU_ATTR_ALWAYS_INLINE static inline void list_remove(ehci_link_t* head, ehci_link_t* prev, ehci_qhd_t* qhd) {
  // TODO deactivate all TD, wait for QHD to inactive before removal
  prev->address = qhd->next.address;
  TU_MEMORY_BARRIER();

  // link the removed qhd's next to list head
  qhd->next.address = ((uint32_t) head) | (EHCI_QTYPE_QHD << 1);

  if (qhd_is_periodic(qhd)) {
    // period list queue element is guarantee to be free in the next frame (1 ms)
    qhd->used = 0;
  } else {
    // async list use async advance handshake. Mark as removing, will completely re-usable when async advance isr occurs
    qhd->removing = 1;
  }
  TU_MEMORY_BARRIER();
}

// Remove queue head belong to this device address
static void list_remove_qhd_by_addr(ehci_link_t *list_head, uint8_t dev_addr, uint8_t ep_addr) {
  ehci_link_t *prev = list_head;

  while (prev && !prev->terminate) {
    ehci_qhd_t *qhd = (ehci_qhd_t *) (uintptr_t) list_next(prev);

    // done if loop back to head
    if ((uintptr_t) qhd == (uintptr_t) list_head) {
      break;
    }

    // ep_addr is 0xff means all endpoints of this device address
    if (qhd->dev_addr == dev_addr &&
        (ep_addr == TUSB_INDEX_INVALID_8 || qhd_ep_addr(qhd) == ep_addr)) {
      list_remove(list_head, prev, qhd);
    } else {
      prev = list_next(prev);
    }
  }
}

//--------------------------------------------------------------------+
// Queue Header helper
//--------------------------------------------------------------------+

// Get queue head for control transfer (always available)
TU_ATTR_ALWAYS_INLINE static inline ehci_qhd_t* qhd_control(uint8_t dev_addr) {
  return &ehci_data.control[dev_addr].qhd;
}

// Find a free queue head
static ehci_qhd_t *qhd_find_free(void) {
  for (uint32_t i = 0; i < QHD_MAX; i++) {
#if EHCI_ISO_ENABLED
    if (qhd_is_iso(i)) {
      continue;
    }
#endif
    if (!ehci_data.qhd_pool[i].qhd.used) {
      return &ehci_data.qhd_pool[i].qhd;
    }
  }
  return NULL;
}

// Next queue head link
TU_ATTR_ALWAYS_INLINE static inline ehci_qhd_t *qhd_next(ehci_qhd_t const *p_qhd) {
  return (ehci_qhd_t *) tu_align32(p_qhd->next.address);
}

// Resolve either endpoint type once; control endpoints have dedicated QHs.
static ehci_edpt_t edpt_find(uint8_t dev_addr, uint8_t ep_addr) {
  ehci_edpt_t result = {0};
  if (tu_edpt_number(ep_addr) == 0) {
    result.qhd = qhd_control(dev_addr);
    return result;
  }

#if EHCI_ISO_ENABLED
  // ISO lookup fields change only on task-context open/close; no IRQ lock needed.
  for (size_t i = iso_ep_next(0); i < QTD_MAX; i = iso_ep_next(i + 1)) {
    iso_ep_t* iso = &ehci_data.qtd_pool[i].iso;
    if (iso->daddr == dev_addr && iso->ep_addr == ep_addr) {
      result.iso = iso;
      return result;
    }
  }
#endif
  // Protect QH ownership flags, which can change during async advance.
  usbh_spin_lock(false);
  for (size_t i = 0; i < QHD_MAX; i++) {
#if EHCI_ISO_ENABLED
    if (qhd_is_iso(i)) {
      continue;
    }
#endif
    ehci_qhd_t* qhd = &ehci_data.qhd_pool[i].qhd;
    if (qhd->used && !qhd->removing && qhd->dev_addr == dev_addr && qhd_ep_addr(qhd) == ep_addr) {
      result.qhd = qhd;
      break;
    }
  }
  usbh_spin_unlock(false);
  return result;
}

// Init queue head with endpoint descriptor
static void qhd_init(ehci_qhd_t *p_qhd, uint8_t dev_addr, tusb_desc_endpoint_t const * ep_desc) {
  // address 0 is used as async head, which always on the list --> cannot be cleared (ehci halted otherwise)
  if (dev_addr != 0) {
    tu_memclr(p_qhd, sizeof(ehci_qhd_t));
  }

  tuh_bus_info_t bus_info;
  tuh_bus_info_get(dev_addr, &bus_info);

  uint8_t const xfer_type = ep_desc->bmAttributes.xfer;
  uint8_t const interval = ep_desc->bInterval;

  p_qhd->dev_addr           = dev_addr;
  p_qhd->fl_inactive_next_xact = 0;
  p_qhd->ep_number          = tu_edpt_number(ep_desc->bEndpointAddress);
  p_qhd->ep_speed           = bus_info.speed;
  p_qhd->data_toggle_control= (xfer_type == TUSB_XFER_CONTROL) ? 1 : 0;
  p_qhd->head_list_flag     = (dev_addr == 0) ? 1 : 0; // addr0's endpoint is the static async list head
  p_qhd->max_packet_size    = tu_edpt_packet_size(ep_desc);
  p_qhd->fl_ctrl_ep_flag    = ((xfer_type == TUSB_XFER_CONTROL) && (p_qhd->ep_speed != TUSB_SPEED_HIGH))  ? 1 : 0;
  p_qhd->nak_reload         = 0;

  switch (xfer_type) {
    case TUSB_XFER_CONTROL:
    case TUSB_XFER_BULK:
      p_qhd->int_smask = p_qhd->fl_int_cmask = 0;
      break;

    case TUSB_XFER_INTERRUPT:
      if (TUSB_SPEED_HIGH == p_qhd->ep_speed) {
        TU_ASSERT(interval <= 16, );
        if (interval < 4) {
          // sub millisecond interval
          p_qhd->interval_ms = 0;
          p_qhd->int_smask = (interval == 1) ? 0xff : // 0b11111111
                             (interval == 2) ? 0xaa /* 0b10101010 */ : 0x44 /* 0b01000100 */;
        } else {
          p_qhd->interval_ms = (uint8_t) tu_min16(1 << (interval - 4), 255);
          p_qhd->int_smask = TU_BIT(interval % 8);
        }
      } else {
        TU_ASSERT(0 != interval, );
        // Full/Low: 4.12.2.1 (EHCI) case 1 schedule start split at 1 us & complete split at 2,3,4 uframes
        p_qhd->int_smask = 0x01;
        p_qhd->fl_int_cmask = 0x1c; // 0b11100
        p_qhd->interval_ms = interval;
      }
      break;

    default: break;
  }

  p_qhd->fl_hub_addr  = bus_info.hub_addr;
  p_qhd->fl_hub_port  = bus_info.hub_port;
  p_qhd->mult         = 1; // TODO not use high bandwidth/park mode yet

  //------------- HCD Management Data -------------//
  p_qhd->used         = 1;
  p_qhd->removing     = 0;
  p_qhd->attached_qtd = NULL;
  p_qhd->pid = tu_edpt_dir(ep_desc->bEndpointAddress) == TUSB_DIR_IN ? EHCI_PID_IN : EHCI_PID_OUT; // PID for TD under this endpoint

  //------------- active, but no TD list -------------//
  p_qhd->qtd_overlay.halted              = 0;
  p_qhd->qtd_overlay.next.terminate      = 1;
  p_qhd->qtd_overlay.alternate.terminate = 1;

  if (TUSB_XFER_BULK == xfer_type && p_qhd->ep_speed == TUSB_SPEED_HIGH && p_qhd->pid == EHCI_PID_OUT) {
    p_qhd->qtd_overlay.ping_err = 1; // do PING for Highspeed Bulk OUT, EHCI section 4.11
  }
}

// Attach a TD to queue head
static void qhd_attach_qtd(ehci_qhd_t *qhd, ehci_qtd_t *qtd) {
  qhd->attached_qtd = qtd;
  qhd->attached_buffer = qtd->buffer[0];

  // Publish the initialized TD before linking it into the active queue.
  TU_MEMORY_BARRIER();
  qhd->qtd_overlay.next.address = (uint32_t) qtd;
  TU_MEMORY_BARRIER();
}

// Remove an attached TD from queue head
static void qhd_remove_qtd(ehci_qhd_t *qhd) {
  ehci_qtd_t * volatile qtd = qhd->attached_qtd;

  qhd->attached_qtd = NULL;
  qhd->attached_buffer = 0;

  qtd->used = 0; // free QTD
}

//--------------------------------------------------------------------+
// Queue TD helper
//--------------------------------------------------------------------+

// Get TD for control transfer (always available)
TU_ATTR_ALWAYS_INLINE static inline ehci_qtd_t* qtd_control(uint8_t dev_addr) {
  return &ehci_data.control[dev_addr].qtd;
}

static ehci_qtd_t *qtd_find_free(void) {
  for (uint32_t i = 0; i < QTD_MAX; i++) {
#if EHCI_ISO_ENABLED
    if (qtd_is_iso(i)) {
      continue;
    }
#endif
    if (!ehci_data.qtd_pool[i].qtd.used) return &ehci_data.qtd_pool[i].qtd;
  }
  return NULL;
}

static void qtd_init(ehci_qtd_t* qtd, void const* buffer, uint16_t total_bytes) {
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
  for(uint8_t i=1; i<5; i++) {
    qtd->buffer[i] |= tu_align4k(qtd->buffer[i - 1] ) + 4096;
  }
}

#endif

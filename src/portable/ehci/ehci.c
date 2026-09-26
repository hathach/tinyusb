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
#include "ehci_api.h"
#include "ehci.h"

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

/* ISO scheduling
 *
 * ChipIdea HS supports ISO using the shared QH/qTD pools. Other
 * EHCI ports need a scheduling clock equivalent to ChipIdea's SOF interrupt.
 * Each submission covers one service interval: HS uses iTDs (up to 3
 * transactions), FS uses siTDs. Requests exceeding interval capacity fail.
 *
 * CFG_TUH_CHIPIDEA_ISO_ENABLE enables one outstanding transfer and one TD
 * per endpoint. Buffers remain owned until terminal completion; completion
 * retires the TD before notifying USBH so its normal callback can refill it.
 * No host stack or class-driver queuing is required.
 *
 * FS behind an HS hub uses a fixed best-effort schedule within one H-frame:
 *   OUT: start-splits from H0, one per 188 bytes, through H5; no completes.
 *   IN (including feedback): start-split H2, complete-splits H4..H7.
 * H-frames lead the SOF frame number by one microframe. No bus-time admission,
 * per-TT slot allocation, or frame-spanning splits are implemented.
 *
 * Packet limits per service interval:
 *   Mic only: IN maximum packet size <= 564; retries share 4 complete slots.
 *   Speaker only, no feedback: OUT maximum packet size <= 1023 (6 splits).
 *   Headset or speaker + feedback: OUT <= 376 avoids the IN start slot.
 * The 376-byte limit is NOT enforced: accepted OUT packets >= 377 overlap H2
 * and can disrupt audio even with successful completions. Smaller packets
 * still contend for FS bus time; multiple IN endpoints share the same slots.
 * Direct FS uses the embedded TT without these masks, allowing 1023 bytes
 * in either direction. Buffer sizes may impose smaller limits.
 *
 * The audio example has 256-byte buffers. At 1 ms, nominal stereo playback
 * needs 192 bytes for 48 kHz S16 (2 splits), 288 for packed S24 (2), or 384
 * for 48 kHz S32 / 96 kHz S16 (3, overlaps IN). Use the largest packet,
 * including rate/feedback variation. Larger buffers do not change the masks.
 * FS hardware validation with one TD is pending. The 564/1023 maxima,
 * 376-byte boundary and FS feedback are untested.
 *
 * Preserve endpoint phase and skip late slots. With no descriptor caching,
 * publish for the next microframe; caching controllers retain their lead time.
 * This reduces EHCI's generic 2-microframe margin: publication must finish
 * before the next traversal. Completion and task refill must occur within
 * the same microframe for consecutive HS 125 us intervals. Long intervals
 * wait inside the 8-frame window.
 * SOF handling must run before the 8 ms ring repeats, or hardware may revisit stale active
 * descriptors. Timeout, close and cancellation quiesce periodic DMA before
 * releasing buffers; software cannot undo packets sent during an IRQ delay
 * and does not retry missed ISO packets.
 *
 * Each endpoint owns one TD. On completion, unlink its TD before returning
 * the buffer to USBH, following NXP's EHCI completion/release sequence.
 * The ISO TD occupies one QH slot and its software state one qTD slot until
 * endpoint close. Bulk/interrupt transfers use the remaining slots.
 * Descriptor/link memory must be coherent or non-cacheable for live unlinking;
 * use DTCM or MPU-configured non-cacheable SRAM on RT1064. Payload buffers
 * retain their cache maintenance.
 *
 * References: EHCI 1.0 ch. 3/4; USB 2.0 ch. 11; RT1064 RM Rev. 2 ch. 42.
 */
#if defined(TUP_USBIP_CHIPIDEA_HS) && CFG_TUH_CHIPIDEA_ISO_ENABLE
// An iTD must not cross a 4 KiB boundary (EHCI chapter 3).
typedef union TU_ATTR_ALIGNED(64) {
  ehci_itd_t itd;
  ehci_sitd_t sitd;
  volatile uint32_t words[16]; // snapshot a hardware status word without repeated bitfield reads
} iso_td_t;

typedef struct {
  iso_td_t* td;
  uint8_t* buffer;
  uint32_t interval;
  uint32_t uframe; // Current service slot while busy, next slot while idle.
  uint16_t max_xfer_bytes;
  uint16_t buflen;
  union {
    struct {
      uint8_t daddr;
      uint8_t ep_addr;
    };
    uint16_t ep_key; // Little-endian view of the two address bytes.
  };
  uint8_t speed;
  bool busy;
  bool armed;
} iso_ep_t;

TU_VERIFY_STATIC(sizeof(iso_ep_t) <= sizeof(ehci_qtd_t), "ISO state must fit in one qTD slot");
#endif

typedef union {
  ehci_qhd_t qhd;
#if defined(TUP_USBIP_CHIPIDEA_HS) && CFG_TUH_CHIPIDEA_ISO_ENABLE
  iso_td_t iso;
#endif
} ehci_ep_t;

typedef union {
  ehci_qtd_t qtd;
#if defined(TUP_USBIP_CHIPIDEA_HS) && CFG_TUH_CHIPIDEA_ISO_ENABLE
  iso_ep_t iso;
#endif
} ehci_td_t;

// Exactly one member is non-NULL for an open endpoint.
typedef struct {
  ehci_qhd_t* qhd;
#if defined(TUP_USBIP_CHIPIDEA_HS) && CFG_TUH_CHIPIDEA_ISO_ENABLE
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
#if defined(TUP_USBIP_CHIPIDEA_HS) && CFG_TUH_CHIPIDEA_ISO_ENABLE
  uint32_t qhd_is_iso[(QHD_MAX + 31) / 32];
  uint32_t qtd_is_iso[(QTD_MAX + 31) / 32];
#endif

#if defined(TUP_USBIP_CHIPIDEA_HS) && CFG_TUH_CHIPIDEA_ISO_ENABLE
  uint32_t iso_uframe;
  uint16_t iso_last_frindex;
  uint8_t iso_saved_itc;
  uint8_t iso_threshold;
  uint8_t iso_frame_offset;
#endif

  ehci_registers_t* regs;         // operational register
  ehci_cap_registers_t* cap_regs; // capability register

  volatile uint32_t uframe_number;
}ehci_data_t;

// Periodic frame list must be 4K alignment
CFG_TUH_MEM_SECTION TU_ATTR_ALIGNED(4096) static ehci_data_t ehci_data;

#if defined(TUP_USBIP_CHIPIDEA_HS) && CFG_TUH_CHIPIDEA_ISO_ENABLE
TU_ATTR_ALWAYS_INLINE static inline bool qhd_is_iso(size_t index) {
  return (ehci_data.qhd_is_iso[index / 32] & TU_BIT(index % 32)) != 0;
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

#if defined(TUP_USBIP_CHIPIDEA_HS) && CFG_TUH_CHIPIDEA_ISO_ENABLE
static bool iso_ep_open(uint8_t rhport, uint8_t daddr, tusb_desc_endpoint_t const* desc);
static bool iso_stop(uint8_t rhport, iso_ep_t* ep, bool close);
static bool iso_xfer(uint8_t rhport, iso_ep_t* ep, uint8_t* buffer, uint16_t buflen);
static void iso_process(bool in_isr);
#endif

static void ehci_disable_schedule(ehci_registers_t* regs, bool is_period) {
  // maybe have a timeout for status
  if (is_period) {
    regs->command_bm.periodic_enable = 0;
    while(regs->status_bm.periodic_status) {}
  } else {
    regs->command_bm.async_enable = 0;
    while(regs->status_bm.async_status) {} // should have a timeout
  }
}

static void ehci_enable_schedule(ehci_registers_t* regs, bool is_period) {
  // maybe have a timeout for status
  if (is_period) {
    regs->command_bm.periodic_enable = 1;
    while ( 0 == regs->status_bm.periodic_status ) {}
  } else {
    regs->command_bm.async_enable = 1;
    while( 0 == regs->status_bm.async_status ) {}
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

#if defined(TUP_USBIP_CHIPIDEA_HS) && CFG_TUH_CHIPIDEA_ISO_ENABLE
  for (size_t i = iso_ep_next(0); i < QTD_MAX; i = iso_ep_next(i + 1)) {
    if (ehci_data.qtd_pool[i].iso.daddr == daddr) {
      TU_ASSERT(iso_stop(rhport, &ehci_data.qtd_pool[i].iso, true), );
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

  hcd_dcache_clean(&ehci_data, sizeof(ehci_data_t));

  //------------- TT Control (NXP only) -------------//
  regs->nxp_tt_control = 0;

  //------------- USB CMD Register -------------//
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
#if defined(TUP_USBIP_CHIPIDEA_HS) && CFG_TUH_CHIPIDEA_ISO_ENABLE
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

  hcd_dcache_clean(p_qhd, sizeof(ehci_qhd_t));
  hcd_dcache_clean(list_head, sizeof(ehci_qhd_t));

  return true;
}

bool hcd_edpt_close(uint8_t rhport, uint8_t daddr, uint8_t ep_addr) {
  ehci_edpt_t const ep = edpt_find(daddr, ep_addr);
#if defined(TUP_USBIP_CHIPIDEA_HS) && CFG_TUH_CHIPIDEA_ISO_ENABLE
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
#if defined(TUP_USBIP_CHIPIDEA_HS) && CFG_TUH_CHIPIDEA_ISO_ENABLE
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
#if defined(TUP_USBIP_CHIPIDEA_HS) && CFG_TUH_CHIPIDEA_ISO_ENABLE
  iso_ep_t* iso = ep.iso;
  if (iso != NULL) {
    return iso_stop(rhport, iso, false);
  }
#endif
  ehci_qhd_t* qhd = ep.qhd;
  TU_VERIFY(qhd != NULL);
  ehci_qtd_t * volatile qtd = qhd->attached_qtd;
  TU_VERIFY(qtd != NULL); // no queued transfer

  hcd_dcache_invalidate(qtd, sizeof(ehci_qtd_t));
  TU_VERIFY(qtd->active); // transfer is already complete

  // HC is still processing, disable HC list schedule before making changes
  bool const is_period = (qhd->interval_ms > 0);

  ehci_disable_schedule(ehci_data.regs, is_period);

  // check active bit again just in case HC has just processed the TD
  bool const still_active = qtd->active;
  if (still_active) {
    // remove TD from QH overlay
    qhd->qtd_overlay.next.terminate = 1;
    hcd_dcache_clean(qhd, sizeof(ehci_qhd_t));

    // remove TD from QH software list
    qhd_remove_qtd(qhd);
  }

  ehci_enable_schedule(ehci_data.regs, is_period);

  return still_active; // true if removed an active transfer
}

bool hcd_edpt_clear_stall(uint8_t rhport, uint8_t daddr, uint8_t ep_addr) {
  (void) rhport;
  ehci_qhd_t *qhd = edpt_find(daddr, ep_addr).qhd;
  TU_VERIFY(qhd != NULL);
  qhd->qtd_overlay.halted = 0;
  qhd->qtd_overlay.data_toggle = 0;
  hcd_dcache_clean_invalidate(qhd, sizeof(ehci_qhd_t));

  return true;
}

#if defined(TUP_USBIP_CHIPIDEA_HS) && CFG_TUH_CHIPIDEA_ISO_ENABLE
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

static uint32_t iso_earliest(uint32_t now) {
  uint32_t const threshold = ehci_data.iso_threshold;
  // Without descriptor caching, try the next microframe. Publication must
  // finish before its traversal; EHCI's generic bound adds another microframe
  // for uncertainty about the current position within FRINDEX.
  return (threshold & 8) ? ((now + 9) & ~7u) : now + threshold + 1;
}

static void iso_ep_free(iso_ep_t* ep) {
  size_t const qhd_index = (ehci_ep_t*) ep->td - ehci_data.qhd_pool;
  size_t const qtd_index = (ehci_td_t*) ep - ehci_data.qtd_pool;
  tu_memclr(ep->td, sizeof(iso_td_t));
  hcd_dcache_clean(ep->td, sizeof(iso_td_t));
  tu_memclr(ep, sizeof(*ep));
  ehci_data.qhd_is_iso[qhd_index / 32] &= ~TU_BIT(qhd_index % 32);
  ehci_data.qtd_is_iso[qtd_index / 32] &= ~TU_BIT(qtd_index % 32);
}

static bool iso_ep_open(uint8_t rhport, uint8_t daddr, tusb_desc_endpoint_t const* desc) {
  TU_VERIFY(daddr != 0 && tu_edpt_number(desc->bEndpointAddress) != 0);
  if (edpt_find(daddr, desc->bEndpointAddress).iso != NULL) {
    return true;
  }

  tuh_bus_info_t bus;
  TU_VERIFY(tuh_bus_info_get(daddr, &bus));
  TU_VERIFY(bus.speed == TUSB_SPEED_FULL || bus.speed == TUSB_SPEED_HIGH);
  TU_VERIFY(desc->bInterval >= 1 && desc->bInterval <= 16);
  uint16_t const mps = tu_edpt_packet_size(desc);
  uint8_t const mult = (uint8_t) (((tu_le16toh(desc->wMaxPacketSize) >> 11) & 3) + 1);
  TU_VERIFY(mps != 0 && mult <= 3);
  if (bus.speed == TUSB_SPEED_FULL) {
    TU_VERIFY(mps <= 1023 && mult == 1);
    // Best effort, single H-frame only. Large FS IN packets require a
    // frame-spanning complete-split schedule (RT1064 RM 42.5.3.12.3.1).
    TU_VERIFY(ehci_data.regs->portsc_bm.nxp_port_speed != TUSB_SPEED_HIGH ||
              tu_edpt_dir(desc->bEndpointAddress) == TUSB_DIR_OUT || mps <= 564);
  } else {
    TU_VERIFY(mps <= 1024);
  }

  // Find the upstream HS transaction translator, including FS hubs between
  // the endpoint and TT. Direct FS uses the embedded TT (hub address zero).
  uint8_t hub_addr = bus.hub_addr;
  uint8_t hub_port = bus.hub_port;
  while (bus.speed != TUSB_SPEED_HIGH && hub_addr != 0) {
    tuh_bus_info_t hub;
    TU_VERIFY(tuh_bus_info_get(hub_addr, &hub));
    if (hub.speed == TUSB_SPEED_HIGH) {
      break;
    }
    hub_addr = hub.hub_addr;
    hub_port = hub.hub_port;
  }

  hcd_int_disable(rhport);
  ehci_qhd_t* qhd = qhd_find_free();
  ehci_qtd_t* qtd = qtd_find_free();
  if (qhd == NULL || qtd == NULL) {
    hcd_int_enable(rhport);
    return false;
  }
  size_t const qhd_index = ((uintptr_t) qhd - (uintptr_t) ehci_data.qhd_pool) / sizeof(ehci_ep_t);
  size_t const qtd_index = (ehci_td_t*) qtd - ehci_data.qtd_pool;
  iso_ep_t* ep = &ehci_data.qtd_pool[qtd_index].iso;
  tu_memclr(ep, sizeof(*ep));
  ep->td = &ehci_data.qhd_pool[qhd_index].iso;
  tu_memclr(ep->td, sizeof(iso_td_t));
  ehci_data.qhd_is_iso[qhd_index / 32] |= TU_BIT(qhd_index % 32);
  ehci_data.qtd_is_iso[qtd_index / 32] |= TU_BIT(qtd_index % 32);
  ep->ep_addr = desc->bEndpointAddress;
  ep->speed = bus.speed;
  ep->max_xfer_bytes = (uint16_t) (mps * mult);
  ep->interval = (1u << (desc->bInterval - 1)) * (bus.speed == TUSB_SPEED_FULL ? 8u : 1u);
  // Keep the extended counter congruent to FRINDEX even when ISO was disabled.
  if (!(ehci_data.regs->inten & EHCI_INT_MASK_NXP_SOF)) {
    ehci_data.iso_last_frindex = (uint16_t) (ehci_data.regs->frame_index & 0x3fff);
    ehci_data.iso_uframe = ehci_data.iso_last_frindex;
    ehci_data.iso_saved_itc = ehci_data.regs->command_bm.int_threshold;
    // These remain fixed for this root connection; refresh when ISO is reopened.
    ehci_data.iso_threshold = ehci_data.cap_regs->hccparams_bm.iso_schedule_threshold;
    ehci_data.iso_frame_offset = ehci_data.regs->portsc_bm.nxp_port_speed == TUSB_SPEED_FULL ? 8 : 0;
  }
  ep->uframe = iso_now();
  ep->daddr = daddr;
  // Retire each packet promptly so task context can submit the next interval.
  ehci_data.regs->command_bm.int_threshold = 0;

  uint8_t const dir = tu_edpt_dir(ep->ep_addr);
  iso_td_t* td = ep->td;
  td->itd.next.terminate = 1;
  if (ep->speed == TUSB_SPEED_HIGH) {
    // Endpoint fields share the low bits of the buffer page pointers.
    td->itd.BufferPointer[0] = ep->daddr | (tu_edpt_number(ep->ep_addr) << 8);
    td->itd.BufferPointer[1] = mps | (dir << 11);
    td->itd.BufferPointer[2] = mult;
  } else {
    // siTD endpoint characteristics (EHCI 3.4.2): address, endpoint, TT, direction.
    td->words[1] = (ep->daddr & 0x7fu) | (tu_edpt_number(ep->ep_addr) << 8) |
                   ((uint32_t) (hub_addr & 0x7f) << 16) | ((uint32_t) (hub_port & 0x7f) << 24) |
                   ((uint32_t) dir << 31);
    td->sitd.back.terminate = 1;
    if (dir == TUSB_DIR_IN && !ehci_data.iso_frame_offset) {
      // Leave H0/H1 for common FS audio OUT packets.
      td->sitd.int_smask = 4;
      td->sitd.fl_int_cmask = 0xf0;
    }
  }
  hcd_dcache_clean(td, sizeof(*td));
  ehci_data.regs->status = EHCI_INT_MASK_NXP_SOF;
  ehci_data.regs->inten |= EHCI_INT_MASK_NXP_SOF;
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
    if (CFG_TUH_MEM_DCACHE_ENABLE) {
      hcd_dcache_invalidate(link, sizeof(iso_td_t));
    }
  }
  if (!link->terminate) {
    *link = td->itd.next;
    hcd_dcache_clean((void*) tu_align32((uint32_t) link), 32);
  }
  // Retire the single transaction, including cancelled or timed-out work.
  td->words[ep->speed == TUSB_SPEED_HIGH ? 1 + (ep->uframe & 7) : 3] = 0;
  hcd_dcache_clean(td, sizeof(*td));
}

// Called with the controller interrupt excluded. Only arm a frame after its
// preceding visit has ended, so long intervals cannot alias onto the short ring.
static void iso_arm(iso_ep_t* ep, uint32_t now) {
  if ((int32_t) (ep->uframe - now) >= (int32_t) ((FRAMELIST_SIZE - 1) * 8)) {
    return;
  }
  iso_td_t* td = ep->td;
  uint32_t const buffer = (uint32_t) ep->buffer;
  uint32_t const page = buffer & ~0xfffu;
  uint8_t const dir = tu_edpt_dir(ep->ep_addr);
  if (ep->speed == TUSB_SPEED_HIGH) {
    ehci_itd_t* itd = &td->itd;
    // One service interval is at most 3072 bytes, spanning at most two pages.
    // BufferPointer[2] keeps only the endpoint's Mult field set at open.
    itd->BufferPointer[0] = page | (itd->BufferPointer[0] & 0xfff);
    itd->BufferPointer[1] = (page + 4096) | (itd->BufferPointer[1] & 0xfff);
    uint8_t const slot = ep->uframe & 7;
    td->words[1 + slot] = (buffer & 0xfff) | TU_BIT(15) | ((uint32_t) ep->buflen << 16);
  } else {
    ehci_sitd_t* sitd = &td->sitd;
    sitd->buffer[0] = buffer;
    sitd->buffer[1] = page + 4096;
    if (dir == TUSB_DIR_OUT && !ehci_data.iso_frame_offset) {
      uint8_t const count = (uint8_t) tu_max32(1, (ep->buflen + 187u) / 188u);
      sitd->int_smask = (uint8_t) ((1u << count) - 1u);
      sitd->buffer[1] |= count | (count > 1 ? TU_BIT(3) : 0); // T-count, TP=Begin/All
    }
    // Reset status, split progress and page selection; retain endpoint/masks.
    td->words[3] = ((uint32_t) ep->buflen << 16) | TU_BIT(31);
  }
  // Publish an inactive descriptor and its link before publishing Active.
  // The previous request in this slot was unlinked before its completion event.
  ehci_link_t* head = &ehci_data.period_framelist[(ep->uframe >> 3) % FRAMELIST_SIZE];
  td->itd.next = *head;
  hcd_dcache_clean(td, sizeof(*td));
  head->address = (uint32_t) td | ((ep->speed == TUSB_SPEED_HIGH ? EHCI_QTYPE_ITD : EHCI_QTYPE_SITD) << 1);
  hcd_dcache_clean((void*) tu_align32((uint32_t) head), 32);
  if (ep->speed == TUSB_SPEED_HIGH) {
    td->itd.xact[ep->uframe & 7].active = 1;
  } else {
    td->sitd.active = 1;
  }
  ep->armed = true;
  hcd_dcache_clean(td, sizeof(*td));
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
  uint32_t const now = iso_now();
  uint32_t earliest = iso_earliest(now);
  if (ep->speed == TUSB_SPEED_FULL) {
    earliest = (earliest + 7) & ~7u;
  }
  uint32_t scheduled = ep->uframe;
  if ((int32_t) (scheduled - earliest) < 0) {
    uint32_t const behind = earliest - scheduled;
    // ISO intervals are powers of two. Round the distance, retaining phase.
    scheduled += (behind + ep->interval - 1) & ~(ep->interval - 1);
  }
  if (ep->speed == TUSB_SPEED_FULL) {
    scheduled = (scheduled + 7) & ~7u;
  }
  ep->buffer = buffer;
  ep->buflen = buflen;
  ep->uframe = scheduled;
  ep->armed = false;
  ep->busy = true;
  iso_arm(ep, now);
  hcd_int_enable(rhport);
  return true;
}

static void iso_process(bool in_isr) {
  // Keep the extended clock running even when all open endpoints are idle.
  (void) iso_now();
  for (size_t i = iso_ep_next(0); i < QTD_MAX; i = iso_ep_next(i + 1)) {
    iso_ep_t* ep = &ehci_data.qtd_pool[i].iso;
    if (!ep->busy) {
      continue;
    }
    // Retiring an earlier endpoint may have waited for periodic DMA to stop.
    uint32_t now = iso_now();
    if (!ep->armed && (int32_t) (ep->uframe - iso_earliest(now)) >= 0) {
      iso_arm(ep, now);
    }
    // Future requests cannot complete yet; keep descriptor cache lines alone.
    if ((int32_t) (now - ep->uframe) < 0) {
      continue;
    }
    bool active = true;
    bool error = false;
    uint32_t actual = 0;
    iso_td_t* td = ep->armed ? ep->td : NULL;
    if (ep->armed) {
      if (CFG_TUH_MEM_DCACHE_ENABLE) {
        hcd_dcache_invalidate(td, sizeof(*td));
      }
      if (ep->speed == TUSB_SPEED_HIGH) {
        uint8_t const slot = ep->uframe & 7;
        uint32_t const status = td->words[1 + slot]; // iTD transaction status/control
        active = (status & TU_BIT(31)) != 0;
        if (!active) {
          error = (status & (TU_BIT(28) | TU_BIT(29) | TU_BIT(30))) != 0;
          actual = tu_edpt_dir(ep->ep_addr) ? (status >> 16) & 0xfff : ep->buflen;
        }
      } else {
        uint32_t const status = td->words[3]; // siTD transfer status/control
        active = (status & TU_BIT(7)) != 0;
        if (!active) {
          error = (status & 0x7c) != 0; // missed microframe, transaction, babble, buffer, error
          actual = ep->buflen - tu_min16(ep->buflen, (status >> 16) & 0x3ff);
        }
      }
    }
    uint32_t const end = ep->speed == TUSB_SPEED_HIGH ? ep->uframe + 1 :
                        (ep->uframe & ~7u) + 8;
    if (active && (int32_t) (now - end) < 0) {
      continue;
    }
    bool const stopped = active && ep->armed;
    if (active) {
      // The service slot has passed. Clear stale work before this frame-list
      // entry recurs; a late ISO packet must not be silently replayed.
      if (ep->armed) {
        // A delayed interrupt may find this TD executing on a later ring
        // visit. Quiesce DMA before returning ownership of its buffer.
        ehci_disable_schedule(ehci_data.regs, true);
        now = iso_now();
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
    if (CFG_TUH_MEM_DCACHE_ENABLE && tu_edpt_dir(ep->ep_addr) && actual != 0) {
      if (!hcd_dcache_invalidate(ep->buffer, actual)) {
        error = true;
        actual = 0;
      }
    }
    if (ep->armed) {
      iso_td_unlink(ep);
      ep->armed = false;
    }
    if (stopped) {
      ehci_enable_schedule(ehci_data.regs, true);
    }
    ep->uframe += ep->interval;
    ep->busy = false;
    hcd_event_xfer_complete(ep->daddr, ep->ep_addr, actual,
                           error ? XFER_RESULT_FAILED : XFER_RESULT_SUCCESS, in_isr);
  }
}

// Close unconditionally, or cancel only work whose service slot is still ahead.
static bool iso_stop(uint8_t rhport, iso_ep_t* ep, bool close) {
  TU_VERIFY(close || ep->busy);
  hcd_int_disable(rhport);
  bool const stopped = close || ep->armed;
  if (stopped) {
    ehci_disable_schedule(ehci_data.regs, true);
  }
  uint32_t const earliest = close ? 0 : iso_earliest(iso_now());
  bool queued = true;
  if (!close && ep->armed) {
    iso_td_t* td = ep->td;
    hcd_dcache_invalidate(td, sizeof(*td));
    bool const active = ep->speed == TUSB_SPEED_HIGH ?
      td->itd.xact[ep->uframe & 7].active : td->sitd.active;
    queued = active && (int32_t) (ep->uframe - earliest) >= 0;
  }
  if (queued) {
    if (ep->armed) {
      iso_td_unlink(ep);
    }
    if (close) {
      iso_ep_free(ep);
      if (iso_ep_next(0) == QTD_MAX) {
        ehci_data.regs->inten &= ~EHCI_INT_MASK_NXP_SOF;
        ehci_data.regs->command_bm.int_threshold = ehci_data.iso_saved_itc;
      }
    } else {
      ep->armed = false;
      ep->busy = false;
      ep->uframe = earliest;
    }
  }
  if (stopped) {
    ehci_enable_schedule(ehci_data.regs, true);
  }
  hcd_int_enable(rhport);
  return queued;
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
#if defined(TUP_USBIP_CHIPIDEA_HS) && CFG_TUH_CHIPIDEA_ISO_ENABLE
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
    hcd_port_reset(rhport);
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
  hcd_dcache_invalidate(qhd, sizeof(ehci_qhd_t)); // HC may have updated the overlay
  volatile ehci_qtd_t *qtd_overlay = &qhd->qtd_overlay;

  // process non-active (completed) QHD with attached (scheduled) TD
  if ( !qtd_overlay->active ) {
    // An unrelated periodic interrupt can arrive before the controller has
    // fetched a newly attached qTD. The inactive overlay then still belongs
    // to the previous transfer; only a retired qTD establishes completion.
    hcd_dcache_invalidate(qtd, sizeof(ehci_qtd_t));
    if (qtd->active) {
      return;
    }
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
    // something seriously wrong, maybe forget to flush/invalidate cache
    TU_BREAKPOINT();
    TU_LOG1("  HC halted\r\n");
    return;
  }

#if defined(TUP_USBIP_CHIPIDEA_HS) && CFG_TUH_CHIPIDEA_ISO_ENABLE
  if (int_status & regs->inten & EHCI_INT_MASK_NXP_SOF) {
    regs->status = EHCI_INT_MASK_NXP_SOF;
  }
#endif

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
#if defined(TUP_USBIP_CHIPIDEA_HS) && CFG_TUH_CHIPIDEA_ISO_ENABLE
  // SOF and completion commonly arrive together. Scan ISO only once, keeping
  // interrupt work short enough for task context to replenish the next slot.
  if (usb_int || (int_status & regs->inten & EHCI_INT_MASK_NXP_SOF)) {
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
  current->address = ((uint32_t) entry) | (type << 1);
}

// Remove a queue head from the list.
// Per EHCI 4.8.2 the removed qhd's next is linked to list head (which always reachable by Host Controller)
// TODO support iTD/siTD
TU_ATTR_ALWAYS_INLINE static inline void list_remove(ehci_link_t* head, ehci_link_t* prev, ehci_qhd_t* qhd) {
  // TODO deactivate all TD, wait for QHD to inactive before removal
  prev->address = qhd->next.address;

  // link the removed qhd's next to list head
  qhd->next.address = ((uint32_t) head) | (EHCI_QTYPE_QHD << 1);

  if (qhd_is_periodic(qhd)) {
    // period list queue element is guarantee to be free in the next frame (1 ms)
    qhd->used = 0;
  } else {
    // async list use async advance handshake. Mark as removing, will completely re-usable when async advance isr occurs
    qhd->removing = 1;
  }

  hcd_dcache_clean(qhd, sizeof(ehci_qhd_t));
  hcd_dcache_clean(prev, sizeof(ehci_qhd_t));
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
#if defined(TUP_USBIP_CHIPIDEA_HS) && CFG_TUH_CHIPIDEA_ISO_ENABLE
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
  uint16_t const key = tu_htole16(tu_u16(ep_addr, dev_addr));
  if (tu_edpt_number(ep_addr) == 0) {
    result.qhd = qhd_control(dev_addr);
    return result;
  }

#if defined(TUP_USBIP_CHIPIDEA_HS) && CFG_TUH_CHIPIDEA_ISO_ENABLE
  // ISO lookup fields change only on task-context open/close; no IRQ lock needed.
  for (size_t i = iso_ep_next(0); i < QTD_MAX; i = iso_ep_next(i + 1)) {
    iso_ep_t* iso = &ehci_data.qtd_pool[i].iso;
    if (iso->ep_key == key) {
      result.iso = iso;
      return result;
    }
  }
#endif
  // Protect QH ownership flags, which can change during async advance.
  usbh_spin_lock(false);
  for (size_t i = 0; i < QHD_MAX; i++) {
#if defined(TUP_USBIP_CHIPIDEA_HS) && CFG_TUH_CHIPIDEA_ISO_ENABLE
    if (qhd_is_iso(i)) {
      continue;
    }
#endif
    ehci_qhd_t* qhd = &ehci_data.qhd_pool[i].qhd;
    if (qhd->used && !qhd->removing && qhd->ep_key == key) {
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
  p_qhd->ep_key       = tu_htole16(tu_u16(ep_desc->bEndpointAddress & 0x8f, dev_addr & 0x7f));
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

  // clean and invalidate cache before physically write
  hcd_dcache_clean_invalidate(qtd, sizeof(ehci_qtd_t));

  qhd->qtd_overlay.next.address = (uint32_t) qtd;
  hcd_dcache_clean_invalidate(qhd, sizeof(ehci_qhd_t));
}

// Remove an attached TD from queue head
static void qhd_remove_qtd(ehci_qhd_t *qhd) {
  ehci_qtd_t * volatile qtd = qhd->attached_qtd;

  qhd->attached_qtd = NULL;
  qhd->attached_buffer = 0;
  hcd_dcache_clean(qhd, sizeof(ehci_qhd_t));

  qtd->used = 0; // free QTD
  hcd_dcache_clean(qtd, sizeof(ehci_qtd_t));
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
#if defined(TUP_USBIP_CHIPIDEA_HS) && CFG_TUH_CHIPIDEA_ISO_ENABLE
    if (ehci_data.qtd_is_iso[i / 32] & TU_BIT(i % 32)) {
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

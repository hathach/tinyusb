/*
 * SPDX-FileCopyrightText: Copyright (c) 2019 Ha Thach (tinyusb.org)
 * SPDX-License-Identifier: MIT
 *
 * This file is part of the TinyUSB stack.
 */

#include "tusb_option.h"

#if CFG_TUD_ENABLED && defined(TUP_USBIP_CHIPIDEA_HS)

#include "device/dcd.h"
#include "ci_hs_type.h"

#if CFG_TUSB_MCU == OPT_MCU_MIMXRT1XXX
  #include "ci_hs_imxrt.h"

  #if CFG_TUD_MEM_DCACHE_ENABLE
  bool dcd_dcache_clean(const void *addr, uint32_t data_size) {
    return imxrt_dcache_clean(addr, data_size);
  }

  bool dcd_dcache_invalidate(const void *addr, uint32_t data_size) {
    return imxrt_dcache_invalidate(addr, data_size);
  }

  bool dcd_dcache_clean_invalidate(const void *addr, uint32_t data_size) {
    return imxrt_dcache_clean_invalidate(addr, data_size);
  }
  #endif

#elif TU_CHECK_MCU(OPT_MCU_LPC18XX, OPT_MCU_LPC43XX)
  #include "ci_hs_lpc18_43.h"

#elif TU_CHECK_MCU(OPT_MCU_MCXN9)
  // MCX N9 only port 1 use this controller
  #include "ci_hs_mcx.h"

#elif TU_CHECK_MCU(OPT_MCU_HPM)
  #include "ci_hs_hpm.h"

#elif TU_CHECK_MCU(OPT_MCU_RW61X)
  #include "ci_hs_rw61x.h"

#else
  #error "Unsupported MCUs"
#endif

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+

// ENDPTCTRL
enum {
  ENDPTCTRL_TYPE_POS = 2, // Endpoint type is 2-bit field
};

enum {
  ENDPTCTRL_STALL          = TU_BIT(0),
  ENDPTCTRL_TOGGLE_INHIBIT = TU_BIT(5), // used for test only
  ENDPTCTRL_TOGGLE_RESET   = TU_BIT(6),
  ENDPTCTRL_ENABLE         = TU_BIT(7),
};

#define ENDPTCTRL_TYPE(_type) ((_type) << ENDPTCTRL_TYPE_POS)
#define ENDPTCTRL_RESET_MASK  (ENDPTCTRL_TYPE(TUSB_XFER_BULK) | (ENDPTCTRL_TYPE(TUSB_XFER_BULK) << 16u))

// USBSTS, USBINTR
enum {
  INTR_USB         = TU_BIT(0),
  INTR_ERROR       = TU_BIT(1),
  INTR_PORT_CHANGE = TU_BIT(2),
  INTR_RESET       = TU_BIT(6),
  INTR_SOF         = TU_BIT(7),
  INTR_SUSPEND     = TU_BIT(8),
  INTR_NAK         = TU_BIT(16)
};

// Queue Transfer Descriptor
typedef struct {
  // Word 0: Next QTD Pointer
  uint32_t next; ///< Next link pointer This field contains the physical memory address of the next dTD to be processed

  // Word 1: qTQ Token
  uint32_t                     : 3;
  volatile uint32_t xact_err   : 1;
  uint32_t                     : 1;
  volatile uint32_t buffer_err : 1;
  volatile uint32_t halted     : 1;
  volatile uint32_t active     : 1;
  uint32_t                     : 2;
  uint32_t iso_mult_override   : 2; ///< This field can be used for transmit ISOs to override the MULT field in the dQH.
                                    ///< This field must be zero for all packet types that are not transmit-ISO.
  uint32_t                          : 3;
  uint32_t          int_on_complete : 1;
  volatile uint32_t total_bytes     : 15;
  uint32_t                          : 1;

  // Word 2-6: Buffer Page Pointer List, Each element in the list is a 4K page aligned, physical memory address. The
  // lower 12 bits in each pointer are reserved (except for the first one) as each memory pointer must reference the
  // start of a 4K page
  uint32_t buffer[5]; ///< buffer1 has frame_n for TODO Isochronous

  //--------------------------------------------------------------------+
  // TD is 32 bytes aligned but occupies only 28 bytes
  // Therefore there are 4 bytes padding that we can use.
  //--------------------------------------------------------------------+
  uint16_t expected_bytes;
  uint8_t  reserved[2];
} dcd_qtd_t;

TU_VERIFY_STATIC(sizeof(dcd_qtd_t) == 32, "size is not correct");

// Queue Head
typedef struct {
  // Word 0: Capabilities and Characteristics
  uint32_t : 15; ///< Number of packets executed per transaction descriptor 00 - Execute N transactions as demonstrated
                 ///< by the USB variable length protocol where N is computed using Max_packet_length and the
                 ///< Total_bytes field in the dTD. 01 - Execute one transaction 10 - Execute two transactions 11 -
                 ///< Execute three transactions Remark: Non-isochronous endpoints must set MULT = 00. Remark:
                 ///< Isochronous endpoints must set MULT = 01, 10, or 11 as needed.
  uint32_t int_on_setup            : 1;  ///< Interrupt on setup This bit is used on control type endpoints to indicate if USBINT is
                                         ///< set in response to a setup being received.
  uint32_t max_packet_size         : 11; ///< Endpoint's wMaxPacketSize
  uint32_t                         : 2;
  uint32_t zero_length_termination : 1;  ///< This bit is used for non-isochronous endpoints to indicate when a zero-length packet is received to
                                         ///< terminate transfers in case the total transfer length is “multiple”. 0 - Enable zero-length packet to
                                         ///< terminate transfers equal to a multiple of Max_packet_length (default). 1 - Disable zero-length packet on
                                         ///< transfers that are equal in length to a multiple Max_packet_length.
  uint32_t iso_mult                : 2;  ///<

  // Word 1: Current qTD Pointer
  volatile uint32_t qtd_addr;

  // Word 2-9: Transfer Overlay
  volatile dcd_qtd_t qtd_overlay;

  // Word 10-11: Setup request (control OUT only)
  volatile tusb_control_request_t setup_request;

  //--------------------------------------------------------------------+
  // QHD is 64 bytes aligned but occupies only 48 bytes
  // Therefore there are 16 bytes padding that we can use.
  //--------------------------------------------------------------------+
  tu_fifo_t *ff;
  uint8_t    reserved[12];
} dcd_qhd_t;

TU_VERIFY_STATIC(sizeof(dcd_qhd_t) == 64, "size is not correct");

//--------------------------------------------------------------------+
// Variables
//--------------------------------------------------------------------+

#define QTD_NEXT_INVALID 0x01

// Bounded spin for register waits. The longest legitimate wait is a flush held off by a packet
// already in progress: ~50 us for a full-speed 64-byte packet, a low thousands of dependent
// register reads, so healthy hardware never approaches this bound. Exceeding it means the
// controller has stopped responding, and the spin then only serves to keep an ISR (or an
// IRQ-masked caller) from hanging outright - the 3 ms reset-cleanup window of IMXRT1060RM 42.5.6.2.1 (p.2394)
// is already unreachable in that state, and the manual's remedy there is a controller reset.
#define CI_HS_BUSY_SPIN 10000u

typedef struct {
  // Must be at 2K alignment
  // Each endpoint with direction (IN/OUT) occupies a queue head
  // for portability, TinyUSB only queue 1 TD for each Qhd
  dcd_qhd_t qhd[TUP_DCD_ENDPOINT_MAX][2] TU_ATTR_ALIGNED(64);
  dcd_qtd_t qtd[TUP_DCD_ENDPOINT_MAX][2] TU_ATTR_ALIGNED(32);
} dcd_data_t;

CFG_TUD_MEM_SECTION TU_ATTR_ALIGNED(2048) static dcd_data_t _dcd_data;

// What the next Port Change Detect will be. Each one is preceded by the interrupt that causes it:
// a reset interrupt for the end of a bus reset - where the speed first becomes final - or a
// suspend interrupt for the resume that ends the suspend. A suspend itself raises no port change,
// which is why there is no such value here. Indexed by rhport, which is 0 or 1 on every ci_hs
// variant (NOT the controller count: mcx/rw61x map rhport 1 to controller 0).
enum {
  PORT_CHANGE_REASON_RESET  = 0,
  PORT_CHANGE_REASON_RESUME = 1,
};
static volatile uint8_t _port_change_reason[2];

//--------------------------------------------------------------------+
// Prototypes and Helper Functions
//--------------------------------------------------------------------+

TU_ATTR_ALWAYS_INLINE static inline uint8_t ci_ep_count(const ci_hs_regs_t *dcd_reg) {
  return dcd_reg->DCCPARAMS & DCCPARAMS_DEN_MASK;
}

static bool controller_reset(uint8_t rhport);

//--------------------------------------------------------------------+
// Controller API
//--------------------------------------------------------------------+

// Flush endpoint buffers, following IMXRT1060RM 42.5.6.6.5 Flushing/De-priming an Endpoint
// (p.2413): write ENDPTFLUSH, wait for the controller
// to acknowledge, then confirm ENDPTSTAT went to zero. The controller refuses the flush when a
// packet is in progress, and the manual requires the procedure be repeated until it takes.
// Callers proceed regardless of the result; the bound only prevents an ISR-context hang on dead
// hardware.
static bool flush_endpoints(ci_hs_regs_t *dcd_reg, uint32_t mask) {
  uint32_t guard = CI_HS_BUSY_SPIN;
  do {
    dcd_reg->ENDPTFLUSH = mask;
    while (dcd_reg->ENDPTFLUSH & mask) {
      if (!guard--) {
        return false;
      }
    }
  } while ((dcd_reg->ENDPTSTAT & mask) && guard--);

  return !(dcd_reg->ENDPTSTAT & mask);
}

/// Everything the manual asks of the DCD when a reset is detected, in its order: clear the setup
/// and completion semaphores, cancel every prime, check the reset is still being driven, and free
/// the dTDs. All of it belongs inside the reset window (IMXRT1060RM 42.5.6.2.1, p.2394); nothing
/// is left for the port change that ends the reset, which only reports the negotiated speed.
static void bus_reset_begin(uint8_t rhport) {
  ci_hs_regs_t *dcd_reg = CI_HS_REG(rhport);

  // The reset value for all endpoint types is the control endpoint. If one endpoint
  // direction is enabled and the paired endpoint of opposite direction is disabled, then the
  // endpoint type of the unused direction must be changed from the control type to any other
  // type (e.g. bulk). Leaving an un-configured endpoint control will cause undefined behavior
  // for the data PID tracking on the active endpoint.
  const uint8_t ep_count = ci_ep_count(dcd_reg);
  for (uint8_t i = 1; i < ep_count; i++) {
    dcd_reg->ENDPTCTRL[i] = ENDPTCTRL_RESET_MASK;
  }

  //------------- Clear All Registers -------------//
  dcd_reg->ENDPTNAK       = dcd_reg->ENDPTNAK;
  dcd_reg->ENDPTNAKEN     = 0;
  dcd_reg->ENDPTSETUPSTAT = dcd_reg->ENDPTSETUPSTAT;
  dcd_reg->ENDPTCOMPLETE  = dcd_reg->ENDPTCOMPLETE;

  uint32_t guard = CI_HS_BUSY_SPIN;
  while (dcd_reg->ENDPTPRIME && guard--) {}
  dcd_reg->ENDPTFLUSH = 0xFFFFFFFFUL;

  // All of the above must land while the reset is still being driven - it lasts at least 3 ms.
  // Arriving late leaves the controller in an undefined state, and the manual's remedy is to
  // hardware-reset it. That clears Run/Stop, so the device detaches and the host will drive a
  // fresh reset and enumeration - which is why nothing below this point is worth doing here.
  if (!(dcd_reg->PORTSC1 & PORTSC1_PORT_RESET)) {
    TU_LOG1("ci_hs: reset cleanup ran past the end of the reset, resetting controller\r\n");
    controller_reset(rhport);
    return; // the controller detached; the host's next reset redoes everything below
  }

  //------------- Free all allocated dTDs: the controller will not execute them again -------------//
  tu_memclr(&_dcd_data, sizeof(dcd_data_t));

  //------------- Set up Control Endpoints (0 OUT, 1 IN) -------------//
  _dcd_data.qhd[0][0].zero_length_termination = _dcd_data.qhd[0][1].zero_length_termination = 1;
  _dcd_data.qhd[0][0].max_packet_size = _dcd_data.qhd[0][1].max_packet_size = CFG_TUD_ENDPOINT0_SIZE;
  _dcd_data.qhd[0][0].qtd_overlay.next = _dcd_data.qhd[0][1].qtd_overlay.next = QTD_NEXT_INVALID;

  _dcd_data.qhd[0][0].int_on_setup = 1; // OUT only

  dcd_dcache_clean_invalidate(&_dcd_data, sizeof(dcd_data_t));
}

/// Reset the controller and bring it back up in device mode. Also the manual's remedy when the
/// reset cleanup misses its window: the controller reset clears Run/Stop and detaches the device,
/// so it must be re-initialised completely afterwards (IMXRT1060RM 42.5.6.2.1, p.2394).
static bool controller_reset(uint8_t rhport) {
  ci_hs_regs_t *dcd_reg = CI_HS_REG(rhport);

  tu_memclr(&_dcd_data, sizeof(dcd_data_t));

  // Reset controller
  dcd_reg->USBCMD |= USBCMD_RESET;
  uint32_t guard = CI_HS_BUSY_SPIN;
  while ((dcd_reg->USBCMD & USBCMD_RESET) && guard--) {}
  TU_VERIFY(!(dcd_reg->USBCMD & USBCMD_RESET)); // reached from the ISR too, so never halt here

  // Set mode to device, must be set immediately after reset
  uint32_t usbmode = dcd_reg->USBMODE & ~USBMOD_CM_MASK;
  usbmode |= USBMODE_CM_DEVICE;
  dcd_reg->USBMODE = usbmode;

  #ifdef CI_HS_SET_AHB_BURST
  CI_HS_SET_AHB_BURST(rhport);
  #endif

  #ifdef CFG_TUD_CI_HS_VBUS_CHARGE
  dcd_reg->OTGSC = OTGSC_VBUS_CHARGE | OTGSC_OTG_TERMINATION;
  #else
  dcd_reg->OTGSC = OTGSC_VBUS_DISCHARGE | OTGSC_OTG_TERMINATION;
  #endif

  #if !TUD_OPT_HIGH_SPEED
  dcd_reg->PORTSC1 |= PORTSC1_FORCE_FULL_SPEED;
  #endif

  #if TU_CHECK_MCU(OPT_MCU_HPM)
  dcd_reg->PORTSC1 &= ~USB_PORTSC1_STS_MASK;
  #endif

  dcd_dcache_clean_invalidate(&_dcd_data, sizeof(dcd_data_t));

  _port_change_reason[rhport] = PORT_CHANGE_REASON_RESET;

  dcd_reg->ENDPTLISTADDR = (uint32_t)_dcd_data.qhd; // Endpoint List Address has to be 2K alignment
  dcd_reg->USBSTS        = dcd_reg->USBSTS;
  dcd_reg->USBINTR       = INTR_USB | INTR_ERROR | INTR_PORT_CHANGE | INTR_RESET | INTR_SUSPEND;

  uint32_t usbcmd = dcd_reg->USBCMD;
  usbcmd &= ~USBCMD_INTR_THRESHOLD_MASK; // Interrupt Threshold Interval = 0
  usbcmd |= USBCMD_RUN_STOP;             // run

  dcd_reg->USBCMD = usbcmd;

  return true;
}

bool dcd_init(uint8_t rhport, const tusb_rhport_init_t *rh_init) {
  (void)rh_init;
  ci_hs_regs_t *dcd_reg = CI_HS_REG(rhport);

  TU_ASSERT(ci_ep_count(dcd_reg) <= TUP_DCD_ENDPOINT_MAX);

  #if TU_CHECK_MCU(OPT_MCU_HPM)
  usb_phy_init((USB_Type *)dcd_reg, false);
  #endif

  return controller_reset(rhport);
}

bool dcd_deinit(uint8_t rhport) {
  ci_hs_regs_t* dcd_reg = CI_HS_REG(rhport);
  _port_change_reason[rhport] = PORT_CHANGE_REASON_RESET;

  // disable all interrupt
  dcd_reg->USBINTR = 0;

  // unattach from bus
  dcd_reg->USBCMD &= ~USBCMD_RUN_STOP;

  // flush all endpoints
  uint32_t guard = CI_HS_BUSY_SPIN;
  while (dcd_reg->ENDPTPRIME && guard--) {}
  flush_endpoints(dcd_reg, 0xFFFFFFFF);

  return true;
}

void dcd_int_enable(uint8_t rhport) {
  CI_DCD_INT_ENABLE(rhport);
}

void dcd_int_disable(uint8_t rhport) {
  CI_DCD_INT_DISABLE(rhport);
}

void dcd_set_address(uint8_t rhport, uint8_t dev_addr) {
  // Response with status first before changing device address. A refused prime means a new
  // setup superseded this transfer; staging an address whose ACK will never arrive would
  // leave the device answering on it, so only arm the address when the status went out.
  if (dcd_edpt_xfer(rhport, tu_edpt_addr(0, TUSB_DIR_IN), NULL, 0, false)) {
    ci_hs_regs_t *dcd_reg = CI_HS_REG(rhport);
    dcd_reg->DEVICEADDR   = (dev_addr << 25) | TU_BIT(24);
  }
}

void dcd_remote_wakeup(uint8_t rhport) {
  ci_hs_regs_t *dcd_reg = CI_HS_REG(rhport);
  dcd_reg->PORTSC1 |= PORTSC1_FORCE_PORT_RESUME;
}

void dcd_connect(uint8_t rhport) {
  ci_hs_regs_t *dcd_reg = CI_HS_REG(rhport);
  dcd_reg->USBCMD |= USBCMD_RUN_STOP;
}

void dcd_disconnect(uint8_t rhport) {
  ci_hs_regs_t *dcd_reg = CI_HS_REG(rhport);
  dcd_reg->USBCMD &= ~USBCMD_RUN_STOP;
}

void dcd_sof_enable(uint8_t rhport, bool en) {
  ci_hs_regs_t *dcd_reg = CI_HS_REG(rhport);
  if (en) {
    dcd_reg->USBINTR |= INTR_SOF;
  } else {
    dcd_reg->USBINTR &= ~INTR_SOF;
  }
}

//--------------------------------------------------------------------+
// HELPER
//--------------------------------------------------------------------+

static void qtd_init(dcd_qtd_t *p_qtd, void *data_ptr, uint16_t total_bytes) {
  dcd_dcache_clean_invalidate((uint32_t *)tu_align((uint32_t)data_ptr, 4), total_bytes);

  tu_memclr(p_qtd, sizeof(dcd_qtd_t));

  p_qtd->next        = QTD_NEXT_INVALID;
  p_qtd->active      = 1;
  p_qtd->total_bytes = p_qtd->expected_bytes = total_bytes;
  p_qtd->int_on_complete                     = true;

  if (data_ptr != NULL) {
    p_qtd->buffer[0] = (uint32_t)data_ptr;

    const uint32_t bufend = p_qtd->buffer[0] + total_bytes;
    for (uint8_t i = 1; i < 5; i++) {
      const uint32_t next_page = tu_align4k(p_qtd->buffer[i - 1]) + 4096;
      if (bufend <= next_page) {
        break;
      }

      p_qtd->buffer[i] = next_page;

      // TODO page[1] FRAME_N for ISO transfer
    }
  }
}

//--------------------------------------------------------------------+
// DCD Endpoint Port
//--------------------------------------------------------------------+
TU_ATTR_ALWAYS_INLINE static inline void ep_ctrl_write(volatile uint32_t *epctrl, uint8_t dir, uint32_t value) {
  if (dir == TUSB_DIR_OUT) {
    *epctrl = (*epctrl & 0xFFFF0000u) | value;
  } else {
    *epctrl = (*epctrl & 0x0000FFFFu) | (value << 16);
  }
}

TU_ATTR_ALWAYS_INLINE static inline void ep_ctrl_mask(volatile uint32_t *epctrl, uint8_t dir, uint32_t and_mask,
                                                      uint32_t or_mask) {
  uint32_t value = *epctrl;
  if (and_mask != 0) {
    value &= (dir == TUSB_DIR_OUT) ? (and_mask | 0xFFFF0000u) : ((and_mask << 16u) | 0x0000FFFFu);
  }
  if (or_mask != 0) {
    value |= (dir == TUSB_DIR_OUT) ? or_mask : (or_mask << 16u);
  }

  *epctrl = value;
}

TU_ATTR_ALWAYS_INLINE static inline void ep_ctrl_set(volatile uint32_t *epctrl, uint8_t dir, uint32_t mask) {
  ep_ctrl_mask(epctrl, dir, 0, mask);
}

TU_ATTR_ALWAYS_INLINE static inline void ep_ctrl_clear(volatile uint32_t *epctrl, uint8_t dir, uint32_t mask) {
  ep_ctrl_mask(epctrl, dir, ~mask, 0);
}

void dcd_edpt_stall(uint8_t rhport, uint8_t ep_addr) {
  const uint8_t epnum = tu_edpt_number(ep_addr);
  const uint8_t dir   = tu_edpt_dir(ep_addr);

  ci_hs_regs_t *dcd_reg = CI_HS_REG(rhport);
  dcd_reg->ENDPTCTRL[epnum] |= ENDPTCTRL_STALL << (dir ? 16 : 0);

  // flush to abort any primed buffer; the aborted transfer's dQH overlay can be left
  // ACTIVE with mid-transfer state - qhd_start_xfer clears it before the next prime
  dcd_reg->ENDPTFLUSH = TU_BIT(epnum + (dir ? 16 : 0));
}

void dcd_edpt_clear_stall(uint8_t rhport, uint8_t ep_addr) {
  const uint8_t epnum = tu_edpt_number(ep_addr);
  const uint8_t dir   = tu_edpt_dir(ep_addr);

  // data toggle also need to be reset
  ci_hs_regs_t *dcd_reg = CI_HS_REG(rhport);
  dcd_reg->ENDPTCTRL[epnum] |= ENDPTCTRL_TOGGLE_RESET << (dir ? 16 : 0);
  dcd_reg->ENDPTCTRL[epnum] &= ~(ENDPTCTRL_STALL << (dir ? 16 : 0));
}

static void qhd_init(dcd_qhd_t *p_qhd, uint16_t max_packet_size, uint8_t iso_mult) {
  tu_memclr(p_qhd, sizeof(dcd_qhd_t));
  p_qhd->zero_length_termination = 1;
  p_qhd->max_packet_size         = max_packet_size;
  p_qhd->iso_mult                = iso_mult;
  p_qhd->qtd_overlay.next        = QTD_NEXT_INVALID;
  dcd_dcache_clean_invalidate(&_dcd_data, sizeof(dcd_data_t));
}

bool dcd_edpt_open(uint8_t rhport, const tusb_desc_endpoint_t *endpoint_desc) {
  ci_hs_regs_t *dcd_reg   = CI_HS_REG(rhport);
  const uint8_t epnum     = tu_edpt_number(endpoint_desc->bEndpointAddress);
  const uint8_t dir       = tu_edpt_dir(endpoint_desc->bEndpointAddress);
  const uint8_t xfer_type = endpoint_desc->bmAttributes.xfer;
  TU_ASSERT(epnum < ci_ep_count(dcd_reg));

  dcd_qhd_t *p_qhd = &_dcd_data.qhd[epnum][dir];
  qhd_init(p_qhd, tu_edpt_packet_size(endpoint_desc), 0u);

  // EP Control
  const uint32_t epctrl = ENDPTCTRL_TYPE(xfer_type) | ENDPTCTRL_ENABLE | ENDPTCTRL_TOGGLE_RESET;
  ep_ctrl_write(&dcd_reg->ENDPTCTRL[epnum], dir, epctrl);

  return true;
}

bool dcd_edpt_iso_alloc(uint8_t rhport, uint8_t ep_addr, uint16_t largest_packet_size) {
  (void)rhport;
  (void)ep_addr;
  (void)largest_packet_size;

  ci_hs_regs_t *dcd_reg = CI_HS_REG(rhport);
  const uint8_t epnum   = tu_edpt_number(ep_addr);
  const uint8_t dir     = tu_edpt_dir(ep_addr);
  TU_ASSERT(epnum < ci_ep_count(dcd_reg));

  // EP Control: set type but not enabled yet
  const uint32_t epctrl = ENDPTCTRL_TYPE(TUSB_XFER_ISOCHRONOUS) | ENDPTCTRL_TOGGLE_RESET;
  ep_ctrl_write(&dcd_reg->ENDPTCTRL[epnum], dir, epctrl);

  return true;
}

bool dcd_edpt_iso_activate(uint8_t rhport, const tusb_desc_endpoint_t *desc_ep) {
  const uint8_t epnum   = tu_edpt_number(desc_ep->bEndpointAddress);
  const uint8_t dir     = tu_edpt_dir(desc_ep->bEndpointAddress);
  ci_hs_regs_t *dcd_reg = CI_HS_REG(rhport);
  TU_ASSERT(epnum < ci_ep_count(dcd_reg));

  dcd_qhd_t         *p_qhd     = &_dcd_data.qhd[epnum][dir];
  volatile uint32_t *endptctrl = &dcd_reg->ENDPTCTRL[epnum];

  // _dcd_data.qhd[epnum][dir].qtd_overlay.halted = 1;
  // dcd_dcache_clean_invalidate(&_dcd_data, sizeof(dcd_data_t));

  // Flush EP
  flush_endpoints(dcd_reg, TU_BIT(epnum + (dir ? 16 : 0)));

  // disable to change max packet size
  ep_ctrl_clear(endptctrl, dir, ENDPTCTRL_ENABLE);

  qhd_init(p_qhd, tu_edpt_packet_size(desc_ep), 1u);

  ep_ctrl_set(endptctrl, dir, ENDPTCTRL_ENABLE);

  return true;
}

void dcd_edpt_close_all(uint8_t rhport) {
  ci_hs_regs_t *dcd_reg = CI_HS_REG(rhport);

  // Disable all non-control endpoints
  const uint8_t ep_count = ci_ep_count(dcd_reg);
  for (uint8_t epnum = 1; epnum < ep_count; epnum++) {
    _dcd_data.qhd[epnum][TUSB_DIR_OUT].qtd_overlay.halted = 1;
    _dcd_data.qhd[epnum][TUSB_DIR_IN].qtd_overlay.halted  = 1;

    dcd_reg->ENDPTFLUSH       = TU_BIT(epnum) | TU_BIT(epnum + 16);
    dcd_reg->ENDPTCTRL[epnum] = ENDPTCTRL_RESET_MASK;
  }
}

static bool qhd_start_xfer(uint8_t rhport, uint8_t epnum, uint8_t dir) {
  ci_hs_regs_t *dcd_reg = CI_HS_REG(rhport);
  dcd_qhd_t    *p_qhd   = &_dcd_data.qhd[epnum][dir];
  dcd_qtd_t    *p_qtd   = &_dcd_data.qtd[epnum][dir];

  p_qhd->qtd_overlay.halted = false;           // clear any previous error
  p_qhd->qtd_overlay.active = false;           // a flushed prime leaves stale ACTIVE state; clear it so the fresh qtd loads
  p_qhd->qtd_overlay.next   = (uint32_t)p_qtd; // link qtd to qhd

  // flush cache
  dcd_dcache_clean_invalidate(&_dcd_data, sizeof(dcd_data_t));

  if (epnum == 0) {
    // Setup lockout (IMXRT1060RM 42.5.6.4.2.1 Setup Phase, p.2403): never prime EP0 while a new
    // SETUP is pending. The ISR
    // normally consumes ENDPTSETUPSTAT quickly; if the guard trips, fail the transfer so usbd
    // releases the endpoint (a pending SETUP supersedes this response anyway; without one, usbd
    // stalls EP0 and the host recovers with a fresh control transfer).
    uint32_t guard = CI_HS_BUSY_SPIN;
    while (dcd_reg->ENDPTSETUPSTAT & TU_BIT(0)) {
      if (!guard--) {
        return false;
      }
    }
  }

  // start transfer
  dcd_reg->ENDPTPRIME = TU_BIT(epnum + (dir ? 16 : 0));
  return true;
}

bool dcd_edpt_xfer(uint8_t rhport, uint8_t ep_addr, uint8_t *buffer, uint16_t total_bytes, bool is_isr) {
  (void)is_isr;
  const uint8_t epnum = tu_edpt_number(ep_addr);
  const uint8_t dir   = tu_edpt_dir(ep_addr);

  dcd_qhd_t *p_qhd = &_dcd_data.qhd[epnum][dir];
  dcd_qtd_t *p_qtd = &_dcd_data.qtd[epnum][dir];

  // Prepare qtd
  qtd_init(p_qtd, buffer, total_bytes);

  // Start qhd transfer
  p_qhd->ff = NULL;
  return qhd_start_xfer(rhport, epnum, dir);
}

  #if !CFG_TUD_MEM_DCACHE_ENABLE
// fifo has to be aligned to 4k boundary
// It's incompatible with dcache enabled transfer, since neither address nor size is aligned to cache line
bool dcd_edpt_xfer_fifo(uint8_t rhport, uint8_t ep_addr, tu_fifo_t *ff, uint16_t total_bytes, bool is_isr) {
  (void)is_isr;
  const uint8_t epnum = tu_edpt_number(ep_addr);
  const uint8_t dir   = tu_edpt_dir(ep_addr);

  dcd_qhd_t *p_qhd = &_dcd_data.qhd[epnum][dir];
  dcd_qtd_t *p_qtd = &_dcd_data.qtd[epnum][dir];

  tu_fifo_buffer_info_t fifo_info;

  if (dir) {
    tu_fifo_get_read_info(ff, &fifo_info);
  } else {
    tu_fifo_get_write_info(ff, &fifo_info);
  }

  if (fifo_info.linear.len >= total_bytes) {
    // Linear length is enough for this transfer
    qtd_init(p_qtd, fifo_info.linear.ptr, total_bytes);
  } else {
    // linear part is not enough

    // prepare TD up to linear length
    qtd_init(p_qtd, fifo_info.linear.ptr, fifo_info.linear.len);

    if (!tu_offset4k((uint32_t)fifo_info.wrapped.ptr) && !tu_offset4k(tu_fifo_depth(ff))) {
      // If buffer is aligned to 4K & buffer size is multiple of 4K
      // We can make use of buffer page array to also combine the linear + wrapped length
      p_qtd->total_bytes = p_qtd->expected_bytes = total_bytes;

      for (uint8_t i = 1, page = 0; i < 5; i++) {
        // pick up buffer array where linear ends
        if (p_qtd->buffer[i] == 0) {
          p_qtd->buffer[i] = (uint32_t)fifo_info.wrapped.ptr + 4096 * page;
          page++;
        }
      }
    } else {
      // TODO we may need to carry the wrapped length after the linear part complete
      // for now only transfer up to linear part
    }
  }

  // Start qhd transfer
  p_qhd->ff = ff;
  return qhd_start_xfer(rhport, epnum, dir);
}
  #endif

//--------------------------------------------------------------------+
// ISR
//--------------------------------------------------------------------+

static void process_edpt_complete_isr(uint8_t rhport, uint8_t epnum, uint8_t dir) {
  dcd_qhd_t *p_qhd = &_dcd_data.qhd[epnum][dir];
  dcd_qtd_t *p_qtd = &_dcd_data.qtd[epnum][dir];

  uint8_t result = p_qtd->halted                            ? XFER_RESULT_STALLED
                   : (p_qtd->xact_err || p_qtd->buffer_err) ? XFER_RESULT_FAILED
                                                            : XFER_RESULT_SUCCESS;

  if (result != XFER_RESULT_SUCCESS) {
    ci_hs_regs_t *dcd_reg = CI_HS_REG(rhport);
    // flush to abort error buffer
    dcd_reg->ENDPTFLUSH = TU_BIT(epnum + (dir ? 16 : 0));
  }

  const uint16_t xferred_bytes = p_qtd->expected_bytes - p_qtd->total_bytes;

  if (p_qhd->ff) {
    if (dir == TUSB_DIR_IN) {
      tu_fifo_advance_read_pointer(p_qhd->ff, xferred_bytes);
    } else {
      tu_fifo_advance_write_pointer(p_qhd->ff, xferred_bytes);
    }
  }

  // only number of bytes in the IOC qtd
  dcd_event_xfer_complete(rhport, tu_edpt_addr(epnum, dir), xferred_bytes, result, true);
}

void dcd_int_handler(uint8_t rhport) {
  ci_hs_regs_t *dcd_reg = CI_HS_REG(rhport);

  const uint32_t int_enable = dcd_reg->USBINTR;
  const uint32_t int_status = dcd_reg->USBSTS & int_enable;
  dcd_reg->USBSTS           = int_status; // Acknowledge handled interrupt

  // disabled interrupt sources
  if (int_status == 0) {
    return;
  }

  const uint8_t pci_reason = _port_change_reason[rhport]; // save current pci_reason

  if (int_status & INTR_SUSPEND) {
    _port_change_reason[rhport] = PORT_CHANGE_REASON_RESUME; // next PCI is resume
    dcd_event_bus_signal(rhport, DCD_EVENT_SUSPEND, true);
  }

  // USB Reset Received: register cleanup runs here within the reset window (IMXRT1060RM 42.5.6.2.1, p.2394)
  // and BUS_RESET_START fires now; BUS_RESET_END, with the final speed, is triggered later by PCI.
  if (int_status & INTR_RESET) {
    _port_change_reason[rhport] = PORT_CHANGE_REASON_RESET;
    bus_reset_begin(rhport);
    dcd_event_bus_signal(rhport, DCD_EVENT_BUS_RESET_START, true);
  }

  // Port entered the full/high-speed operational state: the end of a bus reset, or a resume.
  if (int_status & INTR_PORT_CHANGE) {
    if (pci_reason == PORT_CHANGE_REASON_RESUME) {
      dcd_event_bus_signal(rhport, DCD_EVENT_RESUME, true);
    } else {
      // the undefined encoding falls back to full speed
      const uint32_t pspd = (dcd_reg->PORTSC1 & PORTSC1_PORT_SPEED) >> PORTSC1_PORT_SPEED_POS;
      const tusb_speed_t speed = (pspd == PORTSC1_PORT_SPEED_LOW)    ? TUSB_SPEED_LOW :
                                 (pspd == PORTSC1_PORT_SPEED_HIGH)   ? TUSB_SPEED_HIGH : TUSB_SPEED_FULL;
      dcd_event_bus_reset(rhport, speed, true);
      // This reset is over, so the next port change is a resume. Leaving it at RESET instead would
      // dispatch every later resume as another end-of-reset, clearing the queue heads mid-session.
      _port_change_reason[rhport] = PORT_CHANGE_REASON_RESUME;
    }
  }

  // No unplug detection yet, by the manual rather than by omission: IMXRT1060RM 42.7.31 (p.2470) says a zero
  // Current Connect Status means the device "did not attach successfully or was forcibly
  // disconnected by the software writing a zero to the Run bit ... It does not state the device
  // being disconnected or suspended", so a cable pull raises no port change at all. VBUS via
  // OTGSC BSV is the manual's disconnect indicator, and it is board dependent.

  if (int_status & INTR_USB) {
    // Make sure we read the latest version of _dcd_data.
    dcd_dcache_clean_invalidate(&_dcd_data, sizeof(dcd_data_t));

    const uint32_t edpt_complete = dcd_reg->ENDPTCOMPLETE;
    dcd_reg->ENDPTCOMPLETE       = edpt_complete; // acknowledge

    // 42.5.6.6.4 Transfer Completion (p.2413): a failed dTD also sets ENDPTCOMPLETE
    // nothing to do, we will submit xfer as error to usbd
    // if (int_status & INTR_ERROR) { }

    if (edpt_complete) {
      for (uint8_t epnum = 0; epnum < TUP_DCD_ENDPOINT_MAX; epnum++) {
        if (tu_bit_test(edpt_complete, epnum)) {
          process_edpt_complete_isr(rhport, epnum, TUSB_DIR_OUT);
        }
        if (tu_bit_test(edpt_complete, epnum + 16)) {
          process_edpt_complete_isr(rhport, epnum, TUSB_DIR_IN);
        }
      }
    }

    // Set up Received
    // 42.5.6.4.2 Control Endpoint Operation Model (p.2403)
    // Must be after normal transfer complete since it is possible to have both previous control status + new setup
    // in the same frame and we should handle previous status first.
    if (dcd_reg->ENDPTSETUPSTAT) {
      // 42.5.6.4.2.1 Setup Phase (p.2403) steps 1-2: duplicate the setup payload BEFORE clearing
      // ENDPTSETUPSTAT -
      // the clear releases the setup lockout and a back-to-back SETUP (usbtest case 10) can
      // overwrite the queue-head buffer immediately after. The copy is read through the volatile
      // qualifier rather than memcpy'd because C orders volatile accesses only against each
      // other: a plain copy may legally be sunk past the lockout-releasing store below.
      union {
        tusb_control_request_t request;
        uint8_t byte[8];
      } setup;
      const volatile uint8_t *setup_src = (const volatile uint8_t *)&_dcd_data.qhd[0][0].setup_request;
      for (uint8_t i = 0; i < sizeof(setup.request); i++) {
        setup.byte[i] = setup_src[i];
      }
      dcd_reg->ENDPTSETUPSTAT = dcd_reg->ENDPTSETUPSTAT;

      // Retire a status/handshake phase left primed by the previous control sequence
      // (IMXRT1060RM 42.5.6.4.2.1, p.2403), which would otherwise retire the response the task is about to
      // prime for this setup. Skipped when EP0 has nothing primed or priming, since the manual
      // does not want the flush wait in an interrupt handler when it has nothing to do.
      // One volatile read per statement: C leaves their order unspecified within a single
      // expression, which IAR rejects outright (Pa082).
      const uint32_t ep0_mask  = TU_BIT(0) | TU_BIT(16);
      const uint32_t ep0_stat  = dcd_reg->ENDPTSTAT;
      const uint32_t ep0_prime = dcd_reg->ENDPTPRIME;
      if ((ep0_stat | ep0_prime) & ep0_mask) {
        flush_endpoints(dcd_reg, ep0_mask);
      }
      dcd_event_setup_received(rhport, setup.byte, true);
    }
  }

  if (int_status & INTR_SOF) {
    const uint32_t frame = dcd_reg->FRINDEX;
    dcd_event_sof(rhport, frame, true);
  }
}
#endif

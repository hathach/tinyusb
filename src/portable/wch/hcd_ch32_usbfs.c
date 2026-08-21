/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Mitsumine Suzu (verylowfreq)
 * SPDX-FileCopyrightText: Copyright (c) 2024 Ha Thach (tinyusb.org)
 * SPDX-FileCopyrightText: Copyright (c) 2026 Zhenjiang Zhang
 * SPDX-License-Identifier: MIT
 *
 * This file is part of the TinyUSB stack.
 *
 * CH32V20x USBFS host controller driver, SOF-paced.
 *
 * The USBFS SIE executes one transaction immediately when HOST_EP_PID is
 * written, with no frame-boundary alignment. An isochronous endpoint that is
 * re-submitted on completion is therefore polled at the task-loop rate rather
 * than once per 1 ms USB frame. This driver paces isochronous transfers with
 * the SOF interrupt: each ISO endpoint is allowed at most one transaction per
 * USB frame, armed from the SOF tick and from the ISO completion handler.
 */

#include "tusb_option.h"

#if CFG_TUH_ENABLED && defined(TUP_USBIP_WCH_USBFS) && defined(CFG_TUH_WCH_USBIP_USBFS) && CFG_TUH_WCH_USBIP_USBFS

  #include <stdlib.h>
  #include <string.h>

  #include "host/hcd.h"
  #include "host/usbh.h"
  #include "host/usbh_pvt.h"

  #include "bsp/board_api.h"

  #ifdef __GNUC__
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wstrict-prototypes"
  #endif

  #include "ch32v20x.h"

  #ifdef __GNUC__
    #pragma GCC diagnostic pop
  #endif

  #include "ch32v20x_usb.h"

  // Internal DMA buffers, used only for transfers without an application buffer
  // (setup packet, control status stage, clear-stall). Data transfers point the
  // DMA directly at the application buffer (see hardware_start_xfer): these
  // 64-byte buffers would be overflowed by larger packets (e.g. FS ISO up to
  // 1023 bytes), smashing adjacent statistics such as the endpoint record table.
  #define USBFS_RX_BUF_LEN 64
  #define USBFS_TX_BUF_LEN 64
TU_ATTR_ALIGNED(4) static uint8_t USBFS_RX_Buf[USBFS_RX_BUF_LEN];
TU_ATTR_ALIGNED(4) static uint8_t USBFS_TX_Buf[USBFS_TX_BUF_LEN];

  #define USB_XFER_TIMEOUT_MILLIS 100

  #define PANIC(...)                            \
    do {                                        \
      printf("%s() L%d: ", __func__, __LINE__); \
      printf("\r\n[PANIC] " __VA_ARGS__);       \
      while (true) {}                           \
    } while (false)

  #define LOG_CH32_USBFSH(...) TU_LOG3(__VA_ARGS__)

// Busywait for delay microseconds/nanoseconds
TU_ATTR_ALWAYS_INLINE static inline void loopdelay(uint32_t count) {
  volatile uint32_t c = count / 3;
  if (c == 0) {
    return;
  }
  asm volatile("1:                     \n" // loop label
               "    addi  %0, %0, -1   \n" // c--
               "    bne   %0, zero, 1b \n" // if (c != 0) goto loop
               : "+r"(c)                   // c is input/output operand
  );
}

//--------------------------------------------------------------------+
// Endpoint record
//--------------------------------------------------------------------+

// Per-endpoint state. One record per opened (dev_addr, ep_addr). ISO endpoints
// carry a one-slot pending queue (iso_queued/iso_buf/iso_len) plus the frame
// gate (iso_last_frame) so that an early same-frame re-submission does not
// grab the single in-flight slot and starve the other direction.
typedef struct usb_edpt {
  bool configured;

  uint8_t  dev_addr;
  uint8_t  ep_addr;   // full address including direction bit
  uint16_t max_packet_size;
  uint8_t  xfer_type; // TUSB_XFER_*

  // Data toggle (0 or 1) for DATA0/1. Never used for ISO.
  uint8_t data_toggle;

  // ISO pending queue: written from task context (hcd_edpt_xfer), drained by
  // the SOF/TRANSFER ISR. iso_active marks the endpoint that currently owns
  // the in-flight slot.
  bool     iso_queued;
  bool     iso_active;
  uint8_t *iso_buf;
  uint16_t iso_len;
  uint16_t iso_last_frame; // frame in which this endpoint was last armed

  // NAK retry stash (non-ISO control/bulk). Re-armed once per frame from the
  // SOF ISR (arm_nak_retry) so a NAK'd control transfer is retried with 1 ms
  // backoff, not hammered in a tight task loop (which floods the device's
  // control endpoint and starves the bus + main loop).
  bool     is_nak_pending;
  uint16_t nak_last_frame;
  uint16_t nak_xferred;
  uint8_t  nak_backoff; // frames to wait before the next NAK retry (progressive)
  uint16_t buflen;
  uint8_t *buf;
} usb_edpt_t;

static usb_edpt_t usb_edpt_list[CFG_TUH_DEVICE_MAX * CFG_TUH_ENDPOINT_MAX] = {};

// In-flight transfer: the SIE executes one transaction at a time. xfer_type is
// a snapshot taken at arm time so the completion ISR routes by what was
// actually armed, not by the (possibly reused) record.
typedef struct usb_current_xfer_st {
  bool     is_busy;
  uint8_t  dev_addr;
  uint8_t  ep_addr;
  uint8_t  xfer_type; // snapshot at arm time
  uint32_t start_ms;
  uint8_t *buffer;
  uint16_t bufferlen;
  uint16_t xferred_len;
} usb_current_xfer_t;

static volatile usb_current_xfer_t usb_current_xfer_info = {};

// Software SOF frame counter (no readable frame-number register exists on
// USBFSH). Incremented in the SOF ISR; hcd_frame_number() returns it.
static volatile uint16_t g_sof_frame = 0;

static bool interrupt_enabled = false;

// Set while a port reset is in progress: the USBFS hardware can assert a
// DETECT(attach=0) edge while the bus is in reset (SE0). The stack's event
// loop may re-enable the IRQ between hcd_port_reset() and hcd_port_reset_end()
// (OSAL queue ops toggle the interrupt), so DETECT edges must be ignored
// during that window and only the flag cleared. Otherwise the reset edge is
// reported as a device removal and kills the enumeration.
static bool port_reset_in_progress  = false;
static bool int_state_for_portreset = false;

static usb_edpt_t *get_edpt_record(uint8_t dev_addr, uint8_t ep_addr) {
  for (size_t i = 0; i < TU_ARRAY_SIZE(usb_edpt_list); i++) {
    usb_edpt_t *cur = &usb_edpt_list[i];
    if (cur->configured && cur->dev_addr == dev_addr && cur->ep_addr == ep_addr) {
      return cur;
    }
  }
  return NULL;
}

static usb_edpt_t *get_empty_record_slot(void) {
  for (size_t i = 0; i < TU_ARRAY_SIZE(usb_edpt_list); i++) {
    if (!usb_edpt_list[i].configured) {
      return &usb_edpt_list[i];
    }
  }
  return NULL;
}

static usb_edpt_t *add_edpt_record(uint8_t dev_addr, uint8_t ep_addr, uint16_t max_packet_size, uint8_t xfer_type) {
  usb_edpt_t *slot = get_empty_record_slot();
  TU_ASSERT(slot != NULL, NULL);

  slot->dev_addr        = dev_addr;
  slot->ep_addr         = ep_addr;
  slot->max_packet_size = max_packet_size;
  slot->xfer_type       = xfer_type;
  slot->data_toggle     = 0;
  slot->is_nak_pending  = false;
  slot->nak_last_frame  = 0;
  slot->nak_xferred     = 0;
  slot->nak_backoff     = 1;
  slot->buflen          = 0;
  slot->buf             = NULL;
  slot->iso_queued      = false;
  slot->iso_active      = false;
  slot->iso_buf         = NULL;
  slot->iso_len         = 0;
  slot->iso_last_frame  = 0;

  slot->configured = true;

  return slot;
}

static usb_edpt_t *get_or_add_edpt_record(uint8_t dev_addr, uint8_t ep_addr, uint16_t max_packet_size,
                                          uint8_t xfer_type) {
  usb_edpt_t *ret = get_edpt_record(dev_addr, ep_addr);
  if (ret != NULL) {
    return ret;
  }
  return add_edpt_record(dev_addr, ep_addr, max_packet_size, xfer_type);
}

static void remove_edpt_record_for_device(uint8_t dev_addr) {
  for (size_t i = 0; i < TU_ARRAY_SIZE(usb_edpt_list); i++) {
    if (usb_edpt_list[i].configured && usb_edpt_list[i].dev_addr == dev_addr) {
      usb_edpt_list[i].configured = false;
    }
  }
}

// Briefly mask the USBFS IRQ to make task-context reads/writes of state shared
// with the ISR atomic. Save/restore the previous enabled state. Do NOT call
// from within the USBFS ISR (it is already non-preemptible for this IRQ).
static bool usbfs_irq_save(void) {
  bool prev = interrupt_enabled;
  NVIC_DisableIRQ(USBFS_IRQn);
  return prev;
}

static void usbfs_irq_restore(bool prev) {
  if (prev) {
    NVIC_EnableIRQ(USBFS_IRQn);
  }
}

//--------------------------------------------------------------------+
// Hardware helpers
//--------------------------------------------------------------------+

/** Enable or disable USBFS Host function */
static void hardware_init_host(bool enabled) {
  // Reset USBOTG module
  USBOTG_H_FS->BASE_CTRL = USBFS_UC_RESET_SIE | USBFS_UC_CLR_ALL;

  tusb_time_delay_ms_api(1);
  USBOTG_H_FS->BASE_CTRL = 0;

  if (!enabled) {
    USBOTG_H_FS->BASE_CTRL = 0;
  } else {
    hcd_int_disable(0);
    USBOTG_H_FS->BASE_CTRL   = USBFS_UC_HOST_MODE | USBFS_UC_INT_BUSY | USBFS_UC_DMA_EN;
    USBOTG_H_FS->HOST_EP_MOD = USBFS_UH_EP_TX_EN | USBFS_UH_EP_RX_EN;
    USBOTG_H_FS->HOST_RX_DMA = (uint32_t)USBFS_RX_Buf;
    USBOTG_H_FS->HOST_TX_DMA = (uint32_t)USBFS_TX_Buf;
    // DETECT for connect/disconnect, TRANSFER for completions, HST_SOF for
    // the 1ms frame tick that paces isochronous transfers. UIE_TRANSFER stays
    // enabled for the driver's lifetime (no disable/re-enable per transaction).
    USBOTG_H_FS->INT_EN = USBFS_UIE_DETECT | USBFS_UIE_TRANSFER | USBFS_UIE_HST_SOF;
  }
}

// Arm one transaction. The DMA and transfer length are read from the in-flight
// slot; the caller (control/bulk path or arm_iso_drain) sets those first. For
// OUT the caller must pre-write HOST_TX_LEN. is_iso selects the no-handshake
// bit: IN expects no ACK (R_RES on HOST_RX_CTRL), OUT sends no handshake
// (T_RES on HOST_TX_CTRL). Control/setup transfers leave both RES bits clear
// (ACK expected). Writing HOST_EP_PID kicks the SIE immediately; the flag
// clear that follows is the "go" — see the STOP-before-clear rule in
// hcd_int_handler for why the ISR must not clear this flag while HOST_EP_PID
// is still set.
static bool hardware_start_xfer(uint8_t pid, uint8_t ep_addr, uint8_t data_toggle, bool is_iso) {
  LOG_CH32_USBFSH("hardware_start_xfer(pid=%s(0x%02x), ep_addr=0x%02x, toggle=%d, iso=%d)\r\n",
                  pid == USB_PID_IN      ? "IN"
                  : pid == USB_PID_OUT   ? "OUT"
                  : pid == USB_PID_SETUP ? "SETUP"
                                         : "(other)",
                  pid, ep_addr, data_toggle, is_iso ? 1 : 0);

  // WORKAROUND: For LowSpeed device, insert small delay
  bool is_lowspeed_device = tuh_speed_get(usb_current_xfer_info.dev_addr) == TUSB_SPEED_LOW;
  if (is_lowspeed_device) {
    loopdelay(SystemCoreClock / 1000000 * 40);
  }

  // Point the DMA at the transfer buffer itself: the fixed internal TX/RX
  // buffers are only 64 bytes and would be overflowed by larger packets
  // (e.g. 192-byte FS isochronous packets). Zero-length transfers (control
  // status stage, buffer == NULL) fall back to the internal buffer.
  if (pid == USB_PID_IN) {
    USBOTG_H_FS->HOST_RX_DMA =
      (uint32_t)(usb_current_xfer_info.buffer != NULL ? usb_current_xfer_info.buffer : (uint8_t *)USBFS_RX_Buf);
  } else {
    USBOTG_H_FS->HOST_TX_DMA =
      (uint32_t)(usb_current_xfer_info.buffer != NULL ? usb_current_xfer_info.buffer : (uint8_t *)USBFS_TX_Buf);
  }

  uint8_t ep_num   = tu_edpt_number(ep_addr);
  uint8_t pid_edpt = (pid << 4) | (ep_num & 0x0f);
  // Assigning (not OR-ing) the control registers clears the RES bits, so
  // control/setup transfers get ACK-expected behavior.
  USBOTG_H_FS->HOST_TX_CTRL = (data_toggle != 0) ? USBFS_UH_T_TOG : 0;
  USBOTG_H_FS->HOST_RX_CTRL = (data_toggle != 0) ? USBFS_UH_R_TOG : 0;
  if (is_iso) {
    if (pid == USB_PID_IN) {
      USBOTG_H_FS->HOST_RX_CTRL |= USBFS_UH_R_RES; // ISO IN: no handshake
    } else {
      USBOTG_H_FS->HOST_TX_CTRL |= USBFS_UH_T_RES; // ISO OUT: no handshake
    }
  }
  USBOTG_H_FS->HOST_EP_PID = pid_edpt;             // kick the SIE
  USBOTG_H_FS->INT_FG      = USBFS_UIF_TRANSFER;   // clear any stale flag = "go"
  return true;
}

/** Set device address to communicate */
static void hardware_update_device_address(uint8_t dev_addr) {
  // Keep the bit of GP_BIT. Other 7bits are actual device address.
  USBOTG_H_FS->DEV_ADDR = (USBOTG_H_FS->DEV_ADDR & USBFS_UDA_GP_BIT) | (dev_addr & USBFS_USB_ADDR_MASK);
}

/** Set port speed */
static void hardware_update_port_speed(tusb_speed_t speed) {
  LOG_CH32_USBFSH("hardware_update_port_speed(%s)\r\n", speed == TUSB_SPEED_FULL  ? "Full"
                                                        : speed == TUSB_SPEED_LOW ? "Low"
                                                                                  : "(invalid)");
  switch (speed) {
    case TUSB_SPEED_LOW:
      USBOTG_H_FS->BASE_CTRL |= USBFS_UC_LOW_SPEED;
      USBOTG_H_FS->HOST_CTRL |= USBFS_UH_LOW_SPEED;
      USBOTG_H_FS->HOST_SETUP |= USBFS_UH_PRE_PID_EN;
      return;
    case TUSB_SPEED_FULL:
      USBOTG_H_FS->BASE_CTRL &= ~USBFS_UC_LOW_SPEED;
      USBOTG_H_FS->HOST_CTRL &= ~USBFS_UH_LOW_SPEED;
      USBOTG_H_FS->HOST_SETUP &= ~USBFS_UH_PRE_PID_EN;
      return;
    default:
      PANIC("hardware_update_port_speed(%d)\r\n", speed);
  }
}

static void hardware_set_port_address_speed(uint8_t dev_addr) {
  hardware_update_device_address(dev_addr);
  tusb_speed_t rhport_speed = hcd_port_speed_get(0);
  tusb_speed_t dev_speed    = tuh_speed_get(dev_addr);
  hardware_update_port_speed(dev_speed);
  if (rhport_speed == TUSB_SPEED_FULL && dev_speed == TUSB_SPEED_LOW) {
    USBOTG_H_FS->HOST_CTRL &= ~USBFS_UH_LOW_SPEED;
  }
}

static bool hardware_device_attached(void) {
  return USBOTG_H_FS->MIS_ST & USBFS_UMS_DEV_ATTACH;
}

//--------------------------------------------------------------------+
// SOF-paced isochronous scheduling
//--------------------------------------------------------------------+

// Arm one queued, frame-due ISO endpoint into the in-flight slot. Called from
// the SOF ISR (in_isr=true), the ISO completion ISR (in_isr=true), and
// hcd_edpt_xfer's ISO branch (in_isr=false). When called from task context
// the slot publish is guarded against the ISR; from the ISR no guard is needed
// (the single USBFS IRQ cannot preempt itself). At most one endpoint is armed
// per call (single-transaction SIE); a second due endpoint is armed from the
// completion of the first, yielding one IN + one OUT per frame.
static void arm_iso_drain(bool in_isr) {
  if (usb_current_xfer_info.is_busy) {
    return;
  }

  bool prev = in_isr ? false : usbfs_irq_save();

  usb_edpt_t *best = NULL;
  if (!usb_current_xfer_info.is_busy) {
    for (size_t i = 0; i < TU_ARRAY_SIZE(usb_edpt_list); i++) {
      usb_edpt_t *cur = &usb_edpt_list[i];
      if (cur->configured && cur->xfer_type == TUSB_XFER_ISOCHRONOUS && cur->iso_queued && !cur->iso_active &&
          cur->iso_last_frame != g_sof_frame) {
        best = cur;
        break;
      }
    }
  }

  uint8_t  arm_dev = 0;
  uint8_t  arm_ep  = 0;
  uint16_t arm_len = 0;
  bool     arm_in  = false;
  bool     armed   = false;

  if (best != NULL) {
    // Publish the in-flight slot from the endpoint's queued request.
    usb_current_xfer_info.is_busy     = true;
    usb_current_xfer_info.dev_addr    = best->dev_addr;
    usb_current_xfer_info.ep_addr     = best->ep_addr;
    usb_current_xfer_info.xfer_type   = TUSB_XFER_ISOCHRONOUS;
    usb_current_xfer_info.buffer      = best->iso_buf;
    usb_current_xfer_info.bufferlen   = best->iso_len;
    usb_current_xfer_info.xferred_len = 0;
    usb_current_xfer_info.start_ms    = tusb_time_millis_api();

    best->iso_active     = true;
    best->iso_queued     = false;
    best->iso_last_frame = g_sof_frame;

    arm_dev = best->dev_addr;
    arm_ep  = best->ep_addr;
    arm_len = best->iso_len;
    arm_in  = (tu_edpt_dir(best->ep_addr) == TUSB_DIR_IN);
    armed   = true;
  }

  if (!in_isr) {
    usbfs_irq_restore(prev);
  }

  if (armed) {
    // is_busy is set, so no other arm path can race the HOST_EP_PID write.
    hardware_set_port_address_speed(arm_dev);
    if (arm_in) {
      hardware_start_xfer(USB_PID_IN, arm_ep, 0, true);
    } else {
      USBOTG_H_FS->HOST_TX_LEN = arm_len;
      hardware_start_xfer(USB_PID_OUT, arm_ep, 0, true);
    }
  }
}

// Re-arm a NAK'd control/bulk transfer once per USB frame (1 ms backoff) so the
// device has time to finish processing. Called from the SOF ISR, ahead of ISO,
// so a NAK'd control transfer (e.g. a SET_CUR status stage) makes progress even
// while a non-ISO transfer is in flight. At most one endpoint is armed per
// call (single SIE).
static void arm_nak_retry(void) {
  if (usb_current_xfer_info.is_busy) {
    return;
  }
  usb_edpt_t *best = NULL;
  for (size_t i = 0; i < TU_ARRAY_SIZE(usb_edpt_list); i++) {
    usb_edpt_t *cur = &usb_edpt_list[i];
    if (cur->configured && cur->is_nak_pending && (uint16_t)(g_sof_frame - cur->nak_last_frame) >= cur->nak_backoff &&
        (cur->xfer_type == TUSB_XFER_CONTROL || cur->xfer_type == TUSB_XFER_BULK)) {
      best = cur;
      break;
    }
  }
  if (best == NULL) {
    return;
  }
  uint8_t pid                       = (tu_edpt_dir(best->ep_addr) == TUSB_DIR_IN) ? USB_PID_IN : USB_PID_OUT;
  bool    prev                      = usbfs_irq_save();
  usb_current_xfer_info.is_busy     = true;
  usb_current_xfer_info.dev_addr    = best->dev_addr;
  usb_current_xfer_info.ep_addr     = best->ep_addr;
  usb_current_xfer_info.xfer_type   = best->xfer_type;
  usb_current_xfer_info.buffer      = best->buf;
  usb_current_xfer_info.bufferlen   = best->buflen;
  usb_current_xfer_info.xferred_len = best->nak_xferred;
  usb_current_xfer_info.start_ms    = tusb_time_millis_api();
  best->is_nak_pending              = false;
  best->nak_last_frame              = g_sof_frame;
  usbfs_irq_restore(prev);

  hardware_set_port_address_speed(best->dev_addr);
  if (pid == USB_PID_IN) {
    hardware_start_xfer(USB_PID_IN, best->ep_addr, best->data_toggle, false);
  } else {
    USBOTG_H_FS->HOST_TX_LEN = TU_MIN(best->max_packet_size, best->buflen);
    hardware_start_xfer(USB_PID_OUT, best->ep_addr, best->data_toggle, false);
  }
}

// ISO transfer completion. ISO has no handshake (no NAK/STALL): every armed
// transaction completes successfully. RX_LEN holds received bytes for IN;
// for OUT the whole queued packet was sent.
static void iso_transfer_complete(uint8_t request_pid, usb_edpt_t *edpt) {
  uint16_t done_len = (request_pid == USB_PID_IN) ? (uint16_t)USBOTG_H_FS->RX_LEN : usb_current_xfer_info.bufferlen;
  uint8_t  dev_addr = usb_current_xfer_info.dev_addr;
  uint8_t  ep_addr  = usb_current_xfer_info.ep_addr;

  LOG_CH32_USBFSH("ISO %s completed %d bytes\r\n", request_pid == USB_PID_IN ? "IN" : "OUT", done_len);

  // Clear the slot BEFORE posting the completion event: the class re-submits
  // from the completion handler in task context, and a stuck is_busy would
  // make that re-submit's drain bounce.
  bool prev                     = usbfs_irq_save();
  usb_current_xfer_info.is_busy = false;
  edpt->iso_active              = false;
  usbfs_irq_restore(prev);

  hcd_event_xfer_complete(dev_addr, ep_addr, done_len, XFER_RESULT_SUCCESS, true);

  // Let the other direction's queued ISO fire in the same frame.
  arm_iso_drain(true);
}

// Control/bulk/interrupt transfer completion. Reached only when the in-flight
// slot's xfer_type snapshot is not ISO (the hard routing rule in
// hcd_int_handler). Preserves the working multi-packet and NAK/STALL logic.
static void cb_transfer_complete(uint8_t request_pid, uint8_t response_pid, usb_edpt_t *edpt, bool in_isr) {
  uint8_t dev_addr = usb_current_xfer_info.dev_addr;
  uint8_t ep_addr  = usb_current_xfer_info.ep_addr;

  if (USBOTG_H_FS->INT_ST & USBFS_UIS_TOG_OK) {
    // Toggle tracked only for non-ISO (ISO never reaches here).
    edpt->data_toggle ^= 0x01;

    switch (request_pid) {
      case USB_PID_SETUP:
      case USB_PID_OUT: {
        uint16_t tx_len = USBOTG_H_FS->HOST_TX_LEN;
        usb_current_xfer_info.bufferlen -= tx_len;
        usb_current_xfer_info.xferred_len += tx_len;
        if (usb_current_xfer_info.bufferlen == 0) {
          LOG_CH32_USBFSH("USB_PID_%s completed %d bytes\r\n", request_pid == USB_PID_OUT ? "OUT" : "SETUP",
                          usb_current_xfer_info.xferred_len);
          bool prev                     = usbfs_irq_save();
          usb_current_xfer_info.is_busy = false;
          usbfs_irq_restore(prev);
          hcd_event_xfer_complete(dev_addr, ep_addr, usb_current_xfer_info.xferred_len, XFER_RESULT_SUCCESS, in_isr);
          return;
        }
        LOG_CH32_USBFSH("USB_PID_OUT continue...\r\n");
        usb_current_xfer_info.buffer += tx_len;
        uint16_t copylen         = TU_MIN(edpt->max_packet_size, usb_current_xfer_info.bufferlen);
        USBOTG_H_FS->HOST_TX_LEN = copylen; // DMA reads directly from usb_current_xfer_info.buffer
        hardware_start_xfer(USB_PID_OUT, ep_addr, edpt->data_toggle, false);
        return;
      }
      case USB_PID_IN: {
        uint16_t received_len = USBOTG_H_FS->RX_LEN;
        usb_current_xfer_info.xferred_len += received_len;
        uint16_t xferred_len = usb_current_xfer_info.xferred_len;
        LOG_CH32_USBFSH("Read %d bytes\r\n", received_len);
        if (usb_current_xfer_info.buffer != NULL) {
          usb_current_xfer_info.buffer += received_len;
        }
        if ((received_len < edpt->max_packet_size) || (xferred_len == usb_current_xfer_info.bufferlen)) {
          LOG_CH32_USBFSH("USB_PID_IN completed\r\n");
          bool prev                     = usbfs_irq_save();
          usb_current_xfer_info.is_busy = false;
          usbfs_irq_restore(prev);
          hcd_event_xfer_complete(dev_addr, ep_addr, xferred_len, XFER_RESULT_SUCCESS, in_isr);
          return;
        }
        LOG_CH32_USBFSH("Read more data\r\n");
        hardware_start_xfer(USB_PID_IN, ep_addr, edpt->data_toggle, false);
        return;
      }
      default: {
        LOG_CH32_USBFSH("cb_transfer_complete() unexpected response PID: 0x%02x\r\n", response_pid);
        bool prev                     = usbfs_irq_save();
        usb_current_xfer_info.is_busy = false;
        usbfs_irq_restore(prev);
        hcd_event_xfer_complete(dev_addr, ep_addr, 0, XFER_RESULT_FAILED, in_isr);
        return;
      }
    }
  }

  // No TOG_OK: handshake/timeout handling.
  if (response_pid == USB_PID_STALL) {
    LOG_CH32_USBFSH("STALL response\r\n");
    bool prev                     = usbfs_irq_save();
    usb_current_xfer_info.is_busy = false;
    usbfs_irq_restore(prev);
    hcd_edpt_clear_stall(0, dev_addr, ep_addr);
    edpt->data_toggle = 0;
    // Re-arm the stalled transaction once after clear-stall.
    bool prev2                        = usbfs_irq_save();
    usb_current_xfer_info.is_busy     = true;
    usb_current_xfer_info.xferred_len = 0;
    usbfs_irq_restore(prev2);
    hardware_start_xfer(request_pid, ep_addr, 0, false);
    return;
  }

  if (response_pid == USB_PID_NAK) {
    LOG_CH32_USBFSH("NAK response\r\n");
    if (edpt->xfer_type == TUSB_XFER_INTERRUPT) {
      // Interrupt IN: a NAK means "no data yet"; report success (no bytes).
      bool prev                     = usbfs_irq_save();
      usb_current_xfer_info.is_busy = false;
      usbfs_irq_restore(prev);
      hcd_event_xfer_complete(dev_addr, ep_addr, 0, XFER_RESULT_SUCCESS, in_isr);
    } else {
      // Bulk/control NAK: stash for a SOF-driven retry with progressive backoff
      // (1->2->4...->64 frames). A tight task-loop retry floods the device's
      // endpoint ISR and starves its main loop (and ours); backing off lets the
      // device finish processing and ACK.
      bool prev                     = usbfs_irq_save();
      usb_current_xfer_info.is_busy = false;
      edpt->is_nak_pending          = true;
      edpt->buflen                  = usb_current_xfer_info.bufferlen;
      edpt->buf                     = usb_current_xfer_info.buffer;
      edpt->nak_xferred             = usb_current_xfer_info.xferred_len;
      edpt->nak_backoff             = TU_MIN(edpt->nak_backoff * 2, 64);
      usbfs_irq_restore(prev);
    }
    return;
  }

  if (response_pid == USB_PID_DATA0 || response_pid == USB_PID_DATA1) {
    LOG_CH32_USBFSH("Data toggle mismatched (DATA0/1 not via TOG_OK). RX_LEN=%d\r\n", USBOTG_H_FS->RX_LEN);
    bool prev                     = usbfs_irq_save();
    usb_current_xfer_info.is_busy = false;
    usbfs_irq_restore(prev);
    hcd_event_xfer_complete(dev_addr, ep_addr, 0, XFER_RESULT_FAILED, in_isr);
    return;
  }

  LOG_CH32_USBFSH("cb_transfer_complete() unexpected response PID: 0x%02x\r\n", response_pid);
  bool prev                     = usbfs_irq_save();
  usb_current_xfer_info.is_busy = false;
  usbfs_irq_restore(prev);
  hcd_event_xfer_complete(dev_addr, ep_addr, 0, XFER_RESULT_FAILED, in_isr);
}

//--------------------------------------------------------------------+
// HCD API
//--------------------------------------------------------------------+
bool hcd_init(uint8_t rhport, const tusb_rhport_init_t *rh_init) {
  (void)rhport;
  (void)rh_init;
  hardware_init_host(true);
  g_sof_frame = 0;
  return true;
}

bool hcd_deinit(uint8_t rhport) {
  (void)rhport;
  hardware_init_host(false);
  return true;
}

void hcd_port_reset(uint8_t rhport) {
  (void)rhport;
  LOG_CH32_USBFSH("hcd_port_reset()\r\n");
  port_reset_in_progress  = true;
  int_state_for_portreset = interrupt_enabled;
  hcd_int_disable(rhport);
  hardware_update_device_address(0x00);

  // Drop any in-flight / queued ISO so it does not resume after the reset.
  usb_current_xfer_info.is_busy = false;
  USBOTG_H_FS->HOST_EP_PID      = 0;

  USBOTG_H_FS->HOST_CTRL |= USBFS_UH_BUS_RESET;
}

void hcd_port_reset_end(uint8_t rhport) {
  (void)rhport;
  LOG_CH32_USBFSH("hcd_port_reset_end()\r\n");

  USBOTG_H_FS->HOST_CTRL &= ~USBFS_UH_BUS_RESET;
  tusb_time_delay_ms_api(2);

  if ((USBOTG_H_FS->HOST_CTRL & USBFS_UH_PORT_EN) == 0) {
    if (hcd_port_speed_get(0) == TUSB_SPEED_LOW) {
      hardware_update_port_speed(TUSB_SPEED_LOW);
    }
  }

  USBOTG_H_FS->HOST_CTRL |= USBFS_UH_PORT_EN;
  // SOF generation must be on: it both keeps the enumerated device from
  // suspending and drives the 1ms frame tick for isochronous pacing.
  USBOTG_H_FS->HOST_SETUP |= USBFS_UH_SOF_EN;

  // Suppress the attached event the reset itself may assert.
  USBOTG_H_FS->INT_FG |= USBFS_UIF_DETECT;

  // Port reset complete: resume normal DETECT event reporting.
  port_reset_in_progress = false;

  if (int_state_for_portreset) {
    hcd_int_enable(rhport);
  }
}

bool hcd_port_connect_status(uint8_t rhport) {
  (void)rhport;
  return hardware_device_attached();
}

tusb_speed_t hcd_port_speed_get(uint8_t rhport) {
  (void)rhport;
  if (USBOTG_H_FS->MIS_ST & USBFS_UMS_DM_LEVEL) {
    return TUSB_SPEED_LOW;
  }
  return TUSB_SPEED_FULL;
}

// Close all opened endpoints belonging to this device.
void hcd_device_close(uint8_t rhport, uint8_t dev_addr) {
  (void)rhport;
  LOG_CH32_USBFSH("hcd_device_close(0x%02x)\r\n", dev_addr);
  bool prev = usbfs_irq_save();
  // Drop an in-flight transfer if it belongs to this device.
  if (usb_current_xfer_info.is_busy && usb_current_xfer_info.dev_addr == dev_addr) {
    usb_current_xfer_info.is_busy = false;
    USBOTG_H_FS->HOST_EP_PID      = 0;
  }
  remove_edpt_record_for_device(dev_addr);
  usbfs_irq_restore(prev);
}

uint32_t hcd_frame_number(uint8_t rhport) {
  (void)rhport;
  return g_sof_frame;
}

void hcd_int_enable(uint8_t rhport) {
  (void)rhport;
  NVIC_EnableIRQ(USBFS_IRQn);
  interrupt_enabled = true;
}

void hcd_int_disable(uint8_t rhport) {
  (void)rhport;
  NVIC_DisableIRQ(USBFS_IRQn);
  interrupt_enabled = false;
}

void hcd_int_handler(uint8_t rhport, bool in_isr) {
  (void)rhport;
  uint8_t fg = USBOTG_H_FS->INT_FG;

  if (fg & USBFS_UIF_DETECT) {
    USBOTG_H_FS->INT_FG = USBFS_UIF_DETECT;
    // Ignore DETECT edges while a port reset is in progress (see
    // port_reset_in_progress): the reset asserts an attach=0 edge that is not
    // a real disconnect.
    if (port_reset_in_progress) {
      return;
    }
    bool attached = hardware_device_attached();
    LOG_CH32_USBFSH("hcd_int_handler() attached = %d\r\n", attached ? 1 : 0);
    if (attached) {
      hcd_event_device_attach(rhport, true);
    } else {
      // Drop any in-flight / queued ISO on disconnect.
      bool prev                     = usbfs_irq_save();
      usb_current_xfer_info.is_busy = false;
      USBOTG_H_FS->HOST_EP_PID      = 0;
      usbfs_irq_restore(prev);
      hcd_event_device_remove(rhport, true);
    }
    return;
  }

  // SOF: 1ms frame tick. Increment the software frame counter and try to arm
  // a queued, frame-due ISO endpoint. Fall through: a TRANSFER completion may
  // be pending in the same INT_FG snapshot and must be serviced here too.
  if (fg & USBFS_UIF_HST_SOF) {
    USBOTG_H_FS->INT_FG = USBFS_UIF_HST_SOF;
    g_sof_frame++;
    // ISO has priority over the NAK retry: isochronous transfers must keep
    // their 1/frame cadence even while a control transfer is NAKing (the retry
    // happens in the gaps). Reversing this starves ISO under a persistent
    // control NAK.
    arm_iso_drain(true);
    arm_nak_retry();
    // fall through
  }

  // Re-read INT_FG here rather than trust the entry snapshot: the SOF branch's
  // arm_iso_drain may have armed a transaction (clearing UIF_TRANSFER), in which
  // case there is no pending completion to service.
  if (USBOTG_H_FS->INT_FG & USBFS_UIF_TRANSFER) {
    // Stale-edge guard: only process a completion for a transfer we armed.
    // Clearing the flag while HOST_EP_PID is set re-arms the SIE, so STOP
    // (HOST_EP_PID = 0) BEFORE clearing the flag.
    if (!usb_current_xfer_info.is_busy) {
      USBOTG_H_FS->HOST_EP_PID = 0;
      USBOTG_H_FS->INT_FG      = USBFS_UIF_TRANSFER;
      return;
    }

    uint8_t pid_edpt     = USBOTG_H_FS->HOST_EP_PID;
    uint8_t status       = USBOTG_H_FS->INT_ST;
    uint8_t dev_addr     = USBOTG_H_FS->DEV_ADDR & USBFS_USB_ADDR_MASK;
    uint8_t request_pid  = pid_edpt >> 4;
    uint8_t response_pid = status & USBFS_UIS_H_RES_MASK;
    uint8_t ep_addr      = pid_edpt & 0x0f;
    if (request_pid == USB_PID_IN) {
      ep_addr |= 0x80;
    }

    // STOP first, then clear the flag (order prevents an extra SIE transaction).
    USBOTG_H_FS->HOST_EP_PID = 0;
    USBOTG_H_FS->INT_FG      = USBFS_UIF_TRANSFER;

    LOG_CH32_USBFSH("hcd_int_handler() pid_edpt=0x%02x status=0x%02x\r\n", pid_edpt, status);

    usb_edpt_t *edpt_info = get_edpt_record(dev_addr, ep_addr);
    if (edpt_info == NULL) {
      // Never hang the ISR on a missing record; report failure and drop the slot.
      LOG_CH32_USBFSH("hcd_int_handler() no record for dev=0x%02x ep=0x%02x\r\n", dev_addr, ep_addr);
      bool prev                     = usbfs_irq_save();
      usb_current_xfer_info.is_busy = false;
      usbfs_irq_restore(prev);
      hcd_event_xfer_complete(dev_addr, ep_addr, 0, XFER_RESULT_FAILED, in_isr);
      return;
    }

    // Hard routing by the slot's xfer_type snapshot: ISO and control/bulk take
    // disjoint completion paths. This is what prevents a control GET_DESCRIPTOR
    // IN from being completed through the ISO path with a stale RX_LEN.
    if (usb_current_xfer_info.xfer_type == TUSB_XFER_ISOCHRONOUS) {
      iso_transfer_complete(request_pid, edpt_info);
    } else {
      cb_transfer_complete(request_pid, response_pid, edpt_info, in_isr);
    }
  }
}

//--------------------------------------------------------------------+
// Endpoint API
//--------------------------------------------------------------------+

bool hcd_edpt_open(uint8_t rhport, uint8_t dev_addr, const tusb_desc_endpoint_t *ep_desc) {
  (void)rhport;
  uint8_t  ep_addr         = ep_desc->bEndpointAddress;
  uint16_t max_packet_size = ep_desc->wMaxPacketSize;
  uint8_t  xfer_type       = ep_desc->bmAttributes.xfer;
  LOG_CH32_USBFSH("hcd_edpt_open(dev_addr=0x%02x, ep=0x%02x, mps=%d, type=%d)\r\n", dev_addr, ep_addr, max_packet_size,
                  xfer_type);

  while (usb_current_xfer_info.is_busy) {}

  if (tu_edpt_number(ep_addr) == 0x00) {
    TU_ASSERT(get_or_add_edpt_record(dev_addr, 0x00, max_packet_size, xfer_type) != NULL, false);
    TU_ASSERT(get_or_add_edpt_record(dev_addr, 0x80, max_packet_size, xfer_type) != NULL, false);
  } else {
    TU_ASSERT(get_or_add_edpt_record(dev_addr, ep_addr, max_packet_size, xfer_type) != NULL, false);
  }

  USBOTG_H_FS->HOST_CTRL |= USBFS_UH_PORT_EN;
  USBOTG_H_FS->HOST_SETUP |= USBFS_UH_SOF_EN;

  hardware_set_port_address_speed(dev_addr);

  return true;
}

bool hcd_edpt_close(uint8_t rhport, uint8_t dev_addr, uint8_t ep_addr) {
  (void)rhport;
  LOG_CH32_USBFSH("hcd_edpt_close(dev_addr=0x%02x, ep=0x%02x)\r\n", dev_addr, ep_addr);
  usb_edpt_t *edpt = get_edpt_record(dev_addr, ep_addr);
  if (edpt == NULL) {
    return false;
  }
  bool prev        = usbfs_irq_save();
  edpt->configured = false;
  edpt->iso_queued = false;
  edpt->iso_active = false;
  if (usb_current_xfer_info.is_busy && usb_current_xfer_info.dev_addr == dev_addr &&
      usb_current_xfer_info.ep_addr == ep_addr) {
    usb_current_xfer_info.is_busy = false;
    USBOTG_H_FS->HOST_EP_PID      = 0;
  }
  usbfs_irq_restore(prev);
  return true;
}

bool hcd_edpt_xfer(uint8_t rhport, uint8_t dev_addr, uint8_t ep_addr, uint8_t *buffer, uint16_t buflen) {
  (void)rhport;
  LOG_CH32_USBFSH("hcd_edpt_xfer(dev_addr=0x%02x, ep=0x%02x, len=%d)\r\n", dev_addr, ep_addr, buflen);

  usb_edpt_t *edpt_info = get_edpt_record(dev_addr, ep_addr);
  TU_ASSERT(edpt_info != NULL, false);

  if (edpt_info->xfer_type == TUSB_XFER_ISOCHRONOUS) {
    // ISO: park the request on the endpoint and let the SOF/TRANSFER drain arm
    // it at the frame boundary. Do NOT busy-wait or grab the in-flight slot —
    // an early same-frame re-submit must not starve the other direction.
    bool prev             = usbfs_irq_save();
    edpt_info->iso_buf    = buffer;
    edpt_info->iso_len    = buflen;
    edpt_info->iso_active = false;
    edpt_info->iso_queued = true; // publish last (release ordering vs the ISR)
    usbfs_irq_restore(prev);

    arm_iso_drain(false);
    return true;
  }

  // Control/bulk/interrupt: immediate arm. These only run during enumeration
  // (no ISO competing for the single slot), so the busy-wait is safe.
  while (usb_current_xfer_info.is_busy) {}

  hardware_set_port_address_speed(dev_addr);

  usb_current_xfer_info.is_busy     = true;
  usb_current_xfer_info.dev_addr    = dev_addr;
  usb_current_xfer_info.ep_addr     = ep_addr;
  usb_current_xfer_info.xfer_type   = edpt_info->xfer_type;
  usb_current_xfer_info.buffer      = buffer;
  usb_current_xfer_info.bufferlen   = buflen;
  usb_current_xfer_info.start_ms    = tusb_time_millis_api();
  usb_current_xfer_info.xferred_len = 0;

  edpt_info->nak_backoff = 1; // fresh transfer: reset the progressive NAK backoff

  if (tu_edpt_dir(ep_addr) == TUSB_DIR_IN) {
    LOG_CH32_USBFSH("hcd_edpt_xfer(): READ, dev=0x%02x, ep=0x%02x, len=%d\r\n", dev_addr, ep_addr, buflen);
    return hardware_start_xfer(USB_PID_IN, ep_addr, edpt_info->data_toggle, false);
  }

  LOG_CH32_USBFSH("hcd_edpt_xfer(): WRITE, dev=0x%02x, ep=0x%02x, len=%d\r\n", dev_addr, ep_addr, buflen);
  uint16_t copylen         = TU_MIN(edpt_info->max_packet_size, buflen);
  USBOTG_H_FS->HOST_TX_LEN = copylen;
  return hardware_start_xfer(USB_PID_OUT, ep_addr, edpt_info->data_toggle, false);
}

bool hcd_edpt_abort_xfer(uint8_t rhport, uint8_t dev_addr, uint8_t ep_addr) {
  (void)rhport;
  usb_edpt_t *edpt = get_edpt_record(dev_addr, ep_addr);
  if (edpt == NULL || edpt->xfer_type != TUSB_XFER_ISOCHRONOUS) {
    return false;
  }
  // Only a queued (not-yet-started) ISO transfer can be aborted.
  bool prev    = usbfs_irq_save();
  bool aborted = false;
  if (edpt->iso_queued && !edpt->iso_active) {
    edpt->iso_queued = false;
    aborted          = true;
  }
  usbfs_irq_restore(prev);
  return aborted;
}

bool hcd_setup_send(uint8_t rhport, uint8_t dev_addr, const uint8_t setup_packet[8]) {
  (void)rhport;

  while (usb_current_xfer_info.is_busy) {}

  usb_current_xfer_info.is_busy = true;

  LOG_CH32_USBFSH("hcd_setup_send(dev_addr=0x%02x)\r\n", dev_addr);

  hardware_set_port_address_speed(dev_addr);

  usb_edpt_t *edpt_info_tx = get_edpt_record(dev_addr, 0x00);
  usb_edpt_t *edpt_info_rx = get_edpt_record(dev_addr, 0x80);
  TU_ASSERT(edpt_info_tx != NULL, false);
  TU_ASSERT(edpt_info_rx != NULL, false);

  // SETUP always starts with DATA0 (OUT toggle). A control read's first data
  // packet is DATA1, so the IN endpoint toggle must be primed to DATA1 here.
  edpt_info_tx->data_toggle           = 0;
  edpt_info_rx->data_toggle           = 0x01;
  const uint16_t setup_packet_datalen = 8;
  memcpy(USBFS_TX_Buf, setup_packet, setup_packet_datalen);
  USBOTG_H_FS->HOST_TX_LEN          = setup_packet_datalen;
  uint8_t ep_addr                   = (setup_packet[0] & 0x80) ? 0x80 : 0x00;
  usb_current_xfer_info.dev_addr    = dev_addr;
  usb_current_xfer_info.ep_addr     = ep_addr;
  usb_current_xfer_info.xfer_type   = TUSB_XFER_CONTROL;
  usb_current_xfer_info.start_ms    = tusb_time_millis_api();
  usb_current_xfer_info.buffer      = USBFS_TX_Buf;
  usb_current_xfer_info.bufferlen   = setup_packet_datalen;
  usb_current_xfer_info.xferred_len = 0;

  hardware_start_xfer(USB_PID_SETUP, 0, 0, false);
  return true;
}

bool hcd_edpt_clear_stall(uint8_t rhport, uint8_t dev_addr, uint8_t ep_addr) {
  (void)rhport;
  LOG_CH32_USBFSH("hcd_edpt_clear_stall(dev_addr=0x%02x, ep=0x%02x)\r\n", dev_addr, ep_addr);
  uint8_t edpt_num                     = tu_edpt_number(ep_addr);
  uint8_t setup_request_clear_stall[8] = {0x02, 0x01, 0x00, 0x00, edpt_num, 0x00, 0x00, 0x00};
  memcpy(USBFS_TX_Buf, setup_request_clear_stall, 8);
  USBOTG_H_FS->HOST_TX_LEN = 8;

  bool prev_int_state = interrupt_enabled;
  hcd_int_disable(0);

  // This path bypasses hardware_start_xfer(); re-point the TX DMA manually.
  USBOTG_H_FS->HOST_TX_DMA  = (uint32_t)USBFS_TX_Buf;
  USBOTG_H_FS->HOST_TX_CTRL = 0;
  USBOTG_H_FS->HOST_RX_CTRL = 0;

  hardware_update_device_address(dev_addr);

  USBOTG_H_FS->HOST_EP_PID = (USB_PID_SETUP << 4) | 0x00;
  USBOTG_H_FS->INT_FG      = USBFS_UIF_TRANSFER; // clear stale flag + go
  while ((USBOTG_H_FS->INT_FG & USBFS_UIF_TRANSFER) == 0) {}
  // STOP then clear, so the permanently-enabled UIE_TRANSFER does not re-fire.
  USBOTG_H_FS->HOST_EP_PID = 0;
  USBOTG_H_FS->INT_FG      = USBFS_UIF_TRANSFER;
  uint8_t response_pid     = USBOTG_H_FS->INT_ST & USBFS_UIS_H_RES_MASK;
  (void)response_pid;
  LOG_CH32_USBFSH("hcd_edpt_clear_stall() response pid=0x%02x\r\n", response_pid);

  if (prev_int_state) {
    hcd_int_enable(0);
  }

  return true;
}

#endif

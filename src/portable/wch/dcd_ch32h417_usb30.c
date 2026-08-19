/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 Ha Thach (tinyusb.org)
 * SPDX-License-Identifier: MIT
 *
 * This file is part of the TinyUSB stack.
 */

// USB3.0 SuperSpeed device driver for the WCH CH32H417/CH32H416 USBSS controller
// (@0x40034000), documented in CH32H417RM-EN chapter 27. The LINK layer is the same IP as the
// CH569 (dcd_ch56x_usb30.c) but the endpoint engine is a reworked chain-DMA design with
// hardware sequence numbers and ERDY (SEQ_AUTO / ERDY_AUTO), per-endpoint persistent halt
// (RB_EP_TX/RX_HALT), and per-chain completion flags. Software drives the LTSSM through the
// USBSS_LINK interrupt. The init/PHY/LINK/EP0 register sequences are transcribed from the WCH
// EVT USBSS device demo; IN transfers ride whole multi-packet chains (see queue_in_packet), OUT
// arms one packet at a time. Buffers may live anywhere in the shared SRAM (all DMA-reachable).
//
// No SOF at SuperSpeed: dcd_sof_enable() is a no-op, so tud_sof_cb and the audio-class feedback
// path get nothing even though iso endpoints are supported. The received bus interval counter is
// readable (R32_USBSS_ITP.REG_ITP_INTERVAL, RM 27.2.2.1, +1 every 125 us), but the only ITP
// interrupt is host mode's "transmit ITP complete" (RB_UIE_ITP / RB_UIF_ITP, RM 27.2.1.1 /
// 27.2.1.2), so a device-mode SOF event would have to be polled rather than serviced from the ISR.
//
// With CFG_TUD_WCH_USB30_FALLBACK this dcd owns both controllers: it counts failed SuperSpeed
// training attempts (LINK DISABLE/INACTIVE) with a TIM12 backstop and hands rhport 0 to the USB2
// driver (ch32h417_usb2_* in dcd_ch32h417_usbhs.c) when the host has no SuperSpeed port.
//
// Hardware-verified on nanoch32h417 at 5 Gbps: 12/12 HIL examples and usbtest 29/30
// (case 13 quirk-skipped, bcdDevice 0x20 - a halted endpoint answers exactly one STALL).

#include "tusb_option.h"

#if CFG_TUD_ENABLED && defined(TUP_USBIP_WCH_USB30_H417) && \
    defined(CFG_TUD_WCH_USBIP_USB30) && CFG_TUD_WCH_USBIP_USB30 == 1

#include "device/dcd.h"
#include "dcd_ch32h417.h"
#include "ch32h417_usb30_reg.h"

#define EP_MAX          8
#define EP0_MAX_SIZE    512

#ifndef CFG_TUD_WCH_USB30_MAX_BURST
#define CFG_TUD_WCH_USB30_MAX_BURST 1
#endif

typedef struct {
  uint8_t *buffer;
  uint16_t total_len;
  uint16_t queued_len;
  uint16_t armed_len; // bytes in the IN chain armed last (rewind unit for clear-halt)
  uint16_t max_size;
  bool     is_iso;
  bool     valid;
  bool     bounce; // OUT armed into the bounce buffer (user buffer smaller than one packet)
} xfer_ctl_t;

#define XFER_CTL_BASE(_ep, _dir) (&xfer_status[_ep][_dir])
static xfer_ctl_t xfer_status[EP_MAX][2];

// EP0 buffer in shared SRAM (DMA-reachable); SETUP lands here.
TU_ATTR_ALIGNED(16) static uint8_t ep0_buffer[EP0_MAX_SIZE];

// The RX chain engine has no receive-length limit: an armed packet writes up to max_size bytes at
// UEP_RX_DMA (the vendor demo only ever DMAs into full-size dedicated buffers). Receive into this
// bounce buffer whenever the user buffer cannot hold a whole packet (e.g. MSC's 31-byte CBW at
// 1024-byte SS bulk MPS - zero-copying that sprayed ~1 KB over .bss and crashed usbd).
TU_ATTR_ALIGNED(16) static uint8_t rx_bounce[EP_MAX - 1][1024];
// EP0 DP packet-sequence numbers, one per direction. EP0 has no SEQ_AUTO (RM 27.2.2.5/27.2.2.6:
// the field only auto-clears on SETUP), so software owns them - and they count over the WHOLE data
// stage, which usbd hands down in one-max-packet chunks (CFG_TUD_ENDPOINT0_BUFSIZE), so they cannot
// live in the per-chunk xfer state nor be recovered from the register between chunks.
// volatile: both are read-modify-written from the ISR (arm on completion) and from task context
// (arm on the usbd xfer that starts the next chunk of the same data stage).
static volatile uint8_t ep0_tx_seq; // EP0 IN sequence number [4:0]
static volatile uint8_t ep0_rx_seq; // EP0 OUT next expected sequence number [4:0]
// volatile: written by dcd_set_address() in task context, read by the ISR at the SET_ADDRESS
// status completion (mirrors the CH569's _pending_addr)
static volatile uint8_t pending_addr;
static volatile bool    pending_addr_valid;

// SuperSpeed retrain rate-limiting (see link_backoff_arm)
static uint8_t retrain_fail;
static volatile bool backoff_armed;
static void link_backoff_arm(void);

// the link entered U1/U2/U3 with the low-power PHY config applied (restored on RECOVERY)
static volatile bool link_low_power;

// the link entered U3 (USB suspend), distinct from the U1/U2 low-power link states: only a U3
// exit must produce DCD_EVENT_RESUME (a U1/U2 exit is not a USB resume)
static volatile bool link_suspended;

// the link enumerated (LMP Port Config exchanged / reached U0): a later drop to DISABLE/INACTIVE
// is a real disconnect, whereas the same states during initial training are just retrain attempts
static volatile bool link_established;

// Set when UDIF_STATUS fires with no zero-length EP0 transfer queued yet: the controller ran the
// status stage on its own before usbd got around to arming it (the host pipelines control
// transfers faster than the task loop runs). The late arm then completes immediately instead of
// touching the hardware - a stale EP0 arm would linger into the next control transfer and break it.
static volatile bool ep0_status_done;

// NOTE: the vendor stack services a SETUP synchronously inside the ISR and holds USBSS_UDIF_SETUP
// until the response is armed; usbd defers the response to task context instead. No hold is
// needed: the controller NRDYs the host's data/status requests while EP0 is unarmed and announces
// the late arm itself (validated on hardware: full usbtest battery incl. 1000x ch9 control stress
// and ~75k control transfers, zero failures).

//--------------------------------------------------------------------+
// Runtime USB2 fallback state
//--------------------------------------------------------------------+
#if CFG_TUD_WCH_USB30_FALLBACK
enum { FB_USB3_TRAINING, FB_USB3_UP, FB_USB2_ACTIVE };
// volatile: fb_state selects which controller every dcd_* entry point delegates to. It is
// written from the LINK/TIM12 ISR and read from task context by dcd_edpt_xfer/open/stall/
// clear_stall/close_all, dcd_set_address and dcd_connect - and both WCH families build with
// -flto, so the compiler may inline those bodies into tud_task()'s loop and hoist the load.
// The CH569 twin marks its _fb_state volatile for the same reason.
static volatile uint8_t fb_state;
static volatile uint8_t fb_fail_count;
#define FB_FAIL_LIMIT 3

static void usbss_device_init(bool enable);
static void fallback_timer_start(bool enable);

// Shut the SuperSpeed controller down and bring the USB2 high-speed controller up on rhport 0.
// Must stop TIM12 first: once fb_state==FB_USB2_ACTIVE, dcd_int_handler routes every IRQ to the
// USB2 handler and never clears the TIM12 flag, so a still-running timer would storm the CPU.
static void fallback_to_usb2(uint8_t rhport) {
  fallback_timer_start(false);
  usbss_device_init(false);
  fb_state = FB_USB2_ACTIVE;
  ch32h417_usb2_init(rhport);
  ch32h417_usb2_int_enable();
}
#endif

//--------------------------------------------------------------------+
// PHY / clock / link bring-up (transcribed from the WCH EVT USBSS demo)
//--------------------------------------------------------------------+

static void usbss_rcc_init(bool enable) {
  if (enable) {
    RCC->CTLR |= (uint32_t)RCC_USBSS_PLLON;
    // bounded, best effort: a PLL that never locks (e.g. bad HSE) must not hang the core forever
    uint32_t timeout = 1000000;
    while (((RCC->CTLR & (uint32_t)RCC_USBSS_PLLRDY) != (uint32_t)RCC_USBSS_PLLRDY) && --timeout) {}
    RCC_HBPeriphClockCmd(RCC_HBPeriph_USBSS, ENABLE);
    RCC_PIPECmd(ENABLE);
    RCC_UTMIcmd(ENABLE);
  } else {
    RCC_HBPeriphClockCmd(RCC_HBPeriph_USBSS, DISABLE);
    RCC_UTMIcmd(DISABLE);
    RCC_PIPECmd(DISABLE);
    RCC->CTLR &= ~(uint32_t)RCC_USBSS_PLLON;
  }
}

static void usbss_phy_cfg(uint8_t addr, uint16_t data) {
  USBSS_PHY_CFG_CR = (1u << 23) | ((uint32_t)addr << 16) | data;
  USBSS_PHY_CFG_DAT = 0x01;
  (void)USBSS_PHY_CFG_DAT; // read-back strobe; the returned value is not used
}

static void usbss_cfg_mod(void) {
  usbss_phy_cfg(0x03, 0x7C12);
  usbss_phy_cfg(0x0D, 0x79AA);
  usbss_phy_cfg(0x15, 0x4430);
  usbss_phy_cfg(0x13, 0x0010);
  USBSS_PHY_MISC_REG = 0xB0054000;
}

static void ep_chain_init(void) {
  // RM 27.2.1.1 RB_USB_CLR_ALL: clears all interrupt flags, the device address and every endpoint
  // configuration; "in device mode this bit should be set after receiving HOT_RESET/WARM_RESET,
  // then cleared". Without the pulse a latched UDIF_SETUP survives the reset and dcd_int_handler
  // replays the stale ep0_buffer as a SETUP. The vendor's endpoint init opens with the same pulse;
  // restore the snapshot rather than read back, since the bit also resets configuration registers.
  const uint32_t usb_control = USBSSD->USB_CONTROL;
  USBSSD->USB_CONTROL = usb_control | USBSS_USB_CLR_ALL;
  USBSSD->USB_CONTROL = usb_control & ~(uint32_t)USBSS_USB_CLR_ALL;

  // EP0 buffer for SETUP/data
  USBSSD->UEP0_TX_CTRL = 0;
  USBSSD->UEP0_RX_CTRL = 0;
  USBSSD->UEP0_TX_DMA = (uint32_t)(uintptr_t)ep0_buffer;
  USBSSD->UEP0_RX_DMA = (uint32_t)(uintptr_t)ep0_buffer;
}

static void usbss_device_init(bool enable) {
  if (enable) {
    TU_LOG2("usbss: rcc/pll init\r\n");
    usbss_rcc_init(true);
    TU_LOG2("usbss: pll locked\r\n");

    // TX de-emphasis: match the WCH EVT init literal exactly (it programs both DEEMPH bits)
    USBSSD->LINK_CFG = LINK_RX_EQ_EN | LINK_TX_DEEMPH_MASK | LINK_PHY_RESET;
    USBSSD->LINK_CTRL = LINK_P2_MODE | LINK_GO_DISABLED;
    USBSSD->LINK_CFG = LINK_RX_EQ_EN | LINK_TX_DEEMPH_MASK | LINK_LTSSM_MODE | LINK_TOUT_MODE;
    USBSSD->LINK_LPM_CR |= LINK_LPM_EN;
    USBSSD->LINK_CFG |= LINK_RX_TERM_EN;
    USBSSD->LINK_INT_CTRL = LINK_IE_TX_LMP | LINK_IE_RX_LMP | LINK_IE_RX_LMP_TOUT | LINK_IE_STATE_CHG |
                            LINK_IE_WARM_RST | LINK_IE_TERM_PRES;
    USBSSD->LINK_CTRL = LINK_P2_MODE;
    USBSSD->LINK_U1_WKUP_TMR = 120;
    USBSSD->LINK_U1_WKUP_FILTER = 50;
    USBSSD->LINK_U2_WKUP_FILTER = 0;
    USBSSD->LINK_U3_WKUP_FILTER = 0;

    USBSSD->USB_CONTROL |= USBSS_FORCE_RST;
    USBSSD->USB_STATUS = USBSS_UIF_TRANSFER;
    USBSSD->USB_CONTROL = USBSS_UIE_TRANSFER | USBSS_UDIE_SETUP | USBSS_UDIE_STATUS | USBSS_DMA_EN | USBSS_SETUP_FLOW;

    usbss_cfg_mod();

    USBSSD->UEP_TX_EN = 0;
    USBSSD->UEP_RX_EN = 0;
    ep_chain_init();

    NVIC_EnableIRQ(USBSS_IRQn);
    NVIC_EnableIRQ(USBSS_LINK_IRQn);
    TU_LOG2("usbss: init done LINK_STATUS=%08lx LINK_CFG=%08lx\r\n",
            (unsigned long)USBSSD->LINK_STATUS, (unsigned long)USBSSD->LINK_CFG);
  } else {
    NVIC_DisableIRQ(USBSS_LINK_IRQn);
    NVIC_DisableIRQ(USBSS_IRQn);
    USBSSD->USB_CONTROL = USBSS_FORCE_RST;
    USBSSD->LINK_CFG |= LINK_PHY_RESET | U3_LINK_RESET;
    for (volatile int i = 0; i < 2000; i++) {}
    USBSSD->USB_CONTROL &= ~USBSS_FORCE_RST;
    USBSSD->LINK_CFG &= ~(LINK_PHY_RESET | U3_LINK_RESET);
    usbss_rcc_init(false);
  }
}

// re-init the endpoint engine after a warm/hot reset (keeps the link up)
static void usbss_reset_init(void) {
  USBSSD->USB_CONTROL |= USBSS_FORCE_RST;
  USBSSD->USB_STATUS = USBSS_UIF_TRANSFER;
  USBSSD->USB_CONTROL = USBSS_UIE_TRANSFER | USBSS_UDIE_SETUP | USBSS_UDIE_STATUS | USBSS_DMA_EN | USBSS_SETUP_FLOW;
  ep_chain_init();
  ep0_status_done = false;
  ep0_tx_seq = 0; // a bus reset resets the EP0 packet sequences
  ep0_rx_seq = 0;
  // USB 3.2 Table 9-10: a hot/warm reset clears U1/U2 Enable, so deny link low-power again
  USBSSD->LINK_CFG &= ~(LINK_U1_ALLOW | LINK_U2_ALLOW);
  // Invalidate every in-flight transfer: the reset flushes armed chains and their UIF_TRANSFERs
  // would otherwise complete as stale events into usbd AFTER it wiped the ep->driver map
  // ("Xfer Complete on EP 03" straight after "Bus Reset" -> usbd assert; seen on hardware).
  // Only the in-flight flags: a class xfer_cb interrupted by this reset re-arms from task context
  // right after, and a wiped max_size makes queue_in_packet divide by zero (TU_DIV_CEIL).
  for (uint8_t ep = 0; ep < EP_MAX; ep++) {
    xfer_status[ep][TUSB_DIR_OUT].valid = false;
    xfer_status[ep][TUSB_DIR_IN].valid = false;
  }
}

//--------------------------------------------------------------------+
// LINK layer (LTSSM) - state-change driven, transcribed from the demo
//--------------------------------------------------------------------+

static void handle_link_irq(uint8_t rhport) {
  uint32_t link_int = USBSSD->LINK_INT_FLAG;
  uint32_t link_state = USBSSD->LINK_STATUS & LINK_STATE_MASK;

  // NOTE: no logging in this handler - the LMP exchange after U0 has a ~us deadline and a
  // blocking UART print here breaks SuperSpeed training (proven on hardware).

  if (link_int & LINK_IF_STATE_CHG) {
    USBSSD->LINK_INT_FLAG = LINK_IF_STATE_CHG;

    switch (link_state) {
      case LINK_STATE_DISABLE:
        if (retrain_fail < 0xFF) {
          retrain_fail++;
        }
        if (retrain_fail >= 3) {
          link_backoff_arm(); // hold DISABLED; the timer releases GO_DISABLED in ~20 ms
        } else {
          USBSSD->LINK_CTRL &= ~LINK_GO_DISABLED;
        }
        // DISABLE and INACTIVE both count as a failed SuperSpeed training attempt
        TU_ATTR_FALLTHROUGH;
      case LINK_STATE_INACTIVE:
        if (link_established) {
          // an enumerated SuperSpeed link dropped (cable pull / host port down): report the
          // disconnect once. During initial training link_established is false, so the retrain
          // DISABLE/INACTIVE churn does not spuriously unplug.
          link_established = false;
          link_suspended = false;  // an unplugged link cannot be suspended: no stale RESUME later
          link_low_power = false;
          dcd_event_bus_signal(rhport, DCD_EVENT_UNPLUGGED, true);
        }
#if CFG_TUD_WCH_USB30_FALLBACK
        if (fb_state == FB_USB3_TRAINING && ++fb_fail_count >= FB_FAIL_LIMIT) {
          fallback_to_usb2(rhport); // host has no SuperSpeed port: bring up USB2 on rhport 0
        }
#endif
        break;

      case LINK_STATE_HOTRST:
        usbss_reset_init();
        link_suspended = false; // a reset link starts un-suspended
        link_low_power = false;
        dcd_event_bus_reset(rhport, TUSB_SPEED_SUPER, true);
        USBSSD->LINK_CTRL &= ~LINK_HOT_RESET;
        break;

      case LINK_STATE_U3:
        link_suspended = true; // only U3 is a USB suspend (U1/U2 are link low-power states)
        dcd_event_bus_signal(rhport, DCD_EVENT_SUSPEND, true);
        TU_ATTR_FALLTHROUGH;
      case LINK_STATE_U1:
      case LINK_STATE_U2:
        // low-power entry: PHY reconfig transcribed from the vendor demo. Without it the link
        // cannot exit its first U1/U2/U3 transition and the device drops off the bus ~45 s after
        // enumerating (host resumes -> no response -> error -62 -> disconnect; seen on hardware).
        link_low_power = true;
        usbss_phy_cfg(0x12, 0x67C8 & (uint16_t)~(1u << 9));
        break;

      case LINK_STATE_RECOVERY:
        if (link_low_power) {
          link_low_power = false;
          // ~100 us settle before restoring the PHY (vendor: Delay_Us(100))
          for (volatile uint32_t i = 0; i < 2500; i++) {}
          usbss_phy_cfg(0x12, 0x67C8);
        }
        // emit RESUME only for a real suspend (U3) exit, not for a U1/U2 low-power exit
        if (link_suspended) {
          link_suspended = false;
          dcd_event_bus_signal(rhport, DCD_EVENT_RESUME, true);
        }
        break;

      case LINK_STATE_U0:
      case LINK_STATE_POLLING:
      case LINK_STATE_RXDET:
      default:
        break;
    }
  } else if (link_int & LINK_IF_TERM_PRES) {
    USBSSD->LINK_INT_FLAG = LINK_IF_TERM_PRES;
  } else if (link_int & LINK_IF_RX_LMP_TOUT) {
    USBSSD->LINK_INT_FLAG = LINK_IF_RX_LMP_TOUT;
    USBSSD->LINK_CTRL |= LINK_GO_DISABLED;
    USBSSD->LINK_CTRL |= LINK_GO_RX_DET;
  } else if (link_int & LINK_IF_TX_LMP) {
    USBSSD->LINK_INT_FLAG = LINK_IF_TX_LMP;
    USBSSD->LINK_LMP_TX_DATA0 = LMP_LINK_SPEED | LMP_PORT_CAP | LMP_HP;
    USBSSD->LINK_LMP_TX_DATA1 = LMP_UP_STREAM | LMP_NUM_HP_BUF;
    USBSSD->LINK_LMP_TX_DATA2 = 0;
  } else if (link_int & LINK_IF_RX_LMP) {
    USBSSD->LINK_INT_FLAG = LINK_IF_RX_LMP;
    uint32_t rx0 = USBSSD->LINK_LMP_RX_DATA0;
    if ((rx0 & LMP_SUBTYPE_MASK) == LMP_PORT_CFG) {
      // upstream port received Port Configuration: reply Port Config Response = SS training done
      USBSSD->LINK_LMP_TX_DATA0 = LMP_LINK_SPEED | LMP_PORT_CFG_RES | LMP_HP;
      USBSSD->LINK_LMP_TX_DATA1 = 0;
      USBSSD->LINK_LMP_TX_DATA2 = 0;
      USBSSD->LINK_LMP_PORT_CAP |= LINK_LMP_TX_CAP_VLD;
      retrain_fail = 0; // link trained: reset the retrain backoff
      link_established = true; // enumerated: a later DISABLE/INACTIVE is now a real disconnect
#if CFG_TUD_WCH_USB30_FALLBACK
      fallback_timer_start(false); // SS is up: stop the 0.5 s fallback backstop (mirrors CH569)
#endif
#if CFG_TUD_WCH_USB30_FALLBACK
      fb_state = FB_USB3_UP;
      fb_fail_count = 0;
#endif
      dcd_event_bus_reset(rhport, TUSB_SPEED_SUPER, true);
    } else if ((rx0 & LMP_SUBTYPE_MASK) == LMP_U2_INACT_TOUT) {
      USBSSD->LINK_U2_INACT_TIMER = (rx0 >> 9) & 0xFF;
    }
  } else if (link_int & LINK_IF_WARM_RST) {
    USBSSD->LINK_INT_FLAG = LINK_IF_WARM_RST;
    if (USBSSD->LINK_STATUS & LINK_RX_WARM_RST) {
      usbss_reset_init();
      link_suspended = false; // a reset link starts un-suspended
      link_low_power = false;
      USBSSD->LINK_CTRL |= LINK_GO_DISABLED;
      __NOP(); __NOP(); __NOP(); __NOP();
      USBSSD->LINK_CTRL &= ~LINK_GO_DISABLED;
      dcd_event_bus_reset(rhport, TUSB_SPEED_SUPER, true);
    }
  }
}

//--------------------------------------------------------------------+
// EP0 control
//--------------------------------------------------------------------+

static void ep0_arm_in(uint16_t len) {
  // Clear-first: a set ACT flag write-locks the register.
  USBSSD->UEP0_TX_CTRL = 0;
  // One store, not the vendor demo's write-then-OR-in-ERDY: the read-modify-write raced the ISR
  // (ep0_arm_in runs from both task and interrupt context), and a SETUP landing between the two
  // stores would have re-applied ERDY on top of whatever the ISR had just armed. Hardware-checked
  // that ERDY in the same store still latches DPH/len first: usbtest cases 9 (1000 control
  // transfers) and 10, multi-packet control-OUT, and 3 full suites.
  USBSSD->UEP0_TX_CTRL = USBSS_EP0_TX_DPH | len | ((uint32_t)ep0_tx_seq << 16) | USBSS_EP0_TX_ERDY;
  ep0_tx_seq = (uint8_t)((ep0_tx_seq + 1u) & 0x1Fu);
}

static void ep0_arm_out(void) {
  // Same write-lock as the TX side: while RB_UIF_EP0_RX_ACT (bit31) is set from a completed
  // receive, register writes do not stick - clear first, then arm (found on hardware: a stuck
  // RX_ACT from SET_LINE_CODING's data stage killed the status stage of every later transfer).
  // RB_EP0_RX_SEQ must be rewritten with every arm: the clear zeroes it, and an arm that expects
  // sequence 0 while the host is sending the second DP of a data stage gets NO response out of the
  // engine at all - the host sees a transaction error (usbtest ctrl_out -EPROTO above 512 bytes).
  USBSSD->UEP0_RX_CTRL = 0;
  USBSSD->UEP0_RX_CTRL = USBSS_EP0_RX_ERDY | USBSS_EP0_RX_ACK | ((uint32_t)ep0_rx_seq << 16);
}

static void handle_setup(uint8_t rhport) {
  TU_LOG2("SU: %02x %02x %02x%02x %02x%02x %02x%02x\r\n", ep0_buffer[0], ep0_buffer[1], ep0_buffer[3],
          ep0_buffer[2], ep0_buffer[5], ep0_buffer[4], ep0_buffer[7], ep0_buffer[6]);
  ep0_tx_seq = 0;
  ep0_rx_seq = 0;
  pending_addr_valid = false;
  ep0_status_done = false;
  xfer_status[0][TUSB_DIR_IN].valid = false;
  xfer_status[0][TUSB_DIR_OUT].valid = false;
  // SET_ISOCH_DELAY: usbd accepts it as a stackwide no-op, but the engine times iso DPs off
  // LINK_ISO_DLY - the vendor stack forwards the host's value (RM 27.2.6.11: ns units, lower
  // 3 bits ignored, reset 40 ns). Snoop it here since usbd has no DCD hook for this request.
  const tusb_control_request_t *setup = (const tusb_control_request_t *)(uintptr_t)ep0_buffer;
  if (setup->bmRequestType == 0 && setup->bRequest == TUSB_REQ_SET_ISOCH_DELAY) {
    USBSSD->LINK_ISO_DLY = setup->wValue;
  }
  dcd_event_setup_received(rhport, ep0_buffer, true);
}

static void handle_ep0_in(uint8_t rhport) {
  xfer_ctl_t *xfer = XFER_CTL_BASE(0, TUSB_DIR_IN);
  if (!xfer->valid || xfer->total_len == 0) {
    return; // a zero-length status IN is completed by the UDIF_STATUS interrupt, not here
  }
  uint16_t remaining = xfer->total_len - xfer->queued_len;
  if (remaining == 0) {
    xfer->valid = false;
    USBSSD->UEP0_TX_CTRL = 0; // done: clear the completion flag and park the endpoint (CH569 does the same)
    // Ready the status-OUT stage right here in the ISR, before usbd even sees the completion
    // (the vendor's "ready status step"): RX idles at RES=NRDY, and a status DP that gets NRDYed
    // with no ERDY to revive it times the whole control transfer out.
    ep0_arm_out();
    dcd_event_xfer_complete(rhport, 0x80, xfer->queued_len, XFER_RESULT_SUCCESS, true);
    return;
  }
  uint16_t len = TU_MIN(remaining, (uint16_t)EP0_MAX_SIZE);
  memcpy(ep0_buffer, &xfer->buffer[xfer->queued_len], len);
  xfer->queued_len += len;
  ep0_arm_in(len);
}

static void handle_ep0_out(uint8_t rhport) {
  xfer_ctl_t *xfer = XFER_CTL_BASE(0, TUSB_DIR_OUT);
  if (!xfer->valid) {
    // A zero-length receive with nothing queued is the control-IN status ZLP arriving before usbd
    // queued it (we pre-arm the status stage in handle_ep0_in): remember it so the late queue
    // completes immediately - dropping it stalls usbd's control state and kills the NEXT transfer.
    if ((USBSSD->UEP0_RX_CTRL & USBSS_EP0_RX_LEN_MASK) == 0) {
      USBSSD->UEP0_RX_CTRL = 0; // clear RX_ACT
      ep0_status_done = true;
    }
    return;
  }
  uint16_t rx_len = USBSSD->UEP0_RX_CTRL & USBSS_EP0_RX_LEN_MASK;
  ep0_rx_seq = (uint8_t)((ep0_rx_seq + 1u) & 0x1Fu);
  uint16_t remaining = xfer->total_len - xfer->queued_len;
  uint16_t len = TU_MIN(rx_len, remaining);
  memcpy(&xfer->buffer[xfer->queued_len], ep0_buffer, len);
  xfer->queued_len += len;

  if (xfer->queued_len >= xfer->total_len || len < EP0_MAX_SIZE) {
    xfer->valid = false;
    USBSSD->UEP0_RX_CTRL = 0; // done: clear RX_ACT so it cannot write-lock the next arm
    dcd_event_xfer_complete(rhport, 0x00, xfer->queued_len, XFER_RESULT_SUCCESS, true);
  } else {
    ep0_arm_out();
  }
}

//--------------------------------------------------------------------+
// Data endpoints - single chain armed per packet (burst 1)
//--------------------------------------------------------------------+

// Vendor demos arm 16-packet chains; EXP_NUMP is 8-bit but stay on silicon-proven ground.
#define TX_CHAIN_MAX_PKTS 16

static void queue_in_packet(uint8_t ep_num, xfer_ctl_t *xfer) {
  const uint16_t remaining = xfer->total_len - xfer->queued_len;
  volatile USBSS_EP_TX_TypeDef *tx = usbss_ep_tx(ep_num);

  // Whole-transfer chain arming, vendor-style: DMA_OFS is the per-full-packet stride, CHAIN_LEN
  // the LAST packet's length, EXP_NUMP the packet count (this write is the arming latch). A
  // multi-packet transfer must ride ONE chain so the engine owns intra-chain sequencing and the
  // burst closes at the chain boundary: with per-packet 1-DP chains the burst stayed open across
  // re-arms, and a DP from any OTHER armed IN endpoint emitted into that open burst came out
  // corrupt - the host answered with a transaction error (EPROTO) and the endpoint was dead
  // until the next reset (cdc_msc_throughput's streaming CDC IN wedged every short MSC response
  // into a usb-storage reset storm). No CHAIN_ST.EOB_LPF latch and no sw SEQ: a write before the
  // EXP_NUMP latch is discarded with it, one after it races the departing DP (both hw-proven).
  uint16_t npkt;
  uint16_t last_len;
  if (remaining == 0) {
    npkt = 1; // ZLP
    last_len = 0;
  } else {
    npkt = (uint16_t)TU_DIV_CEIL(remaining, xfer->max_size);
    if (npkt > TX_CHAIN_MAX_PKTS) {
      npkt = TX_CHAIN_MAX_PKTS;
      last_len = xfer->max_size;
    } else {
      last_len = (uint16_t)(remaining - (uint16_t)(npkt - 1) * xfer->max_size);
    }
  }
  tx->UEP_TX_DMA = (uint32_t)(uintptr_t)&xfer->buffer[xfer->queued_len];
  tx->UEP_TX_DMA_OFS = xfer->max_size;
  tx->UEP_TX_CHAIN_LEN = last_len;
  tx->UEP_TX_CHAIN_EXP_NUMP = (uint8_t)npkt; // arms the chain (latches the staged configuration)
  tx->UEP_TX_ST = USBSS_EP_TX_FC_ST; // clear a latched NRDY so ERDY_AUTO announces this arm
  xfer->armed_len = (uint16_t)((uint16_t)(npkt - 1) * xfer->max_size + last_len);
  xfer->queued_len += xfer->armed_len;
}

static void queue_out_packet(uint8_t ep_num, xfer_ctl_t *xfer) {
  volatile USBSS_EP_RX_TypeDef *rx = usbss_ep_rx(ep_num);
  const uint16_t remaining = xfer->total_len - xfer->queued_len;
  xfer->bounce = remaining < xfer->max_size; // see rx_bounce
  rx->UEP_RX_DMA = xfer->bounce ? (uint32_t)(uintptr_t)rx_bounce[ep_num - 1]
                                : (uint32_t)(uintptr_t)&xfer->buffer[xfer->queued_len];
  rx->UEP_RX_DMA_OFS = xfer->max_size;
  rx->UEP_RX_CHAIN_MAX_NUMP = 1; // arms the chain (latches the staged DMA/OFS into a chain slot)
  // If the host already asked while nothing was armed, the controller sent NRDY and latched
  // FC_ST - and ERDY_AUTO will not announce the new arm while it is set, deadlocking the
  // endpoint until the host gives up (~30 s SCSI timeout; MSC's request-response pattern hit
  // this on every second command while continuous streams never did). Write-1-to-clear.
  rx->UEP_RX_ST = USBSS_EP_RX_FC_ST;
}

static void handle_ep_in(uint8_t rhport, uint8_t ep_num) {
  xfer_ctl_t *xfer = XFER_CTL_BASE(ep_num, TUSB_DIR_IN);
  volatile USBSS_EP_TX_TypeDef *tx = usbss_ep_tx(ep_num);
  tx->UEP_TX_CHAIN_ST = USBSS_EP_TX_CHAIN_IF; // release completed chain (pure write: CHAIN_IF is WO, |= re-asserts stale RW bits like EOB_LPF read from the register)

  if (!xfer->valid) {
    return;
  }
  if (xfer->queued_len >= xfer->total_len) {
    xfer->valid = false;
    dcd_event_xfer_complete(rhport, ep_num | TUSB_DIR_IN_MASK, xfer->queued_len, XFER_RESULT_SUCCESS, true);
  } else {
    queue_in_packet(ep_num, xfer);
  }
}

static void handle_ep_out(uint8_t rhport, uint8_t ep_num) {
  xfer_ctl_t *xfer = XFER_CTL_BASE(ep_num, TUSB_DIR_OUT);
  volatile USBSS_EP_RX_TypeDef *rx = usbss_ep_rx(ep_num);
  uint16_t rx_len = rx->UEP_RX_CHAIN_LEN;
  rx->UEP_RX_CHAIN_ST = USBSS_EP_RX_CHAIN_IF; // release completed chain (pure write, see handle_ep_in)

  if (!xfer->valid) {
    return;
  }
  uint16_t remaining = xfer->total_len - xfer->queued_len;
  uint16_t len = TU_MIN(rx_len, TU_MIN(remaining, xfer->max_size));
  if (xfer->bounce) {
    memcpy(&xfer->buffer[xfer->queued_len], rx_bounce[ep_num - 1], len);
  }
  xfer->queued_len += len;

  if (xfer->queued_len >= xfer->total_len || len < xfer->max_size) {
    xfer->valid = false;
    dcd_event_xfer_complete(rhport, ep_num, xfer->queued_len, XFER_RESULT_SUCCESS, true);
  } else {
    queue_out_packet(ep_num, xfer);
  }
}

//--------------------------------------------------------------------+
// USB (transfer) interrupt dispatch
//--------------------------------------------------------------------+

static void handle_usb_irq(uint8_t rhport) {
  uint32_t status = USBSSD->USB_STATUS;

  if ((status & USBSS_UDIF_SETUP) && !(status & USBSS_UDIF_STATUS)) {
    USBSSD->USB_STATUS = USBSS_UDIF_SETUP;
    handle_setup(rhport);
  } else if (status & USBSS_UDIF_STATUS) {
    // Control status stage: on the H417 this is its own interrupt (SET_ADDRESS is applied here).
    TU_LOG2("ST\r\n");
    USBSSD->USB_STATUS = USBSS_UDIF_STATUS;
    if (pending_addr_valid) {
      USBSSD->USB_CONTROL = (USBSSD->USB_CONTROL & 0x00FFFFFF) | ((uint32_t)pending_addr << 24);
      pending_addr_valid = false;
    }
    USBSSD->UEP0_TX_CTRL = 0;
    USBSSD->UEP0_RX_CTRL = 0;
    // Complete the queued zero-length status transfer so usbd runs its status-stage callback
    // (the status ZLP does not raise a UIF_TRANSFER on this controller). If usbd has not queued
    // it yet, remember that the status stage already ran (see ep0_status_done).
    bool completed = false;
    for (uint8_t dir = 0; dir < 2; dir++) {
      xfer_ctl_t *x = &xfer_status[0][dir];
      if (x->valid && x->total_len == 0) {
        x->valid = false;
        completed = true;
        dcd_event_xfer_complete(rhport, (dir == TUSB_DIR_IN) ? 0x80 : 0x00, 0, XFER_RESULT_SUCCESS, true);
      }
    }
    if (!completed) {
      ep0_status_done = true;
    }
  } else if (status & USBSS_UIF_TRANSFER) {
    uint8_t ep_num = USBSS_STATUS_EP_NUM(status);
    bool is_in = USBSS_STATUS_EP_IN(status);
    if (ep_num == 0) {
      if (is_in) { handle_ep0_in(rhport); } else { handle_ep0_out(rhport); }
    } else if (is_in) {
      handle_ep_in(rhport, ep_num);
    } else {
      handle_ep_out(rhport, ep_num);
    }
  }
}

//--------------------------------------------------------------------+
// TIM12 fallback backstop timer (0.5 s), direct-register
//--------------------------------------------------------------------+
// TIM12's bus clock is off out of reset (RM 3.4.8 RCC_HB2PCENR.TIM12EN = 0) and is only enabled
// below - which the default build (FALLBACK=0) reaches only after 3 failed trainings. Every read
// or write of TIM12 elsewhere must be gated on this, or the shared IRQ forwarder touches an
// unclocked APB peripheral on every USBSS/LINK interrupt.
static volatile bool tim12_clocked; // set in task context, read by handle_timer_irq()

static void tim12_program(uint16_t ticks_100us) {
  RCC_HB2PeriphClockCmd(RCC_HB2Periph_TIM12, ENABLE);
  tim12_clocked = true;
  RCC_ClocksTypeDef clk;
  RCC_GetClocksFreq(&clk);
  TIM12->PSC = (uint16_t)(clk.HCLK_Frequency / 10000 - 1); // 10 kHz tick
  TIM12->ATRLR = ticks_100us - 1;
  TIM12->CNT = 0;
  TIM12->INTFR = 0;
  TIM12->DMAINTENR |= TIM_IT_Update;
  TIM12->CTLR1 |= TIM_CEN;
  NVIC_EnableIRQ(TIM12_IRQn);
}

static void tim12_stop(void) {
  NVIC_DisableIRQ(TIM12_IRQn);
  if (!tim12_clocked) {
    return; // never programmed: the register writes would be dropped on the gated bus clock
  }
  TIM12->CTLR1 &= (uint16_t)~TIM_CEN;
  TIM12->DMAINTENR &= (uint16_t)~TIM_IT_Update;
}

// Rate-limit SuperSpeed retraining: after repeated training failures hold the link DISABLED for
// ~20 ms before re-entering RxDetect. Without this the LTSSM loops DISABLE->RxDetect->Polling at
// hardware speed against an unresponsive far end; the resulting interrupt storm starves the
// core-assisted SDI debug interface (and the vendor demo gates retrains behind a timer too).
static void link_backoff_arm(void) {
  backoff_armed = true;
  tim12_program(200); // 20 ms
}

#if CFG_TUD_WCH_USB30_FALLBACK
static void fallback_timer_start(bool enable) {
  if (enable) {
    tim12_program(5000); // 0.5 s
  } else {
    tim12_stop();
  }
}
#endif

static void handle_timer_irq(uint8_t rhport) {
  (void)rhport;
  if (!tim12_clocked || !(TIM12->INTFR & TIM_IT_Update)) {
    return; // the timer is only clocked once tim12_program() ran (see tim12_clocked)
  }
  TIM12->INTFR = (uint16_t)~TIM_IT_Update;
  if (backoff_armed) {
    backoff_armed = false;
    USBSSD->LINK_CTRL &= ~LINK_GO_DISABLED; // release the held link: LTSSM re-enters RxDetect
#if CFG_TUD_WCH_USB30_FALLBACK
    fallback_timer_start(fb_state == FB_USB3_TRAINING); // restore the fallback cadence
#else
    tim12_stop();
#endif
    return;
  }
#if CFG_TUD_WCH_USB30_FALLBACK
  if (fb_state == FB_USB3_TRAINING && ++fb_fail_count >= FB_FAIL_LIMIT) {
    fallback_to_usb2(rhport);
  }
#endif
}

//--------------------------------------------------------------------+
// dcd API
//--------------------------------------------------------------------+

bool dcd_init(uint8_t rhport, const tusb_rhport_init_t *rh_init) {
  (void)rhport;
  (void)rh_init;
  TU_LOG2("dcd_init: ch32h417 usb30\r\n");
  memset(&xfer_status, 0, sizeof(xfer_status));
  ep0_tx_seq = 0;
  ep0_rx_seq = 0;
  pending_addr_valid = false;
  ep0_status_done = false;
  retrain_fail = 0;
  backoff_armed = false;
  link_low_power = false;
  link_suspended = false;
  link_established = false;

#if CFG_TUD_WCH_USB30_FALLBACK
  fb_state = FB_USB3_TRAINING;
  fb_fail_count = 0;
  ch32h417_usb2_deinit(); // quiesce a USB2 controller a prior fallback may have left up
#endif

  usbss_device_init(true);

#if CFG_TUD_WCH_USB30_FALLBACK
  fallback_timer_start(true);
#endif
  return true;
}

bool dcd_deinit(uint8_t rhport) {
  (void)rhport;
#if CFG_TUD_WCH_USB30_FALLBACK
  if (fb_state == FB_USB2_ACTIVE) {
    // the USB2 controller took the port during fallback: tear that down instead
    ch32h417_usb2_int_disable();
    ch32h417_usb2_deinit();
    return true;
  }
#endif
  // Mask first, then tear down: a LINK DISABLE/INACTIVE event or a timer expiry taken between the
  // state updates below still runs the fallback ladder, which would bring the USB2 controller up
  // on a port that is being deinitialized (tud_deinit() returning with the device still attached).
  dcd_int_disable(rhport);
  backoff_armed = false;
  tim12_stop();             // stop the retrain-backoff / fallback backstop timer + mask TIM12 IRQ
  usbss_device_init(false); // gate USBSS clocks and disable the USBSS/LINK IRQs
  return true;
}

void dcd_int_handler(uint8_t rhport) {
#if CFG_TUD_WCH_USB30_FALLBACK
  if (fb_state == FB_USB2_ACTIVE) {
    ch32h417_usb2_int_handler(rhport);
    return;
  }
#endif
  handle_timer_irq(rhport); // retrain backoff (and, when enabled, the USB2-fallback ladder)
#if CFG_TUD_WCH_USB30_FALLBACK
  if (fb_state == FB_USB2_ACTIVE) {
    return;
  }
#endif
  if (USBSSD->LINK_INT_FLAG & USBSSD->LINK_INT_CTRL) {
    handle_link_irq(rhport);
#if CFG_TUD_WCH_USB30_FALLBACK
    if (fb_state == FB_USB2_ACTIVE) {
      return; // handle_link_irq fell back to USB2: USBSS clocks are gated, do not touch it
    }
#endif
  }
  if (USBSSD->USB_STATUS & (USBSS_UIF_TRANSFER | USBSS_UDIF_SETUP | USBSS_UDIF_STATUS)) {
    handle_usb_irq(rhport);
  }
}

void dcd_int_enable(uint8_t rhport) {
#if CFG_TUD_WCH_USB30_FALLBACK
  if (fb_state == FB_USB2_ACTIVE) { ch32h417_usb2_int_enable(); return; }
#endif
  (void)rhport;
  NVIC_EnableIRQ(USBSS_IRQn);
  NVIC_EnableIRQ(USBSS_LINK_IRQn);
  NVIC_EnableIRQ(TIM12_IRQn); // retrain-backoff / fallback backstop timer (symmetric with disable)
}

void dcd_int_disable(uint8_t rhport) {
#if CFG_TUD_WCH_USB30_FALLBACK
  if (fb_state == FB_USB2_ACTIVE) { ch32h417_usb2_int_disable(); return; }
#endif
  (void)rhport;
  NVIC_DisableIRQ(USBSS_IRQn);
  NVIC_DisableIRQ(USBSS_LINK_IRQn);
  NVIC_DisableIRQ(TIM12_IRQn); // mask the timer too, so no timer IRQ fires after tud_deinit
}

void dcd_set_address(uint8_t rhport, uint8_t dev_addr) {
  (void)dev_addr;
#if CFG_TUD_WCH_USB30_FALLBACK
  if (fb_state == FB_USB2_ACTIVE) {
    // USB2 applies the address in ch32h417_usb2_edpt0_status_complete (called by usbd with the
    // real request); here just arm the status ZLP on the USB2 controller.
    ch32h417_usb2_edpt_xfer(rhport, 0x80, NULL, 0);
    return;
  }
#endif
  // SuperSpeed: apply the address at the status stage (USBSS_UDIF_STATUS), per the SIE
  pending_addr = dev_addr;
  pending_addr_valid = true;
  dcd_edpt_xfer(rhport, 0x80, NULL, 0, false); // ZLP status
}

void dcd_remote_wakeup(uint8_t rhport) {
#if CFG_TUD_WCH_USB30_FALLBACK
  if (fb_state == FB_USB2_ACTIVE) {
    ch32h417_usb2_remote_wakeup(); // USB2 owns the port (USBSS clocks are gated): resume from there
    return;
  }
#endif
  (void)rhport;
  USBSSD->LINK_CTRL |= LINK_TX_UX_EXIT; // best effort U-state exit
}

void dcd_disconnect(uint8_t rhport) {
#if CFG_TUD_WCH_USB30_FALLBACK
  if (fb_state == FB_USB2_ACTIVE) {
    ch32h417_usb2_disconnect();
    return;
  }
#endif
  // Mask the USBSS/LINK/TIM12 vectors BEFORE gating USBSS clocks: the aliased TIM12 IRQ would
  // otherwise fire dcd_int_handler, which reads clock-gated USBSSD registers, and a LINK event or
  // timer expiry landing mid-teardown would run the fallback ladder and re-attach as USB2 right
  // after the application asked to disconnect. dcd_connect re-enables them (usbss_device_init /
  // tim12_program).
  dcd_int_disable(rhport);
  backoff_armed = false;
  tim12_stop();
  usbss_device_init(false); // drop RX terminations + gate clocks: the host sees a disconnect
}

void dcd_connect(uint8_t rhport) {
#if CFG_TUD_WCH_USB30_FALLBACK
  if (fb_state == FB_USB2_ACTIVE) {
    ch32h417_usb2_connect();
    return;
  }
#endif
  (void)rhport;
#if CFG_TUD_WCH_USB30_FALLBACK
  fb_state = FB_USB3_TRAINING; // restart the training/fallback ladder alongside the link
  fb_fail_count = 0;
#endif
  usbss_device_init(true); // restore terminations and restart link training
#if CFG_TUD_WCH_USB30_FALLBACK
  fallback_timer_start(true);
#endif
}

void dcd_sof_enable(uint8_t rhport, bool en) {
#if CFG_TUD_WCH_USB30_FALLBACK
  if (fb_state == FB_USB2_ACTIVE) {
    ch32h417_usb2_sof_enable(rhport, en);
    return;
  }
#endif
  (void)rhport;
  (void)en; // no frame interrupt at SuperSpeed (ITP is ignored)
}

void dcd_edpt0_status_complete(uint8_t rhport, const tusb_control_request_t *request) {
#if CFG_TUD_WCH_USB30_FALLBACK
  if (fb_state == FB_USB2_ACTIVE) { ch32h417_usb2_edpt0_status_complete(rhport, request); return; }
#endif
  (void)rhport;
  // Track the host's U1/U2 enable in the link layer: the LTSSM answers LGO_U1/LGO_U2 with LXU
  // (deny) unless the matching ALLOW bit is set (RM 27.2.6.2, reset 0), so without this the link
  // refuses what usbd reports as enabled in GET_STATUS and the U1/U2 handling above is dead code.
  // Applied at the status stage, i.e. only for a request usbd actually accepted.
  if (request->bmRequestType == 0 &&
      (request->bRequest == TUSB_REQ_SET_FEATURE || request->bRequest == TUSB_REQ_CLEAR_FEATURE)) {
    uint32_t allow = 0;
    if (request->wValue == TUSB_REQ_FEATURE_U1_ENABLE) {
      allow = LINK_U1_ALLOW;
    } else if (request->wValue == TUSB_REQ_FEATURE_U2_ENABLE) {
      allow = LINK_U2_ALLOW;
    }
    if (allow) {
      if (request->bRequest == TUSB_REQ_SET_FEATURE) {
        USBSSD->LINK_CFG |= allow;
      } else {
        USBSSD->LINK_CFG &= ~allow;
      }
    }
  }
}

bool dcd_edpt_open(uint8_t rhport, const tusb_desc_endpoint_t *desc_edpt) {
#if CFG_TUD_WCH_USB30_FALLBACK
  if (fb_state == FB_USB2_ACTIVE) { return ch32h417_usb2_edpt_open(rhport, desc_edpt); }
#endif
  (void)rhport;
  const uint8_t ep_num = tu_edpt_number(desc_edpt->bEndpointAddress);
  const tusb_dir_t dir = tu_edpt_dir(desc_edpt->bEndpointAddress);
  TU_ASSERT(ep_num < EP_MAX);
  if (ep_num == 0) {
    return true;
  }

  xfer_ctl_t *xfer = XFER_CTL_BASE(ep_num, dir);
  xfer->max_size = tu_edpt_packet_size(desc_edpt);
  TU_ASSERT(xfer->max_size <= 1024); // rx_bounce rows are 1024 B (SS caps wMaxPacketSize at 1024)
  xfer->is_iso = (desc_edpt->bmAttributes.xfer == TUSB_XFER_ISOCHRONOUS);

  if (dir == TUSB_DIR_IN) {
    volatile USBSS_EP_TX_TypeDef *tx = usbss_ep_tx(ep_num);
    // EOB_MODE=1 (RM 27.2.3.1 bit3) selects the short-packet EOB polarity. The RM text reads
    // inverted (translation): measured on silicon, at the reset default (0) a short DP carries
    // NO EOB/LPF - the only source is a CHAIN_ST.EOB_LPF latch written after the arm, which
    // races the departing DP on a hot link (lost race = burst left open, host EPROTOs the next
    // IN, and the orphaned latch poisons the following chain - MSC short responses wedged the
    // endpoint). With 1 the hw auto-tags EOB on short packets and no latch is needed.
    tx->UEP_TX_CFG = USBSS_EP_TX_CHAIN_AUTO | USBSS_EP_TX_ERDY_AUTO | USBSS_EP_TX_SEQ_AUTO |
                     USBSS_EP_TX_EOB_MODE |
                     (xfer->is_iso ? USBSS_EP_TX_ISO_MODE : 0);
    tx->UEP_TX_CR = (uint8_t)(USBSS_EP_TX_CLR | USBSS_EP_TX_CHAIN_CLR);
    // TX ERDY window = chain depth (the vendor leaves TX_CR at its 0x10 reset). A NumP-1
    // window costs an ERDY TP per DP; with two IN endpoints streaming, that per-DP TP pressure
    // is what the shared TP machinery cannot take (same failure family as the CH569's one-shot
    // USB_FC_CTRL mailbox). The descriptor's bMaxBurst stays CFG_TUD_WCH_USB30_MAX_BURST-1 and
    // the host clamps its side to that.
    tx->UEP_TX_CR = TX_CHAIN_MAX_PKTS;
    USBSSD->UEP_TX_EN |= (uint16_t)(1u << ep_num);
  } else {
    volatile USBSS_EP_RX_TypeDef *rx = usbss_ep_rx(ep_num);
    rx->UEP_RX_CFG = USBSS_EP_RX_CHAIN_AUTO | USBSS_EP_RX_ERDY_AUTO | USBSS_EP_RX_SEQ_AUTO |
                     (xfer->is_iso ? USBSS_EP_RX_ISO_MODE : 0);
    rx->UEP_RX_CR = (uint8_t)(USBSS_EP_RX_CLR | USBSS_EP_RX_CHAIN_CLR);
    rx->UEP_RX_CR = CFG_TUD_WCH_USB30_MAX_BURST;
    USBSSD->UEP_RX_EN |= (uint16_t)(1u << ep_num);
  }
  return true;
}

void dcd_edpt_close(uint8_t rhport, uint8_t ep_addr) {
#if CFG_TUD_WCH_USB30_FALLBACK
  if (fb_state == FB_USB2_ACTIVE) { ch32h417_usb2_edpt_close(rhport, ep_addr); return; }
#endif
  (void)rhport;
  const uint8_t ep_num = tu_edpt_number(ep_addr);
  const tusb_dir_t dir = tu_edpt_dir(ep_addr);
  if (ep_num == 0) {
    return; // EP0 has no bank of its own: usbss_ep_tx(0)/usbss_ep_rx(0) index BEFORE EP1's
  }
  if (dir == TUSB_DIR_IN) {
    usbss_ep_tx(ep_num)->UEP_TX_CR = (uint8_t)(USBSS_EP_TX_CLR | USBSS_EP_TX_CHAIN_CLR);
    USBSSD->UEP_TX_EN &= (uint16_t)~(1u << ep_num);
  } else {
    usbss_ep_rx(ep_num)->UEP_RX_CR = (uint8_t)(USBSS_EP_RX_CLR | USBSS_EP_RX_CHAIN_CLR);
    USBSSD->UEP_RX_EN &= (uint16_t)~(1u << ep_num);
  }
  XFER_CTL_BASE(ep_num, dir)->valid = false;
}

void dcd_edpt_close_all(uint8_t rhport) {
#if CFG_TUD_WCH_USB30_FALLBACK
  if (fb_state == FB_USB2_ACTIVE) { ch32h417_usb2_edpt_close_all(rhport); return; }
#endif
  (void)rhport;
  for (uint8_t ep = 1; ep < EP_MAX; ep++) {
    usbss_ep_tx(ep)->UEP_TX_CR = (uint8_t)(USBSS_EP_TX_CLR | USBSS_EP_TX_CHAIN_CLR);
    usbss_ep_rx(ep)->UEP_RX_CR = (uint8_t)(USBSS_EP_RX_CLR | USBSS_EP_RX_CHAIN_CLR);
    xfer_status[ep][0].valid = false;
    xfer_status[ep][1].valid = false;
  }
  USBSSD->UEP_TX_EN = 0;
  USBSSD->UEP_RX_EN = 0;
}

bool dcd_edpt_xfer(uint8_t rhport, uint8_t ep_addr, uint8_t *buffer, uint16_t total_bytes, bool is_isr) {
#if CFG_TUD_WCH_USB30_FALLBACK
  if (fb_state == FB_USB2_ACTIVE) { return ch32h417_usb2_edpt_xfer(rhport, ep_addr, buffer, total_bytes); }
#endif
  const uint8_t ep_num = tu_edpt_number(ep_addr);
  const tusb_dir_t dir = tu_edpt_dir(ep_addr);

  xfer_ctl_t *xfer = XFER_CTL_BASE(ep_num, dir);
  xfer->buffer = buffer;
  xfer->total_len = total_bytes;
  xfer->queued_len = 0;
  xfer->valid = true;

  if (ep_num == 0) {
    TU_LOG2("X0: %c%u\r\n", dir == TUSB_DIR_IN ? 'I' : 'O', total_bytes);
    if (total_bytes == 0) {
      // Status-stage ZLP (either direction). If the controller already ran the status stage
      // (UDIF_STATUS fired before usbd queued this - the host pipelines control transfers faster
      // than the task loop), complete immediately and touch NO hardware: a stale EP0 arm would
      // leak into the next control transfer and break it.
      if (ep0_status_done) {
        ep0_status_done = false;
        if (pending_addr_valid) { // SET_ADDRESS whose status stage already ran: latch it now
          USBSSD->USB_CONTROL = (USBSSD->USB_CONTROL & 0x00FFFFFF) | ((uint32_t)pending_addr << 24);
          pending_addr_valid = false;
        }
        // park both EP0 registers: their ACT completion flags (bit 31) write-lock all later arms
        USBSSD->UEP0_TX_CTRL = 0;
        USBSSD->UEP0_RX_CTRL = 0;
        xfer->valid = false;
        // synchronous completion: forward the caller's context so the FreeRTOS queue send matches
        dcd_event_xfer_complete(rhport, ep_addr, 0, XFER_RESULT_SUCCESS, is_isr);
      } else if (dir == TUSB_DIR_IN) {
        // control-OUT/no-data: the device transmits the status ZLP - needs the TX arm
        // (clear first: a set ACT flag write-locks the register). UDIF_STATUS completes it.
        ep0_arm_in(0);
        ep0_arm_out();
      } else {
        // control-IN status (OUT ZLP): the RX was pre-armed in handle_ep0_in and the controller
        // accepts the ZLP with NO completion interrupt (neither UDIF_STATUS nor UIF_TRANSFER fires
        // - proven on hardware: every second control transfer died with the event-driven design).
        // Complete it immediately; the FIFO event queue keeps ordering correct for usbd.
        xfer->valid = false;
        dcd_event_xfer_complete(rhport, ep_addr, 0, XFER_RESULT_SUCCESS, is_isr);
      }
    } else if (dir == TUSB_DIR_IN) {
      handle_ep0_in(rhport);
    } else {
      ep0_arm_out(); // control-OUT data stage
    }
  } else if (dir == TUSB_DIR_IN) {
    queue_in_packet(ep_num, xfer);
  } else {
    queue_out_packet(ep_num, xfer);
  }
  return true;
}

void dcd_edpt_stall(uint8_t rhport, uint8_t ep_addr) {
#if CFG_TUD_WCH_USB30_FALLBACK
  if (fb_state == FB_USB2_ACTIVE) { ch32h417_usb2_edpt_stall(rhport, ep_addr); return; }
#endif
  (void)rhport;
  const uint8_t ep_num = tu_edpt_number(ep_addr);
  const tusb_dir_t dir = tu_edpt_dir(ep_addr);
  if (ep_num == 0) {
    USBSSD->UEP0_TX_CTRL = 0; // clear the ACT write-locks first
    USBSSD->UEP0_RX_CTRL = 0;
    USBSSD->UEP0_TX_CTRL = USBSS_EP0_TX_STALL;
    USBSSD->UEP0_RX_CTRL = USBSS_EP0_RX_ERDY | USBSS_EP0_RX_STALL;
  } else if (dir == TUSB_DIR_IN) {
    volatile USBSS_EP_TX_TypeDef *tx = usbss_ep_tx(ep_num);
    // Clear the armed CHAIN only - never RB_EP_TX_CLR, which resets the whole endpoint (RM
    // 27.2.3.2: "clear all configuration values and states of endpoints except UEP_CFG"). A
    // TX_CLR'd endpoint stops emitting a valid STALL TP: the host xHC reports a USB Transaction
    // Error on every IN to the halted endpoint (-EPROTO, never -EPIPE) and resets the device.
    // MSC's BOT case-5 stall (Hi > Di) turned every short SCSI read into a usb-storage reset
    // storm. The vendor demo only ORs in HALT; keep ERDY_NUMP across both writes.
    tx->UEP_TX_CR = (uint8_t)(USBSS_EP_TX_CHAIN_CLR | TX_CHAIN_MAX_PKTS);
    tx->UEP_TX_CR = (uint8_t)(USBSS_EP_TX_HALT | TX_CHAIN_MAX_PKTS);
  } else {
    volatile USBSS_EP_RX_TypeDef *rx = usbss_ep_rx(ep_num);
    // Same rule as TX above: clear only the armed CHAIN, never RB_EP_RX_CLR (endpoint-wide reset
    // per RM 27.2.3.5) - a CLR'd endpoint answers OUT DPs with transaction errors instead of a
    // STALL TP (usbtest case 13 failed its first OUT halt this way).
    rx->UEP_RX_CR = (uint8_t)(USBSS_EP_RX_CHAIN_CLR | CFG_TUD_WCH_USB30_MAX_BURST);
    rx->UEP_RX_CR = (uint8_t)(USBSS_EP_RX_HALT | CFG_TUD_WCH_USB30_MAX_BURST);
  }
}

void dcd_edpt_clear_stall(uint8_t rhport, uint8_t ep_addr) {
#if CFG_TUD_WCH_USB30_FALLBACK
  if (fb_state == FB_USB2_ACTIVE) { ch32h417_usb2_edpt_clear_stall(rhport, ep_addr); return; }
#endif
  (void)rhport;
  const uint8_t ep_num = tu_edpt_number(ep_addr);
  const tusb_dir_t dir = tu_edpt_dir(ep_addr);
  if (ep_num == 0) {
    return;
  }
  if (dir == TUSB_DIR_IN) {
    volatile USBSS_EP_TX_TypeDef *tx = usbss_ep_tx(ep_num);
    tx->UEP_TX_CR = (uint8_t)(USBSS_EP_TX_CLR | USBSS_EP_TX_CHAIN_CLR);
    tx->UEP_TX_CR = TX_CHAIN_MAX_PKTS;
    xfer_ctl_t *xfer = XFER_CTL_BASE(ep_num, TUSB_DIR_IN);
    if (xfer->valid) {
      // Rewind to the CHAIN boundary: queued_len counts ARMED bytes (queue_in_packet advances it at
      // arm time, not completion) and TX_CLR above discarded exactly the one chain still in flight.
      // Resuming from queued_len skips those bytes (hardware-proven: every usbtest bulk/int read
      // failed EREMOTEIO), while restarting from 0 replays chains the host already ACKed - up to
      // TX_CHAIN_MAX_PKTS * 1024 = 16 KB of duplicated payload on a long bulk IN.
      xfer->queued_len -= TU_MIN(xfer->armed_len, xfer->queued_len);
      queue_in_packet(ep_num, xfer);
    }
  } else {
    volatile USBSS_EP_RX_TypeDef *rx = usbss_ep_rx(ep_num);
    rx->UEP_RX_CR = (uint8_t)(USBSS_EP_RX_CLR | USBSS_EP_RX_CHAIN_CLR);
    rx->UEP_RX_CR = CFG_TUD_WCH_USB30_MAX_BURST;
    xfer_ctl_t *xfer = XFER_CTL_BASE(ep_num, TUSB_DIR_OUT);
    if (xfer->valid) {
      queue_out_packet(ep_num, xfer);
    }
  }
}

#endif

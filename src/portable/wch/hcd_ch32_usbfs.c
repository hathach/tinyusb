/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2024-2026 Mitsumine Suzu (verylowfreq)
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

#if CFG_TUH_ENABLED && defined(TUP_USBIP_WCH_USBFS) && defined(CFG_TUH_WCH_USBIP_USBFS) && CFG_TUH_WCH_USBIP_USBFS


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

#define USBFS_RX_BUF_LEN 64
#define USBFS_TX_BUF_LEN 64
TU_ATTR_ALIGNED(4) static uint8_t USBFS_RX_Buf[USBFS_RX_BUF_LEN];
TU_ATTR_ALIGNED(4) static uint8_t USBFS_TX_Buf[USBFS_TX_BUF_LEN];

#define PANIC(...)                            \
  do {                                        \
    printf("%s() L%d: ", __func__, __LINE__); \
    printf("\r\n[PANIC] " __VA_ARGS__);       \
    while (true) {}                           \
  } while (false)

#define LOG_CH32_USBFSH(...) TU_LOG3(__VA_ARGS__)
// Interrupt IN retry policy.
// Keep retries bounded and paced so hub status polling is not starved.
#define CH32_USBFS_INTR_NO_RESP_RETRY_MAX 16
#define CH32_USBFS_INTR_NO_RESP_RETRY_SOF_MIN 4u
#define CH32_USBFS_INTR_NO_RESP_RETRY_SOF_MULT 4u
#define CH32_USBFS_INTR_NAK_RETRY_SOF_MIN 2u
#define CH32_USBFS_INTR_NAK_RETRY_SOF_MULT 2u
// Global retry pick budget to avoid multiple endpoints requeueing in one SOF.
#define CH32_USBFS_RETRY_MAX_PER_SOF 1u
// Escalation backoff after consecutive interrupt retry streaks.
#define CH32_USBFS_RETRY_STREAK_COOLDOWN_THRESHOLD 8u
#define CH32_USBFS_RETRY_STREAK_COOLDOWN_SOF_MIN 8u
#define CH32_USBFS_RETRY_STREAK_COOLDOWN_SOF_MULT 8u



#define LOG_CH32_USBFSH_FUNC_CALL() do { LOG_CH32_USBFSH("%s() called at time=%lu[msec],sof=%lu\r\n", __func__, tusb_time_millis_api(), sof_number); } while (false)
#define LOG_CH32_USBFSH_TRACE() do { LOG_CH32_USBFSH("%s():L%u passed at time=%lu[msec],sof=%lu\r\n", __func__, __LINE__, tusb_time_millis_api(), sof_number); } while (false)


// Busywait for delay microseconds/nanoseconds
TU_ATTR_ALWAYS_INLINE static inline void loopdelay(uint32_t count) {
  volatile uint32_t c = count / 3;
  if (c == 0) { return; }
  // while (c-- != 0);
  asm volatile(
    "1:                     \n" // loop label
    "    addi  %0, %0, -1   \n" // c--
    "    bne   %0, zero, 1b \n" // if (c != 0) goto loop
    : "+r"(c) // c is input/output operand
  );
}

// Endpoint status
typedef struct usb_edpt {
  // Is this a valid struct
  bool configured;

  uint8_t dev_addr;
  uint8_t ep_addr;
  uint8_t max_packet_size;

  uint8_t xfer_type;
  uint8_t interval;

  // Data toggle (0 or not 0) for DATA0/1
  uint8_t data_toggle;
  uint8_t last_request_pid;

  uint32_t xfer_start_ms;
  uint32_t next_retry_sof;
  bool is_nak_pending;
  uint8_t transient_timeout_retry_count;
  // Tracks repeated 0x00 responses on interrupt IN transfers.
  uint8_t intr_no_response_retry_count;
  // Counts consecutive interrupt retry outcomes (NAK/0x00) until a success.
  uint8_t intr_retry_streak;
  // Absolute SOF until this endpoint is cooled down.
  uint32_t retry_cooldown_until_sof;
  uint16_t xfer_seq;
  uint16_t retry_seq;

  uint16_t xferred_len;
  uint16_t buflen;
  uint8_t* buf;
} usb_edpt_t;


#define USB_EDPT_LIST_LENGTH (CFG_TUH_DEVICE_MAX * 6)
static usb_edpt_t usb_edpt_list[USB_EDPT_LIST_LENGTH] = {};

typedef struct usb_current_xfer_st {
  bool is_busy;
  uint8_t dev_addr;
  uint8_t ep_addr;
  usb_edpt_t* edpt_info;
  uint8_t request_pid;
  uint16_t xfer_seq;
} usb_current_xfer_t;

static uint8_t nak_retry_roundrobin = 0;
// Retry budget state is reset each SOF in xfer_retry_next().
static uint32_t retry_budget_sof = 0;
static uint8_t retry_budget_used = 0;

static volatile uint32_t sof_number = 0;
static bool interrupt_enabled = false;

static volatile usb_current_xfer_t usb_current_xfer_info = {};

static bool usbfs_lock_xfer_state(void) {
  bool was_enabled = interrupt_enabled;
  if (was_enabled) {
    NVIC_DisableIRQ(USBFS_IRQn);
  }
  return was_enabled;
}

static void usbfs_unlock_xfer_state(bool was_enabled) {
  if (was_enabled) {
    NVIC_EnableIRQ(USBFS_IRQn);
  }
}

static bool usbfs_xfer_try_begin(uint8_t dev_addr, uint8_t ep_addr, usb_edpt_t* edpt_info) {
  bool lock_state = usbfs_lock_xfer_state();
  if (usb_current_xfer_info.is_busy) {
    usbfs_unlock_xfer_state(lock_state);
    return false;
  }

  usb_current_xfer_info.dev_addr = dev_addr;
  usb_current_xfer_info.ep_addr = ep_addr;
  usb_current_xfer_info.edpt_info = edpt_info;
  usb_current_xfer_info.request_pid = 0;
  usb_current_xfer_info.xfer_seq = 0;
  usb_current_xfer_info.is_busy = true;

  usbfs_unlock_xfer_state(lock_state);
  return true;
}

static void usbfs_xfer_set_signature(uint8_t request_pid, uint16_t xfer_seq) {
  bool lock_state = usbfs_lock_xfer_state();
  if (usb_current_xfer_info.is_busy) {
    usb_current_xfer_info.request_pid = request_pid;
    usb_current_xfer_info.xfer_seq = xfer_seq;
  }
  usbfs_unlock_xfer_state(lock_state);
}

static void usbfs_xfer_end_no_event(void) {
  usb_current_xfer_info.is_busy = false;
  usb_current_xfer_info.dev_addr = 0;
  usb_current_xfer_info.ep_addr = 0;
  usb_current_xfer_info.edpt_info = NULL;
  usb_current_xfer_info.request_pid = 0;
  usb_current_xfer_info.xfer_seq = 0;
}

static bool hardware_device_attached(void);

static bool device_addr_connected(uint8_t dev_addr) {
  // If the root port is physically detached, all downstream devices are gone.
  if (!hardware_device_attached()) {
    return false;
  }

  if (dev_addr == 0) {
    // dev_addr 0 is only valid while enumerating a directly attached device.
    return true;
  }

  return tuh_connected(dev_addr);
}

static void clear_all_nak_pending(void) {
  for (size_t i = 0; i < TU_ARRAY_SIZE(usb_edpt_list); i++) {
    usb_edpt_list[i].is_nak_pending = false;
    usb_edpt_list[i].next_retry_sof = 0;
    usb_edpt_list[i].retry_cooldown_until_sof = 0;
    usb_edpt_list[i].intr_retry_streak = 0;
  }
}

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

  slot->dev_addr = dev_addr;
  slot->ep_addr = ep_addr;
  slot->max_packet_size = max_packet_size;
  slot->xfer_type = xfer_type;
  slot->interval = 0;
  slot->data_toggle = 0;
  slot->last_request_pid = 0;
  slot->xfer_start_ms = 0;
  slot->next_retry_sof = 0;
  slot->is_nak_pending = false;
  slot->transient_timeout_retry_count = 0;
  slot->intr_no_response_retry_count = 0;
  slot->intr_retry_streak = 0;
  slot->retry_cooldown_until_sof = 0;
  slot->xfer_seq = 0;
  slot->retry_seq = 0;
  slot->xferred_len = 0;
  slot->buflen = 0;
  slot->buf = NULL;

  slot->configured = true;

  return slot;
}

static usb_edpt_t *get_or_add_edpt_record(uint8_t dev_addr, uint8_t ep_addr, uint16_t max_packet_size, uint8_t xfer_type) {
  usb_edpt_t *ret = get_edpt_record(dev_addr, ep_addr);
  if (ret != NULL) {
    return ret;
  } else {
    return add_edpt_record(dev_addr, ep_addr, max_packet_size, xfer_type);
  }
}

static void remove_edpt_record_for_device(uint8_t dev_addr) {
  for (size_t i = 0; i < TU_ARRAY_SIZE(usb_edpt_list); i++) {
    if (usb_edpt_list[i].configured && usb_edpt_list[i].dev_addr == dev_addr) {
      usb_edpt_list[i].is_nak_pending = false;
      usb_edpt_list[i].next_retry_sof = 0;
      usb_edpt_list[i].retry_cooldown_until_sof = 0;
      usb_edpt_list[i].intr_retry_streak = 0;
      usb_edpt_list[i].configured = false;
    }
  }
}


static volatile bool port_connected = false;
static volatile bool port_now_resetting = false;
static volatile bool detect_pending_during_reset = false;
// One-shot timing assist for the very first EP0 SETUP after reset.
static volatile bool extra_setup_settle_pending = false;
// One-shot settle wait after successful SET_ADDRESS status stage.
static volatile bool post_set_address_settle_pending = false;
// Root-port ADDR0 EP0 timeout recovery state machine.
// This is intentionally limited to direct root-port initial enumeration.
static volatile bool addr0_setup_recover_pending = false;
static volatile bool addr0_setup_recover_used = false;
static volatile bool root_direct_enum_recovery_allowed = false;

static void log_mis_state(const char* tag) {
  uint8_t mis = USBOTG_H_FS->MIS_ST;
  LOG_CH32_USBFSH(
    "%s: MIS_ST=0x%02x SOF_PRES=%d SOF_ACT=%d SIE_FREE=%d R_FIFO_RDY=%d BUS_RESET=%d SUSPEND=%d DM=%d ATTACH=%d\r\n",
    tag,
    mis,
    !!(mis & USBFS_UMS_SOF_PRES),
    !!(mis & USBFS_UMS_SOF_ACT),
    !!(mis & USBFS_UMS_SIE_FREE),
    !!(mis & USBFS_UMS_R_FIFO_RDY),
    !!(mis & USBFS_UMS_BUS_RESET),
    !!(mis & USBFS_UMS_SUSPEND),
    !!(mis & USBFS_UMS_DM_LEVEL),
    !!(mis & USBFS_UMS_DEV_ATTACH));
  (void)tag;
  (void)mis;
}

/** Enable or disable USBFS Host function */
static void hardware_init_host(bool enabled) {
  // Reset USBOTG module
  USBOTG_H_FS->BASE_CTRL = USBFS_UC_RESET_SIE | USBFS_UC_CLR_ALL;

  tusb_time_delay_ms_api(1);
  USBOTG_H_FS->BASE_CTRL = 0;

  if (!enabled) {
    // Disable all feature
    USBOTG_H_FS->BASE_CTRL = 0;
  } else {
    // Enable USB Host features
    hcd_int_disable(0);
    USBOTG_H_FS->BASE_CTRL = USBFS_UC_HOST_MODE | USBFS_UC_INT_BUSY | USBFS_UC_DMA_EN;
    USBOTG_H_FS->HOST_EP_MOD = USBFS_UH_EP_TX_EN | USBFS_UH_EP_RX_EN;
    USBOTG_H_FS->HOST_RX_DMA = (uint32_t) USBFS_RX_Buf;
    USBOTG_H_FS->HOST_TX_DMA = (uint32_t) USBFS_TX_Buf;
    // USBOTG_H_FS->INT_EN = USBFS_UIE_TRANSFER | USBFS_UIE_DETECT;
    USBOTG_H_FS->INT_EN = USBFS_UIE_DETECT | USBFS_UIE_HST_SOF;
  }
}

static bool hardware_start_xfer(uint8_t pid, uint8_t ep_addr, uint8_t data_toggle) {
  LOG_CH32_USBFSH("hardware_start_xfer(pid=%s(0x%02x), ep_addr=0x%02x, toggle=%d)\r\n",
                        pid == USB_PID_IN ? "IN" : pid == USB_PID_OUT ? "OUT"
                                               : pid == USB_PID_SETUP ? "SETUP"
                                                                      : "(other)",
                        pid, ep_addr, data_toggle);

  //WORKAROUND: For LowSpeed device, insert small delay
  bool is_lowspeed_device = tuh_speed_get(usb_current_xfer_info.dev_addr) == TUSB_SPEED_LOW;
  if (is_lowspeed_device) {
    //NOTE: worked -> SystemCoreClock / 1000000 * 50, 25
    //      NOT worked -> 20 and less  (at 144MHz internal clock)
    loopdelay(SystemCoreClock / 1000000 * 40);
  }

  uint8_t pid_edpt = (pid << 4) | (tu_edpt_number(ep_addr) & 0x0f);
  USBOTG_H_FS->INT_FG = USBFS_UIF_TRANSFER;
  USBOTG_H_FS->INT_EN |= USBFS_UIE_TRANSFER;
  USBOTG_H_FS->HOST_TX_CTRL = (data_toggle != 0) ? USBFS_UH_T_TOG : 0;
  USBOTG_H_FS->HOST_RX_CTRL = (data_toggle != 0) ? USBFS_UH_R_TOG : 0;
  USBOTG_H_FS->HOST_EP_PID = pid_edpt;
  return true;
}


/** Set device address to communicate */
static void hardware_update_device_address(uint8_t dev_addr) {
  // Keep the bit of GP_BIT. Other 7bits are actual device address.
  USBOTG_H_FS->DEV_ADDR = (USBOTG_H_FS->DEV_ADDR & USBFS_UDA_GP_BIT) | (dev_addr & USBFS_USB_ADDR_MASK);
}

/** Set port speed */
static void hardware_update_port_speed(tusb_speed_t speed) {
  LOG_CH32_USBFSH("hardware_update_port_speed(%s)\r\n", speed == TUSB_SPEED_FULL ? "Full" : speed == TUSB_SPEED_LOW ? "Low"
                                                                                                                          : "(invalid)");
  switch (speed) {
    case TUSB_SPEED_LOW:
      USBOTG_H_FS->BASE_CTRL |= USBFS_UC_LOW_SPEED;
      USBOTG_H_FS->HOST_CTRL |= USBFS_UH_LOW_SPEED;
      return;
    case TUSB_SPEED_FULL:
      USBOTG_H_FS->BASE_CTRL &= ~USBFS_UC_LOW_SPEED;
      USBOTG_H_FS->HOST_CTRL &= ~USBFS_UH_LOW_SPEED;
      return;
    default:
      PANIC("hardware_update_port_speed(%d)\r\n", speed);
  }
}

static void hardware_set_port_address_speed(uint8_t dev_addr) {
  hardware_update_device_address(dev_addr);
  tusb_speed_t rhport_speed = hcd_port_speed_get(0);
  tusb_speed_t dev_speed = tuh_speed_get(dev_addr);
  bool const low_speed_direct = (rhport_speed == TUSB_SPEED_LOW);
  bool const low_speed_via_hub = (!low_speed_direct &&
                                  rhport_speed == TUSB_SPEED_FULL &&
                                  dev_speed == TUSB_SPEED_LOW);
  LOG_CH32_USBFSH("hardware_set_port_address_speed(dev=%u): rhport=%u dev_speed=%u ls_direct=%u ls_via_hub=%u\r\n",
                        dev_addr, (unsigned) rhport_speed, (unsigned) dev_speed,
                        low_speed_direct ? 1u : 0u, low_speed_via_hub ? 1u : 0u);

  // PRE token is only for low-speed devices behind a full-speed hub.
  if (low_speed_direct) {
    // For root-port direct attach, trust the physical line state.
    // During address transition, tuh_speed_get(dev_addr) can temporarily
    // report an unset/default speed in older host stack revisions.
    hardware_update_port_speed(TUSB_SPEED_LOW);
    USBOTG_H_FS->HOST_SETUP &= ~USBFS_UH_PRE_PID_EN;
  } else if (low_speed_via_hub) {
    // Keep BASE low-speed timing while driving host tokens at full-speed
    // with PRE preamble for low-speed devices behind a full-speed hub.
    // This matches the original CH32 flow that was known to work via hub.
    hardware_update_port_speed(TUSB_SPEED_LOW);
    USBOTG_H_FS->HOST_CTRL &= ~USBFS_UH_LOW_SPEED;
    USBOTG_H_FS->HOST_SETUP |= USBFS_UH_PRE_PID_EN;
  } else {
    hardware_update_port_speed(dev_speed);
    USBOTG_H_FS->HOST_SETUP &= ~USBFS_UH_PRE_PID_EN;
  }

  // Workaround: re-arm host port state on each transfer scheduling.
  // In newer async enumeration flow, CH32 USBFS can miss the first EP0 transaction
  // right after reset unless PORT_EN/SOF_EN are asserted again.
  USBOTG_H_FS->HOST_CTRL |= USBFS_UH_PORT_EN;
  USBOTG_H_FS->HOST_SETUP |= USBFS_UH_SOF_EN;
}

static bool hardware_device_attached(void) {
  return USBOTG_H_FS->MIS_ST & USBFS_UMS_DEV_ATTACH;
}

//--------------------------------------------------------------------+
// HCD API
//--------------------------------------------------------------------+
bool hcd_init(uint8_t rhport, const tusb_rhport_init_t *rh_init) {
  (void) rhport;
  (void) rh_init;
  LOG_CH32_USBFSH_FUNC_CALL();

  memset(usb_edpt_list, 0x00, sizeof(usb_edpt_list));
  port_connected = false;
  detect_pending_during_reset = false;
  extra_setup_settle_pending = false;
  post_set_address_settle_pending = false;
  addr0_setup_recover_pending = false;
  addr0_setup_recover_used = false;
  root_direct_enum_recovery_allowed = false;

  hardware_init_host(true);

  return true;
}

bool hcd_deinit(uint8_t rhport) {
  (void) rhport;
  LOG_CH32_USBFSH_FUNC_CALL();

  hardware_init_host(false);
  port_connected = false;
  detect_pending_during_reset = false;
  extra_setup_settle_pending = false;
  post_set_address_settle_pending = false;
  addr0_setup_recover_pending = false;
  addr0_setup_recover_used = false;
  root_direct_enum_recovery_allowed = false;

  return true;
}

static bool int_state_for_portreset = false;

void hcd_port_reset(uint8_t rhport) {
  (void) rhport;
  
  LOG_CH32_USBFSH_FUNC_CALL();

  int_state_for_portreset = interrupt_enabled;
  port_now_resetting = true;
  detect_pending_during_reset = false;
  addr0_setup_recover_pending = false;
  addr0_setup_recover_used = false;
  post_set_address_settle_pending = false;
  root_direct_enum_recovery_allowed = false;
  hcd_int_disable(rhport);
  hardware_update_device_address(0x00);

  USBOTG_H_FS->HOST_CTRL |= USBFS_UH_BUS_RESET;

  return;
}

void hcd_port_reset_end(uint8_t rhport) {
  (void) rhport;
  
  LOG_CH32_USBFSH_FUNC_CALL();
  // log_mis_state("hcd_port_reset_end(before)");

  USBOTG_H_FS->HOST_CTRL &= ~USBFS_UH_BUS_RESET;
  tusb_time_delay_ms_api(2);

  USBOTG_H_FS->HOST_CTRL |= USBFS_UH_PORT_EN;
  USBOTG_H_FS->HOST_SETUP |= USBFS_UH_SOF_EN;
  LOG_CH32_USBFSH_TRACE();
  tusb_time_delay_ms_api(10);
  LOG_CH32_USBFSH_TRACE();

  // Suppress the attached event
  USBOTG_H_FS->INT_FG = USBFS_UIF_DETECT;

  // Workaround: reset/attach sequencing can leave DETECT/SUSPEND/TRANSFER latched.
  // Clear them before re-enabling IRQs so first EP0 traffic starts from a clean IRQ state.
  USBOTG_H_FS->INT_FG = USBFS_UIF_DETECT | USBFS_UIF_SUSPEND | USBFS_UIF_TRANSFER;

  port_now_resetting = false;
  LOG_CH32_USBFSH("hcd_port_reset_end(after): HOST_CTRL=0x%02x HOST_SETUP=0x%02x\r\n",
                  (uint8_t) USBOTG_H_FS->HOST_CTRL, (uint8_t) USBOTG_H_FS->HOST_SETUP);
  log_mis_state("hcd_port_reset_end(after)");

  // Root reset just finished: allow ADDR0 one-shot recovery only during this
  // default-address window of direct-root enumeration.
  if (hardware_device_attached()) {
    root_direct_enum_recovery_allowed = true;
  }

  // If DETECT arrived during reset, reflect the final physical state now.
  if (detect_pending_during_reset) {
    bool attached = hardware_device_attached();
    detect_pending_during_reset = false;
    LOG_CH32_USBFSH("hcd_port_reset_end(): replay deferred DETECT attached=%d port_connected=%d\r\n",
                    attached ? 1 : 0, port_connected ? 1 : 0);

    if (attached) {
      if (!port_connected) {
        port_connected = true;
      }
      extra_setup_settle_pending = true;
    } else if (!attached && port_connected) {
      port_connected = false;
      clear_all_nak_pending();
    }
  }

  if (int_state_for_portreset) {
    hcd_int_enable(rhport);
  }
}

bool hcd_port_connect_status(uint8_t rhport) {
  (void) rhport;
  LOG_CH32_USBFSH_FUNC_CALL();

  return hardware_device_attached();
}

tusb_speed_t hcd_port_speed_get(uint8_t rhport) {
  (void) rhport;
  LOG_CH32_USBFSH_FUNC_CALL();

  if (USBOTG_H_FS->MIS_ST & USBFS_UMS_DM_LEVEL) {
    return TUSB_SPEED_LOW;
  } else {
    return TUSB_SPEED_FULL;
  }
}

// Close all opened endpoint belong to this device
void hcd_device_close(uint8_t rhport, uint8_t dev_addr) {
  (void) rhport;
  LOG_CH32_USBFSH_FUNC_CALL();
  LOG_CH32_USBFSH("hcd_device_close(%d, 0x%02x)\r\n", rhport, dev_addr);

  uint8_t aborted_ep_addr = 0;
  bool notify_abort = false;
  bool lock_state = usbfs_lock_xfer_state();
  if (usb_current_xfer_info.is_busy && usb_current_xfer_info.dev_addr == dev_addr) {
    aborted_ep_addr = usb_current_xfer_info.ep_addr;
    notify_abort = (tu_edpt_number(aborted_ep_addr) != 0);
    usbfs_xfer_end_no_event();
  }
  usbfs_unlock_xfer_state(lock_state);
  if (notify_abort) {
    hcd_event_xfer_complete(dev_addr, aborted_ep_addr, 0, XFER_RESULT_FAILED, false);
  }
  if (dev_addr == 0) {
    // End of default-address phase for the root-attached device.
    root_direct_enum_recovery_allowed = false;
  }
  remove_edpt_record_for_device(dev_addr);
}

uint32_t hcd_frame_number(uint8_t rhport) {
  (void) rhport;

  return sof_number;
}

void hcd_int_enable(uint8_t rhport) {
  (void) rhport;
  NVIC_EnableIRQ(USBFS_IRQn);
  interrupt_enabled = true;
}

void hcd_int_disable(uint8_t rhport) {
  (void) rhport;
  NVIC_DisableIRQ(USBFS_IRQn);
  interrupt_enabled = false;
}


static void xfer_retry(void* _params) {
  LOG_CH32_USBFSH("xfer_retry()\r\n");
  usb_edpt_t* edpt_info = (usb_edpt_t*)_params;

  uint8_t dev_addr = edpt_info->dev_addr;
  uint8_t ep_addr = edpt_info->ep_addr;
  uint16_t buflen = edpt_info->buflen;
  uint8_t* buf = edpt_info->buf;
  uint8_t req_pid = edpt_info->last_request_pid;
  uint16_t retry_seq = edpt_info->retry_seq;

  if (!device_addr_connected(dev_addr)) {
    LOG_CH32_USBFSH("xfer_retry() drop disconnected device: dev_addr=%u ep_addr=0x%02x\r\n", dev_addr, ep_addr);
    return;
  }

  // Check connectivity
  usb_edpt_t* edpt_info_current = get_edpt_record(dev_addr, ep_addr);
  if (edpt_info_current == NULL) {
    return;
  }

  // Drop stale retry request that belongs to an older transfer generation.
  if (retry_seq != edpt_info_current->xfer_seq) {
    LOG_CH32_USBFSH("[diag] retry drop stale dev=%u ep=0x%02x retry_seq=%u cur_seq=%u\r\n",
                dev_addr, ep_addr, retry_seq, edpt_info_current->xfer_seq);
    edpt_info_current->is_nak_pending = false;
    edpt_info_current->next_retry_sof = 0;
    edpt_info_current->retry_cooldown_until_sof = 0;
    return;
  }

  if (!usbfs_xfer_try_begin(dev_addr, ep_addr, edpt_info_current)) {
    LOG_CH32_USBFSH("[diag] retry defer busy dev=%u ep=0x%02x seq=%u sof=%lu\r\n",
                dev_addr, ep_addr, edpt_info_current->xfer_seq, (uint32_t) sof_number);
    // Keep retry pending if another transfer won the slot this frame.
    edpt_info_current->is_nak_pending = true;
    if (edpt_info_current->interval != 0) {
      uint32_t retry_sof = sof_number + edpt_info_current->interval;
      if (edpt_info_current->retry_cooldown_until_sof != 0 &&
          (int32_t) (retry_sof - edpt_info_current->retry_cooldown_until_sof) < 0) {
        retry_sof = edpt_info_current->retry_cooldown_until_sof;
      }
      edpt_info_current->next_retry_sof = retry_sof;
    } else {
      edpt_info_current->next_retry_sof = sof_number + 1;
    }
    return;
  }
  edpt_info_current->is_nak_pending = false;

  hardware_set_port_address_speed(dev_addr);

  if (req_pid == 0) {
    req_pid = tu_edpt_dir(ep_addr) == TUSB_DIR_IN ? USB_PID_IN : USB_PID_OUT;
  }
  LOG_CH32_USBFSH("[diag] retry start dev=%u ep=0x%02x req=0x%02x seq=%u retry_seq=%u len=%u\r\n",
              dev_addr, ep_addr, req_pid, edpt_info_current->xfer_seq, retry_seq, buflen);

  if (req_pid == USB_PID_SETUP || req_pid == USB_PID_OUT) {
    usbfs_xfer_set_signature(req_pid, edpt_info_current->xfer_seq);
    uint16_t copylen = TU_MIN(edpt_info_current->max_packet_size, buflen);
    USBOTG_H_FS->HOST_TX_LEN = copylen;
    memcpy(USBFS_TX_Buf, buf, copylen);
    hardware_start_xfer(req_pid, ep_addr, edpt_info_current->data_toggle);
  } else {
    usbfs_xfer_set_signature(USB_PID_IN, edpt_info_current->xfer_seq);
    hardware_start_xfer(USB_PID_IN, ep_addr, edpt_info_current->data_toggle);
  }
}

// Pick up next NAK pending endpoint
static void xfer_retry_next(void) {
  // Budget is global per SOF, not per endpoint.
  if (retry_budget_sof != sof_number) {
    retry_budget_sof = sof_number;
    retry_budget_used = 0;
  }

  if (retry_budget_used >= CH32_USBFS_RETRY_MAX_PER_SOF) {
    return;
  }

  for (uint8_t i = 0; i < USB_EDPT_LIST_LENGTH; i++) {
    uint8_t index = (nak_retry_roundrobin + i) % USB_EDPT_LIST_LENGTH;
    usb_edpt_t* edpt = &usb_edpt_list[index];
    if (!edpt->configured || !edpt->is_nak_pending) {
      continue;
    }

    if (!device_addr_connected(edpt->dev_addr)) {
      edpt->is_nak_pending = false;
      edpt->next_retry_sof = 0;
      edpt->retry_cooldown_until_sof = 0;
      continue;
    }

    if (edpt->retry_cooldown_until_sof != 0 &&
        (int32_t) (sof_number - edpt->retry_cooldown_until_sof) < 0) {
      // Endpoint is intentionally throttled after repeated failures.
      continue;
    }

    if (edpt->interval != 0) {
      if ((int32_t) (sof_number - edpt->next_retry_sof) < 0) {
        continue;
      }
    } else {
      // fallthrough
    }
    LOG_CH32_USBFSH("NAK pending at DEV %d EP %02x\r\n", edpt->dev_addr, edpt->ep_addr);
    retry_budget_used++;
    xfer_retry(edpt);
    break;
  }
  nak_retry_roundrobin = (nak_retry_roundrobin + 1) % USB_EDPT_LIST_LENGTH;
}


static void handle_int_detect(uint8_t rhport, bool in_isr) {
  bool attached = hardware_device_attached();
  LOG_CH32_USBFSH_FUNC_CALL();
  LOG_CH32_USBFSH("handle_int_detect() attached=%d, port_connected=%d\r\n", attached ? 1 : 0, port_connected ? 1 : 0);

  if (port_now_resetting) {
    // Do not consume/reset the connect state transition here.
    // Just remember that a DETECT happened while reset sequencing was in progress.
    detect_pending_during_reset = true;
    LOG_CH32_USBFSH("handle_int_detect() deferred while port_now_resetting\r\n");
    return;
  }

  if (attached) {
    if (port_connected) {
      LOG_CH32_USBFSH("handle_int_detect() ignore duplicated attach while already connected\r\n");
      return;
    }

    port_connected = true;
    addr0_setup_recover_pending = false;
    addr0_setup_recover_used = false;
    root_direct_enum_recovery_allowed = true;
    hcd_event_device_attach(rhport, in_isr);

  } else {
    if (!port_connected) {
      LOG_CH32_USBFSH("handle_int_detect() ignore duplicated detach while already disconnected\r\n");
      clear_all_nak_pending();
      return;
    }

    port_connected = false;
    uint8_t aborted_dev_addr = 0;
    uint8_t aborted_ep_addr = 0;
    bool notify_abort = false;
    bool lock_state = usbfs_lock_xfer_state();
    if (usb_current_xfer_info.is_busy) {
      aborted_dev_addr = usb_current_xfer_info.dev_addr;
      aborted_ep_addr = usb_current_xfer_info.ep_addr;
      notify_abort = (aborted_dev_addr != 0) && (tu_edpt_number(aborted_ep_addr) != 0);
      usbfs_xfer_end_no_event();
    }
    usbfs_unlock_xfer_state(lock_state);
    if (notify_abort) {
      hcd_event_xfer_complete(aborted_dev_addr, aborted_ep_addr, 0, XFER_RESULT_FAILED, in_isr);
    }
    USBOTG_H_FS->HOST_EP_PID = 0x00;
    clear_all_nak_pending();
    addr0_setup_recover_pending = false;
    addr0_setup_recover_used = false;
    root_direct_enum_recovery_allowed = false;
    hcd_event_device_remove(rhport, in_isr);
  }
}
static void handle_int_suspend(void) {
  // WCH USBFS tends to latch suspend/resume related events around reset/attach.
  // We currently do not model suspend state in the HCD, so just log and clear it.
  LOG_CH32_USBFSH_FUNC_CALL();
  LOG_CH32_USBFSH("handle_int_suspend()\r\n");
}


static void handle_int_sof(uint8_t int_flag) {
  sof_number += 1;
  if (!(int_flag & USBFS_UIF_DETECT)
        && !(int_flag & USBFS_UIF_TRANSFER)
        && !usb_current_xfer_info.is_busy) {
    xfer_retry_next();
  }
}

static void handle_int_transfer(bool in_isr, uint8_t pid_edpt, uint8_t status, uint8_t dev_addr) {
  // Clear register to stop transfer
  USBOTG_H_FS->HOST_EP_PID = 0x00;

  LOG_CH32_USBFSH_FUNC_CALL();
  LOG_CH32_USBFSH("handle_int_transfer() pid_edpt=0x%02x\r\n", pid_edpt);

  if (!usb_current_xfer_info.is_busy) {
    LOG_CH32_USBFSH("[Warn] Transfer is not ongoing.\r\n");
    LOG_CH32_USBFSH("[diag] irq drop no-busy pid_ep=0x%02x status=0x%02x reg_dev=0x%02x\r\n",
                pid_edpt, status, dev_addr);
    return;
  }

  usb_edpt_t *edpt_info = usb_current_xfer_info.edpt_info;
  if (edpt_info == NULL || !edpt_info->configured) {
    LOG_CH32_USBFSH("drop transfer irq: current transfer context is invalid\r\n");
    LOG_CH32_USBFSH("[diag] irq drop invalid-context pid_ep=0x%02x status=0x%02x\r\n", pid_edpt, status);
    usbfs_xfer_end_no_event();
    return;
  }

  // Use current transfer context as source of truth. DEV_ADDR register may no longer
  // match the IRQ origin when host state is re-armed across chained schedules.
  uint8_t current_dev_addr = usb_current_xfer_info.dev_addr;
  if (dev_addr != current_dev_addr) {
    LOG_CH32_USBFSH("transfer irq dev-mismatch: reg_dev=0x%02x current_dev=0x%02x pid_ep=0x%02x (continue with current context)\r\n",
                          dev_addr, current_dev_addr, pid_edpt);
    LOG_CH32_USBFSH("[diag] irq dev-mismatch reg=0x%02x cur=0x%02x pid_ep=0x%02x status=0x%02x req=0x%02x seq=%u (continue)\r\n",
                dev_addr, current_dev_addr, pid_edpt, status, usb_current_xfer_info.request_pid,
                usb_current_xfer_info.xfer_seq);
  }
  dev_addr = current_dev_addr;
  uint8_t ep_addr = usb_current_xfer_info.ep_addr;
  uint8_t irq_request_pid = pid_edpt >> 4;
  uint8_t irq_ep_addr = pid_edpt & 0x0f;
  if (irq_request_pid == USB_PID_IN) {
    irq_ep_addr |= 0x80;
  }

  // Drop stale IRQ that belongs to a different endpoint number and keep current
  // transfer state intact. This can happen when a late completion from periodic
  // polling arrives around control transfer scheduling.
  if (tu_edpt_number(irq_ep_addr) != tu_edpt_number(ep_addr)) {
    LOG_CH32_USBFSH("drop stale transfer irq: pid_ep=0x%02x current_ep=0x%02x\r\n",
                          pid_edpt, ep_addr);
    LOG_CH32_USBFSH("[diag] irq drop ep-mismatch pid_ep=0x%02x cur_ep=0x%02x status=0x%02x req=0x%02x seq=%u\r\n",
                pid_edpt, ep_addr, status, usb_current_xfer_info.request_pid,
                usb_current_xfer_info.xfer_seq);
    return;
  }

  uint16_t current_xfer_seq = usb_current_xfer_info.xfer_seq;
  if (current_xfer_seq != 0 && current_xfer_seq != edpt_info->xfer_seq) {
    LOG_CH32_USBFSH("drop stale transfer irq: seq mismatch current=%u edpt=%u\r\n",
                          current_xfer_seq, edpt_info->xfer_seq);
    LOG_CH32_USBFSH("[diag] irq drop seq-mismatch cur_seq=%u edpt_seq=%u dev=%u ep=0x%02x pid_ep=0x%02x\r\n",
                current_xfer_seq, edpt_info->xfer_seq, dev_addr, ep_addr, pid_edpt);
    return;
  }

  uint8_t current_request_pid = usb_current_xfer_info.request_pid;
  if (current_request_pid != 0 && irq_request_pid != current_request_pid) {
    LOG_CH32_USBFSH("drop stale transfer irq: req_pid mismatch irq=0x%02x current=0x%02x\r\n",
                          irq_request_pid, current_request_pid);
    LOG_CH32_USBFSH("[diag] irq drop pid-mismatch irq_req=0x%02x cur_req=0x%02x dev=%u ep=0x%02x seq=%u\r\n",
                irq_request_pid, current_request_pid, dev_addr, ep_addr, current_xfer_seq);
    return;
  }

  uint8_t request_pid = edpt_info->last_request_pid;
  if (request_pid == 0) {
    request_pid = irq_request_pid;
  }
  uint8_t response_pid = status & USBFS_UIS_H_RES_MASK;

  if (status & USBFS_UIS_TOG_OK) {
    edpt_info->intr_no_response_retry_count = 0;
    edpt_info->intr_retry_streak = 0;
    edpt_info->retry_cooldown_until_sof = 0;
    edpt_info->data_toggle ^= 0x01;

    switch (request_pid) {
      case USB_PID_SETUP:
      case USB_PID_OUT: {
        uint16_t tx_len = USBOTG_H_FS->HOST_TX_LEN;
        LOG_CH32_USBFSH("hcd_int_hander() SETUP or OUT: buflen=%d,tx_len=%d\r\n", edpt_info->buflen, tx_len);
        edpt_info->buf += tx_len;
        edpt_info->buflen -= tx_len;
        edpt_info->xferred_len += tx_len;
        if (edpt_info->buflen == 0) {
          LOG_CH32_USBFSH("USB_PID_%s completed %d bytes\r\n", request_pid == USB_PID_OUT ? "OUT" : "SETUP", edpt_info->xferred_len);
          usbfs_xfer_end_no_event();
          hcd_event_xfer_complete(dev_addr, ep_addr, edpt_info->xferred_len, XFER_RESULT_SUCCESS, in_isr);
          return;
        } else {
          LOG_CH32_USBFSH("USB_PID_OUT continue... remains %d bytes\r\n", edpt_info->buflen);
          uint16_t copylen = TU_MIN(edpt_info->max_packet_size, edpt_info->buflen);
          USBOTG_H_FS->HOST_TX_LEN = copylen;
          memcpy(USBFS_TX_Buf, edpt_info->buf, copylen);
          hardware_start_xfer(USB_PID_OUT, ep_addr, edpt_info->data_toggle);
          return;
        }
      }
      case USB_PID_IN: {
        uint16_t received_len = USBOTG_H_FS->RX_LEN;
        edpt_info->xferred_len += received_len;
        uint16_t xferred_len = edpt_info->xferred_len;
        LOG_CH32_USBFSH("Read %d bytes\r\n", received_len);
        // if (received_len > 0 && (usb_current_xfer_info.buffer == NULL || usb_current_xfer_info.bufferlen == 0)) {
        //     PANIC("Data received but buffer not set\r\n");
        // }
        memcpy(edpt_info->buf, USBFS_RX_Buf, received_len);
        edpt_info->buf += received_len;
        edpt_info->buflen -= received_len;
        if ((received_len < edpt_info->max_packet_size) || (edpt_info->buflen == 0)) {
          // USB device sent all data.
          LOG_CH32_USBFSH("USB_PID_IN completed\r\n");
          usbfs_xfer_end_no_event();
          hcd_event_xfer_complete(dev_addr, ep_addr, xferred_len, XFER_RESULT_SUCCESS, in_isr);
          return;
        } else {
          // USB device may send more data.
          LOG_CH32_USBFSH("Read more data\r\n");
          hardware_start_xfer(USB_PID_IN, ep_addr, edpt_info->data_toggle);
          return;
        }
      }
      default: {
        LOG_CH32_USBFSH("hcd_int_handler() L%d: unexpected response PID: 0x%02x\r\n", __LINE__, response_pid);
        usbfs_xfer_end_no_event();
        hcd_event_xfer_complete(dev_addr, ep_addr, 0, XFER_RESULT_FAILED, in_isr);
        return;
      }
    }
  } else {
    if (response_pid == USB_PID_STALL) {
      LOG_CH32_USBFSH("STALL response 0x%02x, HOST_SETUP=0x%04x,HOST_CTRL=0x%04x,BASE_CTRL=0x%04x\r\n", response_pid, USBOTG_H_FS->HOST_SETUP, USBOTG_H_FS->HOST_CTRL, USBOTG_H_FS->BASE_CTRL);

      usbfs_xfer_end_no_event();
      hcd_event_xfer_complete(dev_addr, ep_addr, 0, XFER_RESULT_STALLED, in_isr);
      return;

    } else if (response_pid == USB_PID_NAK) {
      LOG_CH32_USBFSH("NAK reposense. dev_addr=%d, ep_addr=0x%02x\r\n", dev_addr, ep_addr);

      edpt_info->is_nak_pending = true;
      edpt_info->retry_seq = edpt_info->xfer_seq;
      if (edpt_info->xfer_type == TUSB_XFER_INTERRUPT) {
        // Periodic IN endpoints are retried with interval-based pacing.
        if (edpt_info->intr_retry_streak < 0xffu) {
          edpt_info->intr_retry_streak++;
        }
        uint32_t retry_gap_sof = (uint32_t) edpt_info->interval * CH32_USBFS_INTR_NAK_RETRY_SOF_MULT;
        if (retry_gap_sof < CH32_USBFS_INTR_NAK_RETRY_SOF_MIN) {
          retry_gap_sof = CH32_USBFS_INTR_NAK_RETRY_SOF_MIN;
        }
        uint32_t next_retry_sof = sof_number + retry_gap_sof;
        if (edpt_info->intr_retry_streak >= CH32_USBFS_RETRY_STREAK_COOLDOWN_THRESHOLD) {
          // Escalate to a longer cooldown once short retries keep failing.
          uint32_t cooldown_gap_sof = (uint32_t) edpt_info->interval * CH32_USBFS_RETRY_STREAK_COOLDOWN_SOF_MULT;
          if (cooldown_gap_sof < CH32_USBFS_RETRY_STREAK_COOLDOWN_SOF_MIN) {
            cooldown_gap_sof = CH32_USBFS_RETRY_STREAK_COOLDOWN_SOF_MIN;
          }
          edpt_info->retry_cooldown_until_sof = sof_number + cooldown_gap_sof;
          if ((int32_t) (next_retry_sof - edpt_info->retry_cooldown_until_sof) < 0) {
            next_retry_sof = edpt_info->retry_cooldown_until_sof;
          }
        }
        edpt_info->next_retry_sof = next_retry_sof;
      } else {
        edpt_info->next_retry_sof = sof_number + 1;
      }
      usbfs_xfer_end_no_event();

      return;

    } else if (response_pid == 0x00) {
      LOG_CH32_USBFSH("[diag] no-response dev=%u ep=0x%02x req=0x%02x status=0x%02x seq=%u intr_retry=%u busy=%u\r\n",
                  dev_addr, ep_addr, request_pid, status, edpt_info->xfer_seq,
                  edpt_info->intr_no_response_retry_count, usb_current_xfer_info.is_busy ? 1u : 0u);
      // CH32 USBFS can sporadically return no-response on EP0, especially on
      // low-speed direct attach around address transition. Treat early misses
      // as transient and retry a few frames before failing.
      bool const ep0_control = (tu_edpt_number(ep_addr) == 0) &&
                               (edpt_info->xfer_type == TUSB_XFER_CONTROL);
      bool const low_speed_direct = (hcd_port_speed_get(0) == TUSB_SPEED_LOW);
      bool const retryable_ep0 = ep0_control && low_speed_direct;
      if (retryable_ep0 && edpt_info->transient_timeout_retry_count < 4) {
        edpt_info->transient_timeout_retry_count++;
        edpt_info->is_nak_pending = true;
        edpt_info->retry_seq = edpt_info->xfer_seq;
        edpt_info->next_retry_sof = sof_number + 1;
        usbfs_xfer_end_no_event();
        LOG_CH32_USBFSH(
          "Transient no-response on EP0 (dev=%u ls_direct=%u), retry %u/4 (req_pid=0x%02x)\r\n",
          dev_addr, low_speed_direct ? 1u : 0u,
          edpt_info->transient_timeout_retry_count, request_pid);
        return;
      }

      // CH32 USBFS can also report 0x00 on periodic interrupt IN polling.
      // If we propagate FAILED on each poll, class drivers immediately resubmit
      // and flood USBH events, which can starve hub status processing.
      if (tu_edpt_number(ep_addr) != 0 &&
          edpt_info->xfer_type == TUSB_XFER_INTERRUPT &&
          request_pid == USB_PID_IN) {
        if (edpt_info->intr_no_response_retry_count >= CH32_USBFS_INTR_NO_RESP_RETRY_MAX) {
          LOG_CH32_USBFSH(
            "Interrupt IN no-response retries exceeded (%u), fail transfer. dev=%u ep=0x%02x seq=%u\r\n",
            (unsigned int) CH32_USBFS_INTR_NO_RESP_RETRY_MAX, dev_addr, ep_addr, edpt_info->xfer_seq);
          edpt_info->intr_no_response_retry_count = 0;
          usbfs_xfer_end_no_event();
          hcd_event_xfer_complete(dev_addr, ep_addr, 0, XFER_RESULT_FAILED, in_isr);
          return;
        }
        edpt_info->intr_no_response_retry_count++;
        if (edpt_info->intr_retry_streak < 0xffu) {
          edpt_info->intr_retry_streak++;
        }
        edpt_info->is_nak_pending = true;
        edpt_info->retry_seq = edpt_info->xfer_seq;
        // no-response specific backoff to avoid starving hub/status processing.
        uint32_t retry_gap_sof = (uint32_t) edpt_info->interval * CH32_USBFS_INTR_NO_RESP_RETRY_SOF_MULT;
        if (retry_gap_sof < CH32_USBFS_INTR_NO_RESP_RETRY_SOF_MIN) {
          retry_gap_sof = CH32_USBFS_INTR_NO_RESP_RETRY_SOF_MIN;
        }
        uint32_t next_retry_sof = sof_number + retry_gap_sof;
        if (edpt_info->intr_retry_streak >= CH32_USBFS_RETRY_STREAK_COOLDOWN_THRESHOLD) {
          // Same cooldown path as NAK: avoid monopolizing the retry scheduler.
          uint32_t cooldown_gap_sof = (uint32_t) edpt_info->interval * CH32_USBFS_RETRY_STREAK_COOLDOWN_SOF_MULT;
          if (cooldown_gap_sof < CH32_USBFS_RETRY_STREAK_COOLDOWN_SOF_MIN) {
            cooldown_gap_sof = CH32_USBFS_RETRY_STREAK_COOLDOWN_SOF_MIN;
          }
          edpt_info->retry_cooldown_until_sof = sof_number + cooldown_gap_sof;
          if ((int32_t) (next_retry_sof - edpt_info->retry_cooldown_until_sof) < 0) {
            next_retry_sof = edpt_info->retry_cooldown_until_sof;
          }
        }
        edpt_info->next_retry_sof = next_retry_sof;
        usbfs_xfer_end_no_event();
        return;
      }

      if (dev_addr == 0 && tu_edpt_number(ep_addr) == 0 &&
          root_direct_enum_recovery_allowed && !addr0_setup_recover_used) {
        // Defer heavy recovery to next SETUP submit path (task context).
        addr0_setup_recover_pending = true;
      }

      LOG_CH32_USBFSH(
        "Timeout/no-response. status=0x%02x MIS_ST=0x%04x INT_FG=0x%04x INT_ST=0x%04x "
        "HOST_SETUP=0x%04x HOST_CTRL=0x%04x BASE_CTRL=0x%04x DEV_ADDR=0x%04x\r\n",
        status,
        USBOTG_H_FS->MIS_ST, USBOTG_H_FS->INT_FG, USBOTG_H_FS->INT_ST,
        USBOTG_H_FS->HOST_SETUP, USBOTG_H_FS->HOST_CTRL, USBOTG_H_FS->BASE_CTRL, USBOTG_H_FS->DEV_ADDR);
      usbfs_xfer_end_no_event();
      hcd_event_xfer_complete(dev_addr, ep_addr, 0, XFER_RESULT_FAILED, in_isr);
      return;

    } else if (response_pid == USB_PID_DATA0 || response_pid == USB_PID_DATA1) {
      LOG_CH32_USBFSH("Data toggle mismatched and DATA0/1 (not STALL). RX_LEN=%d\r\n", USBOTG_H_FS->RX_LEN);
      usbfs_xfer_end_no_event();
      hcd_event_xfer_complete(dev_addr, ep_addr, 0, XFER_RESULT_FAILED, in_isr);
      return;

    } else {
      LOG_CH32_USBFSH("hcd_int_handler() L%d: unexpected response dev_addr=%d,ep_addr=0x%02x, PID: 0x%02x, HOST_SETUP=0x%04x,HOST_CTRL=0x%04x,BASE_CTRL=0x%04x\r\n",
                      __LINE__, dev_addr, ep_addr, response_pid,
                      USBOTG_H_FS->HOST_SETUP, USBOTG_H_FS->HOST_CTRL, USBOTG_H_FS->BASE_CTRL);
      usbfs_xfer_end_no_event();
      hcd_event_xfer_complete(dev_addr, ep_addr, 0, XFER_RESULT_FAILED, in_isr);
      return;
    }
  }
}


void hcd_int_handler(uint8_t rhport, bool in_isr) {
  (void) rhport;
  uint8_t int_flag = USBOTG_H_FS->INT_FG;

  if ((int_flag & USBFS_UIF_HST_SOF) == 0) {
    LOG_CH32_USBFSH("hcd_int_handler() int_flag=0x%02x (IS_NAK=%d,TOG_OK=%d,SIE_FREE=%d,FIFO=%d,SOF=%d,SUSPEND=%d,TRANSFER=%d,DETECT=%d)\r\n",
      int_flag,
      int_flag & USBFS_U_IS_NAK ? 1 : 0,
      int_flag & USBFS_U_TOG_OK ? 1 : 0,
      int_flag & USBFS_U_SIE_FREE ? 1 : 0,
      int_flag & USBFS_UIF_FIFO_OV ? 1 : 0,
      int_flag & USBFS_UIF_HST_SOF ? 1 : 0,
      int_flag & USBFS_UIF_SUSPEND ? 1 : 0,
      int_flag & USBFS_UIF_TRANSFER ? 1 : 0,
      int_flag & USBFS_UIF_DETECT ? 1 : 0
    );
  }

  if (int_flag & USBFS_UIF_TRANSFER) {
    LOG_CH32_USBFSH("hcd_int_handler(): BASE_CTRL=0x%04x HOST_CTRL=0x%04x HOST_SETUP=0x%04x MIS_ST=0x%04x\r\n",
      USBOTG_H_FS->BASE_CTRL,
      USBOTG_H_FS->HOST_CTRL,
      USBOTG_H_FS->HOST_SETUP,
      USBOTG_H_FS->MIS_ST
    );
  }

  if (int_flag & USBFS_UIF_DETECT) {
    handle_int_detect(rhport, in_isr);
    // Clear after handler has had a chance to defer/consume it.
    USBOTG_H_FS->INT_FG = USBFS_UIF_DETECT;
  }

  if (int_flag & USBFS_UIF_SUSPEND) {
    handle_int_suspend();
    USBOTG_H_FS->INT_FG = USBFS_UIF_SUSPEND;
  }

  if (int_flag & USBFS_UIF_HST_SOF) {
    handle_int_sof(int_flag);
    // Clear flag
    USBOTG_H_FS->INT_FG = USBFS_UIF_HST_SOF;
  }

  if (int_flag & USBFS_UIF_TRANSFER) {
    const uint8_t pid_edpt = USBOTG_H_FS->HOST_EP_PID;
    const uint8_t status = USBOTG_H_FS->INT_ST;
    const uint8_t dev_addr = USBOTG_H_FS->DEV_ADDR & USBFS_USB_ADDR_MASK;
    handle_int_transfer(in_isr, pid_edpt, status, dev_addr);
    // Clear flag
    USBOTG_H_FS->INT_FG = USBFS_UIF_TRANSFER;
  }
}

//--------------------------------------------------------------------+
// Endpoint API
//--------------------------------------------------------------------+

bool hcd_edpt_open(uint8_t rhport, uint8_t dev_addr, tusb_desc_endpoint_t const *ep_desc) {
  (void) rhport;
  uint8_t ep_addr = ep_desc->bEndpointAddress;
  uint8_t ep_num = tu_edpt_number(ep_addr);
  uint16_t max_packet_size = ep_desc->wMaxPacketSize;
  uint8_t xfer_type = ep_desc->bmAttributes.xfer;
  LOG_CH32_USBFSH_FUNC_CALL();
  LOG_CH32_USBFSH("hcd_edpt_open(rhport=%d, dev_addr=0x%02x, %p) EndpointAdderss=0x%02x,maxPacketSize=%d,xfer_type=%d\r\n", rhport, dev_addr, ep_desc, ep_addr, max_packet_size, xfer_type);

  while (usb_current_xfer_info.is_busy) { }

  if (ep_num == 0x00) {
    usb_edpt_t* edpt_out = get_or_add_edpt_record(dev_addr, 0x00, max_packet_size, xfer_type);
    usb_edpt_t* edpt_in = get_or_add_edpt_record(dev_addr, 0x80, max_packet_size, xfer_type);
    TU_ASSERT(edpt_out != NULL, false);
    TU_ASSERT(edpt_in != NULL, false);
    edpt_out->interval = 0;
    edpt_in->interval = 0;
  } else {
    usb_edpt_t* edpt = get_or_add_edpt_record(dev_addr, ep_addr, max_packet_size, xfer_type);
    TU_ASSERT(edpt != NULL, false);
    edpt->interval = (xfer_type == TUSB_XFER_INTERRUPT) ? ep_desc->bInterval : 0;
  }

  hardware_set_port_address_speed(dev_addr);

  LOG_CH32_USBFSH("hcd_edpt_open(after_port_arm): HOST_CTRL=0x%02x HOST_SETUP=0x%02x MIS_ST=0x%02x\r\n",
                  (uint8_t) USBOTG_H_FS->HOST_CTRL,
                  (uint8_t) USBOTG_H_FS->HOST_SETUP,
                  (uint8_t) USBOTG_H_FS->MIS_ST);

  return true;
}

bool hcd_edpt_xfer(uint8_t rhport, uint8_t dev_addr, uint8_t ep_addr, uint8_t *buffer, uint16_t buflen) {
  (void) rhport;

  LOG_CH32_USBFSH_FUNC_CALL();
  LOG_CH32_USBFSH("hcd_edpt_xfer(%d, 0x%02x, 0x%02x, ...)\r\n", rhport, dev_addr, ep_addr);

  if (!device_addr_connected(dev_addr)) {
    LOG_CH32_USBFSH("hcd_edpt_xfer() drop disconnected device: dev_addr=0x%02x ep_addr=0x%02x\r\n", dev_addr, ep_addr);
    return false;
  }

  usb_edpt_t *edpt_info = get_edpt_record(dev_addr, ep_addr);
  TU_ASSERT(edpt_info != NULL);
  if (edpt_info == NULL) {
    return false;
  }

  bool const is_interrupt_in = (edpt_info->xfer_type == TUSB_XFER_INTERRUPT) &&
                               (tu_edpt_dir(ep_addr) == TUSB_DIR_IN);

  // Respect interrupt polling interval even when upper layer resubmits
  // immediately after each completion.
  if (is_interrupt_in && edpt_info->next_retry_sof != 0 &&
      ((int32_t) (sof_number - edpt_info->next_retry_sof) < 0)) {
    edpt_info->buf = buffer;
    edpt_info->buflen = buflen;
    edpt_info->last_request_pid = USB_PID_IN;
    edpt_info->xfer_start_ms = tusb_time_millis_api();
    edpt_info->xferred_len = 0;
    edpt_info->is_nak_pending = true;
    edpt_info->xfer_seq++;
    edpt_info->retry_seq = edpt_info->xfer_seq;
    edpt_info->transient_timeout_retry_count = 0;
    edpt_info->intr_no_response_retry_count = 0;
    edpt_info->intr_retry_streak = 0;
    edpt_info->retry_cooldown_until_sof = 0;
    LOG_CH32_USBFSH("[diag] defer intr-in poll dev=%u ep=0x%02x seq=%u now_sof=%lu target_sof=%lu\r\n",
                dev_addr, ep_addr, edpt_info->xfer_seq, (uint32_t) sof_number,
                (uint32_t) edpt_info->next_retry_sof);
    return true;
  }

  while (!usbfs_xfer_try_begin(dev_addr, ep_addr, edpt_info)) {}

  hardware_set_port_address_speed(dev_addr);

  edpt_info->buf = buffer;
  edpt_info->buflen = buflen;
  edpt_info->last_request_pid = tu_edpt_dir(ep_addr) == TUSB_DIR_IN ? USB_PID_IN : USB_PID_OUT;
  edpt_info->xfer_start_ms = tusb_time_millis_api();
  edpt_info->xferred_len = 0;
  edpt_info->is_nak_pending = false;
  if (is_interrupt_in && edpt_info->interval != 0) {
    edpt_info->next_retry_sof = sof_number + edpt_info->interval;
  } else {
    edpt_info->next_retry_sof = 0;
  }
  edpt_info->xfer_seq++;
  edpt_info->retry_seq = edpt_info->xfer_seq;
  edpt_info->transient_timeout_retry_count = 0;
  edpt_info->intr_no_response_retry_count = 0;
  edpt_info->intr_retry_streak = 0;
  edpt_info->retry_cooldown_until_sof = 0;
  usbfs_xfer_set_signature(edpt_info->last_request_pid, edpt_info->xfer_seq);
  LOG_CH32_USBFSH("[diag] submit xfer dev=%u ep=0x%02x req=0x%02x seq=%u len=%u\r\n",
              dev_addr, ep_addr, edpt_info->last_request_pid, edpt_info->xfer_seq, buflen);

  // WCH SDK control transfer stage gap (Delay_Us(100) around EP0 stages).
  if (tu_edpt_number(ep_addr) == 0) {
    loopdelay(SystemCoreClock / 1000000 * 100 * 10);
  }

  if (tu_edpt_dir(ep_addr) == TUSB_DIR_IN) {
    LOG_CH32_USBFSH("hcd_edpt_xfer(): READ, dev_addr=0x%02x, ep_addr=0x%02x, len=%d\r\n", dev_addr, ep_addr, buflen);
    return hardware_start_xfer(USB_PID_IN, ep_addr, edpt_info->data_toggle);
  } else {
    LOG_CH32_USBFSH("hcd_edpt_xfer(): WRITE, dev_addr=0x%02x, ep_addr=0x%02x, len=%d\r\n", dev_addr, ep_addr, buflen);
    uint16_t copylen = TU_MIN(edpt_info->max_packet_size, buflen);
    USBOTG_H_FS->HOST_TX_LEN = copylen;
    memcpy(USBFS_TX_Buf, buffer, copylen);
    return hardware_start_xfer(USB_PID_OUT, ep_addr, edpt_info->data_toggle);
  }
}

bool hcd_edpt_abort_xfer(uint8_t rhport, uint8_t dev_addr, uint8_t ep_addr) {
  (void) rhport;
  (void) dev_addr;
  (void) ep_addr;

  LOG_CH32_USBFSH_FUNC_CALL();
  LOG_CH32_USBFSH("hcd_edpt_abort_xfer(dev=%d,ep=0x%02x)\r\n", dev_addr, ep_addr);

  usb_edpt_t* edpt = get_edpt_record(dev_addr, ep_addr);
  if (edpt) {
    edpt->is_nak_pending = false;
    edpt->next_retry_sof = 0;
    edpt->retry_cooldown_until_sof = 0;
    edpt->intr_retry_streak = 0;
    return true;
  } else {
  return false;
  }
}

bool hcd_setup_send(uint8_t rhport, uint8_t dev_addr, uint8_t const setup_packet[8]) {
  (void) rhport;

  LOG_CH32_USBFSH_FUNC_CALL();
  LOG_CH32_USBFSH("hcd_setup_send(rhport=%d, dev_addr=0x%02x, %p)\r\n", rhport, dev_addr, setup_packet);

  usb_edpt_t *edpt_info_tx = get_edpt_record(dev_addr, 0x00);
  usb_edpt_t *edpt_info_rx = get_edpt_record(dev_addr, 0x80);
  TU_ASSERT(edpt_info_tx != NULL, false);
  TU_ASSERT(edpt_info_rx != NULL, false);
  if (edpt_info_tx == NULL || edpt_info_rx == NULL) {
    return false;
  }

  while (!usbfs_xfer_try_begin(dev_addr, 0x00, edpt_info_tx)) {}

  hardware_set_port_address_speed(dev_addr);

  if (dev_addr == 0 && root_direct_enum_recovery_allowed &&
      addr0_setup_recover_pending && !addr0_setup_recover_used) {
    bool prev_int_state = interrupt_enabled;
    // One-shot workaround for direct-root initial enumeration:
    // apply short bus-reset recovery after persistent ADDR0 EP0 no-response.
    LOG_CH32_USBFSH("hcd_setup_send(): apply one-shot root-port recovery for ADDR0 EP0\r\n");
    hcd_int_disable(rhport);
    USBOTG_H_FS->HOST_CTRL |= USBFS_UH_BUS_RESET;
    tusb_time_delay_ms_api(25);
    USBOTG_H_FS->HOST_CTRL &= ~USBFS_UH_BUS_RESET;
    tusb_time_delay_ms_api(3);
    USBOTG_H_FS->HOST_CTRL |= USBFS_UH_PORT_EN;
    USBOTG_H_FS->HOST_SETUP |= USBFS_UH_SOF_EN;
    USBOTG_H_FS->INT_FG = USBFS_UIF_DETECT | USBFS_UIF_SUSPEND | USBFS_UIF_TRANSFER;
    addr0_setup_recover_pending = false;
    addr0_setup_recover_used = true;
    if (prev_int_state) {
      hcd_int_enable(rhport);
    }
    log_mis_state("hcd_setup_send(after_recovery)");
  }

  LOG_CH32_USBFSH("hcd_setup_send(after_port_arm): HOST_CTRL=0x%02x HOST_SETUP=0x%02x MIS_ST=0x%02x DEV_ADDR=0x%02x\r\n",
                  (uint8_t) USBOTG_H_FS->HOST_CTRL,
                  (uint8_t) USBOTG_H_FS->HOST_SETUP,
                  (uint8_t) USBOTG_H_FS->MIS_ST,
                  (uint8_t) USBOTG_H_FS->DEV_ADDR);
  log_mis_state("hcd_setup_send(pre_wait)");

  // Initialize data toggle (SETUP always starts with DATA0)
  // Data toggle for OUT is toggled in hcd_int_handler()
  edpt_info_tx->data_toggle = 0;
  edpt_info_tx->last_request_pid = USB_PID_SETUP;
  // Data toggle for IN must be set 0x01 manually.
  edpt_info_rx->data_toggle = 0x01;
  const uint16_t setup_packet_datalen = 8;
  memcpy(USBFS_TX_Buf, setup_packet, setup_packet_datalen);
  USBOTG_H_FS->HOST_TX_LEN = setup_packet_datalen;
  usb_edpt_t* edpt_info = edpt_info_tx;
  uint8_t ep_addr = 0x00;
  edpt_info->xfer_start_ms = tusb_time_millis_api();
  edpt_info->buf = (uint8_t*)(uintptr_t)setup_packet;
  edpt_info->buflen = setup_packet_datalen;
  edpt_info->xferred_len = 0;
  edpt_info->is_nak_pending = false;
  edpt_info->next_retry_sof = 0;
  edpt_info->xfer_seq++;
  edpt_info->retry_seq = edpt_info->xfer_seq;
  edpt_info->transient_timeout_retry_count = 0;
  edpt_info->intr_no_response_retry_count = 0;
  edpt_info->intr_retry_streak = 0;
  edpt_info->retry_cooldown_until_sof = 0;
  usbfs_xfer_set_signature(USB_PID_SETUP, edpt_info->xfer_seq);
  LOG_CH32_USBFSH("[diag] submit setup dev=%u ep=0x00 seq=%u\r\n", dev_addr, edpt_info->xfer_seq);

  // Timing workaround: if attach/detect happened during reset, wait extra time
  // and a few SOF frames before first EP0 SETUP in non-blocking enumeration flow.
  if (extra_setup_settle_pending && tu_edpt_number(ep_addr) == 0 && dev_addr == 0) {
    LOG_CH32_USBFSH("hcd_setup_send(): extra settle delay before first EP0 SETUP\r\n");
    tusb_time_delay_ms_api(20);
    // Guard against async enumeration timing: ensure SOF is running for
    // a few frames before issuing the very first EP0 SETUP.
    uint32_t sof_start = sof_number;
    uint32_t t_start_ms = tusb_time_millis_api();
    while (((uint32_t) (sof_number - sof_start) < 3u) &&
           ((uint32_t) (tusb_time_millis_api() - t_start_ms) < 30u)) {
      // spin
    }
    LOG_CH32_USBFSH("hcd_setup_send(): SOF wait done, delta_sof=%lu elapsed_ms=%lu\r\n",
                    (uint32_t) (sof_number - sof_start),
                    (uint32_t) (tusb_time_millis_api() - t_start_ms));
    log_mis_state("hcd_setup_send(post_wait)");
    extra_setup_settle_pending = false;
  }

  // WCH SDK control transfer stage gap (Delay_Us(100) before SETUP transaction).
  loopdelay(SystemCoreClock / 1000000 * 100);

  // Some low-speed devices need additional settle time after SET_ADDRESS before
  // accepting the first request on the new address.
  if (post_set_address_settle_pending && dev_addr != 0) {
    LOG_CH32_USBFSH("hcd_setup_send(): post-SET_ADDRESS settle wait for dev=%u\r\n", dev_addr);
    tusb_time_delay_ms_api(3);
    uint32_t sof_start = sof_number;
    uint32_t t_start_ms = tusb_time_millis_api();
    while (((uint32_t) (sof_number - sof_start) < 2u) &&
           ((uint32_t) (tusb_time_millis_api() - t_start_ms) < 20u)) {
      // spin
    }
    post_set_address_settle_pending = false;
  }

  USBOTG_H_FS->HOST_SETUP |= USBFS_UH_SOF_EN;
  USBOTG_H_FS->HOST_CTRL |= USBFS_UH_PORT_EN;

  // Give the port state a little time to settle in the delayed-enumeration path.
  loopdelay(SystemCoreClock / 1000000 * 200);

  LOG_CH32_USBFSH(
    "hcd_setup_send(before_start): HOST_TX_LEN=%u HOST_TX_CTRL=0x%02x HOST_RX_CTRL=0x%02x HOST_EP_PID=0x%02x\r\n",
    USBOTG_H_FS->HOST_TX_LEN,
    (uint8_t) USBOTG_H_FS->HOST_TX_CTRL,
    (uint8_t) USBOTG_H_FS->HOST_RX_CTRL,
    (uint8_t) USBOTG_H_FS->HOST_EP_PID);

  // SET_ADDRESS (bRequest=5) is sent to dev_addr=0.
  if (dev_addr == 0 && setup_packet[1] == 0x05u) {
    post_set_address_settle_pending = true;
  }

  hardware_start_xfer(USB_PID_SETUP, 0, 0);

  return true;
}

bool hcd_edpt_clear_stall(uint8_t rhport, uint8_t dev_addr, uint8_t ep_addr) {
  (void) rhport;
  (void) dev_addr;

  LOG_CH32_USBFSH_FUNC_CALL();
  LOG_CH32_USBFSH("hcd_edpt_clear_stall(rhport=%d, dev_addr=0x%02x, ep_addr=0x%02x)\r\n", rhport, dev_addr, ep_addr);
  if (tu_edpt_number(ep_addr) == 0x00) {
    // On EP 0, CLEAR_FEATURE should not be executed.
    return true;
  }
  // uint8_t edpt_num = tu_edpt_number(ep_addr);
  uint8_t edpt_num = ep_addr;
  uint8_t setup_request_clear_stall[8] = {
      0x02, 0x01, 0x00, 0x00, edpt_num, 0x00, 0x00, 0x00
  };
  memcpy(USBFS_TX_Buf, setup_request_clear_stall, 8);
  USBOTG_H_FS->HOST_TX_LEN = 8;

  bool prev_int_state = interrupt_enabled;
  hcd_int_disable(0);

  USBOTG_H_FS->HOST_EP_PID = (USB_PID_SETUP << 4) | 0x00;
  USBOTG_H_FS->INT_FG = USBFS_UIF_TRANSFER;
  while ((USBOTG_H_FS->INT_FG & USBFS_UIF_TRANSFER) == 0) {}
  USBOTG_H_FS->HOST_EP_PID = 0;
  uint8_t response_pid = USBOTG_H_FS->INT_ST & USBFS_UIS_H_RES_MASK;
  (void) response_pid;
  LOG_CH32_USBFSH("hcd_edpt_clear_stall() response pid=0x%02x\r\n", response_pid);

  if (prev_int_state) {
    hcd_int_enable(0);
  }

  return true;
}

#endif

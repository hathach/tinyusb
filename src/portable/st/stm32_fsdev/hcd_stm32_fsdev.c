/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2025 HiFiPhile (Zixun LI)
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

/**********************************************
 * This driver provides USB Host controller support for STM32 MCUs with "USB A"/"PCD"/"HCD" peripheral.
 * This covers these MCU families:
 *
 * C0           2048 byte buffer; 32-bit bus; host mode
 * G0           2048 byte buffer; 32-bit bus; host mode
 * U3           2048 byte buffer; 32-bit bus; host mode
 * H5           2048 byte buffer; 32-bit bus; host mode
 * U535, U545   2048 byte buffer; 32-bit bus; host mode
 *
 */

#include "tusb_option.h"

#if CFG_TUH_ENABLED && defined(TUP_USBIP_FSDEV) && \
    TU_CHECK_MCU(OPT_MCU_STM32C0, OPT_MCU_STM32G0, OPT_MCU_STM32H5, OPT_MCU_STM32U5)

#include "host/hcd.h"
#include "host/usbh.h"
#include "fsdev_common.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+

// Debug level for FSDEV
#define FSDEV_DEBUG 3

// Max number of endpoints application can open, can be larger than FSDEV_EP_COUNT
#ifndef CFG_TUH_FSDEV_ENDPOINT_MAX
  #define CFG_TUH_FSDEV_ENDPOINT_MAX 16u
#endif

TU_VERIFY_STATIC(CFG_TUH_FSDEV_ENDPOINT_MAX <= 255, "currently only use 8-bit for index");

#if CFG_TUSB_MCU == OPT_MCU_STM32H5
  #define CPU_FREQUENCY_MHZ 250U
#elif CFG_TUSB_MCU == OPT_MCU_STM32U5
  #define CPU_FREQUENCY_MHZ 160U
#elif CFG_TUSB_MCU == OPT_MCU_STM32U3
  #define CPU_FREQUENCY_MHZ 96U
#elif CFG_TUSB_MCU == OPT_MCU_STM32G0
  #define CPU_FREQUENCY_MHZ 64U
#elif CFG_TUSB_MCU == OPT_MCU_STM32C0
  #define CPU_FREQUENCY_MHZ 48U
#else
  #error "CPU_FREQUENCY_MHZ not defined for this STM32 MCU"
#endif

enum {
  HCD_XFER_ERROR_MAX = 3,
  HCD_XFER_NAK_MAX = 15,
  HCD_XFER_NAK_DEFAULT = 3,
};

// Host driver struct for each opened endpoint
typedef struct {
  uint8_t *buffer;
  uint16_t buflen;
  uint16_t queued_len;
  uint16_t max_packet_size;
  uint8_t dev_addr;
  uint8_t ep_addr;
  uint8_t ep_type;
  uint8_t interval;
  struct TU_ATTR_PACKED {
    uint8_t ls_pre : 1;
    uint8_t allocated : 1;
    uint8_t next_setup : 1;
    uint8_t pid : 1;
  };
} hcd_endpoint_t;

// Channel direction state
typedef struct {
  hcd_endpoint_t* edpt;
  struct TU_ATTR_PACKED {
    uint8_t allocated : 1;
    uint8_t retry : 3;
    uint8_t nak : 4; // Max NAK count in current frame
  };
} hcd_channel_dir_t;

// Additional info for each channel when it is active
typedef struct {
  uint8_t dev_addr;
  uint8_t ep_num;
  uint8_t ep_type;
  hcd_channel_dir_t out, in;
} hcd_channel_t;

static struct {
  hcd_channel_t channel[FSDEV_EP_COUNT];
  hcd_endpoint_t edpt[CFG_TUH_FSDEV_ENDPOINT_MAX];
  bool connected;
} _hcd_data;

static tuh_configure_fsdev_t _tuh_cfg = {
    .max_nak = HCD_XFER_NAK_DEFAULT,
};

//--------------------------------------------------------------------+
// Prototypes
//--------------------------------------------------------------------+

static uint8_t endpoint_alloc(void);
static uint8_t endpoint_find(uint8_t dev_addr, uint8_t ep_addr);
static uint32_t hcd_pma_alloc(uint8_t channel, tusb_dir_t dir, uint16_t len);
static uint8_t channel_alloc(uint8_t dev_addr, uint8_t ep_addr, uint8_t ep_type);
static bool edpt_xfer_kickoff(uint8_t ep_id);
static bool channel_xfer_start(uint8_t ch_id, tusb_dir_t dir);
static void edpoint_close(uint8_t ep_id);
static void port_status_handler(uint8_t rhport, bool in_isr);
static void ch_handle_ack(uint8_t ch_id, uint32_t ch_reg, tusb_dir_t dir);
static void ch_handle_nak(uint8_t ch_id, uint32_t ch_reg, tusb_dir_t dir);
static void ch_handle_stall(uint8_t ch_id, uint32_t ch_reg, tusb_dir_t dir);
static void ch_handle_error(uint8_t ch_id, uint32_t ch_reg, tusb_dir_t dir);

//--------------------------------------------------------------------+
// Inline Functions
//--------------------------------------------------------------------+

static inline void endpoint_dealloc(hcd_endpoint_t* edpt) {
  edpt->allocated = 0;
}

static inline void channel_dealloc(hcd_channel_t* ch, tusb_dir_t dir) {
  if (dir == TUSB_DIR_OUT) {
    ch->out.allocated = 0;
  } else {
    ch->in.allocated = 0;
  }
}

// Write channel state in specified direction
static inline void channel_write_status(uint8_t ch_id, uint32_t ch_reg, tusb_dir_t dir, ep_stat_t state, bool need_exclusive) {
  ch_reg &= USB_EPREG_MASK | CH_STAT_MASK(dir);
  ch_change_status(&ch_reg, dir, state);
  ch_write(ch_id, ch_reg, need_exclusive);
}

static inline uint16_t channel_get_rx_count(uint8_t ch_id) {
  /* https://www.st.com/resource/en/errata_sheet/es0561-stm32h503cbebkbrb-device-errata-stmicroelectronics.pdf
  * https://www.st.com/resource/en/errata_sheet/es0587-stm32u535xx-and-stm32u545xx-device-errata-stmicroelectronics.pdf
  * From H503/U535 errata: Buffer description table update completes after CTR interrupt triggers
  * Description:
  * - During OUT transfers, the correct transfer interrupt (CTR) is triggered a little before the last USB SRAM accesses
  * have completed. If the software responds quickly to the interrupt, the full buffer contents may not be correct.
  * Workaround:
  * - Software should ensure that a small delay is included before accessing the SRAM contents. This delay
  * should be 800 ns in Full Speed mode and 6.4 μs in Low Speed mode
  *
  * Note: this errata may also apply to G0, U5, H5 etc.
  *
  * We choose the delay count based on max CPU frequency (in MHz) to ensure the delay is at least the required time.
  */

  uint32_t ch_reg = ch_read(ch_id);
  if (FSDEV_REG->ISTR & USB_ISTR_LS_DCONN || ch_reg & USB_CHEP_LSEP) {
    // Low speed mode: 6.4 us delay -> about 2 cycles per MHz
    volatile uint32_t cycle_count = CPU_FREQUENCY_MHZ * 2U;
    while (cycle_count > 0U) {
      cycle_count--; // each count take 3 cycles (1 for sub, jump, and compare)
    }
  } else {
    // Full speed mode: 800 ns delay -> about 0.25 cycles per MHz
    volatile uint32_t cycle_count = CPU_FREQUENCY_MHZ / 4U;
    while (cycle_count > 0U) {
      cycle_count--; // each count take 3 cycles (1 for sub, jump, and compare)
    }
  }

  return btable_get_count(ch_id, BTABLE_BUF_RX);
}

//--------------------------------------------------------------------+
// Controller API
//--------------------------------------------------------------------+

// Optional HCD configuration, called by tuh_configure()
bool hcd_configure(uint8_t rhport, uint32_t cfg_id, const void* cfg_param) {
  (void) rhport;
  TU_VERIFY(cfg_id == TUH_CFGID_FSDEV && cfg_param != NULL);

  tuh_configure_param_t const* cfg = (tuh_configure_param_t const*) cfg_param;
  _tuh_cfg.max_nak = tu_min8(cfg->fsdev.max_nak, HCD_XFER_NAK_MAX);
  return true;
}

// Initialize controller to host mode
bool hcd_init(uint8_t rhport, const tusb_rhport_init_t* rh_init) {
  (void) rh_init;

  fsdev_core_reset();

  FSDEV_REG->CNTR = USB_CNTR_HOST; // Enable USB in Host mode

  tu_memclr(&_hcd_data, sizeof(_hcd_data));

  // Enable interrupts for host mode
  FSDEV_REG->CNTR |= USB_CNTR_RESETM | USB_CNTR_CTRM | USB_CNTR_SOFM | USB_CNTR_SUSPM |
                     USB_CNTR_WKUPM | USB_CNTR_ERRM | USB_CNTR_PMAOVRM;

  // Initialize port state
  _hcd_data.connected = false;

  fsdev_connect(rhport);

  // If DCON_STAT is already set, the controller sometimes misses the initial connection interrupt
  if (FSDEV_REG->ISTR & USB_ISTR_DCON_STAT) {
    // Wait DP/DM stabilize time
    volatile uint32_t cycle_count = CPU_FREQUENCY_MHZ / 4U;
    while (cycle_count > 0U) {
      cycle_count--;
    }
    port_status_handler(rhport, false);
  }

  return true;
}

bool hcd_deinit(uint8_t rhport) {
  (void)rhport;

  fsdev_disconnect(rhport);

  fsdev_deinit();

  return true;
}

//--------------------------------------------------------------------+
// Interrupt Helper Functions
//--------------------------------------------------------------------+

static inline void sof_handler(void) {
  // Reset NAK counters for all active channels
  for (uint8_t ch_id = 0; ch_id < FSDEV_EP_COUNT; ch_id++) {
    hcd_channel_t* channel = &_hcd_data.channel[ch_id];
    if (channel->out.allocated) {
      channel->out.nak = 0;
    }
    if (channel->in.allocated) {
      channel->in.nak = 0;
    }
  }
}

static void port_status_handler(uint8_t rhport, bool in_isr) {
  uint32_t const fnr_reg = FSDEV_REG->FNR;
  uint32_t const istr_reg = FSDEV_REG->ISTR;
  // SE0 detected USB Disconnected state
  if ((fnr_reg & (USB_FNR_RXDP | USB_FNR_RXDM)) == 0U) {
    _hcd_data.connected = false;
    hcd_event_device_remove(rhport, in_isr);
    return;
  }

  if (!_hcd_data.connected) {
    // J-state or K-state detected & LastState=Disconnected
    if (((fnr_reg & USB_FNR_RXDP) != 0U) || ((istr_reg & USB_ISTR_LS_DCONN) != 0U)) {
      _hcd_data.connected = true;
      hcd_event_device_attach(rhport, in_isr);
    }
  } else {
    // J-state or K-state detected & lastState=Connected: a Missed disconnection is detected
    if (((fnr_reg & USB_FNR_RXDP) != 0U) || ((istr_reg & USB_ISTR_LS_DCONN) != 0U)) {
      _hcd_data.connected = false;
      hcd_event_device_remove(rhport, in_isr);
    }
  }
}

// Handle ACK response
static void ch_handle_ack(uint8_t ch_id, uint32_t ch_reg, tusb_dir_t dir) {
  uint8_t const ep_num = ch_reg & USB_EPADDR_FIELD;
  uint8_t const daddr = (ch_reg & USB_CHEP_DEVADDR_Msk) >> USB_CHEP_DEVADDR_Pos;

  uint8_t ep_id = endpoint_find(daddr, ep_num | (dir == TUSB_DIR_IN ? TUSB_DIR_IN_MASK : 0));
  if (ep_id == TUSB_INDEX_INVALID_8) {
    return;
  }

  hcd_endpoint_t *edpt    = &_hcd_data.edpt[ep_id];
  hcd_channel_t  *channel = &_hcd_data.channel[ch_id];

  if (dir == TUSB_DIR_OUT) {
    // OUT/TX direction
    if (edpt->buflen != edpt->queued_len) {
      // More data to send
      uint16_t const len = tu_min16(edpt->buflen - edpt->queued_len, edpt->max_packet_size);
      uint16_t pma_addr = (uint16_t) btable_get_addr(ch_id, BTABLE_BUF_TX);
      tu_hwfifo_write(PMA_BUF_AT(pma_addr), &(edpt->buffer[edpt->queued_len]), len, NULL);
      btable_set_count(ch_id, BTABLE_BUF_TX, len);
      edpt->queued_len += len;
      channel_write_status(ch_id, ch_reg, TUSB_DIR_OUT, EP_STAT_VALID, false);
      channel->out.nak = 0;
    } else {
      // Transfer complete
      channel_dealloc(channel, TUSB_DIR_OUT);
      edpt->pid = (ch_reg & USB_CHEP_DTOG_TX) ? 1 : 0;
      hcd_event_xfer_complete(daddr, ep_num, edpt->queued_len, XFER_RESULT_SUCCESS, true);
    }
  } else {
    // IN/RX direction
    uint16_t const rx_count = channel_get_rx_count(ch_id);
    uint16_t pma_addr = (uint16_t) btable_get_addr(ch_id, BTABLE_BUF_RX);
    tu_hwfifo_read(PMA_BUF_AT(pma_addr), edpt->buffer + edpt->queued_len, rx_count, NULL);
    edpt->queued_len += rx_count;

    if ((rx_count < edpt->max_packet_size) || (edpt->queued_len >= edpt->buflen)) {
      // Transfer complete (short packet or all bytes received)
      channel_dealloc(channel, TUSB_DIR_IN);
      edpt->pid = (ch_reg & USB_CHEP_DTOG_RX) ? 1 : 0;
      hcd_event_xfer_complete(daddr, ep_num | TUSB_DIR_IN_MASK, edpt->queued_len, XFER_RESULT_SUCCESS, true);
    } else {
      // More data expected
      uint16_t const cnt = tu_min16(edpt->buflen - edpt->queued_len, edpt->max_packet_size);
      btable_set_rx_bufsize(ch_id, BTABLE_BUF_RX, cnt);
      channel_write_status(ch_id, ch_reg, TUSB_DIR_IN, EP_STAT_VALID, false);
      channel->in.nak = 0;
    }
  }
}

// Handle NAK response
static void ch_handle_nak(uint8_t ch_id, uint32_t ch_reg, tusb_dir_t dir) {
  uint8_t const ep_num = ch_reg & USB_EPADDR_FIELD;
  uint8_t const daddr = (ch_reg & USB_CHEP_DEVADDR_Msk) >> USB_CHEP_DEVADDR_Pos;

  uint8_t ep_id = endpoint_find(daddr, ep_num | (dir == TUSB_DIR_IN ? TUSB_DIR_IN_MASK : 0));
  if (ep_id == TUSB_INDEX_INVALID_8) return;

  hcd_endpoint_t* edpt = &_hcd_data.edpt[ep_id];
  // Retry non-periodic transfer immediately if NAK count not exceeded
  // Periodic transfer will be retried by next frame automatically
  if (edpt->ep_type == TUSB_XFER_CONTROL || edpt->ep_type == TUSB_XFER_BULK) {
    hcd_channel_dir_t* channel_dir =
      (dir == TUSB_DIR_OUT) ? &(_hcd_data.channel[ch_id].out) : &(_hcd_data.channel[ch_id].in);
    if (channel_dir->nak < HCD_XFER_NAK_MAX) {
      channel_dir->nak++;
    }
    if (channel_dir->nak < _tuh_cfg.max_nak || _tuh_cfg.max_nak == 0) {
      channel_write_status(ch_id, ch_reg, dir, EP_STAT_VALID, false);
    }
  }
}

// Handle STALL response
static void ch_handle_stall(uint8_t ch_id, uint32_t ch_reg, tusb_dir_t dir) {
  uint8_t const ep_num = ch_reg & USB_EPADDR_FIELD;
  uint8_t const daddr = (ch_reg & USB_CHEP_DEVADDR_Msk) >> USB_CHEP_DEVADDR_Pos;

  uint8_t ep_id = endpoint_find(daddr, ep_num | (dir == TUSB_DIR_IN ? TUSB_DIR_IN_MASK : 0));
  if (ep_id == TUSB_INDEX_INVALID_8) return;

  hcd_endpoint_t* edpt = &_hcd_data.edpt[ep_id];
  hcd_channel_t* channel = &_hcd_data.channel[ch_id];
  channel_dealloc(channel, dir);

  channel_write_status(ch_id, ch_reg, dir, EP_STAT_DISABLED, false);

  hcd_event_xfer_complete(daddr, ep_num | (dir == TUSB_DIR_IN ? TUSB_DIR_IN_MASK : 0),
                         edpt->queued_len, XFER_RESULT_STALLED, true);
}

// Handle error response
static void ch_handle_error(uint8_t ch_id, uint32_t ch_reg, tusb_dir_t dir) {
  uint8_t const ep_num = ch_reg & USB_EPADDR_FIELD;
  uint8_t const daddr = (ch_reg & USB_CHEP_DEVADDR_Msk) >> USB_CHEP_DEVADDR_Pos;

  uint8_t ep_id = endpoint_find(daddr, ep_num | (dir == TUSB_DIR_IN ? TUSB_DIR_IN_MASK : 0));
  if (ep_id == TUSB_INDEX_INVALID_8) return;

  hcd_endpoint_t* edpt = &_hcd_data.edpt[ep_id];
  hcd_channel_t* channel = &_hcd_data.channel[ch_id];

  ch_reg &= USB_EPREG_MASK | CH_STAT_MASK(dir);
  ch_reg &= ~(dir == TUSB_DIR_OUT ? USB_CH_ERRTX : USB_CH_ERRRX);

  hcd_channel_dir_t* channel_dir =
      (dir == TUSB_DIR_OUT) ? &(_hcd_data.channel[ch_id].out) : &(_hcd_data.channel[ch_id].in);
  if (channel_dir->retry < HCD_XFER_ERROR_MAX) {
    // Retry
    channel_dir->retry++;
    ch_change_status(&ch_reg, dir, EP_STAT_VALID);
  } else {
    // Failed after retries
    channel_dealloc(channel, dir);
    ch_change_status(&ch_reg, dir, EP_STAT_DISABLED);
    hcd_event_xfer_complete(daddr, ep_num | (dir == TUSB_DIR_IN ? TUSB_DIR_IN_MASK : 0),
                           edpt->queued_len, XFER_RESULT_FAILED, true);
  }
  ch_write(ch_id, ch_reg, false);
}

// Handle CTR interrupt for the TX/OUT direction
static inline void handle_ctr_tx(uint32_t ch_id) {
  uint32_t ch_reg = ch_read(ch_id) | USB_EP_CTR_TX | USB_EP_CTR_RX;
  hcd_channel_t* channel = &_hcd_data.channel[ch_id];
  TU_VERIFY(channel->out.allocated == 1,);

  if ((ch_reg & USB_CH_ERRTX) == 0U) {
    // No error
    if ((ch_reg & USB_CH_TX_STTX) == USB_CH_TX_ACK_SBUF) {
      ch_handle_ack(ch_id, ch_reg, TUSB_DIR_OUT);
    } else if ((ch_reg & USB_CH_TX_STTX) == USB_CH_TX_NAK) {
      ch_handle_nak(ch_id, ch_reg, TUSB_DIR_OUT);
    } else if ((ch_reg & USB_CH_TX_STTX) == USB_CH_TX_STALL) {
      ch_handle_stall(ch_id, ch_reg, TUSB_DIR_OUT);
    }
  } else {
    ch_handle_error(ch_id, ch_reg, TUSB_DIR_OUT);
  }
}

// Handle CTR interrupt for the RX/IN direction
static inline void handle_ctr_rx(uint32_t ch_id) {
  uint32_t ch_reg = ch_read(ch_id) | USB_EP_CTR_TX | USB_EP_CTR_RX;
  hcd_channel_t* channel = &_hcd_data.channel[ch_id];
  TU_VERIFY(channel->in.allocated == 1,);

  if ((ch_reg & USB_CH_ERRRX) == 0U) {
    // No error
    if ((ch_reg & USB_CH_RX_STRX) == USB_CH_RX_ACK_SBUF) {
      ch_handle_ack(ch_id, ch_reg, TUSB_DIR_IN);
    } else if ((ch_reg & USB_CH_RX_STRX) == USB_CH_RX_NAK) {
      ch_handle_nak(ch_id, ch_reg, TUSB_DIR_IN);
    } else if ((ch_reg & USB_CH_RX_STRX) == USB_CH_RX_STALL){
      ch_handle_stall(ch_id, ch_reg, TUSB_DIR_IN);
    }
  } else {
    ch_handle_error(ch_id, ch_reg, TUSB_DIR_IN);
  }
}

// Interrupt Handler
void hcd_int_handler(uint8_t rhport, bool in_isr) {
  uint32_t int_status = FSDEV_REG->ISTR;

  // Start of Frame
  if (int_status & USB_ISTR_SOF) {
    FSDEV_REG->ISTR = (fsdev_bus_t)~USB_ISTR_SOF;
    sof_handler();
  }

  // Port Change Detected (Connection/Disconnection)
  if (int_status & USB_ISTR_DCON) {
    FSDEV_REG->ISTR = (fsdev_bus_t)~USB_ISTR_DCON;
    port_status_handler(rhport, in_isr);
  }

  // Handle transfer complete (CTR)
  while (FSDEV_REG->ISTR & USB_ISTR_CTR) {
    uint32_t const ch_id = FSDEV_REG->ISTR & USB_ISTR_EP_ID;
    uint32_t const ch_reg = ch_read(ch_id);

    if (ch_reg & USB_EP_CTR_RX) {
      ch_write_clear_ctr(ch_id, TUSB_DIR_IN);
      handle_ctr_rx(ch_id);
    }

    if (ch_reg & USB_EP_CTR_TX) {
      ch_write_clear_ctr(ch_id, TUSB_DIR_OUT);
      handle_ctr_tx(ch_id);
    }
  }

  if (int_status & USB_ISTR_ERR) {
    FSDEV_REG->ISTR = (fsdev_bus_t)~USB_ISTR_ERR;
    // TODO: Handle error
  }

  if (int_status & USB_ISTR_PMAOVR) {
    TU_BREAKPOINT();
    FSDEV_REG->ISTR = (fsdev_bus_t)~USB_ISTR_PMAOVR;
  }
}

// Enable USB interrupt
void hcd_int_enable(uint8_t rhport) {
  fsdev_int_enable(rhport);
}

// Disable USB interrupt
void hcd_int_disable(uint8_t rhport) {
  fsdev_int_disable(rhport);
}

// Get frame number (1ms)
uint32_t hcd_frame_number(uint8_t rhport) {
  (void) rhport;
  return FSDEV_REG->FNR & USB_FNR_FN;
}

//--------------------------------------------------------------------+
// Port API
//--------------------------------------------------------------------+

// Get the current connect status of roothub port
bool hcd_port_connect_status(uint8_t rhport) {
  (void) rhport;
  return _hcd_data.connected;
}

// Reset USB bus on the port
void hcd_port_reset(uint8_t rhport) {
  (void) rhport;
  FSDEV_REG->CNTR |= USB_CNTR_FRES;
}

// Complete bus reset sequence
void hcd_port_reset_end(uint8_t rhport) {
  (void) rhport;
  FSDEV_REG->CNTR &= ~USB_CNTR_FRES;
}

// Get port link speed
tusb_speed_t hcd_port_speed_get(uint8_t rhport) {
  (void) rhport;
  if ((FSDEV_REG->ISTR & USB_ISTR_LS_DCONN) != 0U) {
    return TUSB_SPEED_LOW;
  } else {
    return TUSB_SPEED_FULL;
  }
}

// HCD closes all opened endpoints belonging to this device
void hcd_device_close(uint8_t rhport, uint8_t dev_addr) {
  (void) rhport;

  // Close all endpoints for this device
  for(uint32_t i = 0; i < CFG_TUH_FSDEV_ENDPOINT_MAX; i++) {
    hcd_endpoint_t* edpt = &_hcd_data.edpt[i];
    if (edpt->allocated == 1 && edpt->dev_addr == dev_addr) {
      edpoint_close(i);
    }
  }

}

//--------------------------------------------------------------------+
// Endpoints API
//--------------------------------------------------------------------+

// Open an endpoint
bool hcd_edpt_open(uint8_t rhport, uint8_t dev_addr, tusb_desc_endpoint_t const *ep_desc) {
  (void) rhport;

  uint8_t const ep_addr = ep_desc->bEndpointAddress;
  uint16_t const packet_size = tu_edpt_packet_size(ep_desc);
  uint8_t const ep_type = ep_desc->bmAttributes.xfer;

  uint8_t const ep_id = endpoint_alloc();
  TU_ASSERT(ep_id != TUSB_INDEX_INVALID_8);

  hcd_endpoint_t* edpt = &_hcd_data.edpt[ep_id];
  edpt->dev_addr = dev_addr;
  edpt->ep_addr = ep_addr;
  edpt->ep_type = ep_type;
  edpt->max_packet_size = packet_size;
  edpt->interval = ep_desc->bInterval;
  edpt->pid = 0;
  edpt->ls_pre = (hcd_port_speed_get(rhport) == TUSB_SPEED_FULL && tuh_speed_get(dev_addr) == TUSB_SPEED_LOW) ? 1 : 0;

  return true;
}

bool hcd_edpt_close(uint8_t rhport, uint8_t dev_addr, uint8_t ep_addr) {
  (void) rhport;

  uint8_t const ep_id = endpoint_find(dev_addr, ep_addr);
  TU_ASSERT(ep_id != TUSB_INDEX_INVALID_8);

  edpoint_close(ep_id);

  return true;
}

// Submit a transfer
bool hcd_edpt_xfer(uint8_t rhport, uint8_t dev_addr, uint8_t ep_addr, uint8_t *buffer, uint16_t buflen) {
  (void) rhport;

  TU_LOG(FSDEV_DEBUG, "hcd_edpt_xfer addr=%u ep=0x%02X len=%u\r\n", dev_addr, ep_addr, buflen);

  uint8_t const ep_id = endpoint_find(dev_addr, ep_addr);
  TU_ASSERT(ep_id != TUSB_INDEX_INVALID_8);

  hcd_endpoint_t *edpt = &_hcd_data.edpt[ep_id];

  edpt->buffer = buffer;
  edpt->buflen = buflen;
  edpt->queued_len = 0;

  uint8_t const ep_num = tu_edpt_number(ep_addr);
  if (ep_num == 0) {
    // update ep_dir since control endpoint can switch direction
    edpt->ep_addr = ep_addr;
  }

  return edpt_xfer_kickoff(ep_id);
}

// Abort a queued transfer
bool hcd_edpt_abort_xfer(uint8_t rhport, uint8_t dev_addr, uint8_t ep_addr) {
  (void) rhport;

  uint8_t const ep_id = endpoint_find(dev_addr, ep_addr);
  TU_ASSERT(ep_id != TUSB_INDEX_INVALID_8);
  tusb_dir_t const dir = tu_edpt_dir(ep_addr);

  for (uint8_t i = 0; i < FSDEV_EP_COUNT; i++) {
    hcd_channel_t* channel = &_hcd_data.channel[i];
    uint8_t const allocated = (dir == TUSB_DIR_OUT) ? channel->out.allocated : channel->in.allocated;

    if (allocated == 1 &&
        channel->dev_addr == dev_addr &&
        channel->ep_num == tu_edpt_number(ep_addr)) {
      channel_dealloc(channel, dir);
      uint32_t ch_reg = ch_read(i) | USB_EP_CTR_TX | USB_EP_CTR_RX;
      channel_write_status(i, ch_reg, dir, EP_STAT_DISABLED, true);
    }
  }

  return true;
}

// Submit a special transfer to send 8-byte Setup Packet
bool hcd_setup_send(uint8_t rhport, uint8_t dev_addr, uint8_t const setup_packet[8]) {
  (void) rhport;

  uint8_t const ep_id = endpoint_find(dev_addr, 0);
  TU_ASSERT(ep_id != TUSB_INDEX_INVALID_8);

  hcd_endpoint_t *edpt = &_hcd_data.edpt[ep_id];
  edpt->next_setup = true;
  edpt->pid = 0;

  return hcd_edpt_xfer(rhport, dev_addr, 0, (uint8_t*)(uintptr_t) setup_packet, 8);
}

// Clear stall, data toggle is also reset to DATA0
bool hcd_edpt_clear_stall(uint8_t rhport, uint8_t dev_addr, uint8_t ep_addr) {
  (void) rhport;
  (void) dev_addr;
  (void) ep_addr;

  uint8_t const ep_id = endpoint_find(dev_addr, 0);
  TU_ASSERT(ep_id != TUSB_INDEX_INVALID_8);

  hcd_endpoint_t *edpt = &_hcd_data.edpt[ep_id];
  edpt->pid = 0;

  return true;
}

//--------------------------------------------------------------------+
// Helper Functions
//--------------------------------------------------------------------+

static uint8_t endpoint_alloc(void) {
  for (uint32_t i = 0; i < CFG_TUH_FSDEV_ENDPOINT_MAX; i++) {
    hcd_endpoint_t* edpt = &_hcd_data.edpt[i];
    if (edpt->allocated == 0) {
      edpt->allocated = 1;
      return i;
    }
  }
  return TUSB_INDEX_INVALID_8;
}

static uint8_t endpoint_find(uint8_t dev_addr, uint8_t ep_addr) {
  uint8_t const ep_num = tu_edpt_number(ep_addr);
  tusb_dir_t const ep_dir = tu_edpt_dir(ep_addr);

  for (uint32_t i = 0; i < (uint32_t)CFG_TUH_FSDEV_ENDPOINT_MAX; i++) {
    hcd_endpoint_t* edpt = &_hcd_data.edpt[i];
    tusb_dir_t const dir = tu_edpt_dir(edpt->ep_addr);
    uint8_t const num = tu_edpt_number(edpt->ep_addr);
    // Match both ep_num and ep_dir, or match ep_num 0 (control endpoint)
    if (edpt->allocated == 1 && edpt->dev_addr == dev_addr && num == ep_num &&
        (dir == ep_dir || ep_num == 0)) {
      return i;
    }
  }
  return TUSB_INDEX_INVALID_8;
}

// close an opened endpoint
static void edpoint_close(uint8_t ep_id) {
  hcd_endpoint_t* edpt = &_hcd_data.edpt[ep_id];
  endpoint_dealloc(edpt);

  // disable active channel belong to this endpoint
  for (uint8_t i = 0; i < FSDEV_EP_COUNT; i++) {
    hcd_channel_t* channel = &_hcd_data.channel[i];
    uint32_t ch_reg = ch_read(i) | USB_EP_CTR_TX | USB_EP_CTR_RX;
    if (channel->out.allocated == 1 && channel->out.edpt == edpt) {
      channel_dealloc(channel, TUSB_DIR_OUT);
      channel_write_status(i, ch_reg, TUSB_DIR_OUT, EP_STAT_DISABLED, true);
    }
    if (channel->in.allocated == 1 && channel->in.edpt == edpt) {
      channel_dealloc(channel, TUSB_DIR_IN);
      channel_write_status(i, ch_reg, TUSB_DIR_IN, EP_STAT_DISABLED, true);
    }
  }
}

// Allocate PMA buffer
static uint32_t hcd_pma_alloc(uint8_t channel, tusb_dir_t dir, uint16_t len) {
  (void) len;
  // Simple static allocation as we are unlikely to handle ISO endpoints in host mode
  // We just give each channel two buffers of max packet size (64 bytes) for IN and OUT

  uint16_t addr = FSDEV_BTABLE_BASE + 8 * FSDEV_EP_COUNT;
  addr += channel * TUSB_EPSIZE_BULK_FS * 2 + (dir == TUSB_DIR_IN ? TUSB_EPSIZE_BULK_FS : 0);

  TU_ASSERT(addr <= CFG_TUSB_FSDEV_PMA_SIZE, 0xFFFF);

  return addr;
}

// Allocate hardware channel
static uint8_t channel_alloc(uint8_t dev_addr, uint8_t ep_addr, uint8_t ep_type) {
  uint8_t const ep_num = tu_edpt_number(ep_addr);
  tusb_dir_t const dir = tu_edpt_dir(ep_addr);

  // Find channel allocate for same ep_num but other direction
  tusb_dir_t const other_dir = (dir == TUSB_DIR_IN) ? TUSB_DIR_OUT : TUSB_DIR_IN;
  for (uint8_t i = 0; i < FSDEV_EP_COUNT; i++) {
    uint8_t const allocated_dir = (dir == TUSB_DIR_OUT) ? _hcd_data.channel[i].out.allocated : _hcd_data.channel[i].in.allocated;
    uint8_t const allocated_other = (other_dir == TUSB_DIR_OUT) ? _hcd_data.channel[i].out.allocated : _hcd_data.channel[i].in.allocated;
    if (allocated_dir == 0 &&
        allocated_other == 1 &&
        _hcd_data.channel[i].dev_addr == dev_addr &&
        _hcd_data.channel[i].ep_num == ep_num &&
        _hcd_data.channel[i].ep_type == ep_type) {
      if (dir == TUSB_DIR_OUT) {
        _hcd_data.channel[i].out.allocated = 1;
        _hcd_data.channel[i].out.retry = 0;
      } else {
        _hcd_data.channel[i].in.allocated = 1;
        _hcd_data.channel[i].in.retry = 0;
      }
      return i;
    }
  }

  // Find free channel
  for (uint8_t i = 0; i < FSDEV_EP_COUNT; i++) {
    if (_hcd_data.channel[i].out.allocated == 0 && _hcd_data.channel[i].in.allocated == 0) {
      _hcd_data.channel[i].dev_addr = dev_addr;
      _hcd_data.channel[i].ep_num = ep_num;
      _hcd_data.channel[i].ep_type = ep_type;
      if (dir == TUSB_DIR_OUT) {
        _hcd_data.channel[i].out.allocated = 1;
        _hcd_data.channel[i].out.retry = 0;
      } else {
        _hcd_data.channel[i].in.allocated = 1;
        _hcd_data.channel[i].in.retry = 0;
      }
      return i;
    }
  }

  // Allocation failed
  return TUSB_INDEX_INVALID_8;
}

// kick-off transfer with an endpoint
static bool edpt_xfer_kickoff(uint8_t ep_id) {
  hcd_endpoint_t* edpt = &_hcd_data.edpt[ep_id];
  uint8_t ch_id = channel_alloc(edpt->dev_addr, edpt->ep_addr, edpt->ep_type);
  TU_ASSERT(ch_id != TUSB_INDEX_INVALID_8); // all channel are in used

  tusb_dir_t const dir = tu_edpt_dir(edpt->ep_addr);
  hcd_channel_t* channel = &_hcd_data.channel[ch_id];
  if (dir == TUSB_DIR_OUT) {
    channel->out.edpt = edpt;
  } else {
    channel->in.edpt = edpt;
  }

  return channel_xfer_start(ch_id, dir);
}

static bool channel_xfer_start(uint8_t ch_id, tusb_dir_t dir) {
  hcd_channel_t* channel = &_hcd_data.channel[ch_id];
  hcd_endpoint_t* edpt = (dir == TUSB_DIR_OUT) ? channel->out.edpt : channel->in.edpt;

  uint32_t ch_reg = ch_read(ch_id) & ~USB_EPREG_MASK;
  ch_reg |= tu_edpt_number(edpt->ep_addr) | edpt->dev_addr << USB_CHEP_DEVADDR_Pos |
          USB_EP_CTR_TX | USB_EP_CTR_RX;

  // Set type
  switch (edpt->ep_type) {
    case TUSB_XFER_BULK:
      ch_reg |= USB_EP_BULK;
      break;
    case TUSB_XFER_INTERRUPT:
      ch_reg |= USB_EP_INTERRUPT;
      break;

    case TUSB_XFER_CONTROL:
      ch_reg |= USB_EP_CONTROL;
      break;

    default:
      // Note: ISO endpoint is unsupported
      TU_ASSERT(false);
  }

  /* Create a packet memory buffer area. */
  uint16_t pma_addr = hcd_pma_alloc(ch_id, dir, edpt->max_packet_size);
  btable_set_addr(ch_id, dir == TUSB_DIR_OUT ? BTABLE_BUF_TX : BTABLE_BUF_RX, pma_addr);

  if (dir == TUSB_DIR_OUT) {
    uint16_t const len = tu_min16(edpt->buflen - edpt->queued_len, edpt->max_packet_size);
    tu_hwfifo_write(PMA_BUF_AT(pma_addr), &(edpt->buffer[edpt->queued_len]), len, NULL);
    btable_set_count(ch_id, BTABLE_BUF_TX, len);

    edpt->queued_len += len;
  } else {
    btable_set_rx_bufsize(ch_id, BTABLE_BUF_RX, edpt->max_packet_size);
  }

  if (edpt->ls_pre == 1) {
    ch_reg |= USB_CHEP_LSEP;
  } else {
    ch_reg &= ~USB_CHEP_LSEP;
  }

  // Setup DATA/STATUS phase start with DATA1
  if (tu_edpt_number(edpt->ep_addr) == 0) {
    edpt->pid = 1;
  }

  if (edpt->next_setup) {
    edpt->next_setup = false;
    ch_reg |= USB_EP_SETUP;
    edpt->pid = 0;
  }

  ch_change_status(&ch_reg, dir, EP_STAT_VALID);
  ch_change_dtog(&ch_reg, dir, edpt->pid);
  ch_reg &= USB_EPREG_MASK | CH_STAT_MASK(dir) | CH_DTOG_MASK(dir);
  ch_write(ch_id, ch_reg, true);

  return true;
}

#endif
